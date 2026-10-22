cmake_minimum_required(VERSION 3.18)
project(STMSJetson LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-video-1.0)
pkg_check_modules(DEEPSTREAM REQUIRED IMPORTED_TARGET libnvds_meta)

find_package(CUDAToolkit REQUIRED)

add_executable(stms-jetson-cameras gst_dual_stream.cpp)
target_compile_features(stms-jetson-cameras PRIVATE cxx_std_17)
target_include_directories(stms-jetson-cameras PRIVATE ${DEEPSTREAM_INCLUDE_DIRS})
target_link_libraries(stms-jetson-cameras PRIVATE
    PkgConfig::GST
    PkgConfig::DEEPSTREAM
    CUDA::cuda_driver
    CUDA::cudart
)

target_compile_options(stms-jetson-cameras PRIVATE -Wall -Wextra -Wpedantic)
