cmake_minimum_required(VERSION 3.10)
project(DeepStreamAzure)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0)
pkg_check_modules(DEEPSTREAM REQUIRED libnvds_meta)

include_directories(
    ${GST_INCLUDE_DIRS}
    ${DEEPSTREAM_INCLUDE_DIRS}
    /path/to/azure-iot-sdk-c/umqtt/include
    /path/to/azure-iot-sdk-c/iothub_client/inc
)

link_directories(
    ${GST_LIBRARY_DIRS}
    ${DEEPSTREAM_LIBRARY_DIRS}
)

add_executable(DeepStreamAzure main.cpp)
target_link_libraries(DeepStreamAzure ${GST_LIBRARIES} ${DEEPSTREAM_LIBRARIES} iothub_client uamqp umqtt c shared_util)
