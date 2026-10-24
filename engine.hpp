//This makes sure that we don't have duplicate imports
#pragma once

#include <string>
#include <optional>
#include <unordered_map>

class Engine{
public:
    Engine(std::string const & name);

    void start();

    //Optional is basically str || null in TS. It can return one of two things
    std::optional<std::string> get(const std::string& key);

    void set(const std::string& key, const std::string& value);
private:
    std::string db_name;
    std::unordered_map<std::string, std::string> storage;
};