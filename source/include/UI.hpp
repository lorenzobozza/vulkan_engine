//
//  UI.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#ifndef UI_hpp
#define UI_hpp

#include "Device.hpp"
#include "Renderer.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

#include <imgui.h>

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
    virtual ~Widget() = default;
    
private:
    virtual void content(void) = 0;
    virtual void header(void) {}
    bool private_visibility = true;
};

class UI {
public:
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;
    UI(UI&&) = delete;
    UI& operator=(UI&&) = delete;
    
    UI(const Device& device, Renderer& renderer);
    ~UI();
    
    void newFrame(void);
    void updateBuffers(int frameIndex);
    void draw(VkCommandBuffer commandBuffer, int frameIndex);
    std::vector<VkDescriptorSet>* getDescriptorSets(void) { return &m_Descriptor.v_set; }
    static ImGuiKey ImGui_SDL2_KeyEventToImGuiKey(SDL_Keycode keycode);
    static char ImGuiKey_to_Charecter(ImGuiKey imgui_key, bool shift);
    static void setBessDarkColors(void);
    
    static void treeAssetsWidget(void);
    
    template <typename... Args>
    void addWidgets(Args ...arg) {
        static_assert(std::conjunction<std::is_base_of<Widget, typename std::remove_reference<decltype(*arg)>::type>...>::value,
                      "\nThe addWidget function only accepts shared_ptr<Widget>");
        (m_Widgets.push_back(arg), ...);
    }
    
private:
    std::vector<char> readFile(const std::string &filepath);
    void loadFontTexture(void);
    void createDescriptors(void);
    void createPipeline(VkRenderPass renderPass, std::string dynamicShaderPath);
    
    const Device& m_Device;
    Image m_Image{m_Device};
    Renderer& m_Renderer;
    
    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } m_PushConstBlock;
    
    std::vector<std::unique_ptr<Buffer>> m_VertexBuffers;
    std::vector<std::unique_ptr<Buffer>> m_IndexBuffers;
    int m_VertexCount[SwapChain::MAX_FRAMES_IN_FLIGHT], m_IndexCount[SwapChain::MAX_FRAMES_IN_FLIGHT];
    
    VkPipeline m_ImguiPipeline;
    VkPipelineLayout m_ImguiPipelineLayout;
    VkShaderModule m_VertShaderModule;
    VkShaderModule m_FragShaderModule;
    VkImage m_FontImage;
    VkDeviceMemory m_FontMem;
    VkImageView m_FontView;
    VkSampler m_FontSampler;
    VkDescriptorImageInfo m_FontDescriptorInfo;
    DescriptorStruct m_Descriptor;
    
    ImGuiContext* m_ImGuiContext;
    std::vector<std::shared_ptr<Widget>> m_Widgets;
};

#endif /* UI_hpp */
