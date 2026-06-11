#include "Protocol.hpp"

#include <cctype>
#include <charconv>
#include <concepts>
#include <optional>
#include <string_view>

namespace {

[[nodiscard]] constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

/// Splits a line into whitespace-separated tokens without allocating.
class Tokenizer {
public:
    explicit constexpr Tokenizer(std::string_view line) noexcept : m_rest{line} {}

    [[nodiscard]] constexpr std::optional<std::string_view> next() noexcept {
        skipSpace();
        if (m_rest.empty()) return std::nullopt;

        std::size_t len = 0;
        while (len < m_rest.size() && !is_space(m_rest[len])) ++len;

        const auto token = m_rest.substr(0, len);
        m_rest.remove_prefix(len);
        return token;
    }

    /// True iff nothing but whitespace remains.
    [[nodiscard]] constexpr bool exhausted() noexcept {
        skipSpace();
        return m_rest.empty();
    }

private:
    constexpr void skipSpace() noexcept {
        while (!m_rest.empty() && is_space(m_rest.front())) m_rest.remove_prefix(1);
    }

    std::string_view m_rest;
};

/// Parse an entire token as an integer; rejects partial matches like "12x".
template <std::integral T>
[[nodiscard]] std::optional<T> parse_int(std::string_view token) noexcept {
    T value{};
    const char* const first = token.data();
    const char* const last  = first + token.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) return std::nullopt;
    return value;
}

[[nodiscard]] std::optional<Side> parse_side(std::string_view token) noexcept {
    if (token.size() != 1) return std::nullopt;
    switch (std::toupper(static_cast<unsigned char>(token.front()))) {
        using enum Side;
        case 'B': return Buy;
        case 'S': return Sell;
        default:  return std::nullopt;
    }
}

}  // namespace

std::expected<Command, ParseError> parse_command(std::string_view line) noexcept {
    Tokenizer tokens{line};

    const auto cmd = tokens.next();
    if (!cmd) return std::unexpected{ParseError::Empty};

    if (*cmd == "SUBMIT") {
        const auto id      = parse_int<OrderId>(tokens.next().value_or(""));
        const auto sideTok = tokens.next();
        const auto price   = parse_int<Price>(tokens.next().value_or(""));
        const auto qty     = parse_int<Quantity>(tokens.next().value_or(""));

        if (!id || !sideTok || !price || !qty || !tokens.exhausted())
            return std::unexpected{ParseError::BadSubmit};

        const auto side = parse_side(*sideTok);
        if (!side) return std::unexpected{ParseError::BadSide};

        return SubmitCommand{Order{.id = *id, .side = *side, .price = *price, .quantity = *qty}};
    }

    if (*cmd == "CANCEL") {
        const auto id = parse_int<OrderId>(tokens.next().value_or(""));
        if (!id || !tokens.exhausted())
            return std::unexpected{ParseError::BadCancel};
        return CancelCommand{*id};
    }

    if (*cmd == "DUMP") {
        if (!tokens.exhausted())
            return std::unexpected{ParseError::UnknownCommand};
        return DumpCommand{};
    }

    return std::unexpected{ParseError::UnknownCommand};
}

bool process_line(std::string_view line, MatchingEngine& engine, std::string& response) {
    response.clear();

    const auto parsed = parse_command(line);
    if (!parsed) {
        switch (parsed.error()) {
            using enum ParseError;
            case Empty:          return false;  // no-op, nothing to send
            case BadSubmit:      response = "ERR BAD_SUBMIT\n";  return true;
            case BadSide:        response = "ERR BAD_SIDE\n";    return true;
            case BadCancel:      response = "ERR BAD_CANCEL\n";  return true;
            case UnknownCommand: response = "ERR UNKNOWN_CMD\n"; return true;
        }
        std::unreachable();  // C++23: all enumerators handled above
    }

    std::visit([&](const auto& command) {
        using T = std::remove_cvref_t<decltype(command)>;
        if constexpr (std::same_as<T, SubmitCommand>) {
            engine.submit(command.order, FormattingSink{response});
        } else if constexpr (std::same_as<T, CancelCommand>) {
            engine.cancel(command.id, FormattingSink{response});
        } else {
            static_assert(std::same_as<T, DumpCommand>);
            response = engine.dump();
        }
    }, *parsed);

    return true;
}
