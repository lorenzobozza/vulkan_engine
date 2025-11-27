//
//  Application.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Application_hpp
#define Application_hpp

#include "SDLWindow.hpp"
#include "Device.hpp"
#include "Descriptors.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
#include "Primitive.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "CubeMap.hpp"
#include "Material.hpp"
#include "Light.hpp"

#include "ScenePipeline.hpp"
#include "ShadowPipeline.hpp"
#include "SkyboxPipeline.hpp"
#include "CompositingPipeline.hpp"
#include "DebugPipeline.hpp"

#include <memory>
#include <vector>
#include <array>
#include <string>
#include <chrono>
#include <mutex>
#include <print>


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
    
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run(void);
    
private:
    void shortcutCallback(Shortcut shortcut);
    
    SDLWindow m_Window{WIDTH, HEIGHT, "Acinonyx"};
    Device m_Device{m_Window};
    Image m_Image{m_Device};
    
    VkSampleCountFlagBits m_MSAASampleCount = VK_SAMPLE_COUNT_1_BIT;
    Renderer m_Renderer{m_Window, m_Device, m_MSAASampleCount};
    
    struct RenderSystems_s {
        struct {
            std::unique_ptr<PipelineWrapper> ptr;
            std::mutex mutex;
            void render(VkCommandBuffer cb, int idx) { if (ptr) ptr->safe_render(cb, idx, mutex); }
            template<class T>
            T* ptr_cast(void) { return reinterpret_cast<T*>(ptr.get()); }
        } shadow, scene, skybox, composit, debug;
    } m_Pipes;
    
    struct {
        std::unique_ptr<CubeMap> instance;
        VkDescriptorImageInfo* descriptor;
    } m_Environment, m_Prefiltered, m_Irradiance;
    
    std::vector<std::unique_ptr<Texture>> m_Textures{};
    std::unordered_map<std::string, Material> m_Materials{};
    
    Assets m_Assets;
    std::vector<Light> m_Lights;
    
    bool m_AssetsLoaded = false;
    bool m_PreviewMode = false;
    bool m_DebugMode = false;
    
    Primitive::Map m_Primitives;
    
    int m_FrameIndex{0};
    Perf m_Perf;
   
    ScenePipeline::UniformBuffer m_Ubo{};
    std::unique_ptr<Buffer> m_UboBuffers[SwapChain::MAX_FRAMES_IN_FLIGHT];
};

#endif /* Application_hpp */
