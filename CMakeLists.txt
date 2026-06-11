cmake_minimum_required(VERSION 3.20)
project(marketDataHandlerLL VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_NATIVE     "Enable -march=native optimizations" ON)
option(BUILD_BENCHMARKS  "Build benchmark suite"              OFF)
option(BUILD_TESTS       "Build unit tests"                   ON)

# Engine core: the matching engine itself is header-only; this library carries
# the protocol translation unit plus the public usage requirements (include
# path, language level, warnings) for every consumer.
add_library(engine_core STATIC
    src/Protocol.cpp
)

target_include_directories(engine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(engine_core PUBLIC cxx_std_23)
target_compile_options(engine_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow)

if(ENABLE_NATIVE)
    target_compile_options(engine_core PUBLIC -march=native)
endif()

add_executable(marketDataHandlerLL src/main.cpp)
target_link_libraries(marketDataHandlerLL PRIVATE engine_core)

if(BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine_core)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

if(BUILD_BENCHMARKS)
    message(STATUS "Building benchmarks")
    add_subdirectory(benchmark)
endif()
