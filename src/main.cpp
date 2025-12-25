#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <signal.h>
#include <cerrno>

/**
 * Simple single-client TCP server that accepts textual protocol lines
 * and forwards them to the MatchingEngine via process_line.
 */

int main() {
    // Prevent SIGPIPE from killing the process when writing to a closed socket.
    // send() will return -1 with errno==EPIPE instead.
    signal(SIGPIPE, SIG_IGN);

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
    addr.sin_port        = htons(6767);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {

        std::perror("bind");
        ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, 1) < 0) {

        std::perror("listen");
        ::close(listen_fd);
        return 1;
    }

    std::cout << "Listening on port 6767..." << std::endl;

    int client_fd = ::accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {

        std::perror("accept");
        ::close(listen_fd);
        return 1;
    }

    std::cout << "Client connected." << std::endl;

    MatchingEngine engine;
    std::string buffer;
    buffer.reserve(4096);
    char temp[4096];

    bool connection_alive = true;

    while (connection_alive) {
        ssize_t n = ::recv(client_fd, temp, sizeof(temp), 0);
        if (n == 0) {
            std::cout << "Client closed connection." << std::endl;
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

                    const char* ptr = resp.data();
                    size_t remaining = resp.size();
                    while (remaining > 0) {

                        ssize_t written = ::send(client_fd, ptr, remaining, 0);
                        if (written < 0) {

                            if (errno == EINTR) continue;
                            std::perror("send");
                            connection_alive = false;
                            break;

                        }
                        ptr += written;
                        remaining -= static_cast<size_t>(written);
                    }
                    if (!connection_alive) break;
                }
            }
        }
    }

    ::close(client_fd);
    ::close(listen_fd);
    return 0;
}
