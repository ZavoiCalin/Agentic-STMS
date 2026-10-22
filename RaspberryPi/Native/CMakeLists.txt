cmake_minimum_required(VERSION 3.20)
project(stms_native LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(stms-native main.cpp)
target_compile_features(stms-native PRIVATE cxx_std_17)
target_link_libraries(stms-native PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(stms-native PRIVATE winhttp ws2_32)
endif()

if(MSVC)
    target_compile_options(stms-native PRIVATE /W4 /permissive-)
else()
    target_compile_options(stms-native PRIVATE -Wall -Wextra -Wpedantic)
endif()
