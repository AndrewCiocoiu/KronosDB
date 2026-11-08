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
    Engine(std::string const & name, size_t num_shards, bool should_log = true);
    ~Engine();

    void start();

    //Optional is basically str || null in TS. It can return one of two things
    std::optional<std::string> get(const std::string& key);

    void set(const std::string& key, const std::string& value);

    void del(const std::string &key);

    void create_checkpoint();

    bool is_logging();

    size_t size() const;
private:
    std::string db_name;

    void set_memory_only(const std::string& key, const std::string& value);

    void del_memory_only(const std::string &key);

    std::unique_ptr<WAL> wal;

    struct Shard{
        mutable std::mutex mtx; //"mutable" allows locking inside const
        std::unordered_map<std::string, std::string> map;
    };
    size_t shard_count;
    //mutexes cannot be copied or moved in memory, they are tethered, thats why we have to use pointer
    std::vector<std::unique_ptr<Shard>> shards;
    
    size_t get_shard_idx(const std::string& key) const;

    bool logging;

    //We use these to trigger the snapshot, atomics are extremely efficient hardware level stores tgat also prevent data races, static because it is shared
    //with all of the class instances and doesnt change at runtime, ensuring zero runtime memory because contexpr forces eval at compile time
    static constexpr size_t CHECKPOINT_THRESHOLD = 50'000;
    std::atomic<size_t> ops_since_checkpoint{0};

};