#include "MatchingEngine.hpp"

#include <algorithm>
#include <iostream>
#include <type_traits>

void MatchingEngine::submit(const Order& o, std::ostream& out) {
    // Reject duplicate IDs
    if (m_idIndex.find(o.id) != m_idIndex.end()) {
        out << "ERR DUPLICATE_ID " << o.id << '\n';
        return;
    }

    // Work on a mutable copy
    Order incoming = o;

    if (incoming.quantity <= 0) {
        out << "ERR BAD_QTY\n";
        return;
    }

    if (incoming.side == Side::Buy) {
        matchBuy(incoming, out);
    } else {
        matchSell(incoming, out);
    }

    // Rest leftover
    if (incoming.quantity > 0) {
        insertResting(incoming);
    }

    out << "ACK " << o.id << '\n';
}

void MatchingEngine::cancel(int id, std::ostream& out) {
    auto it = m_idIndex.find(id);

    if (it == m_idIndex.end()) {
        out << "ACK " << id << " NOT_FOUND\n";
        return;
    }

    OrderLocator loc = it -> second; // copy is fine; we erase index entry after

    std::visit([&](auto& L) {
        auto& levelIt = L.levelIt;

        auto& orderIt = L.orderIt;

        auto& q = levelIt -> second;
        q.erase(orderIt);

        if (q.empty()) {
            if constexpr (std::is_same_v<std::decay_t<decltype(L)>, BidLoc>) {
                m_bids.erase(levelIt);
            } else {
                m_asks.erase(levelIt);
            }
        }
    }, loc);

    m_idIndex.erase(it);
    out << "ACK " << id << '\n';
}

void MatchingEngine::dump(std::ostream& out) const {
    out << "BIDS:\n";

    for (const auto& [px, q] : m_bids) {

        out << px << ": ";

        for (const auto& o : q) {

            out << o.id << "(" << o.quantity << ") ";
        }
        out << '\n';
    }

    out << "ASKS:\n";

    for (const auto& [px, q] : m_asks) {
        out << px << ": ";

        for (const auto& o : q) {
            out << o.id << "(" << o.quantity << ") ";
        }
        out << '\n';
    }
}


void MatchingEngine::insertResting(const Order& order) {
    if (order.quantity <= 0){
        return;
    }

    if (order.side == Side::Buy) {

        auto [levelIt, _] = m_bids.try_emplace(order.price);

        auto& q = levelIt->second;

        q.push_back(order);

        auto orderIt = std::prev(q.end());

        m_idIndex.emplace(order.id, BidLoc{levelIt, orderIt});

    } else {
        auto [levelIt, _] = m_asks.try_emplace(order.price);

        auto& q = levelIt->second;

        q.push_back(order);

        auto orderIt = std::prev(q.end());

        m_idIndex.emplace(order.id, AskLoc{levelIt, orderIt});
    }
}

void MatchingEngine::matchBuy(Order& incoming, std::ostream& out) {
    while (incoming.quantity > 0 && !m_asks.empty() && m_asks.begin() -> first <= incoming.price) {

        auto levelIt = m_asks.begin();

        int tradePx = levelIt -> first;

        auto& q = levelIt -> second;

        auto it = q.begin();
        while (incoming.quantity > 0 && it != q.end()) {
            Order& resting = *it;
            int traded = std::min(incoming.quantity, resting.quantity);

            incoming.quantity -= traded;
            resting.quantity  -= traded;

            out << "FILL " << incoming.id << ' '
                << resting.id << ' '
                << tradePx << ' '
                << traded << '\n';

            if (resting.quantity == 0) {
                // IMPORTANT: erase index BEFORE erasing iterator
                m_idIndex.erase(resting.id);
                it = q.erase(it);
            } else {
                ++it;
            }
        }

        if (q.empty()) {
            m_asks.erase(levelIt);
        }
    }
}

void MatchingEngine::matchSell(Order& incoming, std::ostream& out) {
    while (incoming.quantity > 0 && !m_bids.empty() && m_bids.begin()->first >= incoming.price) {

        auto levelIt = m_bids.begin();
        int tradePx = levelIt -> first;
        auto& q = levelIt -> second;

        auto it = q.begin();
        while (incoming.quantity > 0 && it != q.end()) {
            Order& resting = *it;
            int traded = std::min(incoming.quantity, resting.quantity);

            incoming.quantity -= traded;
            resting.quantity  -= traded;

            out << "FILL " << incoming.id << ' '
                << resting.id << ' '
                << tradePx << ' '
                << traded << '\n';

            if (resting.quantity == 0) {
                // IMPORTANT: erase index BEFORE erasing iterator
                m_idIndex.erase(resting.id);
                it = q.erase(it);

            } 
            else {
                ++it;
            }
        }

        if (q.empty()) {
            m_bids.erase(levelIt);
        }
    }
}
 