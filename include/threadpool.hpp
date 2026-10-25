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
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    bool stop = false;

    //ORDER MATTERS!! in a class when a destructor runs, declared things are destructed in reverse order from how they are initialized and 
    //in this case workers needs the other variables to not be destructed so that they can finnish their leftover work before being 
    //destructed themselved, thats why we want to destroy workers first aka we need to put them last
    std::vector<std::jthread> workers;
};