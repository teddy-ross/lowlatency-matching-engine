// Simple single-threaded matching engine with TCP interface.
// Protocol (text lines, '\n'-terminated):
//   SUBMIT <id> <B|S> <price> <qty>
//   CANCEL <id>
//
// Responses (text, may be multiple lines per command):
//   ACK <id>
//   FILL <incoming_id> <resting_id> <price> <qty>
//   ACK <id> NOT_FOUND    (for unknown CANCEL)
//   ERR <reason>          (for parse errors)
//
// Example:
//   SUBMIT 1 B 100 10
//   SUBMIT 2 S 99 5
//   CANCEL 1

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

struct Order {
    std::string id;
    char side; // 'B' = buy or 'S' = sell
    double price; 
    int quantity;
    double timestamp; // for time priority
};



struct MatchingEngine {
  public:
  
    std::map<double, std::deque<Order>, std::greater<double>> bids; // best (highest) first
    std::map<double, std::deque<Order>> asks; // best (lowest) first
  

    void submit(Order& order, std::ostream& out){
      if (order.side == 'B'){
        matchBuy(order, out);
      }
      else{
        matchSell(order, out);
      }

      out << "ACK " << order.id << '\n';
    }

    void cancel(const std::string& id, std::ostream& out){
      auto eraseFrom = [&](auto& book)->bool{
        for (auto it{book.begin()}; it != book.end(); ++it){

          auto &dq = it->second;

          for (auto qit {dq.begin()}; qit != dq.end(); ++qit){

            if (qit->id == id){

              dq.erase(qit);

              if (dq.empty()){

                book.erase(it);
              }
              return true;
            }
          }
        }
        return false;
      };

      bool removed {eraseFrom(bids) || eraseFrom(asks)};

      if (removed){
        out << "ACK " << id << '\n';
      }
      else{
        out << "ACK " << id << " NOT_FOUND" << '\n';
      }
    }

  private:
    
    void matchBuy(Order& incoming, std::ostream& out){
      while (incoming.quantity > 0 && !asks.empty() && asks.begin()->first <= incoming.price){
        auto it = asks.begin();
        auto &dq = it->second;

        while (incoming.quantity > 0 && !dq.empty()){
          Order &resting = dq.front();
          int traded = std::min(incoming.quantity, resting.quantity);

          incoming.quantity -= traded;
          resting.quantity  -= traded;

          out << "FILL " << incoming.id << ' ' << resting.id << ' ' << it->first << ' ' << traded << '\n';

          if (resting.quantity == 0) dq.pop_front();
        }

        if (dq.empty()) asks.erase(it);
      }

      if (incoming.quantity > 0) bids[incoming.price].push_back(incoming);
    }



    // Match an incoming SELL order against best bids
    void matchSell(Order& incoming, std::ostream& out) {
        while (incoming.quantity > 0 &&
               !bids.empty() &&
               bids.begin()->first >= incoming.price) {

            auto it = bids.begin();
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
                bids.erase(it);
            }
        }

        if (incoming.quantity > 0) {
            asks[incoming.price].push_back(incoming);
        }
    }
};

static inline std::string trim(const std::string& s){

  std::size_t b{0};

  while(b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))){

    ++b;
  }
  std::size_t e = s.size();

  while ( e > b && std::isspace(static_cast<unsigned char>(s[e-1]))){

    --e;
  }
  return s.substr(b, e-b);
}

/// Take one command line, apply to engine, and produce response text.
// Returns false if line was empty (no response).
bool processLine (const std::string& line, MatchingEngine& engine, std::string& response){

  response.clear();
  std::string s {trim(line)};

  if (s.empty()){
    return false;
  }
  std::istringstream iss(s);
  std::string cmd;
  iss >> cmd;

      if (cmd == "SUBMIT") {
        std::string id;
        double price;
        int qty;
        char side;
        if (!(iss >> id >> side >> price >> qty)) {
            response = "ERR BAD_SUBMIT\n";
            return true;
        }
        side = static_cast<char>(
            std::toupper(static_cast<unsigned char>(side)));
        if (side != 'B' && side != 'S') {
          response = "ERR BAD_SIDE\n";
            return true;
        }

        Order o{id, side, price, qty};
        std::ostringstream oss;
        engine.submit(o, oss);
        response = oss.str();
        return true;

    } else if (cmd == "CANCEL") {
      std::string id;
      if (!(iss >> id)) {
        response = "ERR BAD_CANCEL\n";
        return true;
      }
      std::ostringstream oss;
      engine.cancel(id, oss);
        response = oss.str();
        return true;

    } else if (cmd == "DUMP") {
        // debug: dump current book
        std::ostringstream oss;
        oss << "BIDS:\n";
        for (auto& [px, dq] : engine.bids) {
            oss << px << ": ";
            for (auto& o : dq) {
                oss << o.id << "(" << o.quantity << ") ";
            }
            oss << "\n";
        }
        oss << "ASKS:\n";
        for (auto& [px, dq] : engine.asks) {
            oss << px << ": ";
            for (auto& o : dq) {
                oss << o.id << "(" << o.quantity << ") ";
            }
            oss << "\n";
        }
        response = oss.str();
        return true;

    } else {
        response = "ERR UNKNOWN_CMD\n";
        return true;
    }
}

int main() {
    // Create listening socket
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                 &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(6666); // port for generator.py

    if (::bind(listen_fd,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, 1) < 0) {
        std::perror("listen");
        ::close(listen_fd);
        return 1;
    }

    std::cout << "Listening on port 6666...\n";

    int client_fd = ::accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::perror("accept");
        ::close(listen_fd);
        return 1;
    }

    std::cout << "Client connected.\n";

    MatchingEngine engine;
    std::string buffer;
    buffer.reserve(4096);
    char temp[4096];

    while (true) {
        ssize_t n = ::recv(client_fd, temp, sizeof(temp), 0);
        if (n == 0) {
            std::cout << "Client closed connection.\n";
            break;
        }
        if (n < 0) {
            std::perror("recv");
            break;
        }

        buffer.append(temp, static_cast<std::size_t>(n));

        // Extract complete lines
        for (;;) {
            auto pos = buffer.find('\n');
            if (pos == std::string::npos) break;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            std::string resp;
            if (processLine(line, engine, resp)) {
                if (!resp.empty()) {
                    ::send(client_fd, resp.data(),
                           resp.size(), 0);
                }
            }
        }
    }

    ::close(client_fd);
    ::close(listen_fd);
    return 0;
}














