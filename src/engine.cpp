#include "engine.hpp"
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

//By initializing before the body we avoid having to delete the default value to assign a new value
Engine::Engine(std::string const & name, size_t shard_count) : db_name(name), shard_count(shard_count) {
    for(size_t i = 0; i < shard_count; i++){
        shards.push_back(std::make_unique<Shard>());
    }
}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}

void Engine::set(const std::string & key, const std::string & value){
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
    size_t size = 0;
    for(size_t i = 0; i < shard_count; i++){
        size += shards[i]->map.size();
    }
    return size;
}

size_t Engine::get_shard_idx(std::string key){
    size_t hash = std::hash<std::string>{}(key);
    return hash % shard_count;
}