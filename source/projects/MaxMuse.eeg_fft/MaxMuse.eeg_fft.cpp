/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <iostream>
#include <string>
#include <vector>
#include "c74_min.h"

using namespace c74::min;


class eeg_fft : public object<eeg_fft> {
private:
    std::vector< std::unique_ptr<inlet<>> >     m_inlets;
    std::vector< std::unique_ptr<outlet<>> >    m_outlets;

public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};


    eeg_fft(const atoms& args = {}) {
        for (auto i=0; i < in_channels; ++i) {
            auto an_inlet = std::make_unique<inlet<>>(this, "(float) LSL Channel " + std::to_string(i+1) + "");
            m_inlets.push_back( std::move(an_inlet) );
        }

        if (average) {
            auto an_outlet = std::make_unique<outlet<>>(this, "(float) Average");
            m_outlets.push_back( std::move(an_outlet) );
        }
        else {
            for (auto i=0; i < in_channels; ++i) {
                auto an_outlet = std::make_unique<outlet<>>(this, "(float) FFT Channel " + std::to_string(i+1) + "");
                m_outlets.push_back( std::move(an_outlet) );
            }
        }
    };

    attribute<int> in_channels { this, "in_channels", 4, title {"Number of channels"},
        description {"Number of channels of the LSL stream."}
    };

    attribute<int> sample_rate {this, "sample_rate", 256, title {"Sampling rate"},
        description {"Sampling rate of the LSL stream."}
    };

    attribute<bool> average {this, "average", false, title {"Average"},
        description {"Average the samples of the LSL stream."}
    };

    attribute<int> window {this, "window", 256, title {"Window size"},
        description {"Window size of the LSL stream."}
    };
};



MIN_EXTERNAL(eeg_fft);
