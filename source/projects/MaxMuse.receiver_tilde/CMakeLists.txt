# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.20)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

if(APPLE)
    # min-pretarget.cmake force-sets 10.11; restore the project-wide minimum.
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${MAXMUSE_MACOS_MIN})
endif()

#############################################################
# MAX EXTERNAL
#############################################################

include_directories("${C74_INCLUDES}")

add_library(${PROJECT_NAME} MODULE
    ${PROJECT_NAME}.cpp
    ${MAXMUSE_SHARED_DIR}/maxmuse_lsl.h
    ${MAXMUSE_SHARED_DIR}/maxmuse_resampler.h
)
target_include_directories(${PROJECT_NAME} PRIVATE ${MAXMUSE_SHARED_DIR})

#############################################################
# EXTERNAL DEPENDENCIES
#############################################################
target_link_libraries(${PROJECT_NAME} PRIVATE lsl)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)
