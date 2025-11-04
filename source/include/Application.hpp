//
//  Application.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Application_hpp
#define Application_hpp

#include <iostream>

#ifndef PROD
#define DEBUG_MESSAGE(...) std::cout << __VA_ARGS__ << std::endl;
#else
#define DEBUG_MESSAGE(...)
#endif

#include "SDLWindow.hpp"
#include "Device.hpp"
#include "Descriptors.hpp"
#include "Model.hpp"
#include "Renderer.hpp"
#include "Primitive.hpp"
#include "Camera.hpp"
#include "Keyboard.hpp"
#include "Texture.hpp"
#include "TextRender.hpp"
#include "HDRi.hpp"
#include "CompositionPipeline.hpp"
#include "Material.hpp"

//std
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <chrono>

struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .1f};
    glm::vec4 lightPosition[2] = {{.0f,-1.f,.0f,.0f},{.0f,-1.f,.0f,.0f}};
    glm::vec4 lightColor{1.f, 1.f, 1.f, 10.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

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
    
    Application(const char* binaryPath);
    ~Application() = default;
    
    // Prevent Obj copy
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run();
    void simulate();
    
private:
    
    SDLWindow window{WIDTH, HEIGHT, "Vulkan Engine Development"};
    Device vulkanDevice{window};
    Renderer renderer{window, vulkanDevice};
    Image vulkanImage{vulkanDevice};

    struct RenderSystems_s {
        std::unique_ptr<RenderSystem> depth;
        std::unique_ptr<RenderSystem> pbr;
        std::unique_ptr<RenderSystem> skybox;
        std::unique_ptr<CompositionPipeline> composit;
    } renderSystems;

    std::vector<std::unique_ptr<Texture>> textures{};
    std::unordered_map<std::string, Material> materials{};
    
    bool assetsLoaded = false;
    
    Primitive::Map primitives;
    Primitive::Map env;

    int frameIndex{0};
    
    Perf m_Perf;
    
    std::string binaryDir;
    
    GlobalUbo ubo{};
    
    struct{
        int width;
        int height;
    } surfaceExtent, windowExtent;
};

#endif /* Application_hpp */
