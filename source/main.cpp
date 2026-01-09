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
    Application app;
    
    #ifndef DEBUG
    try {
    #endif
        app.run();
    #ifndef DEBUG
    } catch (const std::exception &e) {
        std::println("[CRITICAL] Unhandled Exception: {}", e.what());
        return EXIT_FAILURE;
    }
    #endif

    return EXIT_SUCCESS;
}

