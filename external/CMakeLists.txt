# liblsl, linked statically into each external so that the .mxo / .mxe64 are
# self-contained (no liblsl.dylib / lsl.dll to ship next to Max).
#
# These must be CACHE ... FORCE: a plain set() is silently ignored by liblsl
# versions whose cmake_minimum_required() predates policy CMP0077, which is how
# the previous build ended up linking liblsl as a shared library.
set(LSL_BUILD_STATIC ON CACHE BOOL "Build liblsl as a static library" FORCE)
set(LSL_FRAMEWORK OFF CACHE BOOL "No Apple framework: we link statically" FORCE)
set(LSL_OPTIMIZATIONS OFF CACHE BOOL "No LTO objects inside the static library" FORCE)
set(LSL_UNITTESTS OFF CACHE BOOL "" FORCE)
set(LSL_INSTALL OFF CACHE BOOL "" FORCE)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${MAXMUSE_MACOS_MIN})
endif()

add_subdirectory(liblsl EXCLUDE_FROM_ALL)
