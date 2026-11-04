#include "benchmark.hpp"
#include "sstream"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>

BenchmarkDB::BenchmarkDB(int shard_count){
    db = std::make_unique<Engine>("Benchmark_DB", shard_count, true);
    pool = std::make_unique<Threadpool>();
}

BenchmarkDB::~BenchmarkDB() = default;

void BenchmarkDB::start_benchmark(int nr_of_jobs, int work_per_job){
    db->start();

    int task_count = nr_of_jobs;
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);
    if (db->is_logging()){
        std::ofstream("logs/Benchmark_DB.wal");
    }
    auto start = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < task_count; i++){
        futures.push_back(
            pool->submit([this, i, work_per_job] {
                for(int j = 0; j < work_per_job; j++){
                    std::ostringstream oss;
                    oss << j << "_" << std::this_thread::get_id();
                    db->set(oss.str(), "dasda");
                }
            })
        );
    }

    for(auto &f : futures){
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Benchmark finished in: " << ms << " ms\n";
    if(db->is_logging()){
        std::filesystem::remove("logs/Benchmark_DB.wal");
    }
}