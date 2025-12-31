#include "MatchingEngine.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>
#include <iomanip>
#include <numeric>
#include <algorithm>

using namespace std::chrono;

struct BenchmarkResult {
    std::string name;
    double avg_latency_ns;
    double p50_ns;
    double p95_ns;
    double p99_ns;
    double throughput_ops_per_sec;
    size_t total_ops;
};

class OrderBookBenchmark {
public:
    OrderBookBenchmark() : gen(42) {}

    void warmup(MatchingEngine& engine, int iterations = 1000) {
        std::ostringstream dummy;
        for (int i = 0; i < iterations; ++i) {
            Order o{i, Side::Buy, 100, 10};
            engine.submit(o, dummy);
        }
    }

    BenchmarkResult benchmarkSubmitOnly(int num_orders) {
        MatchingEngine engine;
        std::ostringstream out;
        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        auto start = high_resolution_clock::now();

        for (int i = 0; i < num_orders; ++i) {
            Order o = generateRandomOrder(i);
            
            auto t1 = high_resolution_clock::now();
            engine.submit(o, out);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        auto end = high_resolution_clock::now();
        double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats("Submit Only", latencies, elapsed_sec);
    }

    BenchmarkResult benchmarkMixedWorkload(int num_ops) {
        MatchingEngine engine;
        std::ostringstream out;
        std::vector<long long> latencies;
        latencies.reserve(num_ops);
        std::vector<int> active_ids;

        auto start = high_resolution_clock::now();

        for (int i = 0; i < num_ops; ++i) {
            auto t1 = high_resolution_clock::now();
            
            // 70% submit, 30% cancel
            if (dist(gen) < 0.7 || active_ids.empty()) {
                Order o = generateRandomOrder(i);
                engine.submit(o, out);
                active_ids.push_back(i);
            } else {
                // Cancel random active order
                size_t idx = gen() % active_ids.size();
                int cancel_id = active_ids[idx];
                active_ids.erase(active_ids.begin() + idx);
                engine.cancel(cancel_id, out);
            }
            
            auto t2 = high_resolution_clock::now();
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        auto end = high_resolution_clock::now();
        double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats("Mixed Workload (70/30)", latencies, elapsed_sec);
    }

    BenchmarkResult benchmarkWorstCase(int num_orders) {
        MatchingEngine engine;
        std::ostringstream out;
        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        // Build deep book on one side
        for (int i = 0; i < num_orders / 2; ++i) {
            Order o{i, Side::Buy, 100, 1};
            engine.submit(o, out);
        }

        auto start = high_resolution_clock::now();

        // Large aggressive sell that crosses entire book
        for (int i = num_orders / 2; i < num_orders; ++i) {
            Order o{i, Side::Sell, 50, num_orders};
            
            auto t1 = high_resolution_clock::now();
            engine.submit(o, out);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        auto end = high_resolution_clock::now();
        double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats("Worst Case (Deep Book Cross)", latencies, elapsed_sec);
    }

    BenchmarkResult benchmarkCancelation(int num_orders) {
        MatchingEngine engine;
        std::ostringstream out;
        std::vector<int> ids;

        // Submit orders first
        for (int i = 0; i < num_orders; ++i) {
            Order o = generateRandomOrder(i);
            engine.submit(o, out);
            ids.push_back(i);
        }

        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        auto start = high_resolution_clock::now();

        // Cancel all orders
        for (int id : ids) {
            auto t1 = high_resolution_clock::now();
            engine.cancel(id, out);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        auto end = high_resolution_clock::now();
        double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats("Cancel Only", latencies, elapsed_sec);
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist{0.0, 1.0};

    Order generateRandomOrder(int id) {
        Side side = (dist(gen) < 0.5) ? Side::Buy : Side::Sell;
        int price = 95 + (gen() % 11); // 95-105
        int qty = 1 + (gen() % 10);    // 1-10
        return Order{id, side, price, qty};
    }

    BenchmarkResult computeStats(const std::string& name, 
                                  std::vector<long long>& latencies,
                                  double elapsed_sec) {
        std::sort(latencies.begin(), latencies.end());
        
        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        double avg = sum / latencies.size();
        
        size_t p50_idx = latencies.size() * 50 / 100;
        size_t p95_idx = latencies.size() * 95 / 100;
        size_t p99_idx = latencies.size() * 99 / 100;
        
        return BenchmarkResult{
            name,
            avg,
            static_cast<double>(latencies[p50_idx]),
            static_cast<double>(latencies[p95_idx]),
            static_cast<double>(latencies[p99_idx]),
            latencies.size() / elapsed_sec,
            latencies.size()
        };
    }
};

void printResult(const BenchmarkResult& result) {
    std::cout << "\n=== " << result.name << " ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total operations:  " << result.total_ops << std::endl;
    std::cout << "Throughput:        " << result.throughput_ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Average latency:   " << result.avg_latency_ns << " ns" << std::endl;
    std::cout << "P50 latency:       " << result.p50_ns << " ns" << std::endl;
    std::cout << "P95 latency:       " << result.p95_ns << " ns" << std::endl;
    std::cout << "P99 latency:       " << result.p99_ns << " ns" << std::endl;
}

int main() {
    OrderBookBenchmark bench;
    
    std::cout << "Starting Order Book Benchmarks..." << std::endl;
    std::cout << "====================================" << std::endl;

    // Warmup
    std::cout << "\nWarming up..." << std::endl;
    MatchingEngine warmup_engine;
    bench.warmup(warmup_engine);

    // Run benchmarks
    const int NUM_OPS = 100000;

    auto r1 = bench.benchmarkSubmitOnly(NUM_OPS);
    printResult(r1);

    auto r2 = bench.benchmarkMixedWorkload(NUM_OPS);
    printResult(r2);

    auto r3 = bench.benchmarkCancelation(NUM_OPS);
    printResult(r3);

    auto r4 = bench.benchmarkWorstCase(10000); // Smaller for worst case
    printResult(r4);

    std::cout << "\n====================================" << std::endl;
    std::cout << "Benchmarks complete!" << std::endl;

    return 0;
}
