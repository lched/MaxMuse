// End-to-end test: a fake Muse EEG outlet on this machine -> maxmuse::lsl_listener
// -> maxmuse::stream_resampler -> a consumer running at audio pace.
// Returns 77 (CTest "skipped") if LSL cannot resolve streams on this machine
// (some CI sandboxes block multicast).

#include "maxmuse_lsl.h"
#include "maxmuse_resampler.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {
const double kPi = 3.14159265358979323846;
int g_failures = 0;
void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}
double channel_signal(int c, double t) { return 100.0 * (c + 1) + 20.0 * std::sin(2.0 * kPi * (c + 1) * t); }
double since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
} // namespace

int main() {
    // ---- argument parsing ----
    std::printf("argument parsing\n");
    using maxmuse::parse_arguments;
    using R = maxmuse::raw_argument;
    auto a1 = parse_arguments({});
    check(a1.prop == "type" && a1.value == "EEG" && a1.channels == 0, "[] -> type EEG");
    auto a2 = parse_arguments({R{"PPG"}, R{"3", true, 3}});
    check(a2.prop == "type" && a2.value == "PPG" && a2.channels == 3, "[PPG 3] -> type PPG, 3 outlets");
    auto a3 = parse_arguments({R{"name"}, R{"Muse-1234"}, R{"5", true, 5}});
    check(a3.prop == "name" && a3.value == "Muse-1234" && a3.channels == 5, "[name Muse-1234 5]");
    auto a4 = parse_arguments({R{"source_id"}, R{"1234", true, 1234}});
    check(a4.prop == "source_id" && a4.value == "1234" && a4.channels == 0, "[source_id 1234] is a value, not a channel count");

    // ---- loopback ----
    std::printf("LSL loopback (liblsl %d)\n", lsl::library_version());
    const int ch = 5;
    const double rate = 256.0, sr = 48000.0;
    std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::string type = "MaxMuseTest" + std::to_string(rng() % 1000000);
    const char* labels[] = {"TP9", "AF7", "AF8", "TP10", "Right AUX"};

    lsl::stream_info info("MaxMuseLoopback", type, ch, rate, lsl::cf_float32, "maxmuse-" + type);
    auto chans = info.desc().append_child("channels");
    for (const char* l : labels)
        chans.append_child("channel").append_child_value("label", l).append_child_value("unit", "microvolts");
    lsl::stream_outlet outlet(info, 12);

    std::atomic<bool> produce{true};
    std::thread producer([&] {
        std::vector<float> buf(12 * ch);
        long k = 0;
        const double t0 = lsl::local_clock();
        auto next = std::chrono::steady_clock::now();
        while (produce.load()) {
            for (int i = 0; i < 12; ++i, ++k)
                for (int c = 0; c < ch; ++c)
                    buf[i * ch + c] = static_cast<float>(channel_signal(c, k / rate));
            outlet.push_chunk_multiplexed(buf.data(), buf.size(), t0 + (k - 1) / rate);
            next += std::chrono::microseconds(46875); // 12 samples @ 256 Hz, like a Muse
            std::this_thread::sleep_until(next);
        }
    });

    maxmuse::stream_resampler rs;
    rs.set_output_rate(sr);
    std::mutex desc_mutex;
    maxmuse::stream_description got;
    std::atomic<bool> connected{false};

    maxmuse::lsl_listener listener("type", type);
    listener.start(
        {},
        [&](const maxmuse::stream_description& d) {
            rs.configure(d.channels, d.rate);
            std::lock_guard<std::mutex> lock(desc_mutex);
            got = d;
            connected.store(true);
            return true;
        },
        [&](const float* f, const double* ts, std::size_t n) { rs.push(f, ts, n, maxmuse::monotonic_seconds()); },
        [](const std::string& m, bool err) { std::printf("    listener: %s%s\n", err ? "ERROR " : "", m.c_str()); });

    const auto t_wait = std::chrono::steady_clock::now();
    while (!connected.load() && since(t_wait) < 10.0)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (!connected.load()) {
        std::printf("  SKIP: LSL could not resolve a local stream on this machine\n");
        produce.store(false);
        producer.join();
        listener.stop();
        return 77;
    }
    std::printf("  connected after %.2f s to %s\n", since(t_wait), got.summary().c_str());
    check(got.channels == ch && got.rate == rate, "channel count and rate");
    bool labels_ok = got.labels.size() == static_cast<std::size_t>(ch);
    for (int c = 0; labels_ok && c < ch; ++c)
        labels_ok = got.labels[c] == labels[c];
    check(labels_ok, "channel labels read from the stream metadata: " + got.label_list());

    // consumer at audio pace for 4 s
    std::vector<std::vector<double>> out(ch, std::vector<double>(64));
    std::vector<double*> op(ch);
    for (int c = 0; c < ch; ++c)
        op[c] = out[c].data();
    std::vector<double> prev(ch, 0.0), sum(ch, 0.0), maxstep(ch, 0.0);
    long counted = 0;
    const auto start = std::chrono::steady_clock::now();
    for (long b = 0;; ++b) {
        std::this_thread::sleep_until(start + std::chrono::duration<double>(b * 64 / sr));
        const double t = since(start);
        if (t > 4.0)
            break;
        rs.process(op.data(), ch, 64);
        for (int i = 0; i < 64; ++i)
            for (int c = 0; c < ch; ++c) {
                if (t > 1.0) {
                    maxstep[c] = std::max(maxstep[c], std::fabs(out[c][i] - prev[c]));
                    sum[c] += out[c][i];
                    if (c == 0)
                        ++counted;
                }
                prev[c] = out[c][i];
            }
    }
    const auto st = rs.stats();
    std::printf("  frames in %llu, underruns %llu, resyncs %llu, buffer %.0f ms (target %.0f ms), packet gap %.0f ms\n",
                (unsigned long long)st.frames_in, (unsigned long long)st.underruns, (unsigned long long)st.resyncs,
                1000 * st.fill_seconds, 1000 * st.target_seconds, 1000 * st.peak_gap_seconds);
    check(st.frames_in > 0.8 * 4.0 * rate, "samples flowed from the outlet through the listener");
    check(st.state == 1, "resampler running");
    bool mapping_ok = true, smooth_ok = true;
    for (int c = 0; c < ch; ++c) {
        const double mean = sum[c] / std::max(1L, counted);
        const double slope = 20.0 * 2.0 * kPi * (c + 1) / sr;
        std::printf("    %-9s mean %7.2f (expected %6.1f), max step %.5f (signal slope %.5f)\n", labels[c], mean, 100.0 * (c + 1),
                    maxstep[c], slope);
        mapping_ok = mapping_ok && std::fabs(mean - 100.0 * (c + 1)) < 5.0;
        smooth_ok = smooth_ok && maxstep[c] < 1.5 * slope;
    }
    check(mapping_ok, "every channel arrives on its own output, in order");
    check(smooth_ok, "no clicks on any channel");

    // shutdown must be prompt (object deletion in Max)
    auto t_stop = std::chrono::steady_clock::now();
    listener.stop();
    const double stop_s = since(t_stop);
    check(stop_s < 1.0, "listener stops promptly while streaming (" + std::to_string(stop_s) + " s)");
    produce.store(false);
    producer.join();

    maxmuse::lsl_listener waiting("type", "MaxMuseDoesNotExist");
    waiting.start({}, [](const maxmuse::stream_description&) { return true; }, [](const float*, const double*, std::size_t) {},
                  [](const std::string&, bool) {});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    t_stop = std::chrono::steady_clock::now();
    waiting.stop();
    const double stop2 = since(t_stop);
    check(stop2 < 1.0, "listener stops promptly while still searching (" + std::to_string(stop2) + " s)");

    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL TESTS PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
