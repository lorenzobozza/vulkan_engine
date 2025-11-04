//
//  SDLWindow.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 04/10/22.
//

#include "include/SDLWindow.hpp"
#include "include/UI.hpp"

#include <stdexcept>
#include <iostream>

#include <imgui.h>

#include <nfd.h>
#include <nfd_sdl2.h>

SDLWindow::SDLWindow(int w, int h, std::string name) :  width{w}, height{h}, windowName{name} {
    initWindow();
}

SDLWindow::SDLWindow(std::string name) : windowName{name}, fullScreen{true} {
    initWindow();
}

SDLWindow::~SDLWindow() {
    NFD_Quit();
    
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
    
    NFD_Init();
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

void SDLWindow::pollWindowEvents(std::function<void()> callback) {
    static SDL_Event sdl_event;
    ImGuiIO& io = ImGui::GetIO();
    int surfaceWidth, surfaceHeight, windowWidth, windowHeight;
    
    ImGuiKey imgui_key;
    static bool mouseLeft = false;
    
    while(SDL_PollEvent(&sdl_event))
    {
        switch (sdl_event.type) {
            case SDL_WINDOWEVENT:
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_Vulkan_GetDrawableSize(getWindow(), &surfaceWidth, &surfaceHeight);
                    SDL_GetWindowSize(getWindow(), &windowWidth, &windowHeight);
                    callback(); // Rebuild swapchain
                    dpi_scale_fact = (float)surfaceWidth / (float)windowWidth;
                    io.DisplaySize = {(float)surfaceWidth, (float)surfaceHeight};
                    io.FontGlobalScale = dpi_scale_fact * (windowWidth / 1920.f);
                }
                break;
            case SDL_QUIT:
                closeWindow();
                break;
            case SDL_KEYDOWN:
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
                        movement |= 0x01;
                        break;
                    case SDLK_a:
                        movement |= 0x02;
                        break;
                    case SDLK_s:
                        movement |= 0x04;
                        break;
                    case SDLK_d:
                        movement |= 0x08;
                        break;
                    case SDLK_LSHIFT:
                        movement |= 0x10;
                        break;
                    case SDLK_SPACE:
                        movement |= 0x20;
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
                        movement &= 0xFE;
                        break;
                    case SDLK_a:
                        movement &= 0xFD;
                        break;
                    case SDLK_s:
                        movement &= 0xFB;
                        break;
                    case SDLK_d:
                        movement &= 0xF7;
                        break;
                    case SDLK_LSHIFT:
                        movement &= 0xEF;
                        break;
                    case SDLK_SPACE:
                        movement &= 0xDF;
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
                io.MouseDown[0] = sdl_event.button.state;
                mouseLeft = true;
                break;
            case SDL_MOUSEBUTTONUP:
                io.MouseDown[0] = sdl_event.button.state;
                mouseLeft = false;
                break;
            case SDL_MOUSEMOTION:
                int wx, wy, mx, my;
                SDL_GetWindowPosition(getWindow(), &wx, &wy);
                SDL_GetGlobalMouseState(&mx, &my);
                io.AddMousePosEvent((mx - wx) * dpi_scale_fact, (my - wy) * dpi_scale_fact);
                if (mouseLeft && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
                    rotate.x = .05f*sdl_event.motion.yrel;
                    rotate.y = -.05f*sdl_event.motion.xrel;
                }
                break;
            case SDL_MOUSEWHEEL:
                io.AddMouseWheelEvent(sdl_event.wheel.preciseX, sdl_event.wheel.preciseX);
                break;
        }
    }
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
    if (result == NFD_OKAY)
    {
        returnString = outPath;
        NFD_FreePathU8(outPath);
    }
    else if (result == NFD_CANCEL)
    {
        //puts("User pressed cancel.");
    }
    else
    {
        //printf("Error: %s\n", NFD_GetError());
    }
    
    return returnString;
}
