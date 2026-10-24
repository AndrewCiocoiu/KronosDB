#include <iostream>
#include "engine.hpp"

int main(){
    
    std::cout << "Initializing project\n";

    Engine db("Kronos-Core");
    db.start();

    return 0;
}