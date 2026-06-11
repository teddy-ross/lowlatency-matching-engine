#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace std::chrono;

// Each benchmark runs under two sink policies:
//   TextSinkAdapter — events formatted into a reused wire-protocol buffer,
//                     comparable to what the server pays per message.
//   NullSinkAdapter — events discarded, measuring pure engine cost.

struct TextSinkAdapter {
    static constexpr const char* label = "wire-format";
    std::string buf;
    FormattingSink sink{buf};
    void beginOp() { buf.clear(); }
};

struct NullSinkAdapter {
    static constexpr const char* label = "engine-only";
    NullSink sink;
    static void beginOp() noexcept {}
};

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
        NullSink drop;
        for (int i = 0; i < iterations; ++i) {
            engine.submit(Order{.id = i, .side = Side::Buy, .price = 100, .quantity = 10}, drop);
        }
    }

    template <class SinkAdapter>
    BenchmarkResult benchmarkSubmitOnly(int num_orders) {
        MatchingEngine engine;
        SinkAdapter out;
        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        const auto start = steady_clock::now();

        for (int i = 0; i < num_orders; ++i) {
            const Order o = generateRandomOrder(i);

            out.beginOp();
            const auto t1 = steady_clock::now();
            engine.submit(o, out.sink);
            const auto t2 = steady_clock::now();

            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        const auto end = steady_clock::now();
        const double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats(named<SinkAdapter>("Submit Only"), latencies, elapsed_sec);
    }

    template <class SinkAdapter>
    BenchmarkResult benchmarkMixedWorkload(int num_ops) {
        MatchingEngine engine;
        SinkAdapter out;
        std::vector<long long> latencies;
        latencies.reserve(num_ops);
        std::vector<int> active_ids;

        const auto start = steady_clock::now();

        for (int i = 0; i < num_ops; ++i) {
            out.beginOp();
            const auto t1 = steady_clock::now();

            // 70% submit, 30% cancel
            if (dist(gen) < 0.7 || active_ids.empty()) {
                engine.submit(generateRandomOrder(i), out.sink);
                active_ids.push_back(i);
            } else {
                const size_t idx = gen() % active_ids.size();
                const int cancel_id = active_ids[idx];
                active_ids.erase(active_ids.begin() + idx);
                engine.cancel(cancel_id, out.sink);
            }

            const auto t2 = steady_clock::now();
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        const auto end = steady_clock::now();
        const double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats(named<SinkAdapter>("Mixed Workload (70/30)"), latencies, elapsed_sec);
    }

    template <class SinkAdapter>
    BenchmarkResult benchmarkCancelation(int num_orders) {
        MatchingEngine engine;
        SinkAdapter out;
        NullSink drop;
        std::vector<int> ids;

        for (int i = 0; i < num_orders; ++i) {
            engine.submit(generateRandomOrder(i), drop);
            ids.push_back(i);
        }

        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        const auto start = steady_clock::now();

        for (const int id : ids) {
            out.beginOp();
            const auto t1 = steady_clock::now();
            engine.cancel(id, out.sink);
            const auto t2 = steady_clock::now();

            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        const auto end = steady_clock::now();
        const double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats(named<SinkAdapter>("Cancel Only"), latencies, elapsed_sec);
    }

    template <class SinkAdapter>
    BenchmarkResult benchmarkWorstCase(int num_orders) {
        MatchingEngine engine;
        SinkAdapter out;
        NullSink drop;
        std::vector<long long> latencies;
        latencies.reserve(num_orders);

        // Build a deep book on one side
        for (int i = 0; i < num_orders / 2; ++i) {
            engine.submit(Order{.id = i, .side = Side::Buy, .price = 100, .quantity = 1}, drop);
        }

        const auto start = steady_clock::now();

        // Large aggressive sells that cross the entire book
        for (int i = num_orders / 2; i < num_orders; ++i) {
            const Order o{.id = i, .side = Side::Sell, .price = 50, .quantity = num_orders};

            out.beginOp();
            const auto t1 = steady_clock::now();
            engine.submit(o, out.sink);
            const auto t2 = steady_clock::now();

            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        const auto end = steady_clock::now();
        const double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

        return computeStats(named<SinkAdapter>("Worst Case (Deep Book Cross)"), latencies, elapsed_sec);
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist{0.0, 1.0};

    template <class SinkAdapter>
    static std::string named(const char* base) {
        return std::format("{} [{}]", base, SinkAdapter::label);
    }

    Order generateRandomOrder(int id) {
        const Side side = (dist(gen) < 0.5) ? Side::Buy : Side::Sell;
        const int price = 95 + static_cast<int>(gen() % 11);  // 95-105
        const int qty   = 1 + static_cast<int>(gen() % 10);   // 1-10
        return Order{.id = id, .side = side, .price = price, .quantity = qty};
    }

    BenchmarkResult computeStats(std::string name,
                                 std::vector<long long>& latencies,
                                 double elapsed_sec) {
        std::ranges::sort(latencies);

        const double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        const double avg = sum / latencies.size();

        const size_t p50_idx = latencies.size() * 50 / 100;
        const size_t p95_idx = latencies.size() * 95 / 100;
        const size_t p99_idx = latencies.size() * 99 / 100;

        return BenchmarkResult{
            std::move(name),
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

template <class SinkAdapter>
void runSuite(OrderBookBenchmark& bench, int num_ops) {
    printResult(bench.benchmarkSubmitOnly<SinkAdapter>(num_ops));
    printResult(bench.benchmarkMixedWorkload<SinkAdapter>(num_ops));
    printResult(bench.benchmarkCancelation<SinkAdapter>(num_ops));
    printResult(bench.benchmarkWorstCase<SinkAdapter>(10000));  // smaller for worst case
}

int main() {
    OrderBookBenchmark bench;

    std::cout << "Starting Order Book Benchmarks..." << std::endl;
    std::cout << "====================================" << std::endl;

    std::cout << "\nWarming up..." << std::endl;
    MatchingEngine warmup_engine;
    bench.warmup(warmup_engine);

    const int NUM_OPS = 100000;

    runSuite<TextSinkAdapter>(bench, NUM_OPS);
    runSuite<NullSinkAdapter>(bench, NUM_OPS);

    std::cout << "\n====================================" << std::endl;
    std::cout << "Benchmarks complete!" << std::endl;

    return 0;
}
