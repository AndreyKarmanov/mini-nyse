#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  // for TCP_NODELAY like a chad
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <chrono>
#include <stdexcept>

class TcpClient {
private:
    int sock_fd = -1;
    struct sockaddr_in server_addr;
    bool connected = false;
    const int CONNECT_TIMEOUT_MS = 1000;
    const int RECV_TIMEOUT_MS = 100;

    void set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) throw std::runtime_error("fcntl F_GETFL skill issue");
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error("fcntl F_SETFL packed");
    }

    bool wait_for_socket(int fd, short events, int timeout_ms) {
        struct pollfd pfd { fd, events, 0 };
        return poll(&pfd, 1, timeout_ms) > 0;
    }

public:
    TcpClient(const char* ip, int port, bool disable_nagle = true) {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd == -1) throw std::runtime_error("socket creation went sideways");
    
        if (disable_nagle) {
            int flag = 1;
            if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
                throw std::runtime_error("TCP_NODELAY failed (ngmi)");

            int quick_ack = 1;
            setsockopt(sock_fd, IPPROTO_TCP, TCP_QUICKACK, &quick_ack, sizeof(quick_ack));
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
            throw std::runtime_error("skill issue: invalid ip");
    }

    ~TcpClient() { if (sock_fd != -1) close(sock_fd); }

    // Attempts to establish a connection with a timeout
    // Returns true if connection is successful or already connected
    bool connect_with_timeout() {
        if (connected) return true;

        // Set socket to non-blocking mode
        set_nonblocking(sock_fd);

        // Attempt to connect to the server
        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            // EINPROGRESS means the connection is in progress (normal for non-blocking)
            if (errno != EINPROGRESS) return false;

            // Wait for the socket to become writable (connected) with timeout
            if (!wait_for_socket(sock_fd, POLLOUT, CONNECT_TIMEOUT_MS))
                return false;

            // Check if the connection was actually successful
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
                return false;
        }

        // Mark as connected and return success
        connected = true;
        return true;
    }

    bool send_data(const std::string& data) {
        if (!connected) return false;
        struct tcp_info info;
        socklen_t len = sizeof(info);
        getsockopt(sock_fd, IPPROTO_TCP, TCP_INFO, &info, &len);
        std::cout << "rtt: " << info.tcpi_rtt << "μs\n";
        size_t total_sent = 0;
        while (total_sent < data.length()) {
            if (!wait_for_socket(sock_fd, POLLOUT, CONNECT_TIMEOUT_MS))
                return false;

            ssize_t sent = send(sock_fd,
                data.c_str() + total_sent,
                data.length() - total_sent,
                MSG_NOSIGNAL);  // SIGPIPE = cringe

            if (sent <= 0) {
                if (errno == EINTR) continue;
                connected = false;
                return false;
            }
            total_sent += sent;
        }
        return true;
    }

    std::string recv_data() {
        if (!connected) return "";

        std::string response;
        char buffer[4096];

        while (true) {
            if (!wait_for_socket(sock_fd, POLLIN, RECV_TIMEOUT_MS))
                break;

            ssize_t bytes = recv(sock_fd, buffer, sizeof(buffer), 0);
            if (bytes < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                connected = false;
                throw std::runtime_error("recv got ratio'd");
            }
            if (bytes == 0) {  // gg go next
                connected = false;
                break;
            }
            response.append(buffer, bytes);
        }
        return response;
    }
};