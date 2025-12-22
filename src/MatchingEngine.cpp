#include "MatchingEngine.hpp"

#include <algorithm>
#include <iostream>

// Submit: copy the order, then match
void MatchingEngine::submit(const Order& o, std::ostream& out) {
    Order order{ o } ; // make a modifiable copy
    if (order.side == 'B') {
        matchBuy(order, out);

    } else {
        matchSell(order, out);
    }
    // After matching, acknowledge the incoming order
    out << "ACK " << o.id << '\n';
}

// Cancel: simple scan through both books, TO DO (cnt.)
void MatchingEngine::cancel(int id, std::ostream& out) {
    auto erase_from = [&](auto& book) {
        for (auto it = book.begin(); it != book.end(); ++it) {
            auto& dq = it->second;
            for (auto qit = dq.begin(); qit != dq.end(); ++qit) {
                if (qit->id == id) {
                    dq.erase(qit);
                    if (dq.empty()) {
                        book.erase(it);
                    }
                    return true;
                }
            }
        }
        return false;
    };

    bool removed = erase_from(bids_) || erase_from(asks_);
    if (removed) {
        out << "ACK " << id << '\n';
    } else {
        out << "ACK " << id << " NOT_FOUND\n";
    }
}

// Debug book dump
void MatchingEngine::dump(std::ostream& out) const {
    out << "BIDS:\n";
    for (const auto& [px, dq] : bids_) {
        out << px << ": ";
        for (const auto& o : dq) {
            out << o.id << "(" << o.quantity << ") ";
        }
        out << '\n';
    }
    out << "ASKS:\n";
    for (const auto& [px, dq] : asks_) {
        out << px << ": ";
        for (const auto& o : dq) {
            out << o.id << "(" << o.quantity << ") ";
        }
        out << '\n';
    }
}

// Private helpers

void MatchingEngine::matchBuy(Order& incoming, std::ostream& out) {
    while (incoming.quantity > 0 &&
           !asks_.empty() &&
           asks_.begin()->first <= incoming.price) {

        auto it = asks_.begin();
        auto& dq = it->second;

        while (incoming.quantity > 0 && !dq.empty()) {
            Order& resting = dq.front();
            int traded = std::min(incoming.quantity, resting.quantity);

            incoming.quantity -= traded;
            resting.quantity  -= traded;

            out << "FILL " << incoming.id << ' '
                << resting.id << ' '
                << it->first << ' '
                << traded << '\n';

            if (resting.quantity == 0) {
                dq.pop_front();
            }
        }

        if (dq.empty()) {
            asks_.erase(it);
        }
    }

    if (incoming.quantity > 0) {
        bids_[incoming.price].push_back(incoming);
    }
}

void MatchingEngine::matchSell(Order& incoming, std::ostream& out) {
    while (incoming.quantity > 0 &&
           !bids_.empty() &&
           bids_.begin()->first >= incoming.price) {

        auto it {bids_.begin()};
        auto& dq {it->second};

        while (incoming.quantity > 0 && !dq.empty()) {
            Order& resting {dq.front()};
            int traded = std::min(incoming.quantity, resting.quantity);

            incoming.quantity -= traded;
            resting.quantity  -= traded;

            out << "FILL " << incoming.id << ' '
                << resting.id << ' '
                << it->first << ' '
                << traded << '\n';

            if (resting.quantity == 0) {
                dq.pop_front();
            }
        }

        if (dq.empty()) {
            bids_.erase(it);
        }
    }

    if (incoming.quantity > 0) {
        asks_[incoming.price].push_back(incoming);
    }
}
