#include "threadpool.hpp"
#include "engine.hpp"
#include "benchmark.hpp"
#include "wal.hpp"
#include <iostream>

int main() {
    Threadpool pool(4);
    Engine db("Kronos", 16);
    BenchmarkDB bench(20);
    bench.start_benchmark();

    db.start();


    auto write_future = pool.submit([&db]() {
        db.set("user:42", "Andrei Ciocoiu\nRole: Systems Engineer");
    });

    write_future.get();


    auto read_future = pool.submit([&db]() {
        auto user = db.get("user:42");
        if(!user.has_value()){
            std::cout << "User not found";
        }

        return user;
    });


    auto result = read_future.get();

    if(result.has_value()){
        std::cout << result.value() << "\n";
    }

    return 0;
}