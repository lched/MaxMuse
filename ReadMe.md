# MaxLSL
This package was created using the Min-DevKit for Max, an API and supporting tools for writing externals in modern C++.


## Building
This must be built by passing the LSL_INSTALL_ROOT variable to CMake.
See the liblsl repository for information about building LSL and/or downloading latest releases.

    "-DLSL_INSTALL_ROOT=path/to/liblsl/build/install/"
Note: CMake accepts arguments passed with no space. Yeah it's weird.

## Prerequisites

You can use the objects provided in this package as-is.

To code your own objects, or to re-compile existing objects, you will need a compiler:

* On the Mac this means **Xcode 9 or later** (you can get from the App Store for free).
* On Windows this means **Visual Studio 2017** (you can download a free version from Microsoft). The installer for Visual Studio 2017 offers an option to install Git, which you should choose to do.

You will also need the Min-DevKit, available from the Package Manager inside of Max or [directly from Github](https://github.com/Cycling74/min-devkit).




## Contributors / Acknowledgements

The MaxLSL is the work of some amazing and creative artists, researchers, and coders.



## Support

For support, please contact the developer of this package.
