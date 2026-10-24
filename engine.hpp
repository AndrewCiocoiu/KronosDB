//This makes sure that we don't have duplicate imports
#pragma once

#include <string>

class Engine{
public:
    Engine(std::string const & name);

    void start();
private:
    std::string db_name;
};