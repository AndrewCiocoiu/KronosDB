#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>

Server::Server(Engine& engine, Threadpool& pool, int port)
    : db(engine), pool(pool), port(port) {}

Server::~Server() {
    stop();
    if (server_fd != -1) close(server_fd);
    if (epoll_fd != -1) close(epoll_fd);
}

void Server::make_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

bool Server::init() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return false;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind failed");
        return false;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        return false;
    }

    make_non_blocking(server_fd);

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) return false;

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    return true;
}

void Server::start() {
    const int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];
    running = true;

    std::cout << "KronosDB listening on port " << port << "...\n";

    while (running) {
        int n_ready = epoll_wait(epoll_fd, events, MAX_EVENTS, 100); // 100ms to check `running`
        if (n_ready < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait error");
            break;
        }

        for (int i = 0; i < n_ready; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept error");
                        break;
                    }
                    make_non_blocking(client_fd);
                    epoll_event client_event{};
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);
                }
            } else {
                handle_client_data(fd);
            }
        }
    }
}

void Server::stop() {
    running = false;
}

void Server::handle_client_data(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string request(buffer, bytes_read);

        // Send to  threadpool
        pool.enqueue([this, client_fd, req = std::move(request)]() {
            std::string response = process_command(req);
            send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        });
    } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);
    }
}

std::string Server::process_command(const std::string& raw_cmd) {
    std::istringstream iss(raw_cmd);
    std::string cmd;
    iss >> cmd;

    // Everything uppercase
    for (auto& c : cmd) c = std::toupper(static_cast<unsigned char>(c));

    if (cmd == "PING") {
        return "PONG\n";
    } else if (cmd == "SET") {
        std::string key;
        iss >> key;
        std::string value;
        if (iss.peek() == ' ') iss.ignore();
        std::getline(iss, value);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
            value.pop_back();
        }
        if (key.empty()) return "ERR: Usage SET <key> <value>\n";
        db.set(key, value);
        return "OK\n";
    } else if (cmd == "GET") {
        std::string key;
        iss >> key;
        if (key.empty()) return "ERR: Usage GET <key>\n";
        auto val = db.get(key);
        return val.has_value() ? (val.value() + "\n") : "(nil)\n";
    } else if (cmd == "DEL") {
        std::string key;
        iss >> key;
        if (key.empty()) return "ERR: Usage DEL <key>\n";
        db.del(key);
        return "OK\n";
    }

    return "ERR: Unknown command\n";
}