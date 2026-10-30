//This makes sure that we don't have duplicate imports
#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include <wal.hpp>

class Engine{
public:
    Engine(std::string const & name, size_t num_shards);

    void start();

    //Optional is basically str || null in TS. It can return one of two things
    std::optional<std::string> get(const std::string& key);

    void set(const std::string& key, const std::string& value);

    size_t size() const;
private:
    std::string db_name;

    void set_memory_only(const std::string& key, const std::string& value);

    std::unique_ptr<WAL> wal;

    struct Shard{
        mutable std::mutex mtx; //"mutable" allows locking inside const
        std::unordered_map<std::string, std::string> map;
    };
    size_t shard_count;
    //mutexes cannot be copied or moved in memory, they are tethered, thats why we have to use pointer
    std::vector<std::unique_ptr<Shard>> shards;
    
    size_t get_shard_idx(const std::string& key) const;

};