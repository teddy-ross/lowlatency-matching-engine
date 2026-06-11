#include "MatchingEngine.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

using namespace std::chrono;

// Stress tests use NullSink throughout: they measure sustained engine
// throughput and memory behavior, not message formatting.

class StressTest {
public:
    StressTest() : gen(std::random_device{}()) {}

    void runMemoryStressTest(int num_orders) {
        std::cout << "\n=== Memory Stress Test ===" << std::endl;
        std::cout << "Creating " << num_orders << " resting orders..." << std::endl;

        MatchingEngine engine;
        NullSink drop;

        auto start = steady_clock::now();

        // Fill book with non-matching orders
        for (int i = 0; i < num_orders; ++i) {
            const Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            const int price = (side == Side::Buy) ? 50 : 150;
            engine.submit(Order{.id = i, .side = side, .price = price, .quantity = 100}, drop);

            if (i % 10000 == 0 && i > 0) {
                std::cout << "  Created " << i << " orders..." << std::endl;
            }
        }

        auto end = steady_clock::now();
        double elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

        std::cout << "Created " << num_orders << " orders in " << elapsed << "s" << std::endl;
        std::cout << "Rate: " << (num_orders / elapsed) << " orders/sec" << std::endl;

        std::cout << "\nCanceling all orders..." << std::endl;
        start = steady_clock::now();

        for (int i = 0; i < num_orders; ++i) {
            engine.cancel(i, drop);

            if (i % 10000 == 0 && i > 0) {
                std::cout << "  Canceled " << i << " orders..." << std::endl;
            }
        }

        end = steady_clock::now();
        elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

        std::cout << "Canceled " << num_orders << " orders in " << elapsed << "s" << std::endl;
        std::cout << "Rate: " << (num_orders / elapsed) << " cancels/sec" << std::endl;
    }

    void runHighThroughputTest(int duration_seconds) {
        std::cout << "\n=== High Throughput Test ===" << std::endl;
        std::cout << "Running for " << duration_seconds << " seconds..." << std::endl;

        MatchingEngine engine;
        NullSink drop;

        const auto start = steady_clock::now();
        const auto end_time = start + seconds(duration_seconds);

        int order_id = 0;
        std::vector<int> active_orders;
        long long total_ops = 0;
        long long submits = 0;
        long long cancels = 0;

        while (steady_clock::now() < end_time) {
            if (dist(gen) < 0.8 || active_orders.empty()) {
                engine.submit(generateRandomOrder(order_id++), drop);
                active_orders.push_back(order_id - 1);
                submits++;
            } else {
                const size_t idx = gen() % active_orders.size();
                engine.cancel(active_orders[idx], drop);
                active_orders.erase(active_orders.begin() + idx);
                cancels++;
            }
            total_ops++;

            if (total_ops % 1000000 == 0) {
                const auto now = steady_clock::now();
                const double elapsed = duration_cast<milliseconds>(now - start).count() / 1000.0;
                std::cout << "  " << total_ops << " ops (" << (total_ops / elapsed) << " ops/sec)" << std::endl;
            }
        }

        const auto end = steady_clock::now();
        const double elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

        std::cout << "\nResults:" << std::endl;
        std::cout << "  Total operations: " << total_ops << std::endl;
        std::cout << "  Submits: " << submits << std::endl;
        std::cout << "  Cancels: " << cancels << std::endl;
        std::cout << "  Throughput: " << (total_ops / elapsed) << " ops/sec" << std::endl;
        std::cout << "  Remaining orders: " << active_orders.size() << std::endl;
    }

    void runDeepBookTest(int depth_per_level, int num_levels) {
        std::cout << "\n=== Deep Book Test ===" << std::endl;
        std::cout << "Building book with " << num_levels << " levels, "
                  << depth_per_level << " orders per level" << std::endl;

        MatchingEngine engine;
        NullSink drop;

        int order_id = 0;

        const auto start = steady_clock::now();
        for (int level = 0; level < num_levels; ++level) {
            const int price = 100 - level;
            for (int i = 0; i < depth_per_level; ++i) {
                engine.submit(Order{.id = order_id++, .side = Side::Buy, .price = price, .quantity = 10}, drop);
            }
        }

        for (int level = 0; level < num_levels; ++level) {
            const int price = 101 + level;
            for (int i = 0; i < depth_per_level; ++i) {
                engine.submit(Order{.id = order_id++, .side = Side::Sell, .price = price, .quantity = 10}, drop);
            }
        }

        const auto build_end = steady_clock::now();
        const double build_time = duration_cast<milliseconds>(build_end - start).count() / 1000.0;

        std::cout << "Book built in " << build_time << "s" << std::endl;
        std::cout << "Total resting orders: " << (num_levels * depth_per_level * 2) << std::endl;

        std::cout << "\nTesting cross-book aggressive orders..." << std::endl;

        std::vector<long long> latencies;
        for (int i = 0; i < 100; ++i) {
            const Order o{.id = order_id++, .side = Side::Buy, .price = 200, .quantity = 1000};

            const auto t1 = steady_clock::now();
            engine.submit(o, drop);
            const auto t2 = steady_clock::now();

            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        std::ranges::sort(latencies);
        const double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

        std::cout << "Average latency for aggressive order: " << avg << " ns" << std::endl;
        std::cout << "P99 latency: " << latencies[99] << " ns" << std::endl;
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist{0.0, 1.0};

    Order generateRandomOrder(int id) {
        const Side side = (dist(gen) < 0.5) ? Side::Buy : Side::Sell;
        const int price = 95 + static_cast<int>(gen() % 11);
        const int qty   = 1 + static_cast<int>(gen() % 10);
        return Order{.id = id, .side = side, .price = price, .quantity = qty};
    }
};

int main() {
    StressTest test;

    std::cout << "Order Book Stress Testing" << std::endl;
    std::cout << "=========================" << std::endl;

    test.runMemoryStressTest(100000);
    test.runHighThroughputTest(10);
    test.runDeepBookTest(100, 50);

    std::cout << "\n=========================" << std::endl;
    std::cout << "All stress tests complete!" << std::endl;

    return 0;
}
