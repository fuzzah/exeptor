cmake_minimum_required(VERSION 3.4)
project(exeptor)

#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
endif()

option(EXEPTOR_TESTS "Build exeptor tests" OFF)
option(EXEPTOR_COVERAGE "Build exeptor with coverage collection" OFF)
option(EXEPTOR_ASAN "Build exeptor with AddressSanitizer" OFF)
option(EXEPTOR_UBSAN "Build exeptor with UndefinedBehaviorSanitizer" OFF)

if (EXEPTOR_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
        message(NOTICE "Using LLVM-profdata coverage collecting mode")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
        message(NOTICE "Using GCOV coverage collecting mode")
    else()
        message(FATAL_ERROR "Sorry, but exeptor's author doesn't know about other compilers than gcc and clang")
    endif()
endif()


if (EXEPTOR_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if (EXEPTOR_UBSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

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

if(EXEPTOR_TESTS)
    add_subdirectory(test)
endif()
