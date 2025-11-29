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

#include <vector>
#include <string>
#include <functional>

enum Shortcut {
    UNDEFINED_SHORTCUT = 0,
    CTRL_F,
    CTRL_D
};

class SDLWindow {
public:
    SDLWindow(const SDLWindow&) = delete;
    SDLWindow& operator=(const SDLWindow&) = delete;
    SDLWindow(SDLWindow&&) = delete;
    SDLWindow& operator=(SDLWindow&&) = delete;
    
    SDLWindow(int w, int h, std::string name);
    ~SDLWindow();
    
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
    
    VkExtent2D getExtent(void) const { return m_windowExtent; }
    VkExtent2D getSurfaceExtent(void) const { return m_surfaceExtent; }
    VkExtent2D getDesktopExtent(void) const { return {(uint32_t)m_DesktopMode.w, (uint32_t)m_DesktopMode.h}; }
    SDL_Window *getWindow(void) const { return m_Window; }
    uint8_t getMovement(void) const { return m_Movement; }
    bool isWindowOpen(void) const { return m_IsRunning; }
    glm::vec3 getRotation(void) { glm::vec3 tmp = m_Rotate; m_Rotate = {}; return tmp; }
    Shortcut getShortcut(void);
    
    void updateUiScaling(void);
    void pollWindowEvents(std::function<void()> callback);
    void closeWindow(void) { m_IsRunning = false; }
    void setWindowExtent(int Width, int Height) { m_windowExtent.width = Width; m_windowExtent.height = Height; }
    void setWindowFullScreen(uint32_t flags, const SDL_DisplayMode& displayMode);
    std::string openFileDialog(std::string folder);
    
    std::string supportedResNames;
    std::vector<SDL_DisplayMode> supportedModes;
    
private:
    void initWindow(void);
    void fetchShortcuts(SDL_Keymod, SDL_KeyCode);
    
    VkExtent2D m_windowExtent;
    VkExtent2D m_surfaceExtent;
    Shortcut m_LastShortcut = UNDEFINED_SHORTCUT;
    
    std::string m_WindowName;
    SDL_Window* m_Window;
    SDL_DisplayMode m_DesktopMode;
    
    float m_DpiScaling{1.f};
    bool m_IsRunning = true;
    
    uint8_t m_Movement{0x00};
    glm::vec3 m_Rotate{.0f};
};

#endif /* SDLWindow_hpp */
