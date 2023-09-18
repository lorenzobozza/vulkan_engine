//
//  SDLWindow.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#include "include/SDLWindow.hpp"

#include <stdexcept>
#include <iostream>
//#include <cstring>

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
    
    int supportedResCount = SDL_GetNumDisplayModes(0);
    supportedModes.resize(supportedResCount);
    for (int mode = 0; mode < supportedResCount; mode++) {
        SDL_GetDisplayMode(0, mode, &supportedModes[mode]);
        supportedResNames += (std::to_string(supportedModes[mode].w) + " x " + std::to_string(supportedModes[mode].h) + '\0');
    }
    
    window = SDL_CreateWindow(
        windowName.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    
    SDL_Surface *surface = IMG_Load("3d.png");
    SDL_SetWindowIcon(window, surface);
    SDL_FreeSurface(surface);
}

void SDLWindow::setWindowFullScreen(uint32_t flags) {
    SDL_DisplayMode displayMode = {
        SDL_PIXELFORMAT_ARGB8888,   // Pixel format
        width,                      // Width
        height,                     // Height
        60,                         // Refresh rate
        nullptr                     // Driver data
    };
    switch (flags) {
        case 0:
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, width, height);
            break;
        case SDL_WINDOW_FULLSCREEN_DESKTOP:
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case SDL_WINDOW_FULLSCREEN:
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) { SDL_SetWindowFullscreen(window, 0); }
        SDL_SetWindowDisplayMode(window, &displayMode);
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
            break;
        default:
            break;
    }
}
