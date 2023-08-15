/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <iostream>
#include <string>
#include <vector>
#include "c74_min.h"


using namespace c74::min;


class jaw_clench : public object<jaw_clench> {
private:
    std::vector< std::unique_ptr<inlet<>> >     m_inlets;
public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};

    outlet<> output {this, "(bloat) Is jaw clenching?"};

    jaw_clench(const atoms& args = {}) {
        for (auto i=0; i < in_channels; ++i) {
            auto an_inlet = std::make_unique<inlet<>>(this, "(float) LSL Channel " + std::to_string(i+1) + "");
            m_inlets.push_back( std::move(an_inlet) );
        }
    };

    attribute<int> in_channels { this, "in_channels", 4, title {"Number of channels"},
        description {"Number of channels of the LSL stream."}
    };
};


MIN_EXTERNAL(jaw_clench);
