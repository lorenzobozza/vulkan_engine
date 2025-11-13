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
    
    void setWindowExtent(int Width, int Height) { m_windowExtent.width = Width; m_windowExtent.height = Height; }
    void setWindowFullScreen(uint32_t flags, const SDL_DisplayMode& displayMode);
    
    VkExtent2D getExtent() const { return m_windowExtent; }
    VkExtent2D getSurfaceExtent() const { return m_surfaceExtent; }
    VkExtent2D getDesktopExtent() const { return {(uint32_t)desktopMode.w, (uint32_t)desktopMode.h}; }
    SDL_Window *getWindow() const { return window; }
    uint8_t getMovement(void) { return movement; }
    glm::vec3 getRotation(void) { glm::vec3 tmp = rotate; rotate = {}; return tmp; }
    bool isWindowOpen(void) { return keepRuning; }
    void updateUiScaling(void);
    
    void pollWindowEvents(std::function<void()> callback);
    void closeWindow(void) { keepRuning = false; }
    
    std::string openFileDialog(std::string folder);
    
    
    std::string supportedResNames;
    std::vector<SDL_DisplayMode> supportedModes;
    SDL_DisplayMode desktopMode;
    
private:
    void initWindow();
    
    VkExtent2D m_windowExtent;
    VkExtent2D m_surfaceExtent;
    
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
