cmake_minimum_required(VERSION 3.15)
project(thermal_vospi_processing C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
add_compile_options(-Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Optional sanitizers: cmake -DENABLE_SANITIZERS=ON ..
# ---------------------------------------------------------------------------
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -g -O0 -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include_directories(${CMAKE_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Main processing pipeline
# (file list mirrors the gcc command documented in README.md — adjust if the
#  actual repo layout differs, e.g. extra headers-only files won't appear here)
# ---------------------------------------------------------------------------
add_executable(main
    app/main.c
    app/app_pipeline.c
    core/frame.c
    input/vospi_parser.c
    input/frame_reconstructor.c
    input/source_emulator.c
    processing/bad_pixels.c
    processing/denoise.c
    processing/normalize.c
    processing/contrast.c
    output/output_image.c
)
target_link_libraries(main PRIVATE m)

# ---------------------------------------------------------------------------
# Standalone video -> packet stream converter
# ---------------------------------------------------------------------------
add_executable(video_to_stream video_to_stream.c)
target_compile_options(video_to_stream PRIVATE -O2)

# ---------------------------------------------------------------------------
# Testing hook (enable once unit tests are added under tests/)
# ---------------------------------------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Convenience install rule
# ---------------------------------------------------------------------------
install(TARGETS main video_to_stream RUNTIME DESTINATION bin)