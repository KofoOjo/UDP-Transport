cmake_minimum_required(VERSION 3.16)
project(reliable_udp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(reliable_udp reliable_udp.cpp)
target_compile_options(reliable_udp PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(reliable_udp PRIVATE pthread)

add_executable(reliable_udp_demo demo.cpp)
target_link_libraries(reliable_udp_demo PRIVATE reliable_udp)

add_executable(udp_bridge udp_bridge.cpp)
target_link_libraries(udp_bridge PRIVATE reliable_udp)
