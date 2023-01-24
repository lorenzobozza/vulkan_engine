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
    SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);// | SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    SDL_Surface *surface = IMG_Load("3d.png");
    SDL_SetWindowIcon(window, surface);
    SDL_FreeSurface(surface);
}

/*
void Window::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    monitor = glfwGetPrimaryMonitor();
    if(fullScreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
    }
    window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
}

void Window::frameBufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto thisWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    thisWindow->frameBufferResized = true;
    thisWindow->width = width;
    thisWindow->height = height;
}
 */
