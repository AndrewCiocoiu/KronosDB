#include "engine.hpp"
#include "threadpool.hpp"
#include "server.hpp"
#include <csignal>
#include <memory>

std::unique_ptr<Server> global_server;

void signal_handler(int) {
    if (global_server) {
        global_server->stop();
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Threadpool pool(4);
    Engine db("Kronos", 16, true);
    db.start();

    global_server = std::make_unique<Server>(db, pool, 8081);
    if (!global_server->init()) {
        return 1;
    }

    global_server->start();
    return 0;
}