#include "engine.hpp"
#include <cstddef>
#include "wal.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <filesystem>

//By initializing before the body we avoid having to delete the default value to assign a new value
Engine::Engine(std::string const & name, size_t shard_count, bool should_log) : db_name(name), shard_count(shard_count), logging(should_log) {
    //On windows if the directory does not exist it will crash
    std::filesystem::create_directories("logs");
    std::string file_name = "logs/" + db_name + ".wal";

    for(size_t i = 0; i < shard_count; i++){
        shards.push_back(std::make_unique<Shard>());
    }

    //Start recovery from snapshot
    std::ifstream snapshot_file("logs/" + db_name + ".snapshot", std::ios::binary);
    WALOp op = WALOp::SET;
    while (snapshot_file.read(reinterpret_cast<char*>(&op), sizeof(op))) {
        uint32_t key_size = 0;
        if (!snapshot_file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size))) {
            break; // Truncated record, stop recovery
        }

        std::string key;
        key.resize(key_size);
        if (!snapshot_file.read(key.data(), key_size)) {
            break;
        }

        uint32_t value_size = 0;
        if (!snapshot_file.read(reinterpret_cast<char*>(&value_size), sizeof(value_size))) {
            break;
        }

        std::string value;
        value.resize(value_size);
        if (!snapshot_file.read(value.data(), value_size)) {
            break;
        }

        set_memory_only(key, value);
    }
    snapshot_file.close();

    wal = std::make_unique<WAL>(file_name);

    wal->recover([this, &file_name](WALOp op, const std::string& k, const std::string& v){

        if (op == WALOp::SET) {
            set_memory_only(k, v);
            std::cout << "Set " << k << " " << v << " from " << file_name << "\n";
        } else if (op == WALOp::DEL) {
            del_memory_only(k);
            std::cout << "Deleted " << k << " from " << file_name << "\n"; 
        }

    });
}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}

void Engine::set(const std::string & key, const std::string & value){
    
    size_t shard_idx = get_shard_idx(key);

    //since we cannot copy a unique ptr we have to get it as a reference
    auto & current_shard = shards[shard_idx];
    //we need to make sure we unlock before calling create checkpoin since create checkpoint will also call lock on the current shard
    //which will cause a double lock which will cause a deadlock
    {
        std::lock_guard<std::mutex> lock(current_shard->mtx);
        if (logging){
            wal->append_set(key, value);
        }
        current_shard->map[key] = value;
    }
    //We check if we need to create a checkpoint
    if (++ops_since_checkpoint >= CHECKPOINT_THRESHOLD) {
        ops_since_checkpoint = 0;
        create_checkpoint();
    }
}

void Engine::set_memory_only(const std::string& key, const std::string& value){
    size_t shard_idx = get_shard_idx(key);

    //since we cannot copy a unique ptr we have to get it as a reference
    auto & current_shard = shards[shard_idx];

    std::lock_guard<std::mutex> lock(current_shard->mtx);
    current_shard->map[key] = value;
}

void Engine::del(const std::string &key){
    size_t shard_idx = get_shard_idx(key);

    auto & current_shard = shards[shard_idx];
    {
        std::lock_guard<std::mutex> lock(current_shard->mtx);
        if (logging){
            wal->append_del(key);
        }
        current_shard->map.erase(key);
    }
    if (++ops_since_checkpoint >= CHECKPOINT_THRESHOLD) {
        ops_since_checkpoint = 0;
        create_checkpoint();
    }
}

void Engine::del_memory_only(const std::string &key){
    size_t shard_idx = get_shard_idx(key);

    auto & current_shard = shards[shard_idx];

    std::lock_guard<std::mutex> lock(current_shard->mtx);
    current_shard->map.erase(key);
}

std::optional<std::string> Engine::get(const std::string & key){
    size_t shard_idx = get_shard_idx(key);

    auto & current_shard = shards[shard_idx];

    std::lock_guard<std::mutex> lock(current_shard->mtx);

    auto it = current_shard->map.find(key);

    //first will be the key and second will be the value
    if (it != current_shard->map.end()){
        return it->second;
    }

    return std::nullopt;
}

bool Engine::is_logging(){
    return logging;
}

size_t Engine::size() const {

    //If a thread calls size while another writes we will have undefined behaviour. So we need to lock.
    size_t total = 0;
    for (size_t i = 0; i < shard_count; ++i) {
        std::lock_guard<std::mutex> lock(shards[i]->mtx);
        total += shards[i]->map.size();
    }
    return total;

}

size_t Engine::get_shard_idx(const std::string& key) const{
    size_t hash = std::hash<std::string>{}(key);
    return hash % shard_count;
}

void Engine::create_checkpoint(){
    std::vector<std::unique_lock<std::mutex>> locks;
    std::string filepath = "logs/" + db_name + ".snapshot.tmp";

    std::ofstream ofile(filepath, std::ios::binary);

    if(!ofile.is_open()){
        std::cerr << "Failed to open file";
        return;
    }


    //We want to go through all or shards and get all of their mutexes so we can lock everything when doing a snapshot, so that we don't
    //read while somebody is writing on that shard
    for(const auto & shard : shards){
        locks.emplace_back(shard->mtx);
    }

    WALOp op = WALOp::SET;

    for(const auto & shard : shards){
        for(const auto & [key, value] : shard->map){
            ofile.write(reinterpret_cast<char*>(&op), sizeof(WALOp));

            uint32_t key_size = static_cast<uint32_t>(key.size());
            ofile.write(reinterpret_cast<char*>(&key_size), sizeof(key_size));

            ofile.write(key.data(), key_size);

            uint32_t value_size = static_cast<uint32_t>(value.size());
            ofile.write(reinterpret_cast<char*>(&value_size), sizeof(value_size));

            ofile.write(value.data(), value_size);
        }
    }
    //We need to make sure our file is closed before renaming it
    ofile.flush();
    ofile.close();

    std::filesystem::rename("logs/" + db_name + ".snapshot.tmp" ,"logs/" + db_name + ".snapshot");
    wal->reset_log();
}

Engine::~Engine(){
    create_checkpoint();
}