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
#include "SolidObject.hpp"
#include "Camera.hpp"
#include "Keyboard.hpp"
#include "Texture.hpp"
#include "TextRender.hpp"
#include "HDRi.hpp"
#include "CompositionPipeline.hpp"

//std
#include <memory>
#include <vector>
#include <array>
#include <string>

struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .1f};
    glm::vec3 lightPosition{.0f,-1.f,.0f};
    alignas(16) glm::vec4 lightColor{1.f, 1.f, 1.f, 10.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

class Application {
public:
    static constexpr int WIDTH = 1920;
    static constexpr int HEIGHT = 1080;
    
    Application(const char* binaryPath);
    ~Application();
    
    // Prevent Obj copy
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run();
    void simulate();
    void renderImguiContent();
    
    static int sum(int a) { return a + a; }
    
private:
    void loadSolidObjects();
    
    SDLWindow window{WIDTH, HEIGHT, "Vulkan Engine Development"};
    Device device{window};
    Renderer renderer{window, device};
    Image vulkanImage{device};
    std::unique_ptr<RenderSystem> renderSystem;
    std::unique_ptr<RenderSystem> skyboxSystem;
    std::unique_ptr<CompositionPipeline> postProcessing;
    
    std::unordered_map<uint32_t, std::unique_ptr<Texture>> textures{};
    std::vector<VkDescriptorImageInfo> textureInfos{};
    bool assetsLoaded = false;
    
    std::unique_ptr<DescriptorPool> globalPool{};
    SolidObject::Map solidObjects;
    SolidObject::Map env;
    
    SDL_Event sdl_event;
    int frameIndex{0};
    std::vector<float> frameTimes{0};
    std::vector<float> framesPerSecond{0};
    
    uint8_t load_phase{0};
    std::string binaryDir;
    
    GlobalUbo ubo{};
    int materialIndex = 0;
    
    std::vector<const char*> aaPresets = {"No AA", "MSAA 2X", "MSAA 4X", "MSAA 8X", "MSAA 16X"};
    
    struct{
        int width;
        int height;
    } surfaceExtent, windowExtent;
};

#endif /* Application_hpp */
