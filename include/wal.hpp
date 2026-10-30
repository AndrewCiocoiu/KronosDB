#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <functional>

class WAL {
public:
    //Explicit prevents the cpp compiler from performing implicit type conversions or accidental assignments
    explicit WAL(const std::string filename);
    ~WAL();

    void append_set(const std::string& key, const std::string& value);

    //We allow a function to be passed since we want to allow the client to choose what to do with the recovered data on their own
    void recover(std::function<void(const std::string& key, const std::string& value)> apply_fn);


private:
    std::string filepath;
    std::ofstream log_file;
    std::mutex mtx;
};