cmake_minimum_required(VERSION 3.20)
project(stms_native LANGUAGES CXX)

add_executable(stms-native main.cpp)
target_compile_features(stms-native PRIVATE cxx_std_17)
# nlohmann/json 3.11.3 is vendored so the build needs no network access.
target_include_directories(stms-native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(WIN32)
    target_link_libraries(stms-native PRIVATE winhttp ws2_32)
endif()

if(MSVC)
    target_compile_options(stms-native PRIVATE /W4 /permissive-)
else()
    target_compile_options(stms-native PRIVATE -Wall -Wextra -Wpedantic)
endif()
