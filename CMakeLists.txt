cmake_minimum_required(VERSION 3.13)
project(lowlatency-matching-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_NATIVE "Enable -march=native optimizations" ON)

add_executable(lowlatency-matching-engine src/main.cpp)

target_compile_options(lowlatency-matching-engine PRIVATE $<$<CONFIG:Release>:-O3 -pipe -fno-exceptions>)

if (ENABLE_NATIVE)
  target_compile_options(lowlatency-matching-engine PRIVATE -march=native)
endif()



                          
                          
