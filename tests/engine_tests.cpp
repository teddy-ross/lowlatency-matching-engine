// Unit tests for the matching engine and protocol layer (GoogleTest).

#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace {

// Each test gets a fresh book; run() pushes one protocol line through the full
// parse -> match -> format pipeline and returns the wire response.
class MatchingEngineTest : public ::testing::Test {
protected:
    std::string run(std::string_view line) {
        std::string response;
        if (!process_line(line, engine, response)) return {};
        return response;
    }

    MatchingEngine engine;
};

TEST_F(MatchingEngineTest, SubmitRestsOnEmptyBook) {
    EXPECT_EQ(run("SUBMIT 1 B 100 10"), "ACK 1\n");
    EXPECT_EQ(engine.openOrders(), 1u);
    EXPECT_EQ(engine.bestBid(), Price{100});
    EXPECT_FALSE(engine.bestAsk().has_value());
}

TEST_F(MatchingEngineTest, FullCrossExecutesAtMakerPrice) {
    EXPECT_EQ(run("SUBMIT 1 B 100 10"), "ACK 1\n");
    // Taker is willing to sell at 95; trade prints at the maker's 100.
    EXPECT_EQ(run("SUBMIT 2 S 95 10"), "FILL 2 1 100 10\nACK 2\n");
    EXPECT_EQ(engine.openOrders(), 0u) << "book should be empty after full cross";
}

TEST_F(MatchingEngineTest, PartialFillRestsRemainder) {
    EXPECT_EQ(run("SUBMIT 1 S 101 10"), "ACK 1\n");
    EXPECT_EQ(run("SUBMIT 2 B 101 4"), "FILL 2 1 101 4\nACK 2\n");
    EXPECT_EQ(engine.dump(), "BIDS:\nASKS:\n101: 1(6) \n")
        << "remainder should rest with reduced quantity";
}

TEST_F(MatchingEngineTest, FifoTimePriorityWithinLevel) {
    EXPECT_EQ(run("SUBMIT 1 B 100 5"), "ACK 1\n");
    EXPECT_EQ(run("SUBMIT 2 B 100 5"), "ACK 2\n");
    // Fills the oldest order first within a level.
    EXPECT_EQ(run("SUBMIT 3 S 100 8"), "FILL 3 1 100 5\nFILL 3 2 100 3\nACK 3\n");
}

TEST_F(MatchingEngineTest, SweepsLevelsBestFirst) {
    EXPECT_EQ(run("SUBMIT 1 S 101 5"), "ACK 1\n");
    EXPECT_EQ(run("SUBMIT 2 S 102 5"), "ACK 2\n");
    EXPECT_EQ(run("SUBMIT 3 B 103 8"), "FILL 3 1 101 5\nFILL 3 2 102 3\nACK 3\n")
        << "should sweep the best ask level before the next";
    EXPECT_EQ(engine.bestAsk(), Price{102}) << "partially-swept level remains best";
}

TEST_F(MatchingEngineTest, CancelPaths) {
    EXPECT_EQ(run("SUBMIT 7 B 99 1"), "ACK 7\n");
    EXPECT_EQ(run("CANCEL 7"), "ACK 7\n");
    EXPECT_EQ(run("CANCEL 7"), "ACK 7 NOT_FOUND\n") << "second cancel should miss";
    EXPECT_FALSE(engine.bestBid().has_value()) << "empty level removed on cancel";
}

TEST_F(MatchingEngineTest, RejectsDuplicateIdAndBadQuantity) {
    EXPECT_EQ(run("SUBMIT 1 B 100 10"), "ACK 1\n");
    EXPECT_EQ(run("SUBMIT 1 S 100 10"), "ERR DUPLICATE_ID 1\n");
    EXPECT_EQ(engine.openOrders(), 1u) << "duplicate must not touch the book";
    EXPECT_EQ(run("SUBMIT 2 B 100 0"), "ERR BAD_QTY\n");
    EXPECT_EQ(run("CANCEL 2"), "ACK 2 NOT_FOUND\n") << "rejected order never rested";
}

TEST(ProtocolTest, ParserAcceptsAndRejects) {
    const auto failsWith = [](std::string_view line, ParseError want) {
        const auto r = parse_command(line);
        return !r && r.error() == want;
    };

    EXPECT_TRUE(parse_command("SUBMIT 1 b 100 10").has_value()) << "lowercase side accepted";
    EXPECT_TRUE(failsWith("SUBMIT 1 B 100", ParseError::BadSubmit)) << "missing field";
    EXPECT_TRUE(failsWith("SUBMIT 1 X 100 10", ParseError::BadSide)) << "bad side";
    EXPECT_TRUE(failsWith("SUBMIT 1 B 100 10 junk", ParseError::BadSubmit)) << "trailing junk";
    EXPECT_TRUE(failsWith("SUBMIT 1x B 100 10", ParseError::BadSubmit)) << "partial-numeric token";
    EXPECT_TRUE(failsWith("CANCEL nope", ParseError::BadCancel)) << "bad cancel id";
    EXPECT_TRUE(failsWith("   ", ParseError::Empty)) << "blank line is a no-op";
    EXPECT_TRUE(failsWith("HELLO", ParseError::UnknownCommand)) << "unknown command";
}

TEST(ProtocolTest, BlankInputProducesNoResponse) {
    MatchingEngine engine;
    std::string response;
    EXPECT_FALSE(process_line("  \t ", engine, response));
    EXPECT_TRUE(response.empty());
}

// Any set of operator() overloads satisfying EventSink works as a sink.
// (Namespace scope: local classes can't have the templated catch-all member.)
struct Recorder {
    std::vector<FillEvent> fills;
    void operator()(const FillEvent& e) { fills.push_back(e); }
    void operator()(const auto&) {}  // ignore everything else
};
static_assert(EventSink<Recorder>);

TEST_F(MatchingEngineTest, CustomRecordingSinkSeesFills) {
    Recorder rec;
    engine.submit(Order{.id = 1, .side = Side::Sell, .price = 100, .quantity = 3}, rec);
    engine.submit(Order{.id = 2, .side = Side::Buy,  .price = 100, .quantity = 5}, rec);

    ASSERT_EQ(rec.fills.size(), 1u);
    EXPECT_EQ(rec.fills.front(),
              (FillEvent{.taker = 2, .maker = 1, .price = 100, .quantity = 3}));
}

TEST_F(MatchingEngineTest, DumpRendersLevelsBestFirstFifoWithin) {
    NullSink drop;
    engine.submitBatch(std::vector<Order>{
        {.id = 1, .side = Side::Buy,  .price = 100, .quantity = 10},
        {.id = 2, .side = Side::Buy,  .price = 100, .quantity = 5},
        {.id = 3, .side = Side::Buy,  .price = 99,  .quantity = 7},
        {.id = 4, .side = Side::Sell, .price = 101, .quantity = 2},
    }, drop);

    EXPECT_EQ(engine.dump(),
              "BIDS:\n100: 1(10) 2(5) \n99: 3(7) \nASKS:\n101: 4(2) \n");
}

}  // namespace
