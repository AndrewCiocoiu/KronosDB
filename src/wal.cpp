#include "wal.hpp"
#include <iostream>

WAL::WAL(const std::string filename)
    : filepath{std::move(filename)}
{
    log_file.open(filepath, std::ios::app);

    if(!log_file.is_open()){
        std::cout << "Could not open log file - " << filepath << "!\n";
    }
}

void WAL::append_set(const std::string &key, const std::string &value){
    std::lock_guard<std::mutex> lock(mtx);

    log_file << "SET " << key << " " << value << "\n";

    //Here I call flush so that we immediatley transfer our data to the OS instead of it living only in the process
    log_file.flush();
}

WAL::~WAL(){
    std::lock_guard<std::mutex> lock(mtx);
    log_file.flush();
    log_file.close();
}