//
//  SDLWindow.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#ifndef SDLWindow_hpp
#define SDLWindow_hpp

#include <vulkan/vulkan.h>
#include <glm.hpp>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_image.h>

// std
#include <vector>
#include <string>
#include <functional>

class SDLWindow {
public:
    SDLWindow(int w, int h, std::string name);
    SDLWindow(std::string name);
    ~SDLWindow();
    
    // Prevent Obj copy
    SDLWindow(const SDLWindow &) = delete;
    SDLWindow &operator=(const SDLWindow &) = delete;
    
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
    
    void setWindowExtent(int Width, int Height) { width = Width; height = Height; }
    void setWindowFullScreen(uint32_t flags);
    
    VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    SDL_Window *getWindow() const { return window; }
    uint8_t getMovement(void) { return movement; }
    glm::vec3 getRotation(void) { return rotate; }
    bool isWindowOpen(void) { return keepRuning; }
    
    void pollWindowEvents(std::function<void()> callback);
    void closeWindow(void) { keepRuning = false; }
    
    std::string openFileDialog(std::string folder);
    
    
    std::string supportedResNames;
    std::vector<SDL_DisplayMode> supportedModes;
    
private:
    void initWindow();
    
    int width;
    int height;
    
    bool fullScreen = false;
    
    std::string windowName;
    SDL_Window* window;
    
    float dpi_scale_fact{1.f};
    bool keepRuning = true;
    
    // Inputs
    uint8_t movement{0x00};
    glm::vec3 rotate{.0f};
};

#endif /* SDLWindow_hpp */
