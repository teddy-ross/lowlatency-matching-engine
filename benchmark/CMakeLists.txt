cmake_minimum_required(VERSION 3.13)

# Benchmark suite for the matching engine
# This file is included from the root CMakeLists.txt when BUILD_BENCHMARKS=ON

# Optimization flags for benchmarking
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# Default to Release build for benchmarks
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories from parent project
include_directories(${CMAKE_SOURCE_DIR}/include)

# Engine source files from parent project
set(ENGINE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/MatchingEngine.cpp
)

# Unit benchmarks - direct MatchingEngine tests
add_executable(benchmark_suite
    unit/benchmark_suite.cpp
    ${ENGINE_SOURCES}
)

add_executable(stress_test
    unit/stress_test.cpp
    ${ENGINE_SOURCES}
)

# Apply same compile options as main project
target_compile_options(benchmark_suite PRIVATE
    $<$<CONFIG:Release>:-O3 -pipe -fno-exceptions>
)

target_compile_options(stress_test PRIVATE
    $<$<CONFIG:Release>:-O3 -pipe -fno-exceptions>
)

if(ENABLE_NATIVE)
    target_compile_options(benchmark_suite PRIVATE -march=native)
    target_compile_options(stress_test PRIVATE -march=native)
endif()

# Link pthread if needed
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(benchmark_suite Threads::Threads)
    target_link_libraries(stress_test Threads::Threads)
endif()

# Custom targets for convenience
add_custom_target(run_unit_benchmarks
    COMMAND benchmark_suite
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit benchmarks..."
)

add_custom_target(run_stress
    COMMAND stress_test
    DEPENDS stress_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running stress tests..."
)

# Integration test - requires running server
add_custom_target(run_integration_test
    COMMAND ${CMAKE_COMMAND} -E echo "Starting integration test (ensure server is running)..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/integration/generator.py 50000
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/integration
    COMMENT "Running integration tests (requires marketDataHandlerLL server running)..."
)

# Run all benchmarks
add_custom_target(run_all_benchmarks
    DEPENDS run_unit_benchmarks run_stress
    COMMENT "Running all benchmarks..."
)

# Performance analysis with perf (Linux only)
add_custom_target(perf_benchmarks
    COMMAND perf stat -e cycles,instructions,cache-misses,branch-misses ./benchmark_suite
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running benchmarks with perf..."
)

# Memory profiling with valgrind
add_custom_target(memcheck
    COMMAND valgrind --tool=massif --massif-out-file=massif.out ./benchmark_suite
    COMMAND ms_print massif.out
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running memory profiling..."
)

# Callgrind profiling
add_custom_target(profile
    COMMAND valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./benchmark_suite
    COMMAND callgrind_annotate callgrind.out
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running callgrind profiling..."
)

# Display build configuration
message(STATUS "Benchmark build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Benchmark CXX Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
