//
//  Window.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#include "include/Window.hpp"

#include <stdexcept>
#include <iostream>

Window::Window(int w, int h, std::string name) :  width{w}, height{h}, windowName{name} {
    initWindow();
}

Window::Window(std::string name) : windowName{name}, fullScreen{true} {
    initWindow();
}

Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if(glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

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
