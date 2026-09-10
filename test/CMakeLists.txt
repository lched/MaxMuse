# Standalone test project (does not need the Max SDK):
#   cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test --output-on-failure
cmake_minimum_required(VERSION 3.19)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(MaxMuseTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)
enable_testing()

set(MAXMUSE_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../source/shared)

add_executable(test_resampler test_resampler.cpp)
target_include_directories(test_resampler PRIVATE ${MAXMUSE_SHARED_DIR})
target_link_libraries(test_resampler PRIVATE Threads::Threads)
add_test(NAME resampler COMMAND test_resampler)

option(MAXMUSE_TEST_LSL "Build the LSL loopback test (needs the external/liblsl submodule)" ON)
if(MAXMUSE_TEST_LSL)
    set(LSL_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(LSL_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(LSL_OPTIMIZATIONS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/liblsl ${CMAKE_BINARY_DIR}/liblsl EXCLUDE_FROM_ALL)

    add_executable(test_lsl_loopback test_lsl_loopback.cpp)
    target_include_directories(test_lsl_loopback PRIVATE ${MAXMUSE_SHARED_DIR})
    target_link_libraries(test_lsl_loopback PRIVATE lsl Threads::Threads)
    add_test(NAME lsl_loopback COMMAND test_lsl_loopback)
    set_tests_properties(lsl_loopback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
endif()
