#include <chrono>
#include <functional>
#include <iostream>
#include "engine.hpp"
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <sstream>

using Task = std::function<void()>;

class TaskQueue {
public:
    void push(Task task){
        {
            std::lock_guard<std::mutex> lock(mtx);
            //Move avoids duplicating the function, we simply change ownership through pointers
            tasks.push(std::move(task));
        }
        //Here we basically tell one of our currently sleeping threads to wake
        cv.notify_one();
    }

    bool pop(Task & out_task){
        std::unique_lock<std::mutex> lock(mtx);

        //We sleep until either we get shutdown or a new task appears
        cv.wait(lock, [this] {
            return stop_requested || !tasks.empty();
        });

        //If we stopped and there are no more task available we return false so we can kill the worker
        if(stop_requested && tasks.empty()){
            return false;
        }

        //Again we use move because it simply changes the internal pointers instead of deep copying
        //We are guaranteed to have a task here
        out_task = std::move(tasks.front());
        tasks.pop();
        //We tell our worker we have a new task
        return true;
    }

    void shutdown(){
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop_requested = true;
        }
        cv.notify_all();
    }
private:
    std::queue<Task> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop_requested = false;
};

void worker_loop(int id, TaskQueue& q){
    Task task;
    while(q.pop(task)){
        task();
    }
    std::cout << "Worker " << id << " exited!\n";
}

int main(){
    TaskQueue q;
    std::vector<std::thread> workers;

    for(int i = 0; i < 2; i++){
        workers.emplace_back(worker_loop, i, std::ref(q));
    }

    for(int i = 0; i < 6; i++){
        q.push([i] {
            std::ostringstream ss;
            ss << "Executing task " << i << " on Thread " << std::this_thread::get_id() << "\n";
            std::cout << ss.str();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    std::cout << "Shutting down queue\n";
    q.shutdown();

    for(auto & worker : workers){
        worker.join();
    }

    std::cout << "All workers have shutdown!\n";

}