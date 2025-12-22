cmake_minimum_required(VERSION 3.13)
project(marketDataHandlerLL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(marketDataHandlerLL
    src/main.cpp
    src/MatchingEngine.cpp
    src/Protocol.cpp
)

target_include_directories(marketDataHandlerLL PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
