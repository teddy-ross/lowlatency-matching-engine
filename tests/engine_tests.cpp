// Unit tests for the matching engine and protocol layer.
//
// No framework: a check() helper with std::source_location that works in
// Release builds too (unlike <cassert>, which NDEBUG compiles away).

#include "Log.hpp"
#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int g_checks = 0;

void check(bool ok, std::string_view what,
           std::source_location loc = std::source_location::current()) {
    ++g_checks;
    if (!ok) {
        logln("FAILED: {} ({}:{})", what, loc.file_name(), loc.line());
        std::abort();
    }
}

/// Run one protocol line, returning the wire response.
std::string run(MatchingEngine& engine, std::string_view line) {
    std::string response;
    if (!process_line(line, engine, response)) return {};
    return response;
}

void test_submit_and_rest() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 B 100 10") == "ACK 1\n", "plain submit acks");
    check(engine.openOrders() == 1, "order rests");
    check(engine.bestBid() == Price{100}, "best bid tracks resting order");
    check(!engine.bestAsk().has_value(), "no asks yet");
}

void test_full_cross_at_maker_price() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 B 100 10") == "ACK 1\n", "maker acks");
    // Taker is willing to sell at 95; trade prints at the maker's 100.
    check(run(engine, "SUBMIT 2 S 95 10") == "FILL 2 1 100 10\nACK 2\n",
          "full cross fills at maker price");
    check(engine.openOrders() == 0, "book empty after full cross");
}

void test_partial_fill_rests_remainder() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 S 101 10") == "ACK 1\n", "maker acks");
    check(run(engine, "SUBMIT 2 B 101 4") == "FILL 2 1 101 4\nACK 2\n", "partial fill");
    check(engine.dump() == "BIDS:\nASKS:\n101: 1(6) \n", "remainder rests with reduced qty");
}

void test_fifo_time_priority() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 B 100 5") == "ACK 1\n", "first maker");
    check(run(engine, "SUBMIT 2 B 100 5") == "ACK 2\n", "second maker, same level");
    check(run(engine, "SUBMIT 3 S 100 8") == "FILL 3 1 100 5\nFILL 3 2 100 3\nACK 3\n",
          "fills oldest order first within a level");
}

void test_walks_levels_best_first() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 S 101 5") == "ACK 1\n", "ask @101");
    check(run(engine, "SUBMIT 2 S 102 5") == "ACK 2\n", "ask @102");
    check(run(engine, "SUBMIT 3 B 103 8") == "FILL 3 1 101 5\nFILL 3 2 102 3\nACK 3\n",
          "sweeps best ask level before the next");
    check(engine.bestAsk() == Price{102}, "partially-swept level remains best");
}

void test_cancel_paths() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 7 B 99 1") == "ACK 7\n", "maker acks");
    check(run(engine, "CANCEL 7") == "ACK 7\n", "cancel acks");
    check(run(engine, "CANCEL 7") == "ACK 7 NOT_FOUND\n", "second cancel misses");
    check(!engine.bestBid().has_value(), "empty level removed on cancel");
}

void test_rejects() {
    MatchingEngine engine;
    check(run(engine, "SUBMIT 1 B 100 10") == "ACK 1\n", "first submit ok");
    check(run(engine, "SUBMIT 1 S 100 10") == "ERR DUPLICATE_ID 1\n", "duplicate id rejected");
    check(engine.openOrders() == 1, "duplicate did not touch the book");
    check(run(engine, "SUBMIT 2 B 100 0") == "ERR BAD_QTY\n", "non-positive qty rejected");
    check(run(engine, "CANCEL 2") == "ACK 2 NOT_FOUND\n", "rejected order never rested");
}

void test_parser() {
    const auto failsWith = [](std::string_view line, ParseError want) {
        const auto r = parse_command(line);
        return !r && r.error() == want;
    };

    check(parse_command("SUBMIT 1 b 100 10").has_value(), "lowercase side accepted");
    check(failsWith("SUBMIT 1 B 100", ParseError::BadSubmit), "missing field rejected");
    check(failsWith("SUBMIT 1 X 100 10", ParseError::BadSide), "bad side flagged");
    check(failsWith("SUBMIT 1 B 100 10 junk", ParseError::BadSubmit), "trailing junk rejected");
    check(failsWith("SUBMIT 1x B 100 10", ParseError::BadSubmit), "partial-numeric token rejected");
    check(failsWith("CANCEL nope", ParseError::BadCancel), "bad cancel id");
    check(failsWith("   ", ParseError::Empty), "blank line is a no-op");
    check(failsWith("HELLO", ParseError::UnknownCommand), "unknown command");

    MatchingEngine engine;
    std::string response;
    check(!process_line("  \t ", engine, response), "no response for blank input");
    check(response.empty(), "blank input leaves response empty");
}

// Any set of operator() overloads satisfying EventSink works as a sink.
// (Namespace scope: local classes can't have the templated catch-all member.)
struct Recorder {
    std::vector<FillEvent> fills;
    void operator()(const FillEvent& e) { fills.push_back(e); }
    void operator()(const auto&) {}  // ignore everything else
};
static_assert(EventSink<Recorder>);

void test_custom_recording_sink() {
    MatchingEngine engine;
    Recorder rec;
    engine.submit(Order{.id = 1, .side = Side::Sell, .price = 100, .quantity = 3}, rec);
    engine.submit(Order{.id = 2, .side = Side::Buy,  .price = 100, .quantity = 5}, rec);

    check(rec.fills.size() == 1, "one fill recorded");
    check(rec.fills.front() == FillEvent{.taker = 2, .maker = 1, .price = 100, .quantity = 3},
          "fill fields correct");
}

void test_dump_format() {
    MatchingEngine engine;
    NullSink drop;
    engine.submitBatch(std::vector<Order>{
        {.id = 1, .side = Side::Buy,  .price = 100, .quantity = 10},
        {.id = 2, .side = Side::Buy,  .price = 100, .quantity = 5},
        {.id = 3, .side = Side::Buy,  .price = 99,  .quantity = 7},
        {.id = 4, .side = Side::Sell, .price = 101, .quantity = 2},
    }, drop);

    check(engine.dump() ==
              "BIDS:\n100: 1(10) 2(5) \n99: 3(7) \nASKS:\n101: 4(2) \n",
          "dump renders levels best-first with FIFO queues");
}

}  // namespace

int main() {
    test_submit_and_rest();
    test_full_cross_at_maker_price();
    test_partial_fill_rests_remainder();
    test_fifo_time_priority();
    test_walks_levels_best_first();
    test_cancel_paths();
    test_rejects();
    test_parser();
    test_custom_recording_sink();
    test_dump_format();

    logln("All {} checks passed.", g_checks);
    return 0;
}
