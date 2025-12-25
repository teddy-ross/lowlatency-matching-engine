#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP
#pragma once

#include <functional>
#include <map>
#include <iosfwd>  // for std::ostream
#include <list>
#include <unordered_map>
#include <variant>


enum class Side{
    Buy,
    Sell
};

struct Order {
    int id;
    Side side;    
    int price;
    int quantity;
};


class MatchingEngine {

public:
    MatchingEngine() = default;

    /**
     * Submit a new order to the matching engine.
     *
     * Attempts to match the incoming order against resting orders on the
     * opposite side. For each match a "FILL" line is written to `out`.
     * Any leftover quantity is inserted into the book as a resting order.
     * After processing an acknowledgement ("ACK <id>") is emitted.
     *
     * @param order  Incoming order (id, side, price, quantity).
     * @param out    Output stream where FILL/ACK/ERR messages are written.
     * @return void
     */
    void submit(const Order& order, std::ostream& out);

    /**
     * Cancel an existing order by id.
     *
     * Looks up the order using the internal index and removes it from the
     * book in O(1) time. Emits "ACK <id>" on success or
     * "ACK <id> NOT_FOUND" if no such order exists.
     *
     * @param id   Client-visible order id to cancel.
     * @param out  Output stream receiving the ACK/NOT_FOUND response.
     * @return void
     */
    void cancel(int id, std::ostream& out);

    /**
     * Dump the current book state (bids and asks) to `out`.
     * Intended for debugging and diagnostic purposes.
     *
     * @param out  Stream to write the textual book dump to.
     * @return void
     */
    void dump(std::ostream& out) const;

private:
    using LevelQueue = std::list<Order>;
    using BidBook = std::map<int, LevelQueue, std::greater<int>>;
    using AskBook = std::map<int, LevelQueue, std::less<int>>;

    struct BidLoc{
        BidBook::iterator levelIt;
        LevelQueue::iterator orderIt;
    };

    struct AskLoc{
        AskBook::iterator levelIt;
        LevelQueue::iterator orderIt;
    };

    using OrderLocator = std::variant<BidLoc, AskLoc>;

    BidBook m_bids;
    AskBook m_asks;
    std::unordered_map<int, OrderLocator> m_idIndex;



    /**
     * Match an incoming BUY order against best asks.
     *
     * @param incoming  Mutable incoming order; its `quantity` is decreased
     *                  as trades are executed.
     * @param out       Output stream for produced FILL lines.
     * @return void
     */
    void matchBuy(Order& incoming, std::ostream& out);

    /**
     * Match an incoming SELL order against best bids.
     *
     * @param incoming  Mutable incoming order; its `quantity` is decreased
     *                  as trades are executed.
     * @param out       Output stream for produced FILL lines.
     * @return void
     */
    void matchSell(Order& incoming, std::ostream& out);

    /**
     * Insert a remaining quantity of an order into the resting book and
     * record its locator for O(1) cancellation.
     *
     * @param order  The resting order to insert (id, side, price, qty).
     * @return void
     */
    void insertResting(const Order& order);
};


#endif 
