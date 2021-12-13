//
//  Application.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "include/Application.hpp"
#include "include/RenderSystem.hpp"
#include "include/Buffer.hpp"

//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

//std
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>


struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .2f};
    glm::vec3 lightPosition{.0f, -.4f, -1.f};
    alignas(16) glm::vec4 lightColor{1.f, 1.f, 1.f, .8f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

Application::Application(const char* binaryPath) {
    // Shader binaries in the same folder as the application binary
    shaderPath = binaryPath;
    while(shaderPath.back() != '/' && !shaderPath.empty()) shaderPath.pop_back();
    
    loadSolidObjects();
}

Application::~Application() {}


void Application::run() {

    glfwSetWindowOpacity(window.getGLFWwindow(), 1.f);
    
    Texture skin{device, "texture/normal_pavement.png"};
    Texture floor{device, "texture/pavement.png"};
    Texture normalMap{device, "texture/normal_pavement.png"};

    textures.push_back(skin.descriptorInfo());
    textures.push_back(floor.descriptorInfo());
    auto normal = normalMap.descriptorInfo();
    
    globalPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)textures.size() * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
           
    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
        uboBuffers[i] = std::make_unique<Buffer>(
            device,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboBuffers[i]->map();
    }

    auto globalSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                (uint32_t)textures.size())
            .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &normal)
            .writeImage(2, textures.data())
            .build(globalDescriptorSets[i]);
    }
    
    // Init Default Render System with "shader" shaders
    RenderSystem renderSystem{
        device,
        renderer.getSwapChainRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        shaderPath+"shader"
    };
    
    // Cursor prototype
        unsigned char pixels[8 * 8 * 4];
        memset(pixels, 0xaf, sizeof(pixels));
     
        GLFWimage image;
        image.width = 8;
        image.height = 8;
        image.pixels = pixels;
     
        GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0);
        glfwSetCursor(window.getGLFWwindow(), cursor);
    
    // Set camera projection based on aspect ratio of the viewport
    Camera camera{};
    float aspect = renderer.getAspectRatio();
    bool orth = false;
    camera.setProjection.perspective(aspect, 1.f, .01f, 100.f);
    
    // Create transparent object for the camera
    auto cameraObj = SolidObject::createSolidObject();
    cameraObj.transform.translation = {.0f, -.4f, -1.f};
    
    // Keyboard inputs
    Keyboard cameraCtrl{};
    
    float fovy{60.f};

    // Gameloop sync
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<float> frameTimes;
    
    int cnt = 0;
    
    glm::vec3 pos{-1.f};
    
    
    // TEXT 2D LAYER RENDERING
    std::unique_ptr<DescriptorPool> fpsPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto fpsSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
            
    TextRender font{device, textMeshes, "fonts/monaco.ttf"};
    auto faces = font.getDescriptor();
    
    std::vector<VkDescriptorSet> fpsDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < fpsDescriptorSets.size(); i++) {
        DescriptorWriter(*fpsSetLayout, *fpsPool)
            .writeImage(0, &faces)
            .build(fpsDescriptorSets[i]);
    }
    
    RenderSystem guiSystem{
        device,
        renderer.getSwapChainRenderPass(),
        fpsSetLayout->getDescriptorSetLayout(),
        shaderPath+"fps"
    };
    
    font.renderText("Vulkan Engine text rendering feature alpha (v0.4)", -.95f, -.9f, .1f, {.2f, .8f, 1.f});

    while(!window.shouldClose()) {
        glfwPollEvents();
        
        cnt = (cnt + 4) % 628;
        
        // Compute frame latency and store the value
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTimes.push_back(frameTime);
        frameTime = glm::min(frameTime, .05f); // Prevent movement glitches when resizing
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_O) == GLFW_PRESS) {
            camera.setOrthographicProjection(-aspect, aspect, -1.f, 1.f, -10.f, 100.f);
            orth = true;
        }
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_P) == GLFW_PRESS) {
            camera.setProjection.perspective();
            orth = false;
        }
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_F) == GLFW_PRESS) {
            fovy = ((int)fovy + 1) % 60 + 60;
            camera.setProjection[1].perspective(glm::radians(fovy));
        }
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_T) == GLFW_PRESS) {
            solidObjects.at(0).textureIndex ^= 1;
        }
        
        // Fix camera projection if the viewport's aspect ratio changes
        if (aspect != renderer.getAspectRatio()) {
            aspect = renderer.getAspectRatio();
            if (orth) {
                camera.setOrthographicProjection(-aspect, aspect, -1.f, 1.f, -10.f, 100.f);
            } else {
                camera.setProjection.perspective(aspect);
            }
        }
        
        // Polling keystrokes and adjusting the camera position/rotation
        cameraCtrl.moveInPlaneXZ(window.getGLFWwindow(), frameTime, cameraObj);
        camera.setViewYXZ(cameraObj.transform.translation, cameraObj.transform.rotation);
        
        
        auto &obj = solidObjects.at(someId);
        obj.transform.rotation.y = glm::mod(obj.transform.rotation.y + 0.02f, glm::two_pi<float>());
        
        pos.x = .8f * glm::sin((float)cnt / 100.f);
        pos.z = .8f * glm::cos((float)cnt / 100.f);
        
        
        if (auto commandBuffer = renderer.beginFrame()) {
            int frameIndex = renderer.getFrameIndex();
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                globalDescriptorSets[frameIndex],
                solidObjects
            };
            
            FrameInfo fpsInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                fpsDescriptorSets[frameIndex],
                textMeshes
            };
            
            //Update
            GlobalUbo ubo{};
            ubo.projectionView = frameInfo.camera.getProjection();
            ubo.viewMatrix = frameInfo.camera.getView();
            ubo.invViewMatrix = frameInfo.camera.getInverseView();
            //ubo.lightPosition = pos;
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            
            //Render
            renderer.beginSwapChainRenderPass(commandBuffer);
            
            
            renderSystem.renderSolidObjects(frameInfo);
            
            guiSystem.renderSolidObjects(fpsInfo);

            
            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
        
        // Compute average of the last n frames
        if (!(cnt % 628)) {
            float avg = 0;
            for (int k = 0; k < frameTimes.size(); k++) {
                avg += frameTimes[k];
            }
            avg = avg / frameTimes.size();
            int fps = (int)(1.f / avg);
            //std::cout << "FPS(AVG): " << fps << '\n';
            vkDeviceWaitIdle(device.device());
            textMeshes.clear();
            font.renderText(std::to_string(fps) + " fps", .82f, -.95f, .1f);
            frameTimes.clear();
        }
        
    }
    vkDeviceWaitIdle(device.device());
}

void Application::loadSolidObjects() {
    auto vase = SolidObject::createSolidObject();
    vase.model = Model::createModelFromFile(device, shaderPath+"monkey_triangle.obj");
    
    vase.color = {1.f, 1.f, 1.f};
    vase.textureIndex = 0;
    vase.transform.translation = {.0f, .0f, .0f};
    vase.transform.scale = {1.f, 1.f, 1.f};
    vase.transform.rotation = {.0f, .0f, .0f};
    
    solidObjects.emplace(vase.getId(), std::move(vase));
    
    
    auto plane = SolidObject::createSolidObject();
    plane.model = Model::createModelFromFile(device, shaderPath+"piano.obj");
    
    plane.color = {1.f, 1.f, 1.f};
    plane.textureIndex = 1;
    plane.transform.translation = {.0f, .0f, .0f};
    plane.transform.scale = {1.f, 1.f, 1.f};
    plane.transform.rotation = {.0f, .0f, .0f};
    
    solidObjects.emplace(plane.getId(), std::move(plane));
    
    someId = vase.getId();
    std::cout << "Vase id: "<< someId << '\n';
    /*
    //FPS
    Model::Data rectMesh{};
    rectMesh.vertices = {
        {{-1.f, -1.f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{0.f,0.f}},
        {{-.96f, -1.f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{1.f,0.f}},
        {{-.96f, -.93f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{1.f,1.f}},
        {{-1.f, -.93f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{0.f,1.f}}
    };
    rectMesh.indices = {0,1,2,2,3,0};
    
    auto rect = SolidObject::createSolidObject();
    rect.model = std::make_shared<Model>(device, rectMesh);
    rect.textureIndex = 0;
    mesh2d.emplace(rect.getId(), std::move(rect));
    
    rectMesh.vertices = {
        {{-.94f, -1.f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{0.f,0.f}},
        {{-.90f, -1.f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{1.f,0.f}},
        {{-.90f, -.93f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{1.f,1.f}},
        {{-.94f, -.93f, 0.f},{1.f, 1.f, 0.f},{0.f,0.f,-1.f},{0.f,1.f}}
    };
    rect.model = std::make_shared<Model>(device, rectMesh);
    mesh2d.emplace(rect.getId()+1, std::move(rect));
    */
}


/*
void Application::loadSolidObjects() {
    std::vector<Model::Vertex> vertices {};
    std::vector<Model::Vertex> vertices2 {};
    
    sierpinski(vertices, 6, {-0.5f, -0.5f}, {0.5f, -0.5f}, {-0.5f, 0.5f} );
    sierpinski(vertices2, 6, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f} );
    
    auto model = std::make_shared<Model>(device, vertices);
    auto model2 = std::make_shared<Model>(device, vertices2);
    
    auto triangle = SolidObject::createSolidObject();
    triangle.model = model;
    triangle.color = {.1f, .8f, .1f};
    triangle.transform2d.translation.x = 0.0f;
    triangle.transform2d.scale = {1.5f, 1.5f};
    triangle.transform2d.rotation = 0.0f;
    
    solidObjects.emplace(std::move(triangle));
    
    auto nuovo = SolidObject::createSolidObject();
    nuovo.model = model2;
    nuovo.color = {.4f, .2f, .4f};
    nuovo.transform2d.translation.y = 0.0f;
    nuovo.transform2d.scale = {1.5f, 1.5f};
    nuovo.transform2d.rotation = 0.0f;
    solidObjects.emplace(std::move(nuovo));
}*/

/*
void Application::sierpinski(
    std::vector<Model::Vertex> &vertices, int depth, glm::vec2 left, glm::vec2 right, glm::vec2 top) {
  if (depth <= 0) {
    vertices.push_back({top});
    vertices.push_back({right});
    vertices.push_back({left});
  } else {
    auto leftTop = 0.5f * (left + top);
    auto rightTop = 0.5f * (right + top);
    auto leftRight = 0.5f * (left + right);
    sierpinski(vertices, depth - 1, left, leftRight, leftTop);
    sierpinski(vertices, depth - 1, leftRight, right, rightTop);
    sierpinski(vertices, depth - 1, leftTop, rightTop, top);
  }
}*/

