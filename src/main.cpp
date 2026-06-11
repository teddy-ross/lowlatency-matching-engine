#include "Log.hpp"
#include "MatchingEngine.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <string>
#include <string_view>

/**
 * Single-threaded TCP server for the text protocol.
 *
 * Serves one client at a time but keeps accepting new connections; the order
 * book persists across reconnects. Responses for each received chunk are
 * batched into a single send() to cut syscall count, and TCP_NODELAY is set
 * so small request/response exchanges aren't delayed by Nagle's algorithm.
 */

namespace {

constexpr std::uint16_t kDefaultPort = 6767;

/// send() the whole buffer, retrying on EINTR. Returns false on hard error.
[[nodiscard]] bool send_all(int fd, std::string_view data) noexcept {
    while (!data.empty()) {
        const ssize_t n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("send");
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

/// Pump one client connection until it closes or errors.
void serve_client(int client_fd, MatchingEngine& engine) {
    std::string rxBuf;       // unparsed bytes carried across recv() calls
    std::string txBuf;       // batched responses for the current chunk
    std::string response;    // per-line scratch, reused
    rxBuf.reserve(4096);
    txBuf.reserve(4096);

    char chunk[4096];

    for (;;) {
        const ssize_t n = ::recv(client_fd, chunk, sizeof(chunk), 0);
        if (n == 0) {
            logln("Client closed connection.");
            return;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            std::perror("recv");
            return;
        }

        rxBuf.append(chunk, static_cast<std::size_t>(n));

        // Walk complete lines via an offset — no per-line erase of the front
        // of the buffer (which would be O(n^2) under batched input).
        std::size_t lineStart = 0;
        for (;;) {
            const std::size_t nl = rxBuf.find('\n', lineStart);
            if (nl == std::string::npos) break;

            const std::string_view line{rxBuf.data() + lineStart, nl - lineStart};
            lineStart = nl + 1;

            if (process_line(line, engine, response)) {
                txBuf += response;
            }
        }
        rxBuf.erase(0, lineStart);  // keep only the trailing partial line

        if (!txBuf.empty()) {
            if (!send_all(client_fd, txBuf)) return;
            txBuf.clear();
        }
    }
}

[[nodiscard]] std::uint16_t parse_port(int argc, char** argv) noexcept {
    if (argc > 1) {
        const std::string_view arg{argv[1]};
        std::uint16_t port{};
        const auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), port);
        if (ec == std::errc{} && ptr == arg.data() + arg.size() && port != 0) return port;
        logln("Ignoring invalid port '{}', using {}.", arg, kDefaultPort);
    }
    return kDefaultPort;
}

}  // namespace

int main(int argc, char** argv) {
    // Belt and braces with MSG_NOSIGNAL: never die on writes to a closed peer.
    std::signal(SIGPIPE, SIG_IGN);

    const std::uint16_t port = parse_port(argc, argv);

    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int opt = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, 16) < 0) {
        std::perror("listen");
        ::close(listen_fd);
        return 1;
    }

    logln("Listening on port {}...", port);

    MatchingEngine engine;  // one book, persists across client connections

    for (;;) {
        const int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            break;
        }

        // Disable Nagle: small request/response messages go out immediately.
        int nodelay = 1;
        ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        logln("Client connected.");
        serve_client(client_fd, engine);
        ::close(client_fd);
        logln("Waiting for next connection (book state persists)...");
    }

    ::close(listen_fd);
    return 0;
}
