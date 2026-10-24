#include <iostream>
#include "engine.hpp"

int main(){
    
    std::cout << "Initializing project\n";

    Engine db("Kronos-Core");
    db.start();

    db.set("user:101", "Andrei");

    auto res1 = db.get("user:101");
    //Has value on an optional will check if that optional is not null
    if(!res1.has_value()){
        std::cout << "Key user:101 was not found!\n";
    }

    auto res2 = db.get("user:102");
    if(!res2.has_value()){
        std::cout << "Key user:102 was not found!\n";
    }
    return 0;
}