#pragma once

#include "MatchingEngine.hpp"

#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

// ---------------------------------------------------------------------------
// Text wire protocol
//
//   SUBMIT <id> <B|S> <price> <qty>
//   CANCEL <id>
//   DUMP
//
// Parsing is allocation-free (std::string_view tokens + std::from_chars) and
// reports failures through std::expected instead of sentinel strings.
// ---------------------------------------------------------------------------

enum class ParseError : std::uint8_t {
    Empty,           // blank/whitespace-only line: a no-op, not an error reply
    BadSubmit,       // SUBMIT with missing/malformed fields
    BadSide,         // side token is not B/S (case-insensitive)
    BadCancel,       // CANCEL with missing/malformed id
    UnknownCommand,
};

struct SubmitCommand { Order order; };
struct CancelCommand { OrderId id; };
struct DumpCommand   {};

using Command = std::variant<SubmitCommand, CancelCommand, DumpCommand>;

/**
 * Parse one protocol line into a Command.
 *
 * Tolerates leading/trailing/repeated whitespace. Numeric fields must be
 * whole tokens (e.g. "12x" is rejected) and trailing junk after a complete
 * command is rejected — both stricter than the old istringstream parser.
 */
[[nodiscard]] std::expected<Command, ParseError> parse_command(std::string_view line) noexcept;

/**
 * Formats engine events into the text wire protocol, appending to a caller-
 * owned response buffer:
 *
 *   AckEvent / CancelAckEvent          -> "ACK <id>\n"
 *   FillEvent                          -> "FILL <taker> <maker> <px> <qty>\n"
 *   RejectEvent{DuplicateId}           -> "ERR DUPLICATE_ID <id>\n"
 *   RejectEvent{BadQuantity}           -> "ERR BAD_QTY\n"
 *   RejectEvent{UnknownOrder} (cancel) -> "ACK <id> NOT_FOUND\n"
 */
class FormattingSink {
public:
    explicit FormattingSink(std::string& out) noexcept : m_out{&out} {}

    void operator()(const AckEvent& e) const {
        std::format_to(std::back_inserter(*m_out), "ACK {}\n", e.id);
    }

    void operator()(const FillEvent& e) const {
        std::format_to(std::back_inserter(*m_out), "FILL {} {} {} {}\n",
                       e.taker, e.maker, e.price, e.quantity);
    }

    void operator()(const CancelAckEvent& e) const {
        std::format_to(std::back_inserter(*m_out), "ACK {}\n", e.id);
    }

    void operator()(const RejectEvent& e) const {
        switch (e.reason) {
            using enum RejectReason;
            case DuplicateId:
                std::format_to(std::back_inserter(*m_out), "ERR DUPLICATE_ID {}\n", e.id);
                return;
            case BadQuantity:
                *m_out += "ERR BAD_QTY\n";
                return;
            case UnknownOrder:
                std::format_to(std::back_inserter(*m_out), "ACK {} NOT_FOUND\n", e.id);
                return;
        }
        std::unreachable();  // C++23: all enumerators handled above
    }

private:
    std::string* m_out;
};
static_assert(EventSink<FormattingSink>);

/**
 * Parse a single protocol line and apply it to `engine`.
 *
 * @param line     Input line (with or without trailing '\n').
 * @param engine   MatchingEngine to apply the parsed command to.
 * @param response Receives the textual response (one or more '\n'-terminated
 *                 lines). Cleared on entry.
 * @return true if a response should be sent; false for empty/no-op input.
 */
[[nodiscard]] bool process_line(std::string_view line, MatchingEngine& engine, std::string& response);
