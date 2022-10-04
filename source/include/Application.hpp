//
//  Application.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Application_hpp
#define Application_hpp

#include "Window.hpp"
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
    static constexpr int WIDTH = 1200;
    static constexpr int HEIGHT = 1200;
    
    Application(const char* binaryPath);
    ~Application();
    
    // Prevent Obj copy
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run();
    void simulate();
    
    static int sum(int a) { return a + a; }
    
private:
    void loadSolidObjects();
    
    Window window{WIDTH, HEIGHT, "Vulkan Engine Development"};
    Device device{window};
    Renderer renderer{window, device};
    Image vulkanImage{device};
    
    std::vector< std::unique_ptr<Texture> > textures{};
    std::vector<VkDescriptorImageInfo> textureInfos{};
    bool assetsLoaded = false;
    
    std::unique_ptr<DescriptorPool> globalPool{};
    SolidObject::Map solidObjects;
    SolidObject::Map textMeshes;
    SolidObject::Map env;
    
    static void pan_callback(GLFWwindow* window, double xoffset, double yoffset);
    
    std::string shaderPath;
    
    SolidObject::id_t someId;
};

#endif /* Application_hpp */
