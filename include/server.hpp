#pragma once

#include "engine.hpp"
#include "threadpool.hpp"
#include <string>
#include <atomic>

class Server {
public:
    Server(Engine& engine, Threadpool& pool, int port = 8081);
    ~Server();

    bool init();
    void start();
    void stop();

private:
    void make_non_blocking(int fd);
    void handle_client_data(int client_fd);
    std::string process_command(const std::string& raw_cmd);

    Engine& db;
    Threadpool& pool;
    int port;
    int server_fd{-1};
    int epoll_fd{-1};
    std::atomic<bool> running{false};
};