# Benchmark suite for the matching engine.
# Included from the root CMakeLists.txt when BUILD_BENCHMARKS=ON.
# Compiler settings (C++23, warnings, -march=native) propagate from engine_core.

add_executable(benchmark_suite unit/benchmark_suite.cpp)
target_link_libraries(benchmark_suite PRIVATE engine_core)

add_executable(stress_test unit/stress_test.cpp)
target_link_libraries(stress_test PRIVATE engine_core)

find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(benchmark_suite PRIVATE Threads::Threads)
    target_link_libraries(stress_test PRIVATE Threads::Threads)
endif()

# --- Convenience targets ---

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

add_custom_target(run_integration_test
    COMMAND ${CMAKE_COMMAND} -E echo "Starting integration test (ensure server is running)..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/integration/generator.py 50000
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/integration
    COMMENT "Running integration tests (requires marketDataHandlerLL server running)..."
)

add_custom_target(run_all_benchmarks
    DEPENDS run_unit_benchmarks run_stress
    COMMENT "Running all benchmarks..."
)

add_custom_target(perf_benchmarks
    COMMAND perf stat -e cycles,instructions,cache-misses,branch-misses ./benchmark_suite
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running benchmarks with perf..."
)

add_custom_target(memcheck
    COMMAND valgrind --tool=massif --massif-out-file=massif.out ./benchmark_suite
    COMMAND ms_print massif.out
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running memory profiling..."
)

add_custom_target(profile
    COMMAND valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./benchmark_suite
    COMMAND callgrind_annotate callgrind.out
    DEPENDS benchmark_suite
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running callgrind profiling..."
)
