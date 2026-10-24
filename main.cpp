#include <chrono>
#include <iostream>
#include "engine.hpp"
#include <string>
#include <thread>
#include <vector>


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
        Engine db_1("test1");

        auto start = std::chrono::high_resolution_clock::now();
        worker(db_1, 0, TOTAL_OPS);
        auto end = std::chrono::high_resolution_clock::now();

        //We need to call count in order to extract an int from the time container
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "1 Thread - " << TOTAL_OPS << " operations in " << ms << " ms.\n";
    
    }
    //Test 4 threads doing 25.000 writes each (100k total)
    {
        const int THREAD_COUNT = 4;
        const int OP_PER_THREAD = TOTAL_OPS / THREAD_COUNT;
        std::vector<std::thread> threads;
        Engine db_2("test2");

        auto start = std::chrono::high_resolution_clock::now();

        for(int i = 0; i < THREAD_COUNT; i++){
            threads.emplace_back(worker, std::ref(db_2), i, OP_PER_THREAD);
        }

        for(auto & t : threads){
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "4 Threads - " << TOTAL_OPS << " operations in " << ms << " ms.\n";


    }
}