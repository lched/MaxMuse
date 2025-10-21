/// @file       MaxMuse.receiver_tilde.cpp
///	@ingroup 	MaxMuselibrary
///	@copyright
///	@license	Use of this source code is governed by the MIT License
/// found in the License.md file.

#include <iostream>
#include <libresample.h>
#include <lsl_cpp.h>
#include <mutex>
#include <string>
#include <vector>

#include "c74_min.h"

#define MINIMUM(A, B) (A) < (B) ? (A) : (B)

const double LSL_SCAN_TIMEOUT = 0.2;
const int LSL_MAX_SAMPLES = 1;
const double LSL_PULL_TIMEOUT = 0.0;
const int SRC_BUFFER_SIZE = 256;

using namespace c74::min;

class receiver_tilde : public object<receiver_tilde>, public vector_operator<> {
  private:
    std::unique_ptr<lsl::stream_inlet> lsl_inlet;
    int n_channels;
    float og_sample_rate, resampling_factor;

    fifo<std::vector<float>> lsl_to_resampler_fifo{512};

    std::unique_ptr<std::thread> lslListenerThread;
    std::unique_ptr<std::thread> resamplerThread;

    bool stopThreadFlag = false;
    bool firstBufferDone = false;
    bool fillFirstHalf = true;

    std::vector<std::unique_ptr<outlet<>>> m_outlets;

    bool dsp_setup_done = false;

    // resample buffers
    void* resample_handle = nullptr;
    int src_current_idx = 0;
    int dst_current_idx = 0;
    int src_len, dst_len;
    int dst_buffer_size;
    float *src = nullptr;
    float *dst = nullptr;

    int dst_write_pos = 0; // where resampler writes next (0..dst_buffer_size-1)
    int dst_read_pos = 0;  // read index used by audio thread

    // resample mutex
    std::mutex dstResamplingMutex;

    // Resample method
    // returns number of produced output samples, or -1 on error
    // returns number of produced output samples, or -1 on error
    int resample_block(float *srcbuf, int srclen, float *outbuf, int outbuf_capacity) {
        if (!resample_handle) {
            std::cerr << "Resampler handle not initialized\n";
            return -1;
        }

        int out = 0;
        int srcpos = 0;
        int srcused = 0;
        int o = 0;

        for (;;) {
            int srcBlock = MIN(srclen - srcpos, srclen); // full remaining block
            int lastFlag = 0; // streaming; only set to 1 when you're shutting down for real

            o = resample_process(resample_handle, resampling_factor, &srcbuf[srcpos], srcBlock,
                                lastFlag, &srcused, &outbuf[out],
                                MIN(outbuf_capacity - out, outbuf_capacity));
            srcpos += srcused;
            if (o >= 0) out += o;
            if (o < 0 || (o == 0 && srcpos == srclen))
                break;
        }

        if (o < 0) {
            std::cerr << "resample_process error: " << o << std::endl;
            return -1;
        }
        return out;
    }


  public:
    MIN_DESCRIPTION{"Receive an LSL stream and send it to Max."};
    MIN_TAGS{"utilities, lsl"};
    MIN_AUTHOR{"l-o"};
    MIN_RELATED{"print, jit.print, dict.print"};

    inlet<> input{this, "(signal) Input", "signal"};

    receiver_tilde(const atoms &args = {})
        : lsl_inlet(nullptr), lslListenerThread(nullptr),
          resamplerThread(nullptr) {
        std::string prop, value;

        if (args.size() < 2) {
            cout << "Not enough arguments provided, defaulting to 'type' 'EEG'" << endl;
            prop = "type";
            value = "EEG";
        } else {
            prop = (std::string)args[0];
            value = (std::string)args[1];
        }
        std::vector<lsl::stream_info> results =
            lsl::resolve_stream(prop, value, 1, LSL_SCAN_TIMEOUT);

        if (results.empty()) {
            error("No LSL stream found!");
        } else {
            lsl::stream_info info = results.at(0);
            lsl_inlet = std::make_unique<lsl::stream_inlet>(info);
            cout << "[LSL] LSL stream resolved" << endl;

            n_channels = lsl_inlet->info().channel_count();
            og_sample_rate = lsl_inlet->info().nominal_srate();
            std::cout << "[LSL] Number of channels: "
                        << std::to_string(n_channels) << std::endl;
            std::cout << "[LSL] Sampling rate: "
                        << std::to_string(og_sample_rate) << std::endl;

            for (auto i = 0; i < n_channels; i++) {
                auto an_outlet = std::make_unique<outlet<>>(
                    this,
                    "(signal) Output signal " + std::to_string(i + 1) + "",
                    "signal");
                m_outlets.push_back(std::move(an_outlet));
            }
        }
    };

    ~receiver_tilde() {
        // Stop the threads gracefully
        stopThreads();
        if (resample_handle) {
            resample_close(resample_handle);
            resample_handle = nullptr;
        }
        if (src) {
            free(src);
            src = nullptr;
        }
        if (dst){
            free(dst);
            dst = nullptr;
        }
    };

    message<> dspsetup{this, "dspsetup",
                       MIN_FUNCTION{number samplerate = args[0];
    int vectorsize = args[1];
    std::cout << "[DSP] Samplerate: " << std::to_string(samplerate)
              << std::endl;
    std::cout << "[DSP] Vectorsize: " << std::to_string(vectorsize)
              << std::endl;

    src_len = SRC_BUFFER_SIZE;
    resampling_factor = samplerate / og_sample_rate;
    dst_len = (int)(src_len * resampling_factor);
    std::cout << "[RESAMPLING] Input buffer size:" << std::to_string(src_len)
              << std::endl;
    std::cout << "[RESAMPLING] Output buffer size:" << std::to_string(dst_len)
              << std::endl;

    dst_buffer_size = dst_len * 2;
    src = (float *)calloc(src_len, sizeof(float));
    dst = (float *)calloc(dst_buffer_size + 16, sizeof(float));

    if (resample_handle)
        resample_close(resample_handle);
    resample_handle = resample_open(1, resampling_factor, resampling_factor);
    dsp_setup_done = true;
    createOrRestartThreads();
    return {};
}
}
;

void stopThreads() {
    stopThreadFlag = true;
    if (lslListenerThread && lslListenerThread->joinable())
        lslListenerThread->join();
    if (resamplerThread && resamplerThread->joinable())
        resamplerThread->join();
}

void createOrRestartThreads() {
    if (!lsl_inlet) {
        std:cerr << "[receiver~] Cannot start threads because there is no LSL inlet.\n";
        return;
    }
    stopThreads();
    stopThreadFlag = false;
    lslListenerThread.reset(
        new std::thread(&receiver_tilde::listenForSamples, this));
    resamplerThread.reset(
        new std::thread(&receiver_tilde::resampleWhenReady, this));
}

void listenForSamples() {
    try {
        std::vector<float> sample(n_channels);
        while (!stopThreadFlag) {
            lsl_inlet->pull_sample(sample);
            if (dsp_setup_done)
                lsl_to_resampler_fifo.try_enqueue(sample);
        }
    } catch (std::exception &e) {
        std::cerr << "LSLPortListener exception" << e.what() << std::endl;
    }
}

void resampleWhenReady() {
    std::vector<float> in_sample(n_channels);
    while (!stopThreadFlag) {
        if (!dsp_setup_done)
            continue;

        if (lsl_to_resampler_fifo.try_dequeue(in_sample)) {
            src[src_current_idx++] = in_sample[0];

            if (src_current_idx >= SRC_BUFFER_SIZE) {
                // We have a full src block, resample it into a temporary buffer
                std::vector<float> temp_out(dst_len + 128);
                int produced = resample_block(src, src_len, temp_out.data(), (int)temp_out.size());
                src_current_idx = 0;

                if (produced > 0) {
                    std::unique_lock<std::mutex> lock(dstResamplingMutex);
                    for (int i = 0; i < produced; ++i) {
                        dst[dst_write_pos] = temp_out[i];
                        dst_write_pos = (dst_write_pos + 1) % dst_buffer_size;
                    }
                    firstBufferDone = true;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
}

void operator()(audio_bundle _, audio_bundle output) {
    if (!dsp_setup_done || !firstBufferDone) {
        // Output silence until first block available
        for (auto ch = 0; ch < output.channel_count(); ++ch)
            for(auto i = 0; i < output.frame_count(); ++i)
                output.samples(ch)[i] = 0.0f;
        return;
    }

    for (auto i = 0; i < output.frame_count(); i++) {
        // read sample under lock for a single sample
        float s;
        {
            std::unique_lock<std::mutex> lock(dstResamplingMutex);
            s = dst[dst_read_pos];
            dst_read_pos = (dst_read_pos + 1) % dst_buffer_size;
        }
        output.samples(0)[i] = s;
    }
}
}
;

MIN_EXTERNAL(receiver_tilde);
