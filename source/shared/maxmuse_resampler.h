/// @file       maxmuse_resampler.h
/// @brief      Real-time, drift-compensating resampler for low-rate LSL streams.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.
///
/// This header has no dependency on Max or LSL so that it can be unit-tested on
/// any platform (see test/test_resampler.cpp).
///
/// Threading model
/// ---------------
///   * one producer thread (the LSL listener) calls push();
///   * one consumer thread (the audio thread) calls process();
///   * any thread may call the control setters (set_latency, set_interp, reset,
///     set_output_rate) and stats().
/// The audio path is lock-free and allocation-free.
///
/// Why not a fixed-ratio resampler?
/// --------------------------------
/// The headband's clock and the audio interface's clock are independent, and
/// Bluetooth delivers samples in irregular bursts (12-sample packets for the Muse
/// EEG). A fixed ratio (audio_sr / nominal_sr) therefore makes any intermediate
/// buffer slowly over- or under-flow, which shows up as periodic clicks. Here the
/// ratio is continuously adjusted by a PI controller that keeps the jitter buffer
/// at a target depth (asynchronous sample-rate conversion). Every remaining
/// discontinuity (start-up, dropouts, resynchronisation, interpolation change) is
/// hidden with a short raised-cosine "declick" ramp, and short packet losses are
/// concealed using the LSL timestamps.

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maxmuse {

// =====================================================================================
// Single-producer / single-consumer lock-free queue of multichannel frames
// =====================================================================================

class frame_queue {
  public:
    enum : std::uint8_t { flag_none = 0, flag_discontinuity = 1 };

    frame_queue() = default;
    frame_queue(const frame_queue&) = delete;
    frame_queue& operator=(const frame_queue&) = delete;

    /// Not thread-safe: call before the producer/consumer start.
    void allocate(int channels, std::size_t min_capacity_frames) {
        std::size_t cap = 1;
        while (cap < min_capacity_frames)
            cap <<= 1;
        m_channels = channels;
        m_capacity = cap;
        m_mask = cap - 1;
        m_data.assign(cap * static_cast<std::size_t>(channels), 0.0f);
        m_flags.assign(cap, flag_none);
        m_write.store(0);
        m_read.store(0);
    }

    int channels() const { return m_channels; }
    std::size_t capacity() const { return m_capacity; }

    /// Producer: returns the number of frames actually written (the rest is dropped).
    std::size_t push(const float* interleaved, const std::uint8_t* flags, std::size_t n_frames) {
        const std::size_t w = m_write.load(std::memory_order_relaxed);
        const std::size_t r = m_read.load(std::memory_order_acquire);
        const std::size_t n = (std::min)(n_frames, m_capacity - (w - r));
        const std::size_t ch = static_cast<std::size_t>(m_channels);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t slot = (w + i) & m_mask;
            std::copy_n(interleaved + i * ch, ch, &m_data[slot * ch]);
            m_flags[slot] = flags ? flags[i] : static_cast<std::uint8_t>(flag_none);
        }
        m_write.store(w + n, std::memory_order_release);
        return n;
    }

    /// Consumer: number of readable frames (approximate when called from another thread).
    std::size_t size() const {
        const std::size_t w = m_write.load(std::memory_order_acquire);
        const std::size_t r = m_read.load(std::memory_order_acquire);
        return w - r;
    }

    /// Consumer: pop one frame. Returns false if the queue is empty.
    bool pop(float* frame, std::uint8_t* flag = nullptr) {
        const std::size_t r = m_read.load(std::memory_order_relaxed);
        const std::size_t w = m_write.load(std::memory_order_acquire);
        if (r == w)
            return false;
        const std::size_t slot = r & m_mask;
        const std::size_t ch = static_cast<std::size_t>(m_channels);
        std::copy_n(&m_data[slot * ch], ch, frame);
        if (flag)
            *flag = m_flags[slot];
        m_read.store(r + 1, std::memory_order_release);
        return true;
    }

    /// Consumer: drop up to n frames, returns the number dropped.
    std::size_t discard(std::size_t n) {
        const std::size_t r = m_read.load(std::memory_order_relaxed);
        const std::size_t w = m_write.load(std::memory_order_acquire);
        const std::size_t d = (std::min)(n, w - r);
        m_read.store(r + d, std::memory_order_release);
        return d;
    }

  private:
    int m_channels = 0;
    std::size_t m_capacity = 0;
    std::size_t m_mask = 0;
    std::vector<float> m_data;
    std::vector<std::uint8_t> m_flags;
    alignas(64) std::atomic<std::size_t> m_write{0};
    alignas(64) std::atomic<std::size_t> m_read{0};
};

// =====================================================================================
// Interpolation kernels
// =====================================================================================

enum class interp_mode : int { linear = 0, cubic = 1, sinc = 2 };

/// Fractional-delay interpolation kernels. For an output time t = n + frac the
/// kernel produces 2*W weights applied to input frames n-W+1 ... n+W.
/// Every weight set sums to exactly 1, so a constant input (e.g. the large DC
/// offset of raw EEG) is reproduced exactly: no ripple at the input rate.
class kernel_bank {
  public:
    static constexpr int max_half_width = 16;
    static constexpr int sinc_half_width = 8;
    static constexpr int sinc_phases = 512;

    explicit kernel_bank(double cutoff = 0.9, double kaiser_beta = 7.0) { build_sinc(cutoff, kaiser_beta); }

    static int half_width(interp_mode m) {
        switch (m) {
        case interp_mode::linear:
            return 1;
        case interp_mode::cubic:
            return 2;
        default:
            return sinc_half_width;
        }
    }

    void weights(interp_mode m, double f, double* w) const {
        switch (m) {
        case interp_mode::linear:
            w[0] = 1.0 - f;
            w[1] = f;
            break;
        case interp_mode::cubic: { // Catmull-Rom (C1-continuous)
            const double f2 = f * f, f3 = f2 * f;
            w[0] = 0.5 * (-f3 + 2.0 * f2 - f);
            w[1] = 0.5 * (3.0 * f3 - 5.0 * f2 + 2.0);
            w[2] = 0.5 * (-3.0 * f3 + 4.0 * f2 + f);
            w[3] = 0.5 * (f3 - f2);
            break;
        }
        default: {
            constexpr int taps = 2 * sinc_half_width;
            const double p = f * sinc_phases;
            const int i = (std::min)(static_cast<int>(p), sinc_phases - 1);
            const double a = p - i;
            const double* r0 = &m_sinc[static_cast<std::size_t>(i) * taps];
            const double* r1 = r0 + taps;
            for (int j = 0; j < taps; ++j)
                w[j] = r0[j] + a * (r1[j] - r0[j]);
            break;
        }
        }
    }

  private:
    static double bessel_i0(double x) {
        double sum = 1.0, term = 1.0;
        const double q = 0.25 * x * x;
        for (int k = 1; k < 64; ++k) {
            term *= q / (static_cast<double>(k) * k);
            sum += term;
            if (term < 1e-15 * sum)
                break;
        }
        return sum;
    }

    // Kaiser-windowed sinc, cutoff relative to the input Nyquist frequency.
    void build_sinc(double cutoff, double beta) {
        constexpr int W = sinc_half_width, taps = 2 * W;
        const double pi = 3.14159265358979323846;
        const double i0b = bessel_i0(beta);
        m_sinc.assign(static_cast<std::size_t>(sinc_phases + 1) * taps, 0.0);
        for (int p = 0; p <= sinc_phases; ++p) {
            const double f = static_cast<double>(p) / sinc_phases;
            double* row = &m_sinc[static_cast<std::size_t>(p) * taps];
            double sum = 0.0;
            for (int j = 0; j < taps; ++j) {
                const double t = (j - W + 1) - f; // distance (in input frames) from tap to output time
                const double x = t / W;
                const double win = std::fabs(x) <= 1.0 ? bessel_i0(beta * std::sqrt(1.0 - x * x)) / i0b : 0.0;
                const double arg = pi * cutoff * t;
                const double s = std::fabs(arg) < 1e-12 ? 1.0 : std::sin(arg) / arg;
                row[j] = cutoff * s * win;
                sum += row[j];
            }
            for (int j = 0; j < taps; ++j)
                row[j] /= sum; // unity DC gain at every phase
        }
    }

    std::vector<double> m_sinc;
};

// =====================================================================================
// The resampler
// =====================================================================================

struct resampler_stats {
    int state = 0;                ///< 0: waiting for data, 1: running, 2: re-buffering after an underrun
    double fill_seconds = 0.0;    ///< current jitter-buffer depth
    double target_seconds = 0.0;  ///< jitter-buffer depth the controller aims for
    double rate_correction = 0.0; ///< relative ratio correction applied (0.001 = +0.1 %)
    double peak_gap_seconds = 0.0;///< largest recent interval between two packet arrivals
    std::uint64_t frames_in = 0;
    std::uint64_t underruns = 0;
    std::uint64_t resyncs = 0;
    std::uint64_t overflows = 0;
    std::uint64_t gaps_filled = 0;
    std::uint64_t discontinuities = 0;
};

class stream_resampler {
  public:
    static constexpr int history_frames = 64; // power of two, > 2 * max_half_width + 1
    static constexpr double max_correction = 0.05;
    // Loop: fill-level low-pass (tau_f) + PI. Damping = 1 / (2 sqrt(tau_f / tau_p)) ~ 0.8
    static constexpr double controller_tau = 5.0;     // s, proportional time constant (tau_p)
    static constexpr double controller_ti = 30.0;     // s, integral time constant
    static constexpr double fill_smoothing = 2.0;     // s (tau_f), averages out packet sawtooth + jitter
    static constexpr double declick_seconds = 0.03;   // raised-cosine ramp length
    static constexpr double max_concealed_gap = 0.5;  // s, longer gaps are treated as discontinuities
    static constexpr int min_missing_to_fill = 3;     // frames; smaller timestamp gaps are timestamp jitter
    static constexpr double max_jitter_gap = 0.3;     // s, longer arrival gaps are stalls (handled by hold/resync)
    static constexpr double max_auto_latency = 0.3;   // s

    stream_resampler() = default;
    stream_resampler(const stream_resampler&) = delete;
    stream_resampler& operator=(const stream_resampler&) = delete;

    /// Allocate everything. Not real-time safe, call once, before push()/process().
    /// input_rate <= 0 means an irregular stream: the latest value is held.
    bool configure(int channels, double input_rate, double queue_seconds = 8.0) {
        if (channels <= 0 || m_configured.load())
            return false;
        m_ch = channels;
        m_in_rate = input_rate > 0.0 ? input_rate : 0.0;
        const double rate_for_queue = m_in_rate > 0.0 ? m_in_rate : 1000.0;
        m_queue.allocate(channels, std::max<std::size_t>(4096, static_cast<std::size_t>(queue_seconds * rate_for_queue)));
        m_max_latency = 0.45 * static_cast<double>(m_queue.capacity()) / rate_for_queue;

        m_hist.assign(static_cast<std::size_t>(history_frames) * channels, 0.0f);
        m_frame.assign(channels, 0.0f);
        m_last_out.assign(channels, 0.0);
        m_y.assign(channels, 0.0);
        m_dc_offset.assign(channels, 0.0);

        m_prod_scratch.assign(static_cast<std::size_t>(scratch_frames) * channels, 0.0f);
        m_prod_flags.assign(scratch_frames, frame_queue::flag_none);
        m_prod_last.assign(channels, 0.0f);

        m_configured.store(true, std::memory_order_release);
        return true;
    }

    bool configured() const { return m_configured.load(std::memory_order_acquire); }
    int channels() const { return m_ch; }
    double input_rate() const { return m_in_rate; }

    // --------------------------------------------------------------------------- control

    /// Jitter-buffer target in seconds; <= 0 selects automatic sizing from the
    /// observed packet arrival pattern.
    void set_latency(double seconds) { m_latency_req.store(seconds); }
    void set_interp(interp_mode m) { m_interp_req.store(static_cast<int>(m)); }
    void set_fill_gaps(bool on) { m_fill_gaps.store(on); }
    void set_output_rate(double sr) { m_out_rate_req.store(sr); }
    void reset() { m_reset_req.store(true); }

    resampler_stats stats() const {
        resampler_stats s;
        s.state = m_st_state.load(std::memory_order_relaxed);
        s.fill_seconds = m_st_fill.load(std::memory_order_relaxed);
        s.target_seconds = m_st_target.load(std::memory_order_relaxed);
        s.rate_correction = m_st_corr.load(std::memory_order_relaxed);
        s.peak_gap_seconds = m_peak_gap.load(std::memory_order_relaxed);
        s.frames_in = m_frames_in.load(std::memory_order_relaxed);
        s.underruns = m_underruns.load(std::memory_order_relaxed);
        s.resyncs = m_resyncs.load(std::memory_order_relaxed);
        s.overflows = m_overflows.load(std::memory_order_relaxed);
        s.gaps_filled = m_gaps_filled.load(std::memory_order_relaxed);
        s.discontinuities = m_discontinuities.load(std::memory_order_relaxed);
        return s;
    }

    // --------------------------------------------------------------------------- producer

    /// Push n interleaved frames. `timestamps` (LSL sample times, may be null) are
    /// used to conceal lost packets; `arrival_time` (any monotonic clock, seconds)
    /// is used to size the jitter buffer automatically.
    void push(const float* interleaved, const double* timestamps, std::size_t n, double arrival_time) {
        if (!configured() || n == 0)
            return;

        if (m_prod_have_arrival) {
            const double gap = arrival_time - m_prod_last_arrival;
            if (gap >= 0.0 && gap < max_jitter_gap) { // longer gaps are stalls, not jitter
                m_prod_peak = (std::max)(gap, m_prod_peak * std::exp(-gap / 30.0));
                m_peak_gap.store(m_prod_peak, std::memory_order_relaxed);
            }
        }
        m_prod_last_arrival = arrival_time;
        m_prod_have_arrival = true;

        const bool fill_gaps = m_fill_gaps.load(std::memory_order_relaxed);
        const std::size_t ch = static_cast<std::size_t>(m_ch);

        for (std::size_t k = 0; k < n; ++k) {
            const float* src = interleaved + k * ch;
            std::uint8_t flag = frame_queue::flag_none;

            if (timestamps && m_in_rate > 0.0) {
                const double ts = timestamps[k];
                if (m_prod_have_ts && ts > 0.0) {
                    const double dt = ts - m_prod_last_ts;
                    if (dt > max_concealed_gap || dt < -max_concealed_gap) {
                        flag = frame_queue::flag_discontinuity;
                        m_discontinuities.fetch_add(1, std::memory_order_relaxed);
                    }
                    else if (fill_gaps) {
                        const long missing = std::lround(dt * m_in_rate) - 1;
                        if (missing >= min_missing_to_fill) {
                            // Linear interpolation across the hole: keeps the timeline
                            // and makes the waveform continuous (no click).
                            for (long m = 1; m <= missing; ++m) {
                                const float a = static_cast<float>(m) / static_cast<float>(missing + 1);
                                float* dst = scratch_slot();
                                for (std::size_t c = 0; c < ch; ++c) {
                                    const float v = std::isfinite(src[c]) ? src[c] : m_prod_last[c];
                                    dst[c] = m_prod_last[c] + a * (v - m_prod_last[c]);
                                }
                                m_prod_flags[m_prod_count++] = frame_queue::flag_none;
                                flush_if_full();
                            }
                            m_gaps_filled.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
                if (ts > 0.0) {
                    m_prod_last_ts = ts;
                    m_prod_have_ts = true;
                }
            }

            float* dst = scratch_slot();
            for (std::size_t c = 0; c < ch; ++c) {
                float v = src[c];
                if (!std::isfinite(v))
                    v = m_prod_last[c]; // never let NaN/inf reach the audio thread
                m_prod_last[c] = v;
                dst[c] = v;
            }
            m_prod_flags[m_prod_count++] = flag;
            flush_if_full();
        }
        flush();
        m_frames_in.fetch_add(n, std::memory_order_relaxed);
    }

    // --------------------------------------------------------------------------- consumer

    /// Render n_frames of output into out[0..n_out_channels-1]. Channels beyond
    /// the stream's channel count are zeroed. `positions` (test hook, may be null)
    /// receives the fractional input-frame index of every output sample, or NaN
    /// when the output is being held.
    void process(double* const* out, int n_out_channels, int n_frames, double* positions = nullptr) {
        if (!configured()) {
            zero(out, 0, n_out_channels, n_frames);
            fill_nan(positions, n_frames);
            return;
        }

        const double sr = m_out_rate_req.load(std::memory_order_relaxed);
        if (sr > 0.0 && sr != m_out_rate) {
            m_out_rate = sr;
            m_dc_len = (std::max)(1, static_cast<int>(std::lround(declick_seconds * sr)));
        }
        if (m_out_rate <= 0.0) {
            zero(out, 0, n_out_channels, n_frames);
            fill_nan(positions, n_frames);
            return;
        }

        const int nc = (std::min)(n_out_channels, m_ch);

        if (m_in_rate <= 0.0) { // irregular stream: sample & hold the latest value
            while (m_queue.pop(m_frame.data()))
                for (int c = 0; c < m_ch; ++c)
                    m_last_out[c] = m_frame[c];
            for (int c = 0; c < nc; ++c)
                std::fill_n(out[c], n_frames, m_last_out[c]);
            zero(out, nc, n_out_channels, n_frames);
            fill_nan(positions, n_frames);
            return;
        }

        const interp_mode req_mode = static_cast<interp_mode>(m_interp_req.load(std::memory_order_relaxed));
        if (req_mode != m_mode) {
            m_mode = req_mode;
            m_pending_declick = true;
        }
        if (m_reset_req.exchange(false)) {
            m_next_index += m_queue.discard(m_queue.size());
            m_state = state_priming;
            m_integ = 0.0;
        }

        update_target();
        const int W = kernel_bank::half_width(m_mode);
        m_step = m_in_rate / m_out_rate * (1.0 + m_corr);

        // ---- state transitions (evaluated once per vector) ----
        if (m_state == state_priming) {
            if (static_cast<double>(m_queue.size()) >= m_target_frames + W + 1) {
                m_queue.pop(m_frame.data());
                restart_from_frame();
                enter_running();
            }
        }
        else if (m_state == state_rebuffering) {
            if (fill_frames(W) >= m_target_frames)
                enter_running();
        }

        // ---- render ----
        for (int i = 0; i < n_frames; ++i) {
            if (m_state == state_running) {
                if (compute(nc)) {
                    if (m_pending_declick) {
                        for (int c = 0; c < nc; ++c)
                            m_dc_offset[c] = m_last_out[c] - m_y[c];
                        m_dc_pos = 0;
                        m_declicking = true;
                        m_pending_declick = false;
                    }
                    if (m_declicking) {
                        const double g = 0.5 * (1.0 + std::cos(3.14159265358979323846 * m_dc_pos / m_dc_len));
                        for (int c = 0; c < nc; ++c)
                            m_y[c] += m_dc_offset[c] * g;
                        if (++m_dc_pos >= m_dc_len)
                            m_declicking = false;
                    }
                    for (int c = 0; c < nc; ++c) {
                        out[c][i] = m_y[c];
                        m_last_out[c] = m_y[c];
                    }
                    if (positions)
                        positions[i] = static_cast<double>(m_base) + m_frac;
                    advance();
                    continue;
                }
                m_state = state_rebuffering; // underrun: freeze time and hold the output
                m_underruns.fetch_add(1, std::memory_order_relaxed);
            }
            for (int c = 0; c < nc; ++c)
                out[c][i] = m_last_out[c];
            if (positions)
                positions[i] = std::nan("");
        }
        zero(out, nc, n_out_channels, n_frames);

        // ---- drift / jitter controller (once per vector) ----
        double fill = fill_frames(W);
        if (m_state == state_running) {
            const double excess = (std::max)(m_target_frames, 0.2 * m_in_rate);
            if (fill > m_target_frames + excess) {
                resync(W);
                fill = fill_frames(W);
            }
            else {
                const double dt = n_frames / m_out_rate;
                const double fill_s = fill / m_in_rate;
                m_fill_filtered += (fill_s - m_fill_filtered) * (1.0 - std::exp(-dt / fill_smoothing));
                const double e = m_fill_filtered - m_target_frames / m_in_rate;
                m_integ = clamp(m_integ + e * dt / (controller_tau * controller_ti), -max_correction, max_correction);
                m_corr = clamp(e / controller_tau + m_integ, -max_correction, max_correction);
            }
        }
        m_st_state.store(m_state, std::memory_order_relaxed);
        if (m_state == state_priming)
            fill = static_cast<double>(m_queue.size());
        m_st_fill.store(fill / m_in_rate, std::memory_order_relaxed);
        m_st_target.store(m_target_frames / m_in_rate, std::memory_order_relaxed);
        m_st_corr.store(m_corr, std::memory_order_relaxed);
    }

  private:
    enum { state_priming = 0, state_running = 1, state_rebuffering = 2 };
    static constexpr int scratch_frames = 256;

    static double clamp(double v, double lo, double hi) { return (std::min)(hi, (std::max)(lo, v)); }

    static void zero(double* const* out, int from, int to, int n) {
        for (int c = from; c < to; ++c)
            std::fill_n(out[c], n, 0.0);
    }
    static void fill_nan(double* p, int n) {
        if (p)
            std::fill_n(p, n, std::nan(""));
    }

    // producer helpers
    float* scratch_slot() { return &m_prod_scratch[m_prod_count * static_cast<std::size_t>(m_ch)]; }
    void flush_if_full() {
        if (m_prod_count == static_cast<std::size_t>(scratch_frames))
            flush();
    }
    void flush() {
        if (m_prod_count == 0)
            return;
        const std::size_t written = m_queue.push(m_prod_scratch.data(), m_prod_flags.data(), m_prod_count);
        if (written < m_prod_count)
            m_overflows.fetch_add(m_prod_count - written, std::memory_order_relaxed);
        m_prod_count = 0;
    }

    // consumer helpers
    std::size_t slot(std::int64_t index) const {
        return (static_cast<std::size_t>(index) & (history_frames - 1)) * static_cast<std::size_t>(m_ch);
    }

    void update_target() {
        const double req = m_latency_req.load(std::memory_order_relaxed);
        double s;
        if (req > 0.0)
            s = clamp(req, 0.005, m_max_latency);
        else {
            const double peak = m_peak_gap.load(std::memory_order_relaxed);
            s = peak > 0.0 ? clamp(peak + 0.03, 0.05, max_auto_latency) : 0.1;
        }
        m_target_frames = s * m_in_rate;
    }

    // Frames buffered beyond what the interpolation kernel needs.
    double fill_frames(int W) const {
        return static_cast<double>(m_queue.size()) + static_cast<double>(m_have - m_base) - m_frac - W;
    }

    // Start a fresh segment at the frame in m_frame (left history padded with it).
    void restart_from_frame() {
        const std::int64_t idx = m_next_index++;
        for (int k = 0; k < kernel_bank::max_half_width + 1; ++k)
            std::copy_n(m_frame.data(), m_ch, &m_hist[slot(idx - k)]);
        m_have = idx;
        m_base = idx;
        m_frac = 0.0;
        m_pending_declick = true;
    }

    void enter_running() {
        m_state = state_running;
        m_fill_filtered = fill_frames(kernel_bank::half_width(m_mode)) / m_in_rate;
    }

    // Jump forward to the target depth after a long stall (hidden by a declick).
    void resync(int W) {
        const std::size_t keep = static_cast<std::size_t>(std::ceil(m_target_frames)) + W + 1;
        const std::size_t q = m_queue.size();
        if (q > keep)
            m_next_index += m_queue.discard(q - keep);
        if (m_queue.pop(m_frame.data())) {
            restart_from_frame();
            enter_running();
        }
        else
            m_state = state_priming;
        m_resyncs.fetch_add(1, std::memory_order_relaxed);
    }

    // Compute one output sample at the current position into m_y. Returns false on underrun.
    bool compute(int nc) {
        const int W = kernel_bank::half_width(m_mode);
        while (m_have < m_base + W) {
            std::uint8_t flag = 0;
            if (!m_queue.pop(m_frame.data(), &flag))
                return false;
            if (flag & frame_queue::flag_discontinuity) {
                // Data after a long gap: don't interpolate across it.
                restart_from_frame();
                continue;
            }
            const std::int64_t idx = m_next_index++;
            std::copy_n(m_frame.data(), m_ch, &m_hist[slot(idx)]);
            m_have = idx;
        }
        double w[2 * kernel_bank::max_half_width];
        m_kernels.weights(m_mode, m_frac, w);
        std::size_t offs[2 * kernel_bank::max_half_width];
        for (int j = 0; j < 2 * W; ++j)
            offs[j] = slot(m_base - W + 1 + j);
        for (int c = 0; c < nc; ++c) {
            double acc = 0.0;
            for (int j = 0; j < 2 * W; ++j)
                acc += w[j] * static_cast<double>(m_hist[offs[j] + c]);
            m_y[c] = acc;
        }
        return true;
    }

    void advance() {
        m_frac += m_step;
        if (m_frac >= 1.0) {
            const double whole = std::floor(m_frac);
            m_base += static_cast<std::int64_t>(whole);
            m_frac -= whole;
        }
    }

    // ---- shared / immutable after configure ----
    std::atomic<bool> m_configured{false};
    int m_ch = 0;
    double m_in_rate = 0.0;
    double m_max_latency = 1.0;
    frame_queue m_queue;
    kernel_bank m_kernels;

    // ---- control (any thread) ----
    std::atomic<double> m_latency_req{0.0};
    std::atomic<int> m_interp_req{static_cast<int>(interp_mode::sinc)};
    std::atomic<bool> m_fill_gaps{true};
    std::atomic<double> m_out_rate_req{0.0};
    std::atomic<bool> m_reset_req{false};

    // ---- statistics ----
    std::atomic<int> m_st_state{0};
    std::atomic<double> m_st_fill{0.0}, m_st_target{0.0}, m_st_corr{0.0};
    std::atomic<double> m_peak_gap{0.0};
    std::atomic<std::uint64_t> m_frames_in{0}, m_underruns{0}, m_resyncs{0}, m_overflows{0};
    std::atomic<std::uint64_t> m_gaps_filled{0}, m_discontinuities{0};

    // ---- producer-only ----
    std::vector<float> m_prod_scratch;
    std::vector<std::uint8_t> m_prod_flags;
    std::size_t m_prod_count = 0;
    std::vector<float> m_prod_last;
    double m_prod_last_ts = 0.0;
    bool m_prod_have_ts = false;
    double m_prod_last_arrival = 0.0;
    bool m_prod_have_arrival = false;
    double m_prod_peak = 0.0;

    // ---- consumer-only ----
    int m_state = state_priming;
    interp_mode m_mode = interp_mode::sinc;
    double m_out_rate = 0.0;
    double m_step = 0.0;
    double m_corr = 0.0;
    double m_integ = 0.0;
    double m_fill_filtered = 0.0;
    double m_target_frames = 0.0;
    std::vector<float> m_hist;
    std::vector<float> m_frame;
    std::vector<double> m_last_out, m_y, m_dc_offset;
    std::int64_t m_next_index = 0; // index assigned to the next frame taken from the queue
    std::int64_t m_have = -1;      // index of the newest frame in the history
    std::int64_t m_base = 0;       // integer part of the read position
    double m_frac = 0.0;           // fractional part of the read position
    bool m_pending_declick = false;
    bool m_declicking = false;
    int m_dc_pos = 0;
    int m_dc_len = 1;
};

} // namespace maxmuse
