// Simple single-threaded matching engine
// Protocol (text lines, newline-terminated):
// SUBMIT <id> <B|S> <price> <qty>
// CANCEL <id>
//
// Responses (text lines):
// ACK <id>
// FILL <incoming_id> <matched_id> <price> <qty>
// DONE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

struct Order {
    std::string id;
    char side; // 'B' = buy or 'S' = sell
    double price; 
    int quantity;
    double timestamp; // for time priority
};

struct Trade{

    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
    double timestamp; 
};

std::map<double, std::deque<Order>, std::greater<double>> buyBook; // price -> orders (sorted by price desc)
std::map<double, std::deque<Order>> sellBook; // price -> orders (sorted by price asc)

void processOrder(Order order){

    if (order.side == 'B') { // buy side
        while (order.quantity > 0 && !sellBook.empty() && sellBook.begin()->first <= order.price) {
            auto& sellOrders = sellBook.begin()->second;
            while (order.quantity > 0 && !sellOrders.empty()) {
                Order& sellOrder = sellOrders.front();
                int tradeQty = std::min(order.quantity, sellOrder.quantity);
                double tradePrice = sellOrder.price;

                // Record the trade
                Trade trade{order.id, sellOrder.id, tradePrice, tradeQty, std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()};
                std::cout << "FILL " << order.id << " " << sellOrder.id << " " << tradePrice << " " << tradeQty << std::endl;

                // Update quantities
                order.quantity -= tradeQty;
                sellOrder.quantity -= tradeQty;

                if (sellOrder.quantity == 0) {
                    sellOrders.pop_front(); // Remove fully filled order
                }
            }
            if (sellOrders.empty()) {
                sellBook.erase(sellBook.begin()); // Remove price level if no orders left
            }
        }

    }
    else{ //sell side
        while (order.quantity > 0 && !buyBook.empty() && buyBook.begin()->first >= order.price) {
            auto& buyOrders = buyBook.begin()->second;
            while (order.quantity > 0 && !buyOrders.empty()) {
                Order& buyOrder = buyOrders.front();
                int tradeQty = std::min(order.quantity, buyOrder.quantity);
                double tradePrice = buyOrder.price;

                // Record the trade
                Trade trade{buyOrder.id, order.id, tradePrice, tradeQty, std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()};
                std::cout << "FILL " << buyOrder.id << " " << order.id << " " << tradePrice << " " << tradeQty << std::endl;

                // Update quantities
                order.quantity -= tradeQty;
                buyOrder.quantity -= tradeQty;

                if (buyOrder.quantity == 0) {
                    buyOrders.pop_front(); // Remove fully filled order
                }
            }
            if (buyOrders.empty()) {
                buyBook.erase(buyBook.begin()); // Remove price level if no orders left
            }
        }



    }




}


int main(){

    

    return 0;


}
