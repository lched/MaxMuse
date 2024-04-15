# MaxMuse
MaxMuse is a set of Max MSP objects to connect the Muse 2 EEG headband (or any device supporting the LSL protocol) directly into Max MSP. It supports receiving the LSL streams both as Max messages (in the native sample rate of the stream) or resampled to audio sample-rate (even though that part still needs a bit of work). 

⚠️ This project is still a work in progress, objects might still be instable or bugged in some cases.

## Installation
You may directly use the objects that are in the _externals_ folder (.mxe64 for Windows and .mxo for OSX), which you can download directly from the releases section. The Max objects are compiled for OSX but not yet tested. You may also build them manually, the whole project is set up using CMake. 

## Building
To compile existing objects, you will need a compiler:

* On OSX this means **Xcode 9 or later** (you can get from the App Store for free).
* On Windows this means **Visual Studio 2017** (you can download a free version from Microsoft). The installer for Visual Studio 2017 offers an option to install Git, which you should choose to do.

First, clone the project recursively with its submodules in the Max _Packages_ folder (or elsewhere but you'll need to move the generated files there). 

    git clone https://github.com/lched/MaxMuse.git --recursive
Then you can follow the build instructions available here: [https://github.com/Cycling74/min-devkit](https://github.com/Cycling74/min-devkit)

## Acknowledgements
This package was created using the Min-DevKit for Max, an API and supporting tools for writing externals in modern C++.
