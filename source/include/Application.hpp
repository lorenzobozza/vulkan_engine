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
#include "Model.hpp"
#include "Renderer.hpp"
#include "SolidObject.hpp"
#include "Camera.hpp"
#include "Keyboard.hpp"
#include "Texture.hpp"
#include "TextRender.hpp"
#include "HDRi.hpp"

//std
#include <memory>
#include <vector>
#include <string>

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
    
    std::vector< std::unique_ptr<Texture> > textures{};
    std::vector<VkDescriptorImageInfo> textureInfos{};
    bool assetsLoaded = false;
    
    std::unique_ptr<DescriptorPool> globalPool{};
    SolidObject::Map solidObjects;
    SolidObject::Map textMeshes;
    SolidObject::Map textHolder;
    SolidObject::Map env;
    
    SDL_Event sdl_event;
    int frameIndex{0};
    
    uint8_t load_phase{0};
    std::string binaryDir;
    
    struct{
        int width;
        int height;
    } surfaceExtent, windowExtent;
    
    id_t obj;
};

#endif /* Application_hpp */
