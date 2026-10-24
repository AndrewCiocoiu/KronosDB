#include <iostream>
#include "engine.hpp"
#include <string>
#include <thread>


void worker(Engine & db, int worker_id, int op_count){
    for(int i = 0; i < op_count; ++i){
        std::string key = "key_" + std::to_string(worker_id) + std::to_string(i);
        std::string val = "val_" + std::to_string(i);

        db.set(key, val);
    }
}

int main(){
    
    std::cout << "Initializing project\n";

    Engine db("Kronos-Core");
    db.start();


    const int OPS_PER_THREAD = 50000;

    //We need to use ref since by default the thread constructor passes everything by value
    std::thread t1(worker, std::ref(db), 1, OPS_PER_THREAD);
    std::thread t2(worker, std::ref(db), 2, OPS_PER_THREAD);

    t1.join();
    t2.join();


    std::cout << db.size();
}