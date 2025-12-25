cmake_minimum_required(VERSION 3.13)
project(marketDataHandlerLL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_NATIVE "Enable -march=native optimizations" ON)

add_executable(marketDataHandlerLL
    src/main.cpp
    src/MatchingEngine.cpp
    src/Protocol.cpp
)

target_include_directories(marketDataHandlerLL PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(marketDataHandlerLL PRIVATE
    $<$<CONFIG:Release>:-O3 -pipe -fno-exceptions>
)

if (ENABLE_NATIVE)
  target_compile_options(marketDataHandlerLL PRIVATE -march=native)
endif()
