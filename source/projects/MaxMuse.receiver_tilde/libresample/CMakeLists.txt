# Create the libresample library target
add_library(libresample
    src/filterkit.c
    src/resample.c
    src/resamplesubs.c
)

# Set include directories for libresample
target_include_directories(libresample PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
