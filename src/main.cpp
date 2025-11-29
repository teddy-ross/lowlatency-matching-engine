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

struct OrderLocation{

  double price;
  char side;
  std::deque<Order>::iterator it;
};



std::map<double, std::deque<Order>, std::greater<double>> buyBook; // price -> orders (sorted by price desc)
std::map<double, std::deque<Order>> sellBook; // price -> orders (sorted by price asc)
std::unordered_map<std::string, OrderLocation> orderIndex; // need O(1) access to order's location inside of order book

bool cancelOrder(const std::string& id){

  auto it = orderIndex.find(id);
  if (it == orderIndex.end()){
    std::cout << "CANCEL REJECT " << id << '\n';
    return false;
  }

  const OrderLocation& location = it->second;

  if (location.side == 'B'){

    auto bookIt = buyBook.find(location.price);
    if (bookIt != buyBook.end()){
      bookIt->second.erase(location.it);
      if (bookIt->second.empty()){
        buyBook.erase(bookIt);
      }
    }


  }
  else{

    auto bookIt = sellBook.find(location.price);
    if (bookIt != sellBook.end()){
      bookIt->second.erase(location.it);
      if (bookIt->second.empty()){
        sellBook.erase(bookIt);
      }
    }

  }

  orderIndex.erase(it);

  std::cout<< "CANCEL_ACK " << id << '\n';
  return true;
}


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
                    orderIndex.erase(sellOrder.id);
                }
            }
            if (sellOrders.empty()) {
                sellBook.erase(sellBook.begin()); // Remove price level if no orders left
            }
        }

    if(order.quantity > 0){

      auto& dequeue = buyBook[order.price];
      dequeue.push_back(order);

      orderIndex[order.id] ={
        order.price,
        'B',
        std::prev(dequeue.end()) //gives iterator to newly inserted order
      };
      
     

      std::cout << "ACK " << order.id << '\n';

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
                    orderIndex.erase(buyOrder.id);
          
                }
            }
            if (buyOrders.empty()) {
                buyBook.erase(buyBook.begin()); // Remove price level if no orders left
            }
        }

    if (order.quantity > 0){

      auto& dequeue = sellBook[order.price];
      dequeue.push_back(order);

      orderIndex[order.id] ={
        order.price,
        'S',
        std::prev(dequeue.end()) //gives iterator to newly inserted order
      };
      

      std::cout << "ACK " << order.id << '\n';


    }

    }




}



int main() {

    std::string line;

    
    while (std::getline(std::cin, line)) {
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "SUBMIT") {
            Order o;
            ss >> o.id >> o.side >> o.price >> o.quantity;
            o.timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            processOrder(o);
        }
        else if (cmd == "CANCEL") {
            std::string id;
            ss >> id;
            cancelOrder(id);
        }
        else if (cmd == "PRINT") {
            // DEBUG
            std::cout << "BUY BOOK:\n";
            for (auto& [price, dq] : buyBook) {
                std::cout << price << ": ";
                for (auto& o : dq) std::cout << o.id << "(" << o.quantity << ") ";
                std::cout << "\n";
            }

            std::cout << "\nSELL BOOK:\n";
            for (auto& [price, dq] : sellBook) {
                std::cout << price << ": ";
                for (auto& o : dq) std::cout << o.id << "(" << o.quantity << ") ";
                std::cout << "\n";
            }
        }
    }

    return 0;
}


    



