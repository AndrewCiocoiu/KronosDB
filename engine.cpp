#include "engine.hpp"
#include <iostream>

//By initializing before the body we avoid having to delete the default value to assign a new value
Engine::Engine(std::string const & name) : db_name(name) {}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}