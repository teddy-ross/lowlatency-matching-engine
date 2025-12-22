#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

int main() {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(6666);

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
            if (process_line(line, engine, resp)) {
                if (!resp.empty()) {
                    ::send(client_fd, resp.data(), resp.size(), 0);
                }
            }
        }
    }

    ::close(client_fd);
    ::close(listen_fd);
    return 0;
}
