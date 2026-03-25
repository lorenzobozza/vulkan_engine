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
#include "Nodes.hpp"

#include <imgui_internal.h>

static unsigned ctz(int n) {
    unsigned bits = 0, x = n;
    if (x) {
        /* mask the 8 low order bits, add 8 and shift them out if they are all 0 */
        if (!(x & 0x000000FF)) { bits +=  8; x >>=  8; }
        /* mask the 4 low order bits, add 4 and shift them out if they are all 0 */
        if (!(x & 0x0000000F)) { bits +=  4; x >>=  4; }
        /* mask the 2 low order bits, add 2 and shift them out if they are all 0 */
        if (!(x & 0x00000003)) { bits +=  2; x >>=  2; }
        /* mask the low order bit and add 1 if it is 0 */
        bits += (x & 1) ^ 1;
    }
    return bits;
}

class NodeTreeViewer : public Widget {
public:
    NodeTreeViewer(Primitive::Map& primitives, Node::Tree& nodeTree) : m_Primitives(primitives), m_NodeTree(nodeTree) {}
        
private:
    Primitive::Map& m_Primitives;
    Node::Tree& m_NodeTree;
    void content(void) override {
        ImGui::Begin("Node Visualizer");
        if (m_NodeTree.nodes.size() > 1) {
            expandTree(m_NodeTree.nodes[0]);
        } else {
            ImGui::TextUnformatted("Load a model");
        }
        ImGui::End();
    }
    
    std::string printProps(uint8_t flags) {
        std::string props = "";
        props += (flags & Node::Flags::TRANSL) ? "T" : "";
        props += (flags & Node::Flags::SCALE) ? "S" : "";
        props += (flags & Node::Flags::QUAT) ? "R" : "";
        props += (flags & Node::Flags::MATRIX) ? "M" : "";
        props += (flags & Node::Flags::MESH) ? " \x7e" : "";
        props += (flags & Node::Flags::LIGHT) ? "§" : "";
        return props;
    }
    
    void expandTree(Node& parentNode) {
        ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_SpanAllColumns;
        if (parentNode.parent == -1) base_flags |= ImGuiTreeNodeFlags_DefaultOpen;
        for (uint32_t childIndex : parentNode.children) {
            Node& child = m_NodeTree.nodes[childIndex];
            bool hasChildren = child.children.size() > 0;
            ImGuiTreeNodeFlags node_flags = hasChildren ? base_flags : base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            bool isOpen = ImGui::TreeNodeEx((child.name.empty() ? "##empty" : child.name.c_str()), node_flags);
            
            if (ImGui::IsItemHovered()) {
                if (!child.primitives.empty()) m_Primitives.map.at(child.primitives.at(0)).showAABB = true;
                child.aabb = true;
            } else if (child.aabb) {
                if (!child.primitives.empty()) m_Primitives.map.at(child.primitives.at(0)).showAABB = false;
                child.aabb = false;
            }
            
            ImVec2 size = ImGui::GetItemRectSize();
            if (ImGui::IsItemHovered()) {
                ImGui::SameLine(size.x - 250.f);
                ImGui::TextUnformatted(std::format("{:.2f} | {:.2f}, {:.2f}, {:.2f}", child.quat.w, child.quat.x, child.quat.y, child.quat.z).c_str());
            } else {
                ImGui::SameLine(size.x - 50.f);
                ImGui::TextUnformatted(printProps(child.flags).c_str());
            }
            
            if (isOpen && hasChildren) {
                expandTree(child);
                ImGui::TreePop();
            }
        }
    }
};

class Viewport : public Widget {
public:
    void addFlags(unsigned int flags) { m_flags |= flags; }
    void setExtent(float width, float height) { m_extent = ImVec2(width * 0.8f, height * 0.8f); }
    uint8_t loading = 0xFF;
    
private:
    void content(void) override {
        ImGui::SetNextWindowContentSize(m_extent);
        ImGui::Begin("Viewport", nullptr, m_flags | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);
        ImGuiDockNode* id = ImGui::GetWindowDockNode();
        id->LocalFlags |= ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_AutoHideTabBar;
        
        ImGui::BeginMenuBar();
        static int source = 1;
        ImGui::Text(" Viewport");
        ImGui::SameLine(m_extent.x * 0.85f);
        ImGui::Text("Framebuffer: ");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Screen Space").x + 50.f);
        ImGui::Combo("##framecombo", &source, "World Space\0Screen Space\0Shadow\0");
        ImGui::EndMenuBar();
        
        static ImTextureRef ref;
        ref._TexData = NULL;
        ref._TexID = (uint64_t)source;
        ImGui::Image(ref , m_extent);
        
        std::string msg;
        ImVec2 pos;
        switch (loading) {
        case 1:
            msg = "Compiling Shaders...";
            break;
        case 2:
            msg = "Loading Environment...";
            break;
        default:
            break;
        }
        switch (loading) {
        case 1:
        case 2:
        case 3:
            pos = ImVec2(ImGui::GetWindowPos().x + (m_extent.x / 2.f) - 200.f,
                         ImGui::GetWindowPos().y + (m_extent.y / 2.f) + 10.f);
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 50.0, pos, 0xFFDDDDDD, msg.c_str());
            break;
        default:
            break;
        }
        
        ImGui::End();
    }
    
    unsigned int m_flags = 0;
    ImVec2 m_extent{};
};

class LogView : public Widget {
    void header(void) override {
        if (getVisibility() == false && Log::getInstance()->notifyErrors()) {
            getVisibility() = true;
        }
    }
    void content(void) override {
        ImGui::Begin("Log Console", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", Log::getInstance()->viewBuffer());
        ImGui::End();
    }
};

class Settings : public Widget {
public:
    Settings(const Device& device, SDLWindow& window, Renderer& renderer, Perf& perf, VkSampleCountFlagBits& msaaSampleCount)
    : m_device(device), m_window(window), m_renderer(renderer), m_Perf(perf), m_MSAASampleCount(msaaSampleCount) {
        aaPresets.resize(1 + ctz(m_device.getSupportedSmapleCount()));
    }
    
    void recreatePipelinesCallback(std::function<void()> fn) { recreatePipelines = fn; }
    
    struct {
        float exposure = 1.5f;
        float peak_brightness = 2.f;
        float gamma = 2.2f;
        unsigned int debugMode = 0;
    } otherData;
    
    unsigned int debugMode = 0;
    
private:
    const Device& m_device;
    SDLWindow& m_window;
    Renderer& m_renderer;
    Perf& m_Perf;
    VkSampleCountFlagBits& m_MSAASampleCount;
    
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
                m_window.setWindowFullScreen(0, *m_window.supportedModes[res]);
                break;
            case 1:
                m_window.setWindowFullScreen(SDL_WINDOW_BORDERLESS, *m_window.supportedModes[res]);
                break;
            case 2:
                m_window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN, *m_window.supportedModes[res]);
                break;
            }
        }
        
        ImGui::NewLine();
        static int debugLocal = 0;
        static bool iblBg = false, iblLight = false, iblMultiScatter = false;
        ImGui::Text("Shader Control");
        if (ImGui::Combo("##debugMode", &debugLocal, "Shaded\0Albedo\0Normal\0Roughness\0Metallic\0")) {
            debugMode = (debugMode & 0xF00) + debugLocal;
        }
        if (ImGui::Checkbox("Environment Map", &iblBg)) { if(iblBg) { debugMode |= (1 << 8); } else { debugMode &= ~(1 << 8); } }
        if (ImGui::Checkbox("IBL Contribution", &iblLight)) { if(iblLight) { debugMode |= (1 << 9); } else { debugMode &= ~(1 << 9); } }
        if (ImGui::Checkbox("IBL MultiScatter", &iblMultiScatter)) {if(iblMultiScatter) { debugMode |= (1 << 10); } else { debugMode &= ~(1 << 10);} }
        if (ImGui::Button("Compile Shaders")) {
            vkDeviceWaitIdle(m_device.device());
            recreatePipelines();
        }
        
        ImGui::NewLine();
        static int aaIndex = ctz(m_MSAASampleCount);
        ImGui::Text("Anti-Aliasing");
        if (ImGui::Combo("##antialiasing", &aaIndex, aaPresets.data(), (int)aaPresets.size())) {
            m_MSAASampleCount = static_cast<VkSampleCountFlagBits>(1 << aaIndex);
            m_renderer.recreateSwapChain(true);
            recreatePipelines();
        }
        static bool vsync = SwapChain::VSync;
        ImGui::Checkbox(vsync ? "VSync Enabled" : "VSync Disabled", &vsync);
        if (SwapChain::VSync != vsync) {
            SwapChain::VSync = vsync;
            m_renderer.recreateSwapChain();
        }
        
        ImGui::NewLine();
        ImGui::Text("Exposure");
        ImGui::SliderFloat("##exposure", &otherData.exposure, 1.f, 5.f);
        ImGui::Text("Peak White Brightness");
        ImGui::SliderFloat("##brightness", &otherData.peak_brightness, 1.f, 15.f);
        ImGui::Text("Gamma Correction");
        ImGui::SliderFloat("##gamma", &otherData.gamma, 1.f, 3.f);
        static bool noise = false;
        if (ImGui::Checkbox("Film Grain", &noise)) otherData.debugMode = noise ? 1 : 0;
        
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
    MeterialViewer(Assets& assets) : m_Assets(assets) {}
private:
    Assets& m_Assets;
    void content(void) override {
        ImGui::Begin("Materials", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTable("material_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Name");
            ImGui::TableNextColumn(); ImGui::Text("Base Color");
            ImGui::TableNextColumn(); ImGui::Text("Normal Map");
            ImGui::TableNextColumn(); ImGui::Text("Roughness Metallic");
            ImGui::TableNextColumn(); ImGui::Text("AO Texture");
            for (auto& kv: m_Assets.materials) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", kv.first.c_str());
                ImGui::TableNextColumn();
                if ((kv.second.getTextureBitmap() & COLOR_TEXTURE) == 0) {ImGui::ColorButton(std::format("##{}", kv.first).c_str(), ImVec4(kv.second.color.r,kv.second.color.g,kv.second.color.b,kv.second.color.a));}
                else { ImGui::Text("%s","Texture"); }
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & NORMAL_TEXTURE) == 0 ? "NO" : "YES");
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & ROUGH_METAL_TEXTURE) == 0 ? ( "M: " + std::to_string(kv.second.metalness) + ", R: " + std::to_string(kv.second.roughness) ).c_str() : "Texture");
                ImGui::TableNextColumn();
                ImGui::Text("%s", (kv.second.getTextureBitmap() & SPLIT_AO_TEXTURE) == 0 &&
                                (kv.second.getTextureBitmap() & COMBO_ARM_TEXTURE) == 0 ? "NO" : "YES");
            }
            ImGui::EndTable();
            }
        ImGui::End();
    }
};

#endif /* Widgets_h */
