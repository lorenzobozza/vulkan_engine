//
//  SDLWindow.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#include "SDLWindow.hpp"
#include "UI.hpp"
#include "Log.hpp"

#include <imgui.h>
#include <stb-master/stb_image.h>

#include <stdexcept>

SDLWindow::SDLWindow(int w, int h, std::string name) : m_WindowName{name} {
    m_windowExtent.width = (uint32_t)w;
    m_windowExtent.height = (uint32_t)h;
    initWindow();
}

SDLWindow::~SDLWindow() {
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

void SDLWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if(!SDL_Vulkan_CreateSurface(m_Window, instance, NULL, surface)) {
        throw std::runtime_error("Failed to create window surface");
    }
}

static void mySDL_SetWindowIcon(SDL_Window* window, const char *file) {
    int depth = STBI_rgb_alpha;
    int texWidth, texHeight, texChannels;
    void* pixels;
    
    stbi_uc* data = stbi_load(file, &texWidth, &texHeight, &texChannels, depth);
    pixels = (void*)data;
    
    if (!pixels) {
        return;
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(texWidth, texHeight, SDL_PIXELFORMAT_ABGR8888, pixels, 1024);
    SDL_SetWindowIcon(window, surface);
    SDL_DestroySurface(surface);
    
    stbi_image_free(pixels);
}

void SDLWindow::initWindow(void) {
    //SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD);
    
    int supportedResCount;
    supportedModes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &supportedResCount);
    for (int mode = 0; mode < supportedResCount; mode++) {
        supportedResNames += std::format("{} x {}", supportedModes[mode]->w, supportedModes[mode]->h);
        supportedResNames += std::format(" {:.0f}Hz", supportedModes[mode]->refresh_rate);
        supportedResNames += SDL_PIXELLAYOUT(supportedModes[mode]->format) == SDL_PACKEDLAYOUT_8888 ? " 8bit" : " 10bit";
        supportedResNames += '\0';
    }
    
    const SDL_DisplayMode* displayMode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (displayMode == NULL) {
        throw std::runtime_error("Error retriving display modes");
    }
    m_DesktopExtent.width = static_cast<uint32_t>(displayMode->w);
    m_DesktopExtent.height = static_cast<uint32_t>(displayMode->h);
    m_windowExtent.width = static_cast<uint32_t>(displayMode->w * 0.9f);
    m_windowExtent.height = static_cast<uint32_t>(displayMode->h * 0.9f);
    
    m_Window = SDL_CreateWindow(m_WindowName.c_str(),
                                m_windowExtent.width, m_windowExtent.height,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    
    SDL_GetWindowSizeInPixels(m_Window, reinterpret_cast<int*>(&m_surfaceExtent.width), reinterpret_cast<int*>(&m_surfaceExtent.height));
    
    m_DpiScaling = SDL_GetWindowPixelDensity(m_Window);
    
    mySDL_SetWindowIcon(m_Window, "../../../assets/icon.png");
}

void SDLWindow::setWindowFullScreen(uint32_t flags, const SDL_DisplayMode& displayMode) {
    switch (flags) {
    case 0:
        SDL_SetWindowFullscreen(m_Window, 0);
        SDL_SetWindowSize(m_Window, (int)(m_DesktopExtent.width * 0.9f), (int)(m_DesktopExtent.height * 0.9f));
        break;
    case SDL_WINDOW_BORDERLESS:
        SDL_SetWindowFullscreenMode(m_Window, &displayMode);
        SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_BORDERLESS);
        break;
    case SDL_WINDOW_FULLSCREEN:
        SDL_SetWindowFullscreenMode(m_Window, &displayMode);
        SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN);
        break;
    default:
        break;
    }
}

void SDLWindow::updateUiScaling(void) {
    m_DpiScaling = SDL_GetWindowPixelDensity(m_Window);
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {(float)m_surfaceExtent.width, (float)m_surfaceExtent.height};
    io.FontGlobalScale = m_DpiScaling * m_windowExtent.width * 0.0005f;
}

void SDLWindow::pollWindowEvents(std::function<void()> callback) {
    static SDL_Event sdl_event;
    ImGuiIO& io = ImGui::GetIO();
    
    ImGuiKey imgui_key;
    static bool mouseRight = false;
    
    while(SDL_PollEvent(&sdl_event)) {
        uint32_t winData1 = static_cast<uint32_t>(sdl_event.window.data1);
        uint32_t winData2 = static_cast<uint32_t>(sdl_event.window.data2);
        
        switch (sdl_event.type) {
        case SDL_EVENT_WINDOW_RESIZED:
            if (m_windowExtent.width != winData1 || m_windowExtent.height != winData2) {
                m_windowExtent.width = winData1;
                m_windowExtent.height = winData2;
                SDL_GetWindowSizeInPixels(m_Window, reinterpret_cast<int*>(&m_surfaceExtent.width), reinterpret_cast<int*>(&m_surfaceExtent.height));
                callback(); // Recreate swapchain
                updateUiScaling();
            }
            break;
        case SDL_EVENT_QUIT:
            closeWindow();
            break;
        case SDL_EVENT_KEY_DOWN:
            fetchShortcuts((SDL_Keymod)sdl_event.key.mod, (SDL_Keycode)sdl_event.key.key);
            imgui_key = UI::ImGui_SDL2_KeyEventToImGuiKey(sdl_event.key.key);
            io.AddKeyEvent(imgui_key, true);
            if (io.WantTextInput && UI::ImGuiKey_to_Charecter(imgui_key, ImGui::IsKeyDown(ImGuiKey_LeftShift)) != '?') {
                io.AddInputCharacter(UI::ImGuiKey_to_Charecter(imgui_key, ImGui::IsKeyDown(ImGuiKey_LeftShift)));
                
            }
            switch (sdl_event.key.key) {
            case SDLK_ESCAPE:
            case SDLK_Q:
                closeWindow();
                break;
            case SDLK_W:
                m_Movement |= 0x01;
                break;
            case SDLK_A:
                m_Movement |= 0x02;
                break;
            case SDLK_S:
                m_Movement |= 0x04;
                break;
            case SDLK_D:
                m_Movement |= 0x08;
                break;
            case SDLK_LSHIFT:
                m_Movement |= 0x10;
                break;
            case SDLK_SPACE:
                m_Movement |= 0x20;
                break;
            default:
                break;
            }
            break;
        case SDL_EVENT_KEY_UP:
            imgui_key = UI::ImGui_SDL2_KeyEventToImGuiKey(sdl_event.key.key);
            io.AddKeyEvent(imgui_key, false);
            switch (sdl_event.key.key) {
            case SDLK_W:
                m_Movement &= 0xFE;
                break;
            case SDLK_A:
                m_Movement &= 0xFD;
                break;
            case SDLK_S:
                m_Movement &= 0xFB;
                break;
            case SDLK_D:
                m_Movement &= 0xF7;
                break;
            case SDLK_LSHIFT:
                m_Movement &= 0xEF;
                break;
            case SDLK_SPACE:
                m_Movement &= 0xDF;
                break;
            default:
                break;
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            SDL_OpenGamepad(0);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            SDL_CloseGamepad(0);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            closeWindow();
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (sdl_event.button.button == 1)
                io.MouseDown[0] = sdl_event.button.down;
            else
                mouseRight = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (sdl_event.button.button == 1)
                io.MouseDown[0] = sdl_event.button.down;
            else
                mouseRight = false;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            int wx, wy;
            float mx, my;
            SDL_GetWindowPosition(getWindow(), &wx, &wy);
            SDL_GetGlobalMouseState(&mx, &my);
            io.AddMousePosEvent((mx - wx) * m_DpiScaling, (my - wy) * m_DpiScaling);
            if (mouseRight) {
                m_Rotate.x = .05f*sdl_event.motion.yrel;
                m_Rotate.y = -.05f*sdl_event.motion.xrel;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            io.AddMouseWheelEvent(sdl_event.wheel.x, sdl_event.wheel.y);
            m_Rotate.x = .5f*sdl_event.wheel.y;
            m_Rotate.y = .5f*sdl_event.wheel.x;
            break;
            
        }
    }
}

void SDLWindow::fetchShortcuts(SDL_Keymod modifier, SDL_Keycode key) {
    switch (key) {
    case SDLK_F:
        if ((modifier & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) > 0) m_LastShortcut = CTRL_F;
        break;
        
    case SDLK_D:
        if ((modifier & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) > 0) m_LastShortcut = CTRL_D;
        break;
        
    case SDLK_R:
        if ((modifier & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) > 0) m_LastShortcut = CTRL_R;
        break;
        
    default:
        break;
    }
}

Shortcut SDLWindow::getShortcut(void) {
    if (m_LastShortcut != UNDEFINED_SHORTCUT) {
        Shortcut temp = m_LastShortcut;
        m_LastShortcut = UNDEFINED_SHORTCUT;
        return temp;
    }
    return UNDEFINED_SHORTCUT;
}
