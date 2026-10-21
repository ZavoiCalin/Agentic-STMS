cmake_minimum_required(VERSION 3.10)
project(DualCamTRTDisp LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CUDA_STANDARD 14)

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)

# Include directories
include_directories(
    ${GST_INCLUDE_DIRS}
    /usr/include/aarch64-linux-gnu
    /usr/local/cuda/include
)

# Link directories
link_directories(
    ${GST_LIBRARY_DIRS}
    /usr/lib/aarch64-linux-gnu
    /usr/local/cuda/lib64
)

# Define executable
add_executable(dual_cam_trt_disp main.cu)

# Link libraries
target_link_libraries(dual_cam_trt_disp
    ${GST_LIBRARIES}
    nvinfer
    cudart
    pthread
)