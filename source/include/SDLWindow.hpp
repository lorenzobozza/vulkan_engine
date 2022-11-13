//
//  SDLWindow.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#ifndef SDLWindow_hpp
#define SDLWindow_hpp

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include  <SDL2/SDL_vulkan.h>

// std
#include <string>

class SDLWindow {
public:
    SDLWindow(int w, int h, std::string name);
    SDLWindow(std::string name);
    ~SDLWindow();
    
    // Prevent Obj copy
    SDLWindow(const SDLWindow &) = delete;
    SDLWindow &operator=(const SDLWindow &) = delete;
    
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
    
    VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    SDL_Window *getWindow() const { return window; }
    
private:
    void initWindow();
    
    int width;
    int height;
    
    bool fullScreen = false;
    
    std::string windowName;
    SDL_Window* window;
};

#endif /* SDLWindow_hpp */

/*
bool wasWindowResized() { return frameBufferResized; }
void resetWindowResizeFlag() { frameBufferResized = false; }
bool frameBufferResized = false;
    
GLFWmonitor* monitor;
bool fullScreen = false;
 */
