#include <chrono>
#include <functional>
#include <iostream>
#include "engine.hpp"
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>


void worker(Engine & db, int worker_id, int op_count){
    for(int i = 0; i < op_count; ++i){
        std::string key = "key_" + std::to_string(worker_id) + std::to_string(i);
        std::string val = "val_" + std::to_string(i);

        db.set(key, val);
    }
}

int main(){
    const int TOTAL_OPS = 100000;

    //Test 1 thread doing 100.000 writes
    {
        Engine db_1("test1", 1);

        auto start = std::chrono::high_resolution_clock::now();
        worker(db_1, 0, TOTAL_OPS);
        auto end = std::chrono::high_resolution_clock::now();

        //We need to call count in order to extract an int from the time container
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "1 Thread - " << TOTAL_OPS << " operations in " << ms << " ms.\n";
    
    }
    //Test 16 threads doing 6.2k writes each (100k total) and having 16 shards
    {
        const int THREAD_COUNT = 16;
        const int OP_PER_THREAD = TOTAL_OPS / THREAD_COUNT;
        std::vector<std::thread> threads;
        Engine db_2("test2", 16);

        auto start = std::chrono::high_resolution_clock::now();

        //Emplace back compared to pushback will construct the object directly in the vector instead of taking an already constructed object and moving it
        for(int i = 0; i < THREAD_COUNT; i++){
            threads.emplace_back(worker, std::ref(db_2), i, OP_PER_THREAD);
        }

        for(auto & t : threads){
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << THREAD_COUNT << " Threads - " << TOTAL_OPS << " operations in " << ms << " ms.\n";


    }
}