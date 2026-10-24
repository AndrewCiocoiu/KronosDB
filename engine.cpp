#include "engine.hpp"
#include <iostream>

Engine::Engine(std::string name){
    db_name = name;
}

void Engine::start(){
    std::cout << "Storage engine [" << db_name << "] initialized and ready.\n";
}