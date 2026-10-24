#include "engine.hpp"
#include <cstddef>
#include <iostream>
#include <mutex>

//By initializing before the body we avoid having to delete the default value to assign a new value
Engine::Engine(std::string const & name) : db_name(name) {}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}

void Engine::set(const std::string & key, const std::string & value){
    std::lock_guard<std::mutex> lock(db_mutex);
    storage[key] = value;
}

std::optional<std::string> Engine::get(const std::string & key){
    std::lock_guard<std::mutex> lock(db_mutex);

    auto it = storage.find(key);

    //first will be the key and second will be the value
    if (it != storage.end()){
        return it->second;
    }

    return std::nullopt;
}

size_t Engine::size() const {
    return storage.size();
}