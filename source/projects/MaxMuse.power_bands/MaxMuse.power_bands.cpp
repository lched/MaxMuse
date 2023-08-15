/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include <iostream>
#include <string>
#include <vector>
#include "c74_min.h"

using namespace c74::min;


class power_bands : public object<power_bands> {
private:

public:
    MIN_DESCRIPTION	{"Receive an LSL stream and send it to Max."};
    MIN_TAGS		{"utilities, lsl"};
    MIN_AUTHOR		{"l-o"};
    MIN_RELATED		{"print, jit.print, dict.print"};

    inlet<>  input	{this, "(bang) send the LSL stream to Max"};
    outlet<> alpha_out {this, "(float) alpha power"};
    outlet<> beta_out {this, "(float) beta power"};
    outlet<> gamma_out {this, "(float) gamma power"};
    outlet<> theta_out {this, "(float) theta power"};
    outlet<> delta_out {this, "(float) delta power"};
};


MIN_EXTERNAL(power_bands);
