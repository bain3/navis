cmake_minimum_required(VERSION 3.16)
project(navis)

include(FetchContent)

FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.8.7
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.9.1
)
FetchContent_MakeAvailable(json)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
add_executable(navis main.cpp libs/sha1.cpp executor.cpp utils.cpp)
target_link_libraries (navis ${CMAKE_THREAD_LIBS_INIT}
        OpenSSL::SSL OpenSSL::Crypto httplib nlohmann_json::nlohmann_json)