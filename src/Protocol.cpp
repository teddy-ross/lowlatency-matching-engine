#include "Protocol.hpp"
#include "MatchingEngine.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>

static std::string_view trim(std::string_view s) noexcept {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool process_line(std::string_view line, MatchingEngine& engine, std::string& response) {
    response.clear();
    std::string_view s { trim(line) };
    if (s.empty()) return false;

    std::istringstream iss{std::string(s)};
    std::string cmd;
    iss >> cmd;

    if (cmd == "SUBMIT") {
        int id, price, qty;
        char sideChar;
        if (!(iss >> id >> sideChar >> price >> qty)) {
            response = "ERR BAD_SUBMIT\n";
            return true;
        }

        sideChar = static_cast<char>(std::toupper(static_cast<unsigned char>(sideChar)));
        using enum Side;  // bring Buy and Sell into scope unqualified
        Side side;
        if (sideChar == 'B') side = Buy;
        else if (sideChar == 'S') side = Sell;
        else {
            response = "ERR BAD_SIDE\n";
            return true;
        }

        Order o{id, side, price, qty};
        std::ostringstream oss;
        engine.submit(o, oss);
        response = oss.str();
        return true;

    } else if (cmd == "CANCEL") {
        int id;
        if (!(iss >> id)) {
            response = "ERR BAD_CANCEL\n";
            return true;
        }

        std::ostringstream oss;
        engine.cancel(id, oss);
        response = oss.str();
        return true;

    } else if (cmd == "DUMP") {
        std::ostringstream oss;
        engine.dump(oss);
        response = oss.str();
        return true;
    }

    response = "ERR UNKNOWN_CMD\n";
    return true;
}
