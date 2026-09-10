/// @file       maxmuse_lsl.h
/// @brief      Background LSL stream listener shared by the MaxMuse externals.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.
///
/// One thread per object:
///   * resolves the stream in the background (a Max patch can be opened before the
///     headband starts streaming) and re-resolves it if it is lost;
///   * wakes up as soon as a sample arrives and drains everything available, so no
///     latency is added on top of the Bluetooth packetisation;
///   * never blocks for more than ~0.25 s, so deleting the object never hangs Max.
/// No Max dependency, so it can be tested with a local LSL outlet (test/test_lsl_loopback.cpp).

#pragma once

#include <lsl_cpp.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace maxmuse {

struct stream_description {
    std::string name, type, source_id, hostname;
    int channels = 0;
    double rate = 0.0; ///< nominal rate, 0 = irregular
    std::vector<std::string> labels;
    std::vector<std::string> units;

    std::string summary() const {
        std::ostringstream s;
        s << "'" << name << "' (type " << type << ", " << channels << " ch @ ";
        if (rate > 0.0)
            s << rate << " Hz";
        else
            s << "irregular rate";
        if (!hostname.empty())
            s << ", on " << hostname;
        s << ")";
        return s.str();
    }

    std::string label(int c) const {
        if (c >= 0 && c < static_cast<int>(labels.size()) && !labels[c].empty())
            return labels[c];
        return type + " " + std::to_string(c + 1);
    }

    std::string label_list() const {
        std::string s;
        for (int c = 0; c < channels; ++c) {
            s += (c ? ", " : "") + label(c);
            if (c < static_cast<int>(units.size()) && !units[c].empty())
                s += " (" + units[c] + ")";
        }
        return s;
    }
};

inline stream_description describe(const lsl::stream_info& info) {
    stream_description d;
    d.name = info.name();
    d.type = info.type();
    d.source_id = info.source_id();
    d.hostname = info.hostname();
    d.channels = info.channel_count();
    d.rate = info.nominal_srate();
    // Channel metadata, if the streamer provides it (muselsl does: TP9, AF7, AF8, TP10, Right AUX).
    try {
        lsl::stream_info copy = info;
        auto ch = copy.desc().child("channels").child("channel");
        for (int c = 0; c < d.channels && !ch.empty(); ++c, ch = ch.next_sibling()) {
            d.labels.emplace_back(ch.child_value("label"));
            d.units.emplace_back(ch.child_value("unit"));
        }
    } catch (...) {
    }
    return d;
}

/// Blocking resolution of streams where `prop` == `value` (returns early when found).
inline std::vector<lsl::stream_info> resolve_now(const std::string& prop, const std::string& value, double timeout) {
    try {
        return lsl::resolve_stream(prop, value, 1, timeout);
    } catch (...) {
        return {};
    }
}

/// Blocking listing of every stream visible on the network.
inline std::vector<stream_description> list_streams(double wait_time) {
    std::vector<stream_description> out;
    try {
        for (const auto& info : lsl::resolve_streams(wait_time))
            out.push_back(describe(info));
    } catch (...) {
    }
    return out;
}

/// Destroying an lsl::stream_inlet takes ~0.5 s inside liblsl. Doing it on the
/// caller's thread would freeze Max each time a connected object is deleted, so
/// inlets are handed to this background thread instead. The thread is joined when
/// the external is unloaded (static destruction), after any pending inlet is gone.
class inlet_reaper {
  public:
    static inlet_reaper& instance() {
        static inlet_reaper r;
        return r;
    }

    void dispose(std::unique_ptr<lsl::stream_inlet> inlet) {
        if (!inlet)
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending.push_back(std::move(inlet));
        }
        m_cv.notify_one();
    }

    ~inlet_reaper() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_one();
        if (m_thread.joinable())
            m_thread.join();
    }

  private:
    inlet_reaper() { m_thread = std::thread([this] { run(); }); }

    void run() {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (;;) {
            m_cv.wait(lock, [this] { return m_quit || !m_pending.empty(); });
            while (!m_pending.empty()) {
                auto inlet = std::move(m_pending.front());
                m_pending.pop_front();
                lock.unlock();
                inlet.reset();
                lock.lock();
            }
            if (m_quit)
                return;
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::unique_ptr<lsl::stream_inlet>> m_pending;
    bool m_quit = false;
    std::thread m_thread; // started last, in the constructor body
};

class lsl_listener {
  public:
    /// Called on the listener thread when a stream is found. Return false to reject it.
    using connect_fn = std::function<bool(const stream_description&)>;
    /// Called on the listener thread for every batch of samples (interleaved frames).
    using data_fn = std::function<void(const float* frames, const double* timestamps, std::size_t n_frames)>;
    /// Called on the listener thread for status messages (error = true for errors).
    using log_fn = std::function<void(const std::string& message, bool error)>;

    lsl_listener(std::string prop, std::string value) : m_prop(std::move(prop)), m_value(std::move(value)) {}
    ~lsl_listener() { stop(); }

    lsl_listener(const lsl_listener&) = delete;
    lsl_listener& operator=(const lsl_listener&) = delete;

    /// `already_resolved` may contain a stream found synchronously by the caller.
    void start(std::vector<lsl::stream_info> already_resolved, connect_fn on_connect, data_fn on_data, log_fn on_log) {
        stop();
        m_initial = std::move(already_resolved);
        m_on_connect = std::move(on_connect);
        m_on_data = std::move(on_data);
        m_on_log = std::move(on_log);
        m_running.store(true);
        m_thread = std::thread(&lsl_listener::run, this);
    }

    void stop() {
        m_running.store(false);
        if (m_thread.joinable())
            m_thread.join();
        m_connected.store(false);
    }

    bool connected() const { return m_connected.load(); }
    const std::string& prop() const { return m_prop; }
    const std::string& value() const { return m_value; }

  private:
    static constexpr std::size_t max_chunk = 512; // frames drained per wake-up
    static constexpr double pull_timeout = 0.25;  // s, bounds the shutdown time

    void log(const std::string& m, bool error = false) {
        if (m_on_log)
            m_on_log(m, error);
    }

    void sleep_while_running(double seconds) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (m_running.load() && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Try to open an inlet on one of the candidates. Returns true when connected.
    bool try_connect(const std::vector<lsl::stream_info>& candidates) {
        for (const auto& candidate : candidates) {
            try {
                if (candidate.channel_format() == lsl::cf_string) {
                    log("stream " + describe(candidate).summary() + " carries strings (markers); it cannot be received as numbers", true);
                    continue;
                }
                // max_buflen: 30 s of backlog at most; recover: follow the stream if it restarts.
                auto inlet = std::make_unique<lsl::stream_inlet>(candidate, 30, 0, true);
                lsl::stream_info full = candidate;
                try {
                    full = inlet->info(2.0); // includes channel labels
                } catch (...) {
                }
                const stream_description d = describe(full);
                if (m_on_connect && !m_on_connect(d))
                    continue;
                try {
                    inlet->open_stream(2.0);
                } catch (...) { // will be opened implicitly by the first pull
                }
                m_channels = d.channels;
                m_frames.assign(max_chunk * static_cast<std::size_t>(m_channels), 0.0f);
                m_stamps.assign(max_chunk, 0.0);
                m_inlet = std::move(inlet);
                m_connected.store(true);
                return true;
            } catch (const std::exception& e) {
                log(std::string("could not open the stream: ") + e.what(), true);
            }
        }
        return false;
    }

    void run() {
        std::unique_ptr<lsl::continuous_resolver> resolver;
        bool announced_wait = false;

        while (m_running.load()) {
            if (!m_inlet) {
                std::vector<lsl::stream_info> found;
                if (!m_initial.empty())
                    found.swap(m_initial);
                else {
                    if (!resolver) {
                        try {
                            resolver = std::make_unique<lsl::continuous_resolver>(m_prop, m_value, 5.0);
                        } catch (const std::exception& e) {
                            log(std::string("LSL resolver error: ") + e.what(), true);
                            sleep_while_running(1.0);
                            continue;
                        }
                    }
                    try {
                        found = resolver->results();
                    } catch (...) {
                    }
                }
                if (found.empty() || !try_connect(found)) {
                    if (!announced_wait) {
                        log("waiting for an LSL stream with " + m_prop + " = " + m_value + "...");
                        announced_wait = true;
                    }
                    sleep_while_running(found.empty() ? 0.1 : 1.0);
                    continue;
                }
                resolver.reset(); // stop querying the network while connected
                announced_wait = false;
                continue;
            }

            try {
                const std::size_t ch = static_cast<std::size_t>(m_channels);
                const double t0 = m_inlet->pull_sample(m_frames.data(), m_channels, pull_timeout);
                if (t0 == 0.0)
                    continue; // timeout, check m_running
                m_stamps[0] = t0;
                const std::size_t more = m_inlet->pull_chunk_multiplexed(m_frames.data() + ch, m_stamps.data() + 1,
                                                                         (max_chunk - 1) * ch, max_chunk - 1, 0.0);
                if (m_on_data)
                    m_on_data(m_frames.data(), m_stamps.data(), 1 + more / ch);
            } catch (const lsl::lost_error&) {
                log("stream lost, searching for it again", true);
                inlet_reaper::instance().dispose(std::move(m_inlet));
                m_connected.store(false);
            } catch (const std::exception& e) {
                log(std::string("LSL error: ") + e.what(), true);
                inlet_reaper::instance().dispose(std::move(m_inlet));
                m_connected.store(false);
                sleep_while_running(0.5);
            }
        }
        inlet_reaper::instance().dispose(std::move(m_inlet));
    }

    const std::string m_prop, m_value;
    std::vector<lsl::stream_info> m_initial;
    connect_fn m_on_connect;
    data_fn m_on_data;
    log_fn m_on_log;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::thread m_thread;

    // listener-thread only
    std::unique_ptr<lsl::stream_inlet> m_inlet;
    int m_channels = 0;
    std::vector<float> m_frames;
    std::vector<double> m_stamps;
};

/// Seconds on a monotonic clock (arrival times for the jitter estimator).
inline double monotonic_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/// Parse the object arguments: [prop value] [n_channels]
///   (nothing)            -> type EEG
///   PPG                  -> type PPG
///   name MyMuse          -> name MyMuse
///   ... 5 (trailing int) -> create 5 outlets without waiting for the stream
struct object_arguments {
    std::string prop = "type";
    std::string value = "EEG";
    int channels = 0;
};

struct raw_argument {
    std::string text;
    bool is_number = false;
    double number = 0.0;
};

inline bool is_lsl_property(const std::string& p) {
    for (const char* k : {"name", "type", "source_id", "hostname", "channel_count", "nominal_srate", "uid", "session_id",
                          "channel_format", "created_at", "version"})
        if (p == k)
            return true;
    return false;
}

inline object_arguments parse_arguments(const std::vector<raw_argument>& args) {
    object_arguments a;
    std::size_t n = args.size();
    // A trailing positive integer is the channel count, unless it is the value of a property
    // (e.g. [source_id 1234]).
    if (n >= 1 && args[n - 1].is_number && args[n - 1].number >= 1.0 &&
        std::floor(args[n - 1].number) == args[n - 1].number && !(n == 2 && is_lsl_property(args[0].text))) {
        a.channels = static_cast<int>(args[n - 1].number);
        --n;
    }
    if (n == 1)
        a.value = args[0].text;
    else if (n >= 2) {
        a.prop = args[0].text;
        a.value = args[1].text;
    }
    return a;
}

} // namespace maxmuse
