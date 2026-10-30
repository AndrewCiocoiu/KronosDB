#include "wal.hpp"
#include <iostream>
#include <string>
#include <sstream>

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

void WAL::recover(std::function<void(const std::string& key, const std::string& value)> apply_fn){
    std::ifstream infile(filepath);
    if (!infile.is_open()){
        std::cout << "No existing backup!\n";
    }

    std::string line;
    while(std::getline(infile, line)){
        std::stringstream ss(line);
        std::string command, key;
        //Because our values can have spaces in them we want to read the whole rest of the line
        if (ss >> command >> key) {
            std::string value;
            // Skip single delimiter space if present, then read remainder
            if (ss.peek() == ' ') {
                ss.ignore();
            }
            std::getline(ss, value);

            if (command == "SET") {
                apply_fn(key, value);
            }
        }
    }
}

WAL::~WAL(){
    std::lock_guard<std::mutex> lock(mtx);
    log_file.flush();
    log_file.close();
}