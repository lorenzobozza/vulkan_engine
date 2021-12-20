//
//  main.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/11/21.
//

#include "include/Application.hpp"

//std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, const char * argv[]) {
    
    Application app{argv[0]};
    
    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

