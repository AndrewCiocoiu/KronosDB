#include "wal.hpp"
#include <iostream>
#include <string>
#include <sstream>

WAL::WAL(const std::string filename)
    : filepath{std::move(filename)}
{
    //We need to open in binary append mode so that we dont remove the log file every time we start db (default for binary mode)
    log_file.open(filepath, std::ios::binary | std::ios::app);

    if(!log_file.is_open()){
        std::cout << "Could not open log file - " << filepath << "!\n";
    }
}

void WAL::append_set(const std::string &key, const std::string &value){
    std::lock_guard<std::mutex> lock(mtx);

    //We write logs using binary framing so that we dont have to deal with delimiters
    //1 byte for op type | 4 bytes for key size | size bytes for key chars | 4 bytes for value size | size bytes for valye chars
    WALOp op = WALOp::SET;
    log_file.write(reinterpret_cast<char*>(&op), sizeof(WALOp));

    uint32_t key_size = static_cast<uint32_t>(key.size());
    log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

    log_file.write(key.data(), key_size);

    uint32_t value_size = static_cast<uint32_t>(value.size());
    log_file.write(reinterpret_cast<char*>(&value_size), sizeof(value_size));

    log_file.write(value.data(), value_size);


    //Here I call flush so that we immediatley transfer our data to the OS instead of it living only in the process
    log_file.flush();
}

void WAL::append_del(const std::string &key){
    std::lock_guard<std::mutex> lock(mtx);

    //We write logs using binary framing so that we dont have to deal with delimiters
    //1 byte for op type | 4 bytes for key size | size bytes for key chars | 4 bytes for value size | size bytes for valye chars
    WALOp op = WALOp::DEL;
    log_file.write(reinterpret_cast<char*>(&op), sizeof(WALOp));

    uint32_t key_size = static_cast<uint32_t>(key.size());
    log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

    log_file.write(key.data(), key_size);

    log_file.flush();
}

void WAL::recover(std::function<void(WALOp op, const std::string& key, const std::string& value)> apply_fn) {
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile.is_open()) {
        return;
    }

    std::cout << "Starting recovery process.\n";

    //if we have had a crash it means we have incomplete memory in our disk so when read attempts to read it it will trigger its
    //failbit which will make it return false. In this case we want to stop our recovery since it means we have reached garbage data.

    WALOp op;
    while (infile.read(reinterpret_cast<char*>(&op), sizeof(op))) {
        if (op == WALOp::SET){
            uint32_t key_size = 0;
            if (!infile.read(reinterpret_cast<char*>(&key_size), sizeof(key_size))) {
                break; // Truncated record, stop recovery
            }

            std::string key;
            key.resize(key_size);
            if (!infile.read(key.data(), key_size)) {
                break;
            }

            uint32_t value_size = 0;
            if (!infile.read(reinterpret_cast<char*>(&value_size), sizeof(value_size))) {
                break;
            }

            std::string value;
            value.resize(value_size);
            if (!infile.read(value.data(), value_size)) {
                break;
            }

            apply_fn(WALOp::SET, key, value);
        } else if (op == WALOp::DEL){
            uint32_t key_size = 0;
            if (!infile.read(reinterpret_cast<char*>(&key_size), sizeof(key_size))) {
                break; // Truncated record, stop recovery
            }

            std::string key;
            key.resize(key_size);
            if (!infile.read(key.data(), key_size)) {
                break;
            }

            apply_fn(WALOp::DEL, key, "");
        }

        
    }
}

WAL::~WAL(){
    std::lock_guard<std::mutex> lock(mtx);
    log_file.flush();
    log_file.close();
}