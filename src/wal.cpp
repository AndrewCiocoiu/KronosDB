#include "wal.hpp"
#include <iostream>
#include <string>
#include <sstream>

WAL::WAL(const std::string filename)
    : filepath{std::move(filename)}
{
    log_file.open(filepath, std::ios::binary | std::ios::app);
    if (!log_file.is_open()){
        std::cerr << "Could not open log file - " << filepath << "!\n";
    }

    flusher_thread = std::jthread([this]{ flusher_loop(); });
}

void WAL::append_set(const std::string &key, const std::string &value){

    std::promise<bool> prom;
    std::future<bool> fut = prom.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.emplace_back(LogRecord{WALOp::SET, key, value, std::move(prom)});
    }
    cv.notify_one();

    fut.get();
    // //We write logs using binary framing so that we dont have to deal with delimiters
    // //1 byte for op type | 4 bytes for key size | size bytes for key chars | 4 bytes for value size | size bytes for valye chars
    // WALOp op = WALOp::SET;
    // log_file.write(reinterpret_cast<char*>(&op), sizeof(WALOp));

    // uint32_t key_size = static_cast<uint32_t>(key.size());
    // log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

    // log_file.write(key.data(), key_size);

    // uint32_t value_size = static_cast<uint32_t>(value.size());
    // log_file.write(reinterpret_cast<char*>(&value_size), sizeof(value_size));

    // log_file.write(value.data(), value_size);


    // //Here I call flush so that we immediatley transfer our data to the OS instead of it living only in the process
    // log_file.flush();
}

void WAL::append_del(const std::string &key){
    std::promise<bool> prom;
    std::future<bool> fut = prom.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.emplace_back(LogRecord{WALOp::DEL, key, "", std::move(prom)});
    }

    cv.notify_one();

    fut.get();

    // //We write logs using binary framing so that we dont have to deal with delimiters
    // //1 byte for op type | 4 bytes for key size | size bytes for key chars | 4 bytes for value size | size bytes for valye chars
    // WALOp op = WALOp::DEL;
    // log_file.write(reinterpret_cast<char*>(&op), sizeof(WALOp));

    // uint32_t key_size = static_cast<uint32_t>(key.size());
    // log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

    // log_file.write(key.data(), key_size);

    // log_file.flush();
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

void WAL::flusher_loop(){
    while (true){
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        cv.wait(lock, [this]{
            return !queue.empty() || stop;
        });

        if(stop && queue.empty()){
            break;
        }

        std::deque<LogRecord> batch;
        //We instantly swap all of the tasks in our queue to our batch so that we can unlock
        batch.swap(queue);
        lock.unlock();

        //We nmever want to do IO operations while we are locked since they are extrmeely slow. While we are writing to disk we want
        //our other threads to be able to commit logs to the queue which we then instantly swap with our current batch 

        for(auto & log : batch){
            if(log.op == WALOp::SET){
                log_file.write(reinterpret_cast<char*>(&log.op), sizeof(WALOp));

                uint32_t key_size = static_cast<uint32_t>(log.key.size());
                log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

                log_file.write(log.key.data(), key_size);

                uint32_t value_size = static_cast<uint32_t>(log.value.size());
                log_file.write(reinterpret_cast<char*>(&value_size), sizeof(value_size));

                log_file.write(log.value.data(), value_size);

            } else if(log.op == WALOp::DEL){
                log_file.write(reinterpret_cast<char*>(&log.op), sizeof(WALOp));

                uint32_t key_size = static_cast<uint32_t>(log.key.size());
                log_file.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

                log_file.write(log.key.data(), key_size);

            }
        }

        log_file.flush();

        for(auto & log : batch){
            log.promise.set_value(true);
        }
    }
}

WAL::~WAL(){
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }

    cv.notify_all();

    //We wait for our writer to finnish writing everythong to the OS before flushing
    if (flusher_thread.joinable()){
        flusher_thread.join();
    }

    log_file.flush();
    log_file.close();
}