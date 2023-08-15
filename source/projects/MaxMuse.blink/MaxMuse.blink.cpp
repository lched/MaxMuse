/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <iostream>
#include <string>
#include <vector>
#include "c74_min.h"

using namespace c74::min;


class blink : public object<blink> {
public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};

    inlet<> input {this, "(toggle) on/off"};
    outlet<> timestamp_out {this, "(float) Timestamps"};



};


MIN_EXTERNAL(blink);
