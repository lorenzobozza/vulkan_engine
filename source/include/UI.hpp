//
//  UI.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#ifndef UI_hpp
#define UI_hpp

#include "imgui.h"
#include "Application.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Descriptors.hpp"
#include "Image.hpp"
#include "Renderer.hpp"

#include <functional>

class UI {
public:
    UI(Device &device, Renderer& renderer);
    ~UI();
    
    struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;
 
    enum Widget {
        Viewport = 0,
        Log,
        
        TotalCount
    };
    
    void newFrame(Application *app);
    void updateBuffers(int frameIndex);
    void draw(VkCommandBuffer commandBuffer, int frameIndex);
    
    std::vector<VkDescriptorSet>* getDescriptorSets(void) { return &descriptor.v_set; }
    
    static ImGuiKey ImGui_SDL2_KeyEventToImGuiKey(SDL_Keycode keycode);
    static char ImGuiKey_to_Charecter(ImGuiKey imgui_key, bool shift);
    static void setBessDarkColors(void);
    static void OnImGui(std::string directoryPath);

private:
    std::vector<char> readFile(const std::string &filepath);
    void loadFontTexture(void);
    void createDescriptors(void);
    void createPipeline(VkRenderPass renderPass, std::string dynamicShaderPath);

    Device &device;
    Image vulkanImage{device};
    Renderer& m_Renderer;
    
    VkPipeline imguiPipeline;
    VkPipelineLayout imguiPipelineLayout;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    
    std::vector<std::unique_ptr<Buffer>> *vertexBuffers;
    std::vector<std::unique_ptr<Buffer>> *indexBuffers;
    int vertexCount[SwapChain::MAX_FRAMES_IN_FLIGHT], indexCount[SwapChain::MAX_FRAMES_IN_FLIGHT];
    
    VkImage fontImage;
    VkDeviceMemory fontMem;
    VkImageView fontView;
    VkSampler fontSampler;
    
    VkDescriptorImageInfo fontDescriptorInfo;
    
    DescriptorStruct descriptor;
    
    ImGuiContext* context;
    
    
    struct Widget_s {
        Widget_s(bool showWidget, std::function<void()> callback) : m_callback(callback), isVisible(showWidget) {};
        Widget_s() = default;
        
        bool isVisible = false;
        void bind(std::function<void()> f) { m_callback = f; }
        void draw(void) { if (isVisible && m_callback != nullptr) m_callback(); }

    private:
        std::function<void()> m_callback;
    };
    
    std::array<Widget_s, Widget::TotalCount> widgets;
    
    void buildWidgets(void);
    
public:
    void showWidget(Widget id) { if (id < Widget::TotalCount) widgets[id].isVisible = true; }
    void hideWidget(Widget id) { if (id < Widget::TotalCount) widgets[id].isVisible = false; }
    void toggleWidget(Widget id) { if (id < Widget::TotalCount) widgets[id].isVisible ^= true; }
    
    void bindWidget(Widget id, std::function<void()> f) { if (id < Widget::TotalCount) widgets[id].bind(f); }
    
};

#endif /* UI_hpp */
