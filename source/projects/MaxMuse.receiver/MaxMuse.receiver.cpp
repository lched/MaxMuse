/// @file       MaxMuse.receiver.cpp
/// @ingroup    MaxMuse
/// @brief      Receive an LSL stream as Max floats, one outlet per channel, at the
///             stream's native rate.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.
///
/// Usage:  [MaxMuse.receiver <property> <value> [<channels>]]   e.g. [MaxMuse.receiver type ACC 3]
/// Every received sample is output on the scheduler thread, right-to-left (the
/// first channel fires last). Samples arrive in Bluetooth packets, so they come in
/// small bursts; for evenly timed values use [MaxMuse.receiver~] into [snapshot~].

#include "c74_min.h"
#include "maxmuse_lsl.h"
#include "maxmuse_resampler.h" // frame_queue

#include <algorithm>
#include <atomic>
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

} // namespace

class receiver : public object<receiver> {
  public:
    MIN_DESCRIPTION{"Receive an LSL stream (e.g. the EEG, PPG, ACC or GYRO stream of a Muse 2 headband) as floats, one outlet "
                    "per channel, at the stream's own rate. Arguments: [property value] [channels], e.g. 'type ACC 3'. When "
                    "the number of channels is given the object is created immediately and connects as soon as the stream "
                    "appears."};
    MIN_TAGS{"lsl, eeg, muse, biosignals"};
    MIN_AUTHOR{"Léo Chédin"};
    MIN_RELATED{"MaxMuse.receiver~"};

  private:
    static constexpr double deliver_interval_ms = 2.0;

    // ---- state used by the LSL thread callbacks ----
    maxmuse::frame_queue m_queue;
    std::atomic<bool> m_queue_ready{false};
    std::atomic<std::uint64_t> m_frames_in{0};
    std::atomic<std::uint64_t> m_overflows{0};
    std::atomic<bool> m_flush_req{false};
    int m_channels = 0;          // written once, before m_queue_ready
    std::vector<float> m_frame;  // scheduler-thread scratch, allocated before m_queue_ready
    maxmuse::object_arguments m_args;
    int m_n_outlets = 0;

    std::mutex m_log_mutex;
    std::vector<std::pair<std::string, bool>> m_log;

    std::mutex m_info_mutex;
    maxmuse::stream_description m_info;
    bool m_has_info = false;

    std::vector<std::unique_ptr<outlet<>>> m_outlets;
    std::unique_ptr<maxmuse::lsl_listener> m_listener;

    void log_async(const std::string& msg, bool is_error) {
        {
            std::lock_guard<std::mutex> lock(m_log_mutex);
            m_log.emplace_back(msg, is_error);
        }
        m_log_flush.set();
    }

    bool on_connect(const maxmuse::stream_description& d) {
        if (!m_queue_ready.load(std::memory_order_acquire)) {
            m_channels = d.channels;
            m_frame.assign(d.channels, 0.0f);
            const double rate = d.rate > 0.0 ? d.rate : 1000.0;
            m_queue.allocate(d.channels, std::max<std::size_t>(4096, static_cast<std::size_t>(8.0 * rate)));
            m_queue_ready.store(true, std::memory_order_release);
        }
        else if (d.channels != m_channels) {
            log_async("found " + d.summary() + " but this object was set up for " + std::to_string(m_channels) +
                          " channels; re-create the object to use it",
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
        log_async(msg, false);
        return true;
    }

    void on_data(const float* frames, std::size_t n) {
        const std::size_t written = m_queue.push(frames, nullptr, n);
        m_frames_in.fetch_add(n, std::memory_order_relaxed);
        if (written < n)
            m_overflows.fetch_add(n - written, std::memory_order_relaxed);
    }

    void deliver() {
        if (!m_queue_ready.load(std::memory_order_acquire))
            return;
        if (m_flush_req.exchange(false)) // the queue has a single consumer: this thread
            m_queue.discard(m_queue.size());
        const int nc = (std::min)(m_n_outlets, m_channels);
        while (m_queue.pop(m_frame.data()))
            for (int c = nc - 1; c >= 0; --c) // right-to-left, Max convention
                m_outlets[c]->send(static_cast<double>(m_frame[c]));
    }

  public:
    inlet<> m_inlet{this, "(message) status, flush, scan"};

    receiver(const atoms& args = {}) {
        if (dummy())
            return; // instance used by Max for class registration: no network, no threads

        m_args = maxmuse::parse_arguments(to_raw(args));

        std::vector<lsl::stream_info> found;
        if (m_args.channels <= 0) {
            found = maxmuse::resolve_now(m_args.prop, m_args.value, 2.0);
            if (found.empty())
                error("no LSL stream with " + m_args.prop + " = " + m_args.value +
                      " found. Start the stream first, or add the number of channels as last argument "
                      "(e.g. [MaxMuse.receiver " + m_args.prop + " " + m_args.value +
                      " 3]) to create the object now and connect when the stream appears.");
            m_n_outlets = found.front().channel_count();
        }
        else
            m_n_outlets = m_args.channels;

        const std::string prefix = found.empty() ? m_args.value : found.front().type();
        for (int c = 0; c < m_n_outlets; ++c)
            m_outlets.push_back(std::make_unique<outlet<>>(this, "(float) " + prefix + " channel " + std::to_string(c + 1)));

        m_listener = std::make_unique<maxmuse::lsl_listener>(m_args.prop, m_args.value);
        m_listener->start(
            std::move(found), [this](const maxmuse::stream_description& d) { return on_connect(d); },
            [this](const float* frames, const double*, std::size_t n) { on_data(frames, n); },
            [this](const std::string& msg, bool is_error) { log_async(msg, is_error); });

        m_deliverer.delay(deliver_interval_ms);
    }

    ~receiver() {
        if (m_listener)
            m_listener->stop();
    }

    timer<> m_deliverer{this, MIN_FUNCTION{
        deliver();
        m_deliverer.delay(deliver_interval_ms);
        return {};
    }};

    message<> status{this, "status", "Post the stream status to the Max console.", MIN_FUNCTION{
        const std::string who = "[" + m_args.prop + " " + m_args.value + "] ";
        std::lock_guard<std::mutex> lock(m_info_mutex);
        if (m_listener && m_listener->connected() && m_has_info)
            cout << who << "connected to " << m_info.summary() << ": " << m_info.label_list() << endl;
        else
            cout << who << "waiting for the stream" << endl;
        cout << "  frames received " << m_frames_in.load() << ", dropped (queue full) " << m_overflows.load() << endl;
        return {};
    }};

    message<> flush{this, "flush", "Discard samples waiting to be output.", MIN_FUNCTION{
        m_flush_req.store(true);
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
};

MIN_EXTERNAL(receiver);
