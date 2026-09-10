/// @file       MaxMuse.receiver_tilde.cpp
/// @ingroup    MaxMuse
/// @brief      Receive an LSL stream (Muse 2 EEG / PPG / ACC / GYRO, or any numeric
///             LSL stream) as audio-rate signals, one outlet per channel.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.
///
/// Usage:  [MaxMuse.receiver~ <property> <value> [<channels>]]
///         [MaxMuse.receiver~ type EEG]      resolve now (up to 2 s) and create one outlet per channel
///         [MaxMuse.receiver~ type EEG 5]    create 5 outlets now, connect whenever the stream appears
///         [MaxMuse.receiver~ PPG 3]         shorthand for "type PPG 3"
///
/// The stream is converted to the audio rate by maxmuse::stream_resampler
/// (band-limited interpolation + clock-drift compensation + click-free handling of
/// dropouts). See source/shared/maxmuse_resampler.h.

#include "c74_min.h"
#include "maxmuse_lsl.h"
#include "maxmuse_resampler.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace c74::min;

namespace {

std::vector<maxmuse::raw_argument> to_raw(const atoms& args) {
    std::vector<maxmuse::raw_argument> raw;
    for (const auto& a : args) {
        maxmuse::raw_argument r;
        if (a.type() == message_type::int_argument || a.type() == message_type::float_argument) {
            r.is_number = true;
            r.number = static_cast<double>(a);
            const double n = r.number;
            r.text = (n == static_cast<double>(static_cast<long long>(n))) ? std::to_string(static_cast<long long>(n)) : std::to_string(n);
        }
        else
            r.text = static_cast<std::string>(a);
        raw.push_back(r);
    }
    return raw;
}

std::string fmt(const char* format, double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), format, v);
    return buf;
}

} // namespace

class receiver_tilde : public object<receiver_tilde>, public vector_operator<> {
  public:
    MIN_DESCRIPTION{"Receive an LSL stream (e.g. the EEG, PPG, ACC or GYRO stream of a Muse 2 headband) as audio-rate signals, "
                    "one outlet per channel. Arguments: [property value] [channels], e.g. 'type EEG 5'. When the number of "
                    "channels is given the object is created immediately and connects as soon as the stream appears. The stream "
                    "is resampled in real time with band-limited interpolation, clock-drift compensation and click-free handling "
                    "of Bluetooth dropouts."};
    MIN_TAGS{"lsl, eeg, muse, biosignals"};
    MIN_AUTHOR{"Léo Chédin"};
    MIN_RELATED{"MaxMuse.receiver, snapshot~, scope~"};

  private:
    // ---- state used by the LSL thread callbacks: declared first so it outlives them ----
    maxmuse::stream_resampler m_rs;
    maxmuse::object_arguments m_args;
    int m_n_outlets = 0;

    std::mutex m_log_mutex;
    std::vector<std::pair<std::string, bool>> m_log;

    mutable std::mutex m_info_mutex;
    maxmuse::stream_description m_info;
    bool m_has_info = false;

    std::vector<std::unique_ptr<outlet<>>> m_outlets;
    std::unique_ptr<maxmuse::lsl_listener> m_listener;

    // Called from the LSL thread: hand messages over to the main thread.
    void log_async(const std::string& msg, bool is_error) {
        {
            std::lock_guard<std::mutex> lock(m_log_mutex);
            m_log.emplace_back(msg, is_error);
        }
        m_log_flush.set();
    }

    // Called from the LSL thread when a stream is found.
    bool on_connect(const maxmuse::stream_description& d) {
        if (!m_rs.configured())
            m_rs.configure(d.channels, d.rate);
        else if (d.channels != m_rs.channels() || d.rate != m_rs.input_rate()) {
            log_async("found " + d.summary() + " but this object was set up for " + std::to_string(m_rs.channels()) + " ch @ " +
                          fmt("%g", m_rs.input_rate()) + " Hz; re-create the object to use it",
                      true);
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_info_mutex);
            m_info = d;
            m_has_info = true;
        }
        std::string msg = "connected to " + d.summary() + ": " + d.label_list();
        if (d.channels > m_n_outlets)
            msg += " -- only the first " + std::to_string(m_n_outlets) + " channel(s) have an outlet";
        else if (d.channels < m_n_outlets)
            msg += " -- outlets " + std::to_string(d.channels + 1) + " to " + std::to_string(m_n_outlets) + " stay silent";
        log_async(msg, false);
        return true;
    }

    void post_status() {
        const std::string who = "[" + m_args.prop + " " + m_args.value + "] ";
        if (!m_listener) {
            cout << who << "inactive" << endl;
            return;
        }
        maxmuse::stream_description info;
        bool has_info;
        {
            std::lock_guard<std::mutex> lock(m_info_mutex);
            info = m_info;
            has_info = m_has_info;
        }
        if (m_listener->connected() && has_info)
            cout << who << "connected to " << info.summary() << endl;
        else
            cout << who << "waiting for the stream" << endl;
        if (!m_rs.configured())
            return;
        if (has_info)
            cout << "  channels: " << info.label_list() << endl;
        const auto st = m_rs.stats();
        static const char* states[] = {"waiting for data", "running", "re-buffering"};
        cout << "  resampler: " << states[st.state < 0 || st.state > 2 ? 0 : st.state] << ", buffer "
             << fmt("%.0f", 1000.0 * st.fill_seconds) << " ms (target " << fmt("%.0f", 1000.0 * st.target_seconds)
             << " ms), clock correction " << fmt("%+.3f", 100.0 * st.rate_correction) << " %, largest packet gap "
             << fmt("%.0f", 1000.0 * st.peak_gap_seconds) << " ms" << endl;
        cout << "  frames received " << st.frames_in << ", underruns " << st.underruns << ", resyncs " << st.resyncs
             << ", gaps filled " << st.gaps_filled << ", discontinuities " << st.discontinuities << ", overflows " << st.overflows
             << endl;
    }

  public:
    inlet<> m_inlet{this, "(message) status, reset, scan, attributes"};

    receiver_tilde(const atoms& args = {}) {
        if (dummy())
            return; // instance used by Max for class registration: no network, no threads

        m_args = maxmuse::parse_arguments(to_raw(args));

        std::vector<lsl::stream_info> found;
        if (m_args.channels <= 0) {
            // The outlet count depends on the stream: we have to find it now.
            found = maxmuse::resolve_now(m_args.prop, m_args.value, 2.0);
            if (found.empty())
                error("no LSL stream with " + m_args.prop + " = " + m_args.value +
                      " found. Start the stream first, or add the number of channels as last argument "
                      "(e.g. [MaxMuse.receiver~ " + m_args.prop + " " + m_args.value +
                      " 5]) to create the object now and connect when the stream appears.");
            m_n_outlets = found.front().channel_count();
        }
        else
            m_n_outlets = m_args.channels;

        const std::string prefix = found.empty() ? m_args.value : found.front().type();
        for (int c = 0; c < m_n_outlets; ++c)
            m_outlets.push_back(std::make_unique<outlet<>>(this, "(signal) " + prefix + " channel " + std::to_string(c + 1), "signal"));

        m_rs.set_output_rate(samplerate());
        m_listener = std::make_unique<maxmuse::lsl_listener>(m_args.prop, m_args.value);
        m_listener->start(
            std::move(found), [this](const maxmuse::stream_description& d) { return on_connect(d); },
            [this](const float* frames, const double* stamps, std::size_t n) {
                m_rs.push(frames, stamps, n, maxmuse::monotonic_seconds());
            },
            [this](const std::string& msg, bool is_error) { log_async(msg, is_error); });
    }

    ~receiver_tilde() {
        // Stop the LSL thread before any member it uses is destroyed.
        if (m_listener)
            m_listener->stop();
    }

    // ------------------------------------------------------------------ attributes

    attribute<number> latency{
        this, "latency", 0.0,
        description{"Jitter-buffer depth in milliseconds. 0 (default) sizes it automatically from the observed Bluetooth "
                    "packet timing. Total latency = this buffer + the interpolation look-ahead (8 samples for sinc, 2 for "
                    "cubic, 1 for linear)."},
        range{0.0, 2000.0},
        setter{MIN_FUNCTION{
            m_rs.set_latency(static_cast<double>(args[0]) / 1000.0);
            return args;
        }}};

    attribute<symbol> interp{
        this, "interp", "sinc",
        description{"Interpolation: sinc (16-tap Kaiser-windowed sinc, best quality, default), cubic (lower latency), "
                    "linear (lowest latency)."},
        range{"linear", "cubic", "sinc"},
        setter{MIN_FUNCTION{
            const std::string m = static_cast<std::string>(args[0]);
            if (m == "linear")
                m_rs.set_interp(maxmuse::interp_mode::linear);
            else if (m == "cubic")
                m_rs.set_interp(maxmuse::interp_mode::cubic);
            else if (m == "sinc")
                m_rs.set_interp(maxmuse::interp_mode::sinc);
            else {
                cerr << "interp must be linear, cubic or sinc" << endl;
                return {symbol("sinc")};
            }
            return args;
        }}};

    attribute<bool> fillgaps{
        this, "fillgaps", true,
        description{"Conceal lost Bluetooth packets (detected with the LSL timestamps) by interpolating across the gap. "
                    "Keeps the timeline and avoids clicks."},
        setter{MIN_FUNCTION{
            m_rs.set_fill_gaps(static_cast<bool>(args[0]));
            return args;
        }}};

    // ------------------------------------------------------------------ messages

    message<> dspsetup{this, "dspsetup", MIN_FUNCTION{
        m_rs.set_output_rate(static_cast<double>(args[0]));
        return {};
    }};

    message<> status{this, "status", "Post the stream and resampler status to the Max console.", MIN_FUNCTION{
        post_status();
        return {};
    }};

    message<> reset{this, "reset", "Flush the buffer and resynchronise to the live stream.", MIN_FUNCTION{
        m_rs.reset();
        return {};
    }};

    message<> scan{this, "scan", "Post every LSL stream visible on the network to the Max console (blocks ~1 s).", MIN_FUNCTION{
        const auto streams = maxmuse::list_streams(1.0);
        if (streams.empty())
            cout << "no LSL stream found on the network" << endl;
        for (const auto& s : streams)
            cout << s.summary() << ": " << s.label_list() << endl;
        return {};
    }};

    // Drains messages posted by the LSL thread, on Max's main thread.
    queue<> m_log_flush{this, MIN_FUNCTION{
        std::vector<std::pair<std::string, bool>> pending;
        {
            std::lock_guard<std::mutex> lock(m_log_mutex);
            pending.swap(m_log);
        }
        for (const auto& m : pending) {
            if (m.second)
                cerr << "[" << m_args.prop << " " << m_args.value << "] " << m.first << endl;
            else
                cout << "[" << m_args.prop << " " << m_args.value << "] " << m.first << endl;
        }
        return {};
    }};

    // ------------------------------------------------------------------ audio

    void operator()(audio_bundle input, audio_bundle output) {
        (void)input;
        m_rs.process(output.samples(), static_cast<int>(output.channel_count()), static_cast<int>(output.frame_count()));
    }
};

MIN_EXTERNAL(receiver_tilde);
