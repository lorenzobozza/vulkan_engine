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
#include "Image.hpp"

//std
#include <memory>
#include <vector>
#include <string>

class Application {
public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 800;
    
    Application(const char* binaryPath);
    ~Application();
    
    // Prevent Obj copy
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    
    void run();
    void simulate();
    
private:
    void loadSolidObjects();
    
    Window window{WIDTH, HEIGHT, "Vulkan on MacOS"};
    Device device{window};
    Renderer renderer{window, device};
    
    std::unique_ptr<DescriptorPool> globalPool{};
    SolidObject::Map solidObjects;
    
    //void sierpinski(std::vector<Model::Vertex> &vertices, int depth, glm::vec2 left, glm::vec2 right, glm::vec2 top);
    
    std::string shaderPath;
    
    SolidObject::id_t someId;
};

#endif /* Application_hpp */
