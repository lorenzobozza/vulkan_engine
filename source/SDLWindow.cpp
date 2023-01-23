//
//  SDLWindow.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#include "include/SDLWindow.hpp"

#include <stdexcept>
#include <iostream>

SDLWindow::SDLWindow(int w, int h, std::string name) :  width{w}, height{h}, windowName{name} {
    initWindow();
}

SDLWindow::SDLWindow(std::string name) : windowName{name}, fullScreen{true} {
    initWindow();
}

SDLWindow::~SDLWindow() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void SDLWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if(SDL_Vulkan_CreateSurface(window, instance, surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void SDLWindow::initWindow() {
    SDL_Init(SDL_INIT_EVERYTHING);
    
    window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
    SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);// | SDL_WINDOW_FULLSCREEN);
    
    SDL_GetWindowSize(window, &width, &height);
    
    SDL_Surface *surface = IMG_Load("3d.png");
    SDL_SetWindowIcon(window, surface);
    SDL_FreeSurface(surface);
}
