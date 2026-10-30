#include "engine.hpp"
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <filesystem>

//By initializing before the body we avoid having to delete the default value to assign a new value
Engine::Engine(std::string const & name, size_t shard_count) : db_name(name), shard_count(shard_count) {
    //On windows if the directory does not exist it will crash
    std::filesystem::create_directories("logs");
    std::string file_name = "logs/" + db_name + ".wal";
    
    wal = std::make_unique<WAL>(file_name);

    for(size_t i = 0; i < shard_count; i++){
        shards.push_back(std::make_unique<Shard>());
    }

    wal->recover([this, &file_name](const std::string& k, const std::string& v){
        set_memory_only(k, v);
        std::cout << "Recovered " << k << " " << v << " from " << file_name << "\n";
    });
}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}

void Engine::set(const std::string & key, const std::string & value){
    size_t shard_idx = get_shard_idx(key);

    //since we cannot copy a unique ptr we have to get it as a reference
    auto & current_shard = shards[shard_idx];

    std::lock_guard<std::mutex> lock(current_shard->mtx);
    wal->append_set(key, value);
    current_shard->map[key] = value;
}

void Engine::set_memory_only(const std::string& key, const std::string& value){
    size_t shard_idx = get_shard_idx(key);

    //since we cannot copy a unique ptr we have to get it as a reference
    auto & current_shard = shards[shard_idx];

    std::lock_guard<std::mutex> lock(current_shard->mtx);
    current_shard->map[key] = value;
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

size_t Engine::size() const {

    //If a thread calls size while another writes we will have undefined behaviour. So we need to lock.
    size_t total = 0;
    for (size_t i = 0; i < shard_count; ++i) {
        std::lock_guard<std::mutex> lock(shards[i]->mtx);
        total += shards[i]->map.size();
    }
    return total;

}

size_t Engine::get_shard_idx(std::string key){
    size_t hash = std::hash<std::string>{}(key);
    return hash % shard_count;
}