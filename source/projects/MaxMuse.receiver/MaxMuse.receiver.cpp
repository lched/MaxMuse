/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <lsl_cpp.h>
#include "c74_min.h"

const double LSL_SCAN_TIMEOUT = 0.2;
const int LSL_MAX_SAMPLES = 1;
const double LSL_PULL_TIMEOUT = 0.1;

using namespace c74::min;

class receiver : public object<receiver> {
private:
    // variables related to the LSL stream
    std::unique_ptr<lsl::stream_inlet> lsl_inlet;
    int n_channels;
    std::thread lslListenerThread;
    fifo<std::vector<float>> sample_fifo { 100 };
    double deliverer_delay;

    // the number of outlers is defined at runtime depending on the number of channels of the LSL inlet
    std::vector< std::unique_ptr<outlet<>> >    m_outlets;

    // A flag to signal the thread to stop carefully
    bool stopThreadFlag = false;
public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};

    receiver(const atoms& args = {}) : lsl_inlet(nullptr) {
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

            // Define the number of outlets depending on the number of channels of the LSL inlet
            n_channels = info.channel_count();
            std::cout << "[LSL] Number of channels: " << std::to_string(n_channels) << std::endl;
            for (auto i=0; i < n_channels; ++i) {
                auto an_outlet = std::make_unique<outlet<>>(this, "(float) " + info.type() + std::to_string(i+1) + "");
                m_outlets.push_back( std::move(an_outlet) );
            }

            // Start the thread to listen for samples
            deliverer_delay = 1 / info.nominal_srate() * 1000;
            lslListenerThread = std::thread(&receiver::listenForSamples, this);

            // Start the deliverer
            deliverer.delay(0.0);
        }
    }

    ~receiver() {
        // Stop the thread gracefully
        stopListening();
        if (lslListenerThread.joinable())
            lslListenerThread.join();
    }

    void stopListening() {
        stopThreadFlag = true;
    }

    void listenForSamples() {
        try {
            std::vector<float> sample;
            while (!stopThreadFlag) {
                // Blocking call to get the next sample
                lsl_inlet->pull_sample(sample);
                sample_fifo.try_enqueue(sample);
            }
        }
        catch (std::exception& e) {
            std::cerr << "LSLPortListener exception: " << e.what() << std::endl;
        }
    }

    timer<> deliverer { this,
        MIN_FUNCTION {
            std::vector<float> sample;
            while(sample_fifo.try_dequeue(sample)) {
                for (auto i=0; i < n_channels; i++) {
                    m_outlets[i]->send(sample[i]);
                }
            }
            deliverer.delay(deliverer_delay);
            return {};
        }
    };
};

MIN_EXTERNAL(receiver);
