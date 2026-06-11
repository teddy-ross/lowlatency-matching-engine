#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

// ---------------------------------------------------------------------------
// Core domain types
// ---------------------------------------------------------------------------

using OrderId  = std::int64_t;
using Price    = std::int64_t;
using Quantity = std::int64_t;

enum class Side : std::uint8_t { Buy, Sell };

[[nodiscard]] constexpr std::string_view side_label(Side s) noexcept {
    constexpr std::array labels{std::string_view{"BUY"}, std::string_view{"SELL"}};
    return labels[std::to_underlying(s)];  // C++23: std::to_underlying
}
static_assert(side_label(Side::Buy)  == "BUY",  "side_label: Buy  label mismatch");
static_assert(side_label(Side::Sell) == "SELL", "side_label: Sell label mismatch");

struct Order {
    OrderId  id;
    Side     side;
    Price    price;
    Quantity quantity;

    [[nodiscard]] bool operator==(const Order&) const = default;
};

// ---------------------------------------------------------------------------
// Engine events
//
// The engine never formats text. It reports what happened through small,
// strongly-typed event structs delivered to a caller-supplied sink. The
// protocol layer turns these into wire messages; benchmarks can drop them;
// tests can record them. This keeps formatting/IO cost out of the hot path
// and makes the engine independently testable.
// ---------------------------------------------------------------------------

enum class RejectReason : std::uint8_t {
    DuplicateId,   // SUBMIT with an id that is already resting
    BadQuantity,   // SUBMIT with quantity <= 0
    UnknownOrder,  // CANCEL for an id that is not resting
};

struct AckEvent {  // SUBMIT accepted
    OrderId id;
    [[nodiscard]] bool operator==(const AckEvent&) const = default;
};
struct FillEvent {
    OrderId taker; OrderId maker; Price price; Quantity quantity;
    [[nodiscard]] bool operator==(const FillEvent&) const = default;
};
struct CancelAckEvent {  // CANCEL succeeded
    OrderId id;
    [[nodiscard]] bool operator==(const CancelAckEvent&) const = default;
};
struct RejectEvent {
    OrderId id; RejectReason reason;
    [[nodiscard]] bool operator==(const RejectEvent&) const = default;
};

/// Anything invocable with each event type can act as a sink: a struct with
/// operator() overloads, an overloaded-lambda set, a recording vector, ...
template <typename S>
concept EventSink =
    std::invocable<S&, const AckEvent&>       &&
    std::invocable<S&, const FillEvent&>      &&
    std::invocable<S&, const CancelAckEvent&> &&
    std::invocable<S&, const RejectEvent&>;

/// Discards all events. Useful for benchmarks that measure pure engine cost.
struct NullSink {
    static constexpr void operator()(const auto&) noexcept {}  // C++23: static operator()
};
static_assert(EventSink<NullSink>);

template <typename R>
concept OrderRange = std::ranges::input_range<R> &&
                     std::same_as<std::ranges::range_value_t<R>, Order>;

// ---------------------------------------------------------------------------
// MatchingEngine
//
// Price/time-priority limit order book:
//   - price levels:  std::pmr::map (bids descending, asks ascending)
//   - level queues:  std::pmr::list, FIFO within a level
//   - cancel index:  id -> {level iterator, queue iterator} for O(1) cancels
//
// All node allocations are served from an unsynchronized_pool_resource owned
// by the engine, so steady-state submit/cancel traffic recycles fixed-size
// blocks instead of hitting the global allocator.
//
// Single-threaded by design (the pool resource is unsynchronized). The engine
// is neither copyable nor movable: its containers hold a pointer to the
// member arena, which must not be re-seated.
// ---------------------------------------------------------------------------

class MatchingEngine {
public:
    explicit MatchingEngine(std::size_t expectedOpenOrders = 1u << 16) {
        m_index.reserve(expectedOpenOrders);
    }

    MatchingEngine(const MatchingEngine&)            = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    /**
     * Submit a new order.
     *
     * Matches the incoming order against resting orders on the opposite side
     * (best price first, FIFO within a level), emitting a FillEvent per
     * execution at the maker's price. Leftover quantity rests in the book.
     * Emits AckEvent on acceptance or RejectEvent (DuplicateId/BadQuantity).
     */
    template <EventSink S>
    void submit(const Order& order, S&& sink) {
        if (m_index.contains(order.id)) {
            sink(RejectEvent{order.id, RejectReason::DuplicateId});
            return;
        }
        if (order.quantity <= 0) {
            sink(RejectEvent{order.id, RejectReason::BadQuantity});
            return;
        }

        Order incoming = order;  // mutable working copy

        switch (incoming.side) {
            using enum Side;
            case Buy:  matchAgainst(m_asks, incoming, sink); break;
            case Sell: matchAgainst(m_bids, incoming, sink); break;
            default:   std::unreachable();  // C++23
        }

        if (incoming.quantity > 0) {
            if (incoming.side == Side::Buy) rest(m_bids, incoming);
            else                            rest(m_asks, incoming);
        }

        sink(AckEvent{order.id});
    }

    /// Submit a range of orders (span, vector, array, ...) in sequence.
    template <OrderRange R, EventSink S>
    void submitBatch(R&& orders, S&& sink) {
        for (const Order& o : orders) submit(o, sink);
    }

    /**
     * Cancel a resting order by id in O(1) via the locator index.
     * Emits CancelAckEvent on success or RejectEvent{UnknownOrder}.
     */
    template <EventSink S>
    void cancel(OrderId id, S&& sink) {
        const auto it = m_index.find(id);
        if (it == m_index.end()) {
            sink(RejectEvent{id, RejectReason::UnknownOrder});
            return;
        }

        std::visit([this](const auto& loc) {
            auto& queue = loc.level->second;
            queue.erase(loc.order);
            if (queue.empty()) bookFor(loc).erase(loc.level);
        }, it->second);

        m_index.erase(it);
        sink(CancelAckEvent{id});
    }

    /// Render the book state as text (debug/diagnostic; not a hot path).
    [[nodiscard]] std::string dump() const {
        std::string out;
        out.reserve(64 + 24 * m_index.size());
        dumpSide(out, "BIDS:\n", m_bids);
        dumpSide(out, "ASKS:\n", m_asks);
        return out;
    }

    // --- observers (handy for tests and snapshots) ---
    [[nodiscard]] std::size_t openOrders() const noexcept { return m_index.size(); }

    [[nodiscard]] std::optional<Price> bestBid() const noexcept {
        if (m_bids.empty()) return std::nullopt;
        return m_bids.begin()->first;
    }

    [[nodiscard]] std::optional<Price> bestAsk() const noexcept {
        if (m_asks.empty()) return std::nullopt;
        return m_asks.begin()->first;
    }

private:
    using LevelQueue = std::pmr::list<Order>;

    template <class Compare>
    using BookOf = std::pmr::map<Price, LevelQueue, Compare>;

    using BidBook = BookOf<std::greater<>>;  // begin() = highest bid
    using AskBook = BookOf<std::less<>>;     // begin() = lowest ask

    /// Where an order lives: its price level and its slot in the FIFO queue.
    template <class BookT>
    struct Locator {
        typename BookT::iterator level;
        LevelQueue::iterator     order;
    };

    using OrderLocator = std::variant<Locator<BidBook>, Locator<AskBook>>;

    [[nodiscard]] BidBook& bookFor(const Locator<BidBook>&) noexcept { return m_bids; }
    [[nodiscard]] AskBook& bookFor(const Locator<AskBook>&) noexcept { return m_asks; }

    /**
     * Match `incoming` against the opposite book, best level first.
     *
     * Works for both sides through the book's own ordering predicate: a book
     * orders its levels best-first, so the incoming order crosses the best
     * level exactly when its price does NOT sort strictly *behind* that
     * level's price under that predicate.
     *   asks (less):    stop when incoming.price <  best ask
     *   bids (greater): stop when incoming.price >  best bid
     */
    template <class BookT, EventSink S>
    void matchAgainst(BookT& book, Order& incoming, S&& sink) {
        const typename BookT::key_compare sortsBefore{};

        while (incoming.quantity > 0 && !book.empty()) {
            const auto  levelIt = book.begin();
            const Price levelPx = levelIt->first;
            if (sortsBefore(incoming.price, levelPx)) break;  // best level not crossed

            auto& queue = levelIt->second;
            for (auto it = queue.begin(); incoming.quantity > 0 && it != queue.end();) {
                Order& resting = *it;
                const Quantity traded = std::min(incoming.quantity, resting.quantity);

                incoming.quantity -= traded;
                resting.quantity  -= traded;

                sink(FillEvent{incoming.id, resting.id, levelPx, traded});

                if (resting.quantity == 0) {
                    m_index.erase(resting.id);  // erase index BEFORE the node it points at
                    it = queue.erase(it);
                } else {
                    ++it;
                }
            }

            if (queue.empty()) book.erase(levelIt);
        }
    }

    /// Insert leftover quantity as a resting order and record its locator.
    template <class BookT>
    void rest(BookT& book, const Order& order) {
        const auto levelIt = book.try_emplace(order.price).first;
        auto& queue = levelIt->second;  // pmr propagates: queue allocates from m_arena
        queue.push_back(order);
        m_index.emplace(order.id, Locator<BookT>{levelIt, std::prev(queue.end())});
    }

    template <class BookT>
    static void dumpSide(std::string& out, std::string_view header, const BookT& book) {
        out += header;
        for (const auto& [price, queue] : book) {
            std::format_to(std::back_inserter(out), "{}: ", price);
            for (const Order& o : queue) {
                std::format_to(std::back_inserter(out), "{}({}) ", o.id, o.quantity);
            }
            out += '\n';
        }
    }

    // Arena must be declared before (and thus destroyed after) the containers.
    std::pmr::unsynchronized_pool_resource m_arena{};

    BidBook m_bids{&m_arena};
    AskBook m_asks{&m_arena};
    std::pmr::unordered_map<OrderId, OrderLocator> m_index{&m_arena};
};
