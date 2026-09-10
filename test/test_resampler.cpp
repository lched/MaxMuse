// Simulation tests for maxmuse::stream_resampler.
//
// A Muse-like device is simulated (independent clock with drift, samples delivered
// in Bluetooth-like packets with random arrival jitter) and fed to the resampler
// while an "audio thread" pulls vectors at the audio rate. We check that:
//   * the output never jumps (no clicks), including across dropouts, lost packets,
//     resyncs, sample-rate changes and interpolation changes;
//   * every channel is reproduced accurately (the interpolated value matches the
//     analytic input signal at the reported read position);
//   * the drift controller converges to the real clock ratio without underruns;
//   * a constant input (EEG DC offset) is reproduced exactly (no ripple/buzz);
//   * spectral images of the upsampling are strongly attenuated.
// Plus a real multi-threaded run (meant to be executed under ThreadSanitizer).

#include "maxmuse_resampler.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <thread>
#include <vector>

using maxmuse::interp_mode;
using maxmuse::stream_resampler;

namespace {

const double kPi = 3.14159265358979323846;
int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("    [%s] %s\n", ok ? " ok " : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

const char* mode_name(interp_mode m) {
    return m == interp_mode::linear ? "linear" : m == interp_mode::cubic ? "cubic" : "sinc";
}

struct scenario {
    std::string name;
    int channels = 5;           // Muse EEG: TP9, AF7, AF8, TP10, Right AUX
    double rate = 256.0;        // nominal LSL rate
    int packet = 12;            // frames per Bluetooth packet
    bool frame_by_frame = false;// push 1 frame at a time (muselsl ACC/GYRO behaviour)
    double drift = 0.0;         // device clock error (0.002 = +0.2 %)
    double jitter = 0.03;       // uniform arrival jitter, seconds
    double out_rate = 48000.0;
    int block = 64;
    bool random_blocks = false;
    double duration = 40.0;
    interp_mode mode = interp_mode::sinc;
    double latency = 0.0;       // 0 = auto
    double amplitude = 50.0;
    double dc = 800.0;
    bool constant = false;      // DC-only input
    // events
    double stall_at = -1.0, stall_len = 0.0;
    bool stall_lost = false;    // lost: data during the stall never arrives; else it arrives late
    double lose_packet_every = 0.0;
    double out_rate_change_at = -1.0, out_rate2 = 44100.0;
    double mode_change_at = -1.0;
    interp_mode mode2 = interp_mode::cubic;
    // expectations
    bool check_accuracy = true;
    double accuracy_tol = 0.0;  // absolute, same unit as the signal (µV)
    bool expect_no_underrun = true;
    bool expect_no_resync = true;
};

double freq_fraction(int c) {
    static const double f[] = {0.005, 0.028, 0.04, 0.089, 0.123};
    return f[c % 5];
}

double signal(const scenario& s, int c, double t) {
    const double dc = s.dc + 37.0 * c;
    if (s.constant)
        return dc;
    return dc + s.amplitude * std::sin(2.0 * kPi * freq_fraction(c) * s.rate * t + 0.7 * c);
}

struct packet_event {
    double arrival;
    long first_frame;
    int n;
};

// Build the packet delivery schedule.
std::vector<packet_event> make_schedule(const scenario& s, std::mt19937& rng) {
    std::uniform_real_distribution<double> jit(0.0, s.jitter);
    const double actual = s.rate * (1.0 + s.drift);
    std::vector<packet_event> ev;
    double last_arrival = 0.0;
    double next_loss = s.lose_packet_every > 0 ? s.lose_packet_every : 1e30;
    const long total = static_cast<long>((s.duration + 2.0) * actual);
    for (long k = 0; k + s.packet <= total; k += s.packet) {
        const double sent = (k + s.packet - 1) / actual;
        double arrival = sent + 0.01 + jit(rng);
        if (sent >= next_loss) {
            next_loss += s.lose_packet_every;
            continue; // single packet lost over the air
        }
        if (s.stall_at >= 0 && arrival >= s.stall_at && arrival < s.stall_at + s.stall_len) {
            if (s.stall_lost)
                continue;
            arrival = s.stall_at + s.stall_len;
        }
        arrival = std::max(arrival, last_arrival); // in-order delivery
        last_arrival = arrival;
        ev.push_back({arrival, k, s.packet});
    }
    return ev;
}

void run_scenario(const scenario& s) {
    std::printf("\n== %s (%d ch @ %.0f Hz -> %.0f Hz, %s, drift %+.2f %%, jitter %.0f ms)\n", s.name.c_str(), s.channels, s.rate,
                s.out_rate, mode_name(s.mode), 100.0 * s.drift, 1000.0 * s.jitter);

    std::mt19937 rng(1234);
    const auto schedule = make_schedule(s, rng);
    const double actual = s.rate * (1.0 + s.drift);

    stream_resampler rs;
    rs.configure(s.channels, s.rate);
    rs.set_interp(s.mode);
    rs.set_latency(s.latency);
    rs.set_output_rate(s.out_rate);

    std::vector<float> frames;
    std::vector<double> stamps;
    std::vector<std::vector<double>> out(s.channels, std::vector<double>(4096));
    std::vector<double*> outp(s.channels);
    std::vector<double> pos(4096);
    std::vector<double> prev(s.channels, 0.0);
    bool have_prev = false;

    double out_rate = s.out_rate;
    double t = 0.0;
    std::size_t next_ev = 0;
    std::uniform_int_distribution<int> blk(1, 1024);

    // allowed per-sample change: true signal slope + declick ramp of a jump of up to (2A + margin)
    auto allowed_step = [&](int c, double sr) {
        const double slope = s.constant ? 0.0 : s.amplitude * 2.0 * kPi * freq_fraction(c) * s.rate / sr;
        const double ramp = (2.0 * s.amplitude + 40.0 * c + 1.0) * kPi / (2.0 * stream_resampler::declick_seconds * sr);
        return 1.25 * slope * 1.06 + ramp + 1e-9;
    };

    double max_ratio = 0.0, max_err = 0.0, max_dc_err = 0.0;
    double worst_step = 0.0, worst_allowed = 0.0;
    double fill_min = 1e9, fill_max = -1e9;
    double corr_sum = 0.0;
    long corr_n = 0;
    bool mode_changed = false, rate_changed = false;
    double last_event_t = 0.0;

    while (t < s.duration) {
        if (s.out_rate_change_at >= 0 && !rate_changed && t >= s.out_rate_change_at) {
            out_rate = s.out_rate2;
            rs.set_output_rate(out_rate);
            rate_changed = true;
        }
        if (s.mode_change_at >= 0 && !mode_changed && t >= s.mode_change_at) {
            rs.set_interp(s.mode2);
            mode_changed = true;
            last_event_t = t;
        }
        // deliver everything that has arrived by now
        while (next_ev < schedule.size() && schedule[next_ev].arrival <= t) {
            const auto& e = schedule[next_ev++];
            frames.resize(static_cast<std::size_t>(e.n) * s.channels);
            stamps.resize(e.n);
            for (int i = 0; i < e.n; ++i) {
                const double ft = (e.first_frame + i) / actual;
                stamps[i] = 1000.0 + ft; // smooth, sample-accurate stamps (like muselsl)
                for (int c = 0; c < s.channels; ++c)
                    frames[static_cast<std::size_t>(i) * s.channels + c] = static_cast<float>(signal(s, c, ft));
            }
            if (s.frame_by_frame) {
                for (int i = 0; i < e.n; ++i)
                    rs.push(&frames[static_cast<std::size_t>(i) * s.channels], &stamps[i], 1, e.arrival + 1e-4 * i);
            }
            else
                rs.push(frames.data(), stamps.data(), e.n, e.arrival);
        }

        const int n = s.random_blocks ? blk(rng) : s.block;
        for (int c = 0; c < s.channels; ++c)
            outp[c] = out[c].data();
        rs.process(outp.data(), s.channels, n, pos.data());

        for (int i = 0; i < n; ++i) {
            const double ti = t + i / out_rate;
            for (int c = 0; c < s.channels; ++c) {
                const double y = out[c][i];
                if (have_prev && ti > 0.5) {
                    const double step = std::fabs(y - prev[c]);
                    const double allowed = allowed_step(c, out_rate);
                    if (step / allowed > max_ratio) {
                        max_ratio = step / allowed;
                        worst_step = step;
                        worst_allowed = allowed;
                    }
                }
                prev[c] = y;
                // accuracy: compare with the analytic signal at the read position,
                // skipping the start-up and 0.1 s after any declick-triggering event
                if (s.check_accuracy && !std::isnan(pos[i]) && ti > 2.0 && ti - last_event_t > 0.1) {
                    const double ideal = signal(s, c, pos[i] / actual);
                    max_err = std::max(max_err, std::fabs(y - ideal));
                }
                if (s.constant && ti > 1.0)
                    max_dc_err = std::max(max_dc_err, std::fabs(y - (s.dc + 37.0 * c)));
            }
        }
        have_prev = true;
        t += n / out_rate;
        const auto st = rs.stats();
        if (t > 10.0 && st.state == 1) {
            fill_min = std::min(fill_min, st.fill_seconds);
            fill_max = std::max(fill_max, st.fill_seconds);
        }
        if (t > s.duration - 10.0) {
            corr_sum += st.rate_correction;
            ++corr_n;
        }
    }

    const auto st = rs.stats();
    const double corr_mean = corr_n ? corr_sum / corr_n : 0.0;
    std::printf("    target %.0f ms, fill [%.1f, %.1f] ms, correction %+.4f %% now / %+.4f %% mean of last 10 s (true drift %+.4f %%)\n",
                1000 * st.target_seconds, 1000 * fill_min, 1000 * fill_max, 100 * st.rate_correction, 100 * corr_mean, 100 * s.drift);
    std::printf("    underruns %llu, resyncs %llu, overflows %llu, gaps filled %llu, discontinuities %llu\n",
                (unsigned long long)st.underruns, (unsigned long long)st.resyncs, (unsigned long long)st.overflows,
                (unsigned long long)st.gaps_filled, (unsigned long long)st.discontinuities);
    std::printf("    worst sample-to-sample step %.4g (allowed %.4g), max interpolation error %.4g\n", worst_step, worst_allowed, max_err);

    check(max_ratio <= 1.0, "no discontinuity / click on any channel");
    if (s.check_accuracy && !s.constant)
        check(max_err <= s.accuracy_tol, "all channels match the input signal (tol " + std::to_string(s.accuracy_tol) + ")");
    if (s.constant)
        check(max_dc_err < 1e-6, "constant input reproduced exactly (err " + std::to_string(max_dc_err) + ")");
    if (s.expect_no_underrun)
        check(st.underruns == 0, "no underrun");
    if (s.expect_no_resync)
        check(st.resyncs == 0, "no resync");
    if (s.stall_at < 0 && s.lose_packet_every == 0 && s.mode_change_at < 0 && s.out_rate_change_at < 0)
        check(std::fabs(corr_mean - s.drift) < 0.0005 && std::fabs(st.rate_correction - s.drift) < 0.0015,
              "drift controller converged to the device clock");
    check(st.overflows == 0, "no queue overflow");
}

// ---------------------------------------------------------------------------------
// Spectral image rejection (quality of the band-limited interpolation)
// ---------------------------------------------------------------------------------

double goertzel_db(const std::vector<double>& x, double f, double sr) {
    const double w = 2.0 * kPi * f / sr;
    double re = 0.0, im = 0.0;
    const std::size_t n = x.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double win = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (n - 1));
        re += win * x[i] * std::cos(w * i);
        im -= win * x[i] * std::sin(w * i);
    }
    return 20.0 * std::log10(std::sqrt(re * re + im * im) + 1e-300);
}

void test_images(interp_mode mode, double max_image_db) {
    const double rate = 256.0, sr = 48000.0, f0 = 20.0;
    stream_resampler rs;
    rs.configure(1, rate);
    rs.set_interp(mode);
    rs.set_latency(0.1);
    rs.set_output_rate(sr);
    std::vector<double> y;
    std::vector<double> buf(64);
    double* bp = buf.data();
    long frame = 0;
    const double seconds = 12.0;
    for (long b = 0; b < static_cast<long>(seconds * sr / 64); ++b) {
        const double t = b * 64 / sr;
        while (frame < static_cast<long>((t + 0.05) * rate)) { // steady delivery, 50 ms ahead
            float v = static_cast<float>(std::sin(2.0 * kPi * f0 * frame / rate));
            double ts = 1.0 + frame / rate;
            rs.push(&v, &ts, 1, t);
            ++frame;
        }
        rs.process(&bp, 1, 64);
        if (t > 4.0)
            y.insert(y.end(), buf.begin(), buf.end());
    }
    const double ref = goertzel_db(y, f0, sr);
    double worst = -400.0;
    for (int k = 1; k <= 3; ++k)
        for (double img : {k * rate - f0, k * rate + f0})
            worst = std::max(worst, goertzel_db(y, img, sr) - ref);
    std::printf("    %-6s strongest image: %6.1f dB below the signal\n", mode_name(mode), -worst);
    check(worst < max_image_db, std::string(mode_name(mode)) + " image rejection better than " + std::to_string((int)-max_image_db) + " dB");
}

// ---------------------------------------------------------------------------------
// Real threads (run under ThreadSanitizer in CI)
// ---------------------------------------------------------------------------------

void test_threads() {
    std::printf("\n== real producer / audio / control threads (3 s)\n");
    const int ch = 5;
    const double rate = 256.0, sr = 48000.0;
    stream_resampler rs;
    rs.configure(ch, rate);
    rs.set_output_rate(sr);
    std::atomic<bool> run{true};
    const auto t0 = std::chrono::steady_clock::now();
    auto now = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };

    std::thread producer([&] {
        std::vector<float> f(12 * ch);
        std::vector<double> ts(12);
        long k = 0;
        // Absolute schedule: a late wake-up (common on CI VMs) must not delay every
        // following packet, otherwise the "device" runs slower than its nominal rate.
        auto next = std::chrono::steady_clock::now();
        while (run.load()) {
            for (int i = 0; i < 12; ++i, ++k) {
                ts[i] = 1.0 + k / rate;
                for (int c = 0; c < ch; ++c)
                    f[i * ch + c] = static_cast<float>(100.0 * std::sin(2.0 * kPi * (c + 1) * k / rate));
            }
            rs.push(f.data(), ts.data(), 12, now());
            next += std::chrono::microseconds(46875); // 12 frames @ 256 Hz
            std::this_thread::sleep_until(next);
        }
    });
    std::thread control([&] {
        int i = 0;
        while (run.load()) {
            rs.set_interp(static_cast<interp_mode>(i % 3));
            rs.set_latency(i % 2 ? 0.0 : 0.15);
            (void)rs.stats();
            ++i;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    });

    std::vector<std::vector<double>> out(ch, std::vector<double>(64));
    std::vector<double*> op(ch);
    for (int c = 0; c < ch; ++c)
        op[c] = out[c].data();
    double max_abs = 0.0;
    bool finite = true;
    const auto start = std::chrono::steady_clock::now();
    for (long b = 0;; ++b) {
        const auto due = start + std::chrono::duration<double>(b * 64 / sr);
        std::this_thread::sleep_until(due);
        if (now() > 3.0)
            break;
        rs.process(op.data(), ch, 64);
        for (int c = 0; c < ch; ++c)
            for (double v : out[c]) {
                finite = finite && std::isfinite(v);
                max_abs = std::max(max_abs, std::fabs(v));
            }
    }
    run.store(false);
    producer.join();
    control.join();
    const auto st = rs.stats();
    std::printf("    frames in %llu, underruns %llu, peak |y| %.1f\n", (unsigned long long)st.frames_in,
                (unsigned long long)st.underruns, max_abs);
    check(finite && max_abs < 110.0, "output finite and bounded");
    // ~768 frames expected in 3 s; allow for a heavily loaded runner.
    check(st.frames_in > 500, "data flowed through all threads (" + std::to_string(st.frames_in) + " frames)");
}

} // namespace

int main() {
    std::vector<scenario> all;

    scenario eeg;
    eeg.name = "Muse EEG, steady";
    eeg.drift = 0.002;
    eeg.accuracy_tol = 0.02;
    all.push_back(eeg);

    scenario eeg2 = eeg;
    eeg2.name = "Muse EEG, slow clock, 44.1 kHz, random vector sizes";
    eeg2.drift = -0.005;
    eeg2.out_rate = 44100.0;
    eeg2.random_blocks = true;
    all.push_back(eeg2);

    scenario eegc = eeg;
    eegc.name = "Muse EEG, cubic";
    eegc.mode = interp_mode::cubic;
    eegc.accuracy_tol = 0.6;
    all.push_back(eegc);

    scenario eegl = eeg;
    eegl.name = "Muse EEG, linear, fixed 60 ms latency, 512-sample vectors";
    eegl.mode = interp_mode::linear;
    eegl.latency = 0.06;
    eegl.block = 512;
    eegl.jitter = 0.01;
    eegl.accuracy_tol = 4.0;
    all.push_back(eegl);

    scenario ppg;
    ppg.name = "Muse PPG";
    ppg.channels = 3;
    ppg.rate = 64.0;
    ppg.packet = 6;
    ppg.drift = 0.001;
    ppg.jitter = 0.04;
    ppg.accuracy_tol = 0.02;
    all.push_back(ppg);

    scenario acc;
    acc.name = "Muse ACC (pushed frame by frame)";
    acc.channels = 3;
    acc.rate = 52.0;
    acc.packet = 3;
    acc.frame_by_frame = true;
    acc.drift = -0.002;
    acc.accuracy_tol = 0.02;
    all.push_back(acc);

    scenario dc = eeg;
    dc.name = "Constant input (EEG DC offset), sinc";
    dc.constant = true;
    all.push_back(dc);

    scenario late = eeg;
    late.name = "Bluetooth stall, data arrives late (backlog)";
    late.stall_at = 15.0;
    late.stall_len = 0.6;
    late.expect_no_underrun = false;
    late.expect_no_resync = false;
    late.check_accuracy = false;
    all.push_back(late);

    scenario lost = eeg;
    lost.name = "1 s dropout, data lost";
    lost.stall_at = 15.0;
    lost.stall_len = 1.0;
    lost.stall_lost = true;
    lost.expect_no_underrun = false;
    lost.expect_no_resync = false;
    lost.check_accuracy = false;
    all.push_back(lost);

    scenario loss = eeg;
    loss.name = "Single lost packet every 3 s (concealed with timestamps)";
    loss.lose_packet_every = 3.0;
    loss.check_accuracy = false;
    all.push_back(loss);

    scenario sw = eeg;
    sw.name = "Sample-rate change 48k -> 44.1k and interpolation change mid-stream";
    sw.out_rate_change_at = 12.0;
    sw.mode_change_at = 20.0;
    sw.mode2 = interp_mode::cubic;
    sw.accuracy_tol = 0.6;
    all.push_back(sw);

    for (const auto& s : all)
        run_scenario(s);

    std::printf("\n== spectral images (20 Hz tone @ 256 Hz -> 48 kHz)\n");
    test_images(interp_mode::sinc, -60.0);
    test_images(interp_mode::cubic, -25.0);
    test_images(interp_mode::linear, -20.0);

    test_threads();

    std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL TESTS PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
