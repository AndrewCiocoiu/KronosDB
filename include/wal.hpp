#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <functional>
#include <deque>
#include <future>
#include <thread>

enum class WALOp : uint8_t {
    SET = 1,
    DEL = 2
};

//We are going to also store a promise so that we know when the log record has been processed by our permanent group commit thread
struct LogRecord{
    WALOp op;
    std::string key;
    std::string value;
    std::promise<bool> promise;
};

class WAL {
public:
    //Explicit prevents the cpp compiler from performing implicit type conversions or accidental assignments
    explicit WAL(const std::string filename);
    ~WAL();

    void append_set(const std::string& key, const std::string& value);

    void append_del(const std::string &key);

    //We allow a function to be passed since we want to allow the client to choose what to do with the recovered data on their own
    void recover(std::function<void(WALOp op, const std::string& key, const std::string& value)> apply_fn);

    //This is our permanent group commit thread that continously writes batches of currently appended logs
    void flusher_loop();

    //Will reset our log file (will be called by engine when writing its snapshot)
    void reset_log();

private:
    std::string filepath;
    std::ofstream log_file;

    std::deque<LogRecord> queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool is_writing_batch;
    std::jthread flusher_thread;
    bool stop{false};

};