#pragma once
#include "threadpool.hpp"
#include "engine.hpp"

class BenchmarkDB {
public:
    BenchmarkDB(int shard_count);
    ~BenchmarkDB();

    void start_benchmark(int nr_of_jobs = 500, int work_per_job = 200);
private:
    std::unique_ptr<Engine> db;
    std::unique_ptr<Threadpool> pool;
};