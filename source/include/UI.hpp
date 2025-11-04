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

class Widget {
public:
    void draw(void) {
        header();
        if(getVisibility()) {
            content();
        }
    }
    bool& getVisibility(void) { return private_visibility; }
    const std::string getName(void) { return typeid(*this).name(); }
    ~Widget() = default;

private:
    virtual void content(void) = 0;
    virtual void header(void) {}
    bool private_visibility = true;
};

class UI {
public:
    UI(Device &device, Renderer& renderer);
    ~UI();
    
    struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;
    
    void newFrame(void);
    void updateBuffers(int frameIndex);
    void draw(VkCommandBuffer commandBuffer, int frameIndex);
    
    std::vector<VkDescriptorSet>* getDescriptorSets(void) { return &descriptor.v_set; }
    
    static ImGuiKey ImGui_SDL2_KeyEventToImGuiKey(SDL_Keycode keycode);
    static char ImGuiKey_to_Charecter(ImGuiKey imgui_key, bool shift);
    static void setBessDarkColors(void);
    
    static void treeAssetsWidget(void);
    
    template <typename... Args>
    void addWidgets(Args ...arg) {
        static_assert(std::conjunction<std::is_base_of<Widget, typename std::remove_reference<decltype(*arg)>::type>...>::value,
            "\nThe addWidget function only accepts shared_ptr<Widget>");
        (widgets.push_back(arg), ...);
    }

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
    
    std::vector<std::shared_ptr<Widget>> widgets;
};

#endif /* UI_hpp */
