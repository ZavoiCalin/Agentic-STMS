cmake_minimum_required(VERSION 3.20)
project(stms_native LANGUAGES CXX)

add_executable(stms-native main.cpp)
target_compile_features(stms-native PRIVATE cxx_std_17)
# Use a system nlohmann/json (apt install nlohmann-json3-dev) if present,
# otherwise download the pinned, hash-verified release.
find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
target_link_libraries(stms-native PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(stms-native PRIVATE winhttp ws2_32)
endif()

if(MSVC)
    target_compile_options(stms-native PRIVATE /W4 /permissive-)
else()
    target_compile_options(stms-native PRIVATE -Wall -Wextra -Wpedantic)
endif()
