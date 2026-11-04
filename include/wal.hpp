#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <functional>

enum class WALOp : uint8_t {
    SET = 1,
    DEL = 2
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

private:
    std::string filepath;
    std::ofstream log_file;
    std::mutex mtx;
};