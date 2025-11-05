//
//  Widgets.hpp
//  
//
//  Created by Lorenzo Bozza on 03/11/25.
//

#ifndef Widgets_h
#define Widgets_h

#include "UI.hpp"
#include "Log.hpp"
#include "utils.h"

#include <imgui_internal.h>

class Viewport : public Widget {
public:
    void addFlags(unsigned int flags) { m_flags |= flags; }
    void setExtent(float width, float height) { m_extent = ImVec2(width, height); }

private:
    void content(void) override {
        ImGui::SetNextWindowContentSize(m_extent);
        ImGui::Begin("Viewport", nullptr, m_flags | ImGuiWindowFlags_NoCollapse);
        ImGuiDockNode* id = ImGui::GetWindowDockNode();
        id->LocalFlags |= ImGuiDockNodeFlags_NoResize;
        
        ImGui::Image( (void*)(intptr_t) 1, m_extent);

        ImGui::End();
    }

    unsigned int m_flags = 0;
    ImVec2 m_extent{};
};

class LogView : public Widget {
    void header(void) override {
        if (Log::getInstance()->notifyErrors()) {
            ImGuiWindow* log = ImGui::FindWindowByName("Log Console");
            if (log != nullptr) ImGui::SetScrollY(log, 1000.f);
            getVisibility() = true;
        }
    }
    void content(void) override {
        ImGui::Begin("Log Console", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", Log::getInstance()->getBuffer());
        ImGui::End();
    }
};

class Settings : public Widget {
public:
    Settings(Device& device, SDLWindow& window, Renderer& renderer, Perf& perf)
        : m_device(device), m_window(window), m_renderer(renderer), m_Perf(perf) {
        aaPresets.resize(1 + ctz(m_device.maxSampleCount));
    }
        
    void recreatePipelinesCallback(std::function<void()> fn) { recreatePipelines = fn; }
    
    struct {
        float exposure = 1.5f;
        float peak_brightness = 2.f;
        float gamma = 2.2f;
        glm::vec4 light{1.f};
        glm::vec3 lightPos{0.f};
    } data;
    
private:
    Device& m_device;
    SDLWindow& m_window;
    Renderer& m_renderer;
    Perf& m_Perf;
    
    std::vector<const char*> aaPresets = {"No AA", "MSAA 2X", "MSAA 4X", "MSAA 8X", "MSAA 16X"};
    
    std::function<void()> recreatePipelines;
    void content(void) override {
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse);
    
        ImGui::Text("CPU Time %.2fms", m_Perf.cpuTime * 1000.f);
        ImGui::Text("GPU Time %.2fms", m_Perf.gpuTime * 1000.f);
        
        ImGui::NewLine();
        static bool info = false;
        if (ImGui::Button("Display Info")) info ^= true;
        if (info) {
            ImGui::Text("Window:  %ix%i", m_window.getExtent().width, m_window.getExtent().height);
            ImGui::Text("Desktop: %ix%i", m_window.getDesktopExtent().width, m_window.getDesktopExtent().height);
            ImGui::Text("Surface:  %ix%i", m_window.getSurfaceExtent().width, m_window.getSurfaceExtent().height);
        }
        static int windowMode = 0, res = 0;
        bool setNewMode = false;
        setNewMode = ImGui::Combo("##fullscreen", &windowMode, "Windowed\0Windowed Borderless\0Full Screen\0");
        setNewMode = setNewMode || ImGui::Combo("##resolution", &res, m_window.supportedResNames.c_str());
        if (setNewMode) {
            switch (windowMode) {
                case 0:
                    m_window.setWindowFullScreen(0, m_window.supportedModes[res]);
                    break;
                case 1:
                    m_window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN_DESKTOP, m_window.supportedModes[res]);
                    break;
                case 2:
                    m_window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN, m_window.supportedModes[res]);
                    break;
            }
        }
        
        ImGui::NewLine();
        if (ImGui::Button("Refresh pipelines")) {
            vkDeviceWaitIdle(m_device.device());
            recreatePipelines();
        }
        ImGui::SameLine();
        static bool vsync = SwapChain::enableVSync;
        ImGui::Checkbox(vsync ? "VSync Enabled" : "VSync Disabled", &vsync);
        if (SwapChain::enableVSync != vsync) {
            SwapChain::enableVSync = vsync;
            m_renderer.recreateSwapChain();
        }
        
        ImGui::NewLine();
        static int aaIndex = ctz(m_device.msaaSamples);
        ImGui::Text("Anti-Aliasing");
        if (ImGui::Combo("##antialiasing", &aaIndex, aaPresets.data(), (int)aaPresets.size())) {
            m_device.msaaSamples = static_cast<VkSampleCountFlagBits>(1 << aaIndex);
            m_renderer.recreateSwapChain(true);
            recreatePipelines();
        }
        
        ImGui::NewLine();
        ImGui::Text("Exposure");
        ImGui::SliderFloat("##exposure", &data.exposure, 1.f, 5.f);
        ImGui::Text("Peak White Brightness");
        ImGui::SliderFloat("##brightness", &data.peak_brightness, 1.f, 15.f);
        ImGui::Text("Gamma Correction");
        ImGui::SliderFloat("##gamma", &data.gamma, 1.f, 3.f);
        
        ImGui::NewLine();
        float color[4] = {data.light.r, data.light.g, data.light.b, data.light.a};
        ImGui::ColorEdit3("Light Color", color);
        ImGui::SliderFloat("##strength", &color[3], 1.f, 100.f);
        data.light = {color[0], color[1], color[2], color[3]};
        
        ImGui::SliderFloat("LPosX", &data.lightPos.x, -3.f, 3.f);
        ImGui::SliderFloat("LPosY", &data.lightPos.y, -.5f, -5.f);
        ImGui::SliderFloat("LPosZ", &data.lightPos.z, -4.f, 4.f);

        ImGui::End();
    }
};

class AssetTree : public Widget {
    void content(void) override {
        UI::treeAssetsWidget();
    }
};

class Menu : public Widget {
public:
    Menu(bool& logV, bool& matV) : m_logV(logV), m_materialV(matV) {}
    
private:
    void content(void) override {
        ImGui::BeginMainMenuBar();
        
        if (ImGui::Button("Log")) {
            m_logV ^= true;
        }
        if (ImGui::Button("Materials")) {
            m_materialV ^= true;
        }
        
        ImGui::EndMainMenuBar();
    }
    bool& m_logV;
    bool& m_materialV;
};

class MeterialViewer : public Widget {
public:
    MeterialViewer(std::unordered_map<std::string, Material>& materials) : m_materials(materials) {}
private:
    std::unordered_map<std::string, Material>& m_materials;
    void content(void) override {
        ImGui::Begin("Materials", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTable("material_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Name");
            ImGui::TableNextColumn(); ImGui::Text("Color");
            ImGui::TableNextColumn(); ImGui::Text("Normal");
            ImGui::TableNextColumn(); ImGui::Text("Occlusion");
            ImGui::TableNextColumn(); ImGui::Text("Metal/Rough");
            for (auto& kv: m_materials) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", kv.first.c_str());
                ImGui::TableNextColumn();
                if ((kv.second.getTextureBitmap() & 0x1) == 0) {ImGui::ColorButton("", ImVec4(kv.second.color.r,kv.second.color.g,kv.second.color.b,kv.second.color.a));}
                else { ImGui::Text("%s","Texture"); }
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x2) == 0 ? "NO" : "Texture");
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x4) == 0 ? "NO" : "Texture");
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x8) == 0 ? ( "M: " + std::to_string(kv.second.metalness) + ", R: " + std::to_string(kv.second.roughness) ).c_str() : "Texture");
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
};

#endif /* Widgets_h */
