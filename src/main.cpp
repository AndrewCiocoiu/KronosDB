#include "threadpool.hpp"
#include "sstream"
#include "iostream"

int main(){
    std::cout << "Intializing threadpool.";

    {
        Threadpool pool;

        for(int i = 0; i < 100000; i++){
            pool.enqueue([i] {
                std::ostringstream ss;
                ss << "Task " << i << " running on thread: " << std::this_thread::get_id() << " \n";
                std::cout << ss.str();

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
        }
        
    }
    std::cout << "All tasks completed!\n";
}