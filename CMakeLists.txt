cmake_minimum_required(VERSION 3.20)
project(apple-midi-mcp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

# Fetch nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(apple-midi-mcp
    src/main.cpp
    src/mcp_server.cpp
    src/midi_bridge.cpp
)

target_link_libraries(apple-midi-mcp PRIVATE
    nlohmann_json::nlohmann_json
    "-framework CoreMIDI"
    "-framework CoreFoundation"
)

target_include_directories(apple-midi-mcp PRIVATE src)
