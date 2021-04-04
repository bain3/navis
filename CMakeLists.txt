cmake_minimum_required(VERSION 3.16)
project(navis)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
add_executable(navis main.cpp libs/sha1.cpp libs/httplib.cpp executor.cpp utils.cpp)
target_link_libraries (navis ${CMAKE_THREAD_LIBS_INIT} OpenSSL::SSL OpenSSL::Crypto)