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
#include <nfd.h>
#include <nfd_sdl2.h>

#include <stdexcept>

SDLWindow::SDLWindow(int w, int h, std::string name) : m_WindowName{name} {
    m_windowExtent.width = (uint32_t)w;
    m_windowExtent.height = (uint32_t)h;
    initWindow();
}

SDLWindow::~SDLWindow() {
    NFD_Quit();
    
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

void SDLWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if(SDL_Vulkan_CreateSurface(m_Window, instance, surface) != SDL_TRUE) {
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
    
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, texWidth, texHeight, 8, 1024, SDL_PIXELFORMAT_ABGR8888);
    SDL_SetWindowIcon(window, surface);
    SDL_FreeSurface(surface);
    
    stbi_image_free(pixels);
}

void SDLWindow::initWindow(void) {
    SDL_Init(SDL_INIT_EVERYTHING);
    
    int supportedResCount = SDL_GetNumDisplayModes(0);
    supportedModes.resize(supportedResCount);
    for (int mode = 0; mode < supportedResCount; mode++) {
        SDL_GetDisplayMode(0, mode, &supportedModes[mode]);
        supportedResNames += std::to_string(supportedModes[mode].w) + " x " + std::to_string(supportedModes[mode].h);
        supportedResNames += " " + std::to_string(supportedModes[mode].refresh_rate) + "Hz";
        supportedResNames += SDL_PIXELLAYOUT(supportedModes[mode].format) == SDL_PACKEDLAYOUT_8888 ? " 8bit" : " 10bit";
        supportedResNames += '\0';
    }
    
    SDL_GetDesktopDisplayMode(0, &m_DesktopMode);
    m_windowExtent.width = (int)(m_DesktopMode.w * 0.9f);
    m_windowExtent.height = (int)(m_DesktopMode.h * 0.9f);
    
    m_Window = SDL_CreateWindow(m_WindowName.c_str(),
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                m_windowExtent.width, m_windowExtent.height,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    
    SDL_Vulkan_GetDrawableSize(m_Window, reinterpret_cast<int*>(&m_surfaceExtent.width), reinterpret_cast<int*>(&m_surfaceExtent.height));
    
    m_DpiScaling = (float)m_surfaceExtent.width / (float)m_windowExtent.width;
    
    mySDL_SetWindowIcon(m_Window, "../../../assets/icon.png");
    
    NFD_Init();
}

void SDLWindow::setWindowFullScreen(uint32_t flags, const SDL_DisplayMode& displayMode) {
    switch (flags) {
    case 0:
        SDL_SetWindowFullscreen(m_Window, 0);
        SDL_SetWindowSize(m_Window, (int)(m_DesktopMode.w * 0.9f), (int)(m_DesktopMode.h * 0.9f));
        break;
    case SDL_WINDOW_FULLSCREEN_DESKTOP:
        SDL_SetWindowDisplayMode(m_Window, &displayMode);
        SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        break;
    case SDL_WINDOW_FULLSCREEN:
        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) {
            SDL_SetWindowFullscreen(m_Window, 0);
        }
        SDL_SetWindowDisplayMode(m_Window, &displayMode);
        SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN);
        break;
    default:
        break;
    }
}

void SDLWindow::updateUiScaling(void) {
    m_DpiScaling = (float)m_surfaceExtent.width / (float)m_windowExtent.width;
    
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
        case SDL_WINDOWEVENT:
            if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED && (m_windowExtent.width != winData1 || m_windowExtent.height != winData2)) {
                m_windowExtent.width = winData1;
                m_windowExtent.height = winData2;
                SDL_Vulkan_GetDrawableSize(m_Window, reinterpret_cast<int32_t*>(&m_surfaceExtent.width), reinterpret_cast<int32_t*>(&m_surfaceExtent.height));
                callback(); // Recreate swapchain
                updateUiScaling();
            }
            break;
        case SDL_QUIT:
            closeWindow();
            break;
        case SDL_KEYDOWN:
            fetchShortcuts((SDL_Keymod)sdl_event.key.keysym.mod, (SDL_KeyCode)sdl_event.key.keysym.sym);
            imgui_key = UI::ImGui_SDL2_KeyEventToImGuiKey(sdl_event.key.keysym.sym);
            io.AddKeyEvent(imgui_key, true);
            if (io.WantTextInput && UI::ImGuiKey_to_Charecter(imgui_key, ImGui::IsKeyDown(ImGuiKey_LeftShift)) != '?') {
                io.AddInputCharacter(UI::ImGuiKey_to_Charecter(imgui_key, ImGui::IsKeyDown(ImGuiKey_LeftShift)));
                
            }
            switch (sdl_event.key.keysym.sym) {
            case SDLK_ESCAPE:
                closeWindow();
                break;
            case SDLK_w:
                m_Movement |= 0x01;
                break;
            case SDLK_a:
                m_Movement |= 0x02;
                break;
            case SDLK_s:
                m_Movement |= 0x04;
                break;
            case SDLK_d:
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
        case SDL_KEYUP:
            imgui_key = UI::ImGui_SDL2_KeyEventToImGuiKey(sdl_event.key.keysym.sym);
            io.AddKeyEvent(imgui_key, false);
            switch (sdl_event.key.keysym.sym) {
            case SDLK_w:
                m_Movement &= 0xFE;
                break;
            case SDLK_a:
                m_Movement &= 0xFD;
                break;
            case SDLK_s:
                m_Movement &= 0xFB;
                break;
            case SDLK_d:
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
        case SDL_CONTROLLERDEVICEADDED:
            SDL_GameControllerOpen(0);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            SDL_GameControllerClose(0);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            closeWindow();
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (sdl_event.button.button == 1)
                io.MouseDown[0] = sdl_event.button.state;
            else
                mouseRight = true;
            break;
        case SDL_MOUSEBUTTONUP:
            if (sdl_event.button.button == 1)
                io.MouseDown[0] = sdl_event.button.state;
            else
                mouseRight = false;
            break;
        case SDL_MOUSEMOTION:
            int wx, wy, mx, my;
            SDL_GetWindowPosition(getWindow(), &wx, &wy);
            SDL_GetGlobalMouseState(&mx, &my);
            io.AddMousePosEvent((mx - wx) * m_DpiScaling, (my - wy) * m_DpiScaling);
            if (mouseRight) {
                m_Rotate.x = .05f*sdl_event.motion.yrel;
                m_Rotate.y = -.05f*sdl_event.motion.xrel;
            }
            break;
        case SDL_MOUSEWHEEL:
            io.AddMouseWheelEvent(sdl_event.wheel.preciseX, sdl_event.wheel.preciseY);
            m_Rotate.x = .5f*sdl_event.wheel.preciseY;
            m_Rotate.y = .5f*sdl_event.wheel.preciseX;
            break;
        }
    }
}

void SDLWindow::fetchShortcuts(SDL_Keymod modifier, SDL_KeyCode key) {
    switch (key) {
    case SDLK_f:
        if ((modifier & (KMOD_CTRL | KMOD_GUI)) > 0) m_LastShortcut = CTRL_F;
        break;
        
    case SDLK_d:
        if ((modifier & (KMOD_CTRL | KMOD_GUI)) > 0) m_LastShortcut = CTRL_D;
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

std::string SDLWindow::openFileDialog(std::string folder) {
    nfdu8filteritem_t filters[2] = { { "Source code", "c,cpp,cc" }, { "Headers", "h,hpp" } };
    nfdopendialogu8args_t args = {0};
    NFD_GetNativeWindowFromSDLWindow(getWindow() , &args.parentWindow);
    args.filterList = filters;
    args.filterCount = 2;
    args.defaultPath = folder.c_str();
    
    std::string returnString = "none";
    nfdu8char_t *outPath;
    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
    if (result == NFD_OKAY) {
        returnString = outPath;
        NFD_FreePathU8(outPath);
    } else if (result == NFD_CANCEL) {
        //puts("User pressed cancel.");
    } else {
        //printf("Error: %s\n", NFD_GetError());
    }
    
    return returnString;
}
