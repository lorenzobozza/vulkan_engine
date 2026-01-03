//
//  main.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/11/21.
//

#include "include/Application.hpp"

//std
#include <cstdlib>
#include <stdexcept>
#include <print>

int main(int argc, const char * argv[]) {
    
    Application app{};
    
    try {
        app.run();
    } catch (const std::exception &e) {
        std::println("[CRITICAL] Unhandled Exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

