//
//  Application.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Application_hpp
#define Application_hpp

#include <print>

#include "SDLWindow.hpp"
#include "Device.hpp"
#include "Descriptors.hpp"
#include "Model.hpp"
#include "Renderer.hpp"
#include "Primitive.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "TextRender.hpp"
#include "HDRi.hpp"
#include "Material.hpp"
#include "Light.hpp"

#include "ScenePipeline.hpp"
#include "ShadowPipeline.hpp"
#include "SkyboxPipeline.hpp"
#include "CompositingPipeline.hpp"

//std
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <chrono>


struct Perf {
private:
    std::chrono::high_resolution_clock::time_point start{};
    std::chrono::high_resolution_clock::time_point cpuStop{};
    std::chrono::high_resolution_clock::time_point gpuStop{};
public:
    float cpuTime{.001f};
    float gpuTime{.016f};
    void startFrame(void) { start = std::chrono::high_resolution_clock::now(); }
    void cpuEnd(void) { cpuStop = std::chrono::high_resolution_clock::now(); cpuTime = std::chrono::duration<float, std::chrono::seconds::period>(cpuStop - start).count(); }
    void gpuEnd(void) { gpuStop = std::chrono::high_resolution_clock::now(); gpuTime = std::chrono::duration<float, std::chrono::seconds::period>(gpuStop - cpuStop).count(); }
};

class Application {
public:
    static constexpr int WIDTH = 1920;
    static constexpr int HEIGHT = 1080;
    
    Application() = default;
    ~Application() = default;
    
    // Prevent Obj copy
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run(void);
    
private:
    void shortcutCallback(Shortcut);
    
    SDLWindow window{WIDTH, HEIGHT, "Vulkan Engine Development"};
    Device vulkanDevice{window};
    Renderer renderer{window, vulkanDevice};
    Image vulkanImage{vulkanDevice};
    
    struct RenderSystems_s {
        std::unique_ptr<ShadowPipeline> shadow;
        std::unique_ptr<ScenePipeline> scene;
        std::unique_ptr<SkyboxPipeline> skybox;
        std::unique_ptr<CompositingPipeline> composit;
    } m_Pipelines;
    
    struct {
        std::unique_ptr<HDRi> instance;
        VkDescriptorImageInfo* descriptor;
    } m_Environment, m_Prefiltered, m_Irradiance;
    
    std::vector<std::unique_ptr<Texture>> textures{};
    std::unordered_map<std::string, Material> materials{};
    std::vector<Light> lights{};
    
    bool assetsLoaded = false;
    bool previewMode = false;
    
    Primitive::Map primitives;
    
    int frameIndex{0};
    
    Perf m_Perf;
    
    const std::string binaryDir = "./";
    
    ScenePipeline::UniformBuffer ubo{};
    std::unique_ptr<Buffer> uboBuffers[SwapChain::MAX_FRAMES_IN_FLIGHT];
};

#endif /* Application_hpp */
