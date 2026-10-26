#include "sstream"
#include "iostream"
#include "engine.hpp"
#include "format"
#include "threadpool.hpp"

int main(){
    std::cout << "Intializing threadpool.\n";

    auto start = std::chrono::high_resolution_clock::now();
    {
        Engine db("Hello", 20);
        Threadpool pool;

        
        
        for(int i = 0; i < 500; i++){
            pool.enqueue([&db, i] {
                for(int i = 0; i < 200; i++){
                std::ostringstream oss;
                oss << i << "_" << std::this_thread::get_id();
                db.set(oss.str(), "dasda");
                }
            });
        }
        
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << ms << "\n";
        
    
}