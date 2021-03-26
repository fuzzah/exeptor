cmake_minimum_required(VERSION 3.19)
project(exeptor)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

add_executable(app-proxy app-proxy.c)

add_library(exeptor SHARED config.hpp exeptor.cpp)
add_subdirectory(external/yaml-cpp yaml-cpp)
target_link_libraries(exeptor PRIVATE dl PUBLIC yaml-cpp)

set_property(TARGET exeptor PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET yaml-cpp PROPERTY POSITION_INDEPENDENT_CODE ON)