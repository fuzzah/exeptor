cmake_minimum_required(VERSION 3.8)
project(exeptor)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

add_subdirectory(external/yaml-cpp yaml-cpp)
set_property(TARGET yaml-cpp PROPERTY POSITION_INDEPENDENT_CODE ON)

set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")

add_executable(app-proxy ${SRC_DIR}/app-proxy.cpp)

add_library(exeptor SHARED
    ${SRC_DIR}/config.hpp
    ${SRC_DIR}/exeptor.cpp
)
target_link_libraries(exeptor PRIVATE dl PRIVATE yaml-cpp)

set_property(TARGET exeptor PROPERTY POSITION_INDEPENDENT_CODE ON)

