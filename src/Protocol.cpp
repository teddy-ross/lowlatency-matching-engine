#include "Protocol.hpp"
#include "MatchingEngine.hpp"

#include <cctype>
#include <sstream>
#include <string>

// Removes leading and trailing whitespace.
static inline std::string trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool process_line(const std::string& line, MatchingEngine& engine, std::string& response) {
    response.clear();
    std::string s { trim(line) };
    if (s.empty()) return false;

    std::istringstream iss(s);
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
        Side side;
        if (sideChar == 'B') side = Side::Buy;
        else if (sideChar == 'S') side = Side::Sell;
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
