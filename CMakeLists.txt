cmake_minimum_required(VERSION 3.19)

# ---------------------------------------------------------------------------
# Platform settings -- must come before project()
# ---------------------------------------------------------------------------

# min-api force-caches CMAKE_OSX_DEPLOYMENT_TARGET=10.11, which is too old for
# liblsl >= 1.16 (std::shared_mutex needs 10.12). Every target re-applies this value
# (see source/projects/*/CMakeLists.txt and external/CMakeLists.txt).
set(MAXMUSE_MACOS_MIN "10.15" CACHE STRING "Minimum macOS version of the externals")
set(CMAKE_OSX_DEPLOYMENT_TARGET ${MAXMUSE_MACOS_MIN})

# Universal (Apple Silicon + Intel) binaries by default.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

# Static MSVC runtime everywhere (Max externals must not need the VC++ redistributable,
# and liblsl must use the same runtime as the externals it is linked into).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MaxMuse VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    message(STATUS "MaxMuse: macOS ${MAXMUSE_MACOS_MIN}+, architectures: ${CMAKE_OSX_ARCHITECTURES}")
endif()

set(MAXMUSE_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/shared)

# Third-party libraries (liblsl, built statically)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external)

# Misc setup and subroutines (package-info.json, version, SUBDIRLIST)
include(${CMAKE_CURRENT_SOURCE_DIR}/source/min-api/script/min-package.cmake)

# Add the Lib, if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/min-lib/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/min-lib)
endif()

# Generate a project for every folder in the "source/projects" folder
SUBDIRLIST(PROJECT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/source/projects)
foreach(project_dir ${PROJECT_DIRS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir}/CMakeLists.txt")
        message("Generating: ${project_dir}")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/projects/${project_dir})
    endif()
endforeach()

# Comment the line below if you want automatic cmake regeneration enabled
set(CMAKE_SUPPRESS_REGENERATION true)
