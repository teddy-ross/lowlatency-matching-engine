#include "MatchingEngine.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <chrono>
#include <vector>
#include <thread>
#include <iomanip>

using namespace std::chrono;

class StressTest {
public:
    StressTest() : gen(std::random_device{}()) {}

    void runMemoryStressTest(int num_orders) {
        std::cout << "\n=== Memory Stress Test ===" << std::endl;
        std::cout << "Creating " << num_orders << " resting orders..." << std::endl;

        MatchingEngine engine;
        std::ostringstream out;
        
        auto start = high_resolution_clock::now();

        // Fill book with non-matching orders
        for (int i = 0; i < num_orders; ++i) {
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            int price = (side == Side::Buy) ? 50 : 150;
            Order o{i, side, price, 100};
            engine.submit(o, out);

            if (i % 10000 == 0 && i > 0) {
                std::cout << "  Created " << i << " orders..." << std::endl;
            }
        }

        auto end = high_resolution_clock::now();
        double elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

        std::cout << "Created " << num_orders << " orders in " << elapsed << "s" << std::endl;
        std::cout << "Rate: " << (num_orders / elapsed) << " orders/sec" << std::endl;

        // Now cancel them all
        std::cout << "\nCanceling all orders..." << std::endl;
        start = high_resolution_clock::now();

        for (int i = 0; i < num_orders; ++i) {
            engine.cancel(i, out);

            if (i % 10000 == 0 && i > 0) {
                std::cout << "  Canceled " << i << " orders..." << std::endl;
            }
        }

        end = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

        std::cout << "Canceled " << num_orders << " orders in " << elapsed << "s" << std::endl;
        std::cout << "Rate: " << (num_orders / elapsed) << " cancels/sec" << std::endl;
    }

    void runHighThroughputTest(int duration_seconds) {
        std::cout << "\n=== High Throughput Test ===" << std::endl;
        std::cout << "Running for " << duration_seconds << " seconds..." << std::endl;

        MatchingEngine engine;
        std::ostringstream out;
        
        auto start = high_resolution_clock::now();
        auto end_time = start + seconds(duration_seconds);
        
        int order_id = 0;
        std::vector<int> active_orders;
        int total_ops = 0;
        int submits = 0;
        int cancels = 0;

        while (high_resolution_clock::now() < end_time) {
            // Random operation
            if (dist(gen) < 0.8 || active_orders.empty()) {
                // Submit
                Order o = generateRandomOrder(order_id++);
                engine.submit(o, out);
                active_orders.push_back(order_id - 1);
                submits++;
            } else {
                // Cancel
                size_t idx = gen() % active_orders.size();
                engine.cancel(active_orders[idx], out);
                active_orders.erase(active_orders.begin() + idx);
                cancels++;
            }
            total_ops++;

            if (total_ops % 100000 == 0) {
                auto now = high_resolution_clock::now();
                double elapsed = duration_cast<milliseconds>(now - start).count() / 1000.0;
                std::cout << "  " << total_ops << " ops (" << (total_ops / elapsed) << " ops/sec)" << std::endl;
            }
        }

        auto end = high_resolution_clock::now();
        double elapsed = duration_cast<milliseconds>(end - start).count() / 1000.0;

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
        std::ostringstream out;
        
        int order_id = 0;

        // Build deep bids
        auto start = high_resolution_clock::now();
        for (int level = 0; level < num_levels; ++level) {
            int price = 100 - level;
            for (int i = 0; i < depth_per_level; ++i) {
                Order o{order_id++, Side::Buy, price, 10};
                engine.submit(o, out);
            }
        }

        // Build deep asks
        for (int level = 0; level < num_levels; ++level) {
            int price = 101 + level;
            for (int i = 0; i < depth_per_level; ++i) {
                Order o{order_id++, Side::Sell, price, 10};
                engine.submit(o, out);
            }
        }

        auto build_end = high_resolution_clock::now();
        double build_time = duration_cast<milliseconds>(build_end - start).count() / 1000.0;

        std::cout << "Book built in " << build_time << "s" << std::endl;
        std::cout << "Total resting orders: " << (num_levels * depth_per_level * 2) << std::endl;

        // Now submit aggressive orders that cross the book
        std::cout << "\nTesting cross-book aggressive orders..." << std::endl;
        
        std::vector<long long> latencies;
        for (int i = 0; i < 100; ++i) {
            Order o{order_id++, Side::Buy, 200, 1000};
            
            auto t1 = high_resolution_clock::now();
            engine.submit(o, out);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
        }

        std::sort(latencies.begin(), latencies.end());
        double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

        std::cout << "Average latency for aggressive order: " << avg << " ns" << std::endl;
        std::cout << "P99 latency: " << latencies[99] << " ns" << std::endl;
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist{0.0, 1.0};

    Order generateRandomOrder(int id) {
        Side side = (dist(gen) < 0.5) ? Side::Buy : Side::Sell;
        int price = 95 + (gen() % 11);
        int qty = 1 + (gen() % 10);
        return Order{id, side, price, qty};
    }
};

int main() {
    StressTest test;

    std::cout << "Order Book Stress Testing" << std::endl;
    std::cout << "=========================" << std::endl;

    // Test 1: Memory stress with many resting orders
    test.runMemoryStressTest(100000);

    // Test 2: High throughput sustained load
    test.runHighThroughputTest(10);

    // Test 3: Deep book with many price levels
    test.runDeepBookTest(100, 50);

    std::cout << "\n=========================" << std::endl;
    std::cout << "All stress tests complete!" << std::endl;

    return 0;
}
