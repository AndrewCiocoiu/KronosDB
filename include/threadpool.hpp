#pragma once 

#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>


class Threadpool{
public:
    using Task = std::function<void()>;

    Threadpool(size_t thread_num = std::thread::hardware_concurrency());

    ~Threadpool();

    void enqueue(Task task);
private:
    std::vector<std::jthread> workers;

    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
};