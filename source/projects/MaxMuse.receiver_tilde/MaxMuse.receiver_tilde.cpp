/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <mutex>
#include <iostream>
#include <string>
#include <vector>
#include <lsl_cpp.h>
#include <libresample.h>

#include "c74_min.h"

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

    fifo<std::vector<float>> lsl_to_resampler_fifo { 512 };
    // fifo<std::vector<float>> resampler_to_outlet_fifo { 450000 };  // yeah this is big

    std::thread lslListenerThread;
    std::thread resamplerThread;
    bool stopThreadFlag = false;
    bool firstBufferDone = false;
    bool fillFirstHalf = true;

    std::vector< std::unique_ptr<outlet<>> >   m_outlets;

    bool dsp_setup_done = false;

    // resample buffers
    int src_current_idx = 0;
    int dst_current_idx = 0;
    int src_len, dst_len;
    int dst_buffer_size;
    float *src;
    float *dst;

    // resample mutex
    std::mutex dstResamplingMutex;

    // Resample method
    void resample(float *src, float *dst, int srclen, double factor,
			  int srcblocksize, int dstblocksize)
    {
        int expectedlen = (int)(srclen * factor);
        int dstlen = expectedlen + 1000;
        void *handle;
        double sum, sumsq, err, rmserr;
        int i, out, o, srcused, errcount, rangecount;
        int statlen, srcpos, lendiff;
        int fwidth;

        handle = resample_open(1, factor, factor);
        fwidth = resample_get_filter_width(handle);
        out = 0;
        srcpos = 0;
        for (;;)
        {
            int srcBlock = MIN(srclen - srcpos, srcblocksize);
            int lastFlag = (srcBlock == srclen - srcpos);

            o = resample_process(handle, factor,
                                &src[srcpos], srcBlock,
                                lastFlag, &srcused,
                                &dst[out], MIN(dstlen - out, dstblocksize));
            srcpos += srcused;
            if (o >= 0)
                out += o;
            if (o < 0 || (o == 0 && srcpos == srclen))
                break;
            // std::cout << "Out:" << std::to_string(out) << std::endl;
        }
        resample_close(handle);

        if (o < 0)
            std::cout << "Error: resample_process returned an error: " << o << std::endl;

        if (out <= 0)
        {
            std::cout << "Error: resample_process returned " << out << " samples" << std::endl;
            return;
        }

        lendiff = abs(out - expectedlen);
        if (lendiff > (int)(2 * factor + 1.0))
            std::cout << "Expected " << expectedlen << " samples, got " << out << std::endl;
    }

public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};

    inlet<>  input {this, "(signal) Input", "signal"};

    receiver_tilde(const atoms& args = {}) : lsl_inlet(nullptr) {
        std::string prop;
        std::string value;

        if (args.size() < 2) {
            cout << "Not enough arguments provided, defaulting to default 'type' 'EEG'" << endl;
            prop = "type";
            value = "EEG";
        }
        else {
            prop = args[0];
            value = args[1];
        }
        std::vector<lsl::stream_info> results = lsl::resolve_stream(prop, value, 1, LSL_SCAN_TIMEOUT);

        if (results.empty())
            error("No LSL stream found!");
        else {
            lsl::stream_info info = results.at(0);
            lsl_inlet = std::make_unique<lsl::stream_inlet>(info);
            cout << "[LSL] LSL stream resolved" << endl;

            n_channels = lsl_inlet->info().channel_count();
            og_sample_rate = lsl_inlet->info().nominal_srate();
            std::cout << "[LSL] Number of channels: " << std::to_string(n_channels) << std::endl;
            std::cout << "[LSL] Sampling rate: " << std::to_string(og_sample_rate) << std::endl;

            for (auto i=0; i<n_channels; i++) {
                auto an_outlet = std::make_unique<outlet<>>(this, "(signal) Output signal " + std::to_string(i+1) + "", "signal");
                m_outlets.push_back( std::move(an_outlet) );
            }

            // Start the thread to listen for LSL samples
            lslListenerThread = std::thread(&receiver_tilde::listenForSamples, this);

            // Start the thread to resample
            resamplerThread = std::thread(&receiver_tilde::resampleWhenReady, this);
        }
    };

    ~receiver_tilde() {
        // Stop the threads gracefully
        stopListening();
    };

    message<> dspsetup { this, "dspsetup",
        MIN_FUNCTION {
            number samplerate = args[0];
            int vectorsize = args[1];
            std::cout << "[DSP] Samplerate: " << std::to_string(samplerate) << std::endl;
            std::cout << "[DSP] Vectorsize: " << std::to_string(vectorsize) << std::endl;
            // src_len = vectorsize;  // I know, it's just to be clear
            src_len = SRC_BUFFER_SIZE;
            resampling_factor = samplerate / og_sample_rate;
            dst_len = (int)(src_len * resampling_factor);
            std::cout << "[RESAMPLING] Input buffer size:" << std::to_string(src_len) << std::endl;
            std::cout << "[RESAMPLING] Output buffer size:" << std::to_string(dst_len) << std::endl;

            dst_buffer_size = dst_len * 2;
            src = (float*)calloc(src_len, sizeof(float));
            dst = (float*)calloc(dst_buffer_size + 100, sizeof(float));
            dsp_setup_done = true;
            return{};
        }
    };

    void stopListening() {
        stopThreadFlag = true;
        if (lslListenerThread.joinable())
            lslListenerThread.join();
        if (resamplerThread.joinable())
            resamplerThread.join();
    }

    void listenForSamples() {
        try {
            std::vector<float> sample(n_channels);
            while (!stopThreadFlag) {
                lsl_inlet->pull_sample(sample);
                if (dsp_setup_done)
                    lsl_to_resampler_fifo.try_enqueue(sample);
            }
        }
        catch (std::exception& e) {
            std::cerr << "LSLPortListener exception" << e.what() << std::endl;
        }
    }

    void resampleWhenReady() {
        std::vector<float> in_sample(n_channels);
        int dst_index;

        while (!stopThreadFlag) {
            if (!dsp_setup_done)
                continue;

            if (lsl_to_resampler_fifo.try_dequeue(in_sample)) {
                src[src_current_idx] = in_sample[0];
                src_current_idx++;
                if (src_current_idx == SRC_BUFFER_SIZE) {
                    if (fillFirstHalf)
                        dst_index = 0;
                    else
                        dst_index = dst_len;
                    std::unique_lock<std::mutex> lock(dstResamplingMutex);
                    std::cout << "dst index" << std::to_string(dst_index) << std::endl;
                    resample(src, dst + dst_index, src_len, resampling_factor, src_len, dst_len);
                    lock.unlock();
                    src_current_idx = 0;
                    firstBufferDone = true;
                    fillFirstHalf = !fillFirstHalf;
                }
            }
        }
    }

    void operator()(audio_bundle _, audio_bundle output) {
        std::vector<float> sample(n_channels);

        for (auto i = 0; i < output.frame_count(); i++) {
            if (dst_current_idx <= dst_buffer_size && firstBufferDone) {
                output.samples(0)[i] = dst[dst_current_idx];
            } else {
                dst_current_idx = 0;
                output.samples(0)[i] = dst[dst_current_idx];
            }
            dst_current_idx++;
        }
    }
};

MIN_EXTERNAL(receiver_tilde);
