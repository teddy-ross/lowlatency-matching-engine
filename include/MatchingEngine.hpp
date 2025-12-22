#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <deque>
#include <map>
#include <iosfwd>  // for std::ostream


struct Order {
    int id;
    char side;    // 'B' or 'S'
    int price;
    int quantity;
};

// Trade event (you can extend this with timestamp etc.)
struct Trade {
    int takerId;
    int makerId;
    int price;
    int quantity;
};

using AskBook = std::map<int, std::deque<Order>>;                 // ascending price
using BidBook = std::map<int, std::deque<Order>, std::greater<>>; // descending price

class MatchingEngine {
public:
    MatchingEngine() = default;

    // Submit a new order and emit FILLs into 'out'
    void submit(const Order& order, std::ostream& out);

    // Cancel by id. TO DO: MAKE O(1)
    void cancel(int id, std::ostream& out);

    // Debug helper: dump the book state to out
    void dump(std::ostream& out) const;

private:
    BidBook bids_;
    AskBook asks_;

    void matchBuy(Order& incoming, std::ostream& out);
    void matchSell(Order& incoming, std::ostream& out);
};


#endif 