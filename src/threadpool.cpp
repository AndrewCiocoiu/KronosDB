#include "threadpool.hpp"

Threadpool::Threadpool(size_t thread_num){
    if(thread_num == 0){
        thread_num = 2;
    }

    for(size_t i = 0; i < thread_num; i++){
        //This runs inside the new thread we are creating
        workers.emplace_back([this] {
            //We run this while true because we want the thread once spawned to be available for the whole lifecycle of the threadpool
            //If there is no work the thread will be put to sleep using a conditional variable so it will idle at 0% CPU instead of jamming the while
            while (true){
                Task task;

                std::unique_lock<std::mutex> lock(queue_mtx);
                cv.wait(lock, [this] {
                    return !tasks.empty() || stop;
                });

                //If we have destroyed the threadpool object we want to also exit out of the while loop so that we can destroy the threads
                if (stop && tasks.empty()){
                    break;
                }

                task = std::move(tasks.front());
                tasks.pop();

                lock.unlock();
                //We never want to do client work on a locked thread
                task();
            }


        });

    }
}

void Threadpool::enqueue(Task task){
    {
        std::lock_guard<std::mutex> lock(queue_mtx);

        tasks.push(std::move(task));
    }
    cv.notify_one();
}

Threadpool::~Threadpool(){
    {
        std::lock_guard<std::mutex> lock(queue_mtx);

        stop = true;
    }
    cv.notify_all();
}