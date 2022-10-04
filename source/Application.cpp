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
#include <future>


struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .4f};
    glm::vec3 lightPosition{.0f,-5.f,-1.f};
    alignas(16) glm::vec4 lightColor{1.f, 1.f, .7f, 200.f};
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

glm::vec3 rotate{.0f};

void Application::pan_callback(GLFWwindow* window, double xoffset, double yoffset) {
    rotate.x = -yoffset;
    rotate.y = xoffset;
}

void Application::run() {
    //glfwSetWindowOpacity(window.getGLFWwindow(), 1.f);
    
    // Cursor prototype
    unsigned char pixels[8 * 8 * 4];
    memset(pixels, 0xaf, sizeof(pixels));
 
    GLFWimage image;
    image.width = 8;
    image.height = 8;
    image.pixels = pixels;
 
    //GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0);
    //glfwSetCursor(window.getGLFWwindow(), cursor);
    glfwSetInputMode(window.getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // GAMELOOP TIMING
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<float> frameTimes;
    int cnt{0};
    auto startTime = currentTime;
    
    Camera camera{};
    
    // Set camera projection based on aspect ratio of the viewport
    float aspect = renderer.getAspectRatio();
    bool orth = false;
    camera.setProjection.perspective(aspect, 1.f, .01f, 100.f);
    
    // Create transparent object for the camera
    SolidObject cameraObj = SolidObject::createSolidObject();
    cameraObj.transform.translation = {.0f, -.4f, -2.f};
    
    glfwSetScrollCallback(window.getGLFWwindow(), pan_callback);
    
    
    // Keyboard inputs
    Keyboard cameraCtrl{};
    
    float fovy{60.f};
    
    // 2D LAYER RENDERING
    std::unique_ptr<DescriptorPool> guiPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto guiSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
            
    TextRender font{device, textMeshes, "fonts/monaco.ttf"};
    auto faces = font.getDescriptor();
    
    std::vector<VkDescriptorSet> guiDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < guiDescriptorSets.size(); i++) {
        DescriptorWriter(*guiSetLayout, *guiPool)
            .writeImage(0, &faces)
            .build(guiDescriptorSets[i]);
    }
    
    RenderSystem guiSystem{
        device,
        renderer.getSwapChainRenderPass(),
        guiSetLayout->getDescriptorSetLayout(),
        shaderPath+"font"
    };
    
    font.renderText("Vulkan Engine v0.5, MSAA x" + std::to_string(device.msaaSamples) + ", V-Sync: " + (renderer.isVSyncEnabled() ? "On" : "Off") , -.95f, -.9f, .1f, {.2f, .8f, 1.f});

    // Load heavy assets on a separate thread
    std::thread([this]() {
        std::string textureName = "Metal007_1K";
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_A.tga"));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_N.tga", VK_FORMAT_R8G8B8A8_UNORM));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_M.tga"));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_R.tga"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/hdri/indoor.jpg"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/ibl_brdf_lut.png", VK_FORMAT_R8G8_UNORM));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/" + textureName + "_Color.png"));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/" + textureName + "_NormalGL.png", VK_FORMAT_R8G8B8A8_UNORM));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/" + textureName + "_Metalness.png"));
        //this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/" + textureName + "_Roughness.png"));
        this->assetsLoaded = true;
    }).detach();
    



    font.renderText("-> Loading Assets...", -.95f, -.8f, .1f, { 1.f, .3f, 1.f});
    auto loading = font.renderText("-", 0.f, 0.f, .25f, { 1.f, .2f, .2f});

    while (!assetsLoaded) {
        glfwPollEvents();
        
        cnt = (cnt + 130) % 628;
        
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, .05f);
        
        textMeshes.at(loading).transform.rotation.z = - cnt / 100.f;

        if (auto commandBuffer = renderer.beginFrame()) {
            int frameIndex = renderer.getFrameIndex();
            
            FrameInfo guiInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                guiDescriptorSets[frameIndex],
                textMeshes
            };
            
            //Render
            renderer.beginSwapChainRenderPass(commandBuffer);
            
            guiSystem.renderSolidObjects(guiInfo);

            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
    }
    
    vkDeviceWaitIdle(device.device());
    textMeshes.clear();
    
    font.renderText("Textures: " + std::to_string(std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - startTime).count()).substr(0, 5) + " s", -.95f, -.9f, .1f, { 1.f, .3f, 1.f});
    
    
    //HDRi Pre-Processing
    auto equitangular = textures.at(0)->descriptorInfo();
    
    HDRi environmentMap{device, equitangular, {1024, 1024}, "equirectangular", 5};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{device, environment, {256, 256}, "prefiltering", 5};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{device, environment, {32, 32}, "irradiance"};
    auto irradiance = irradianceMap.descriptorInfo();
    
    font.renderText(glfwRawMouseMotionSupported() ? "RAW MOUSE SUPPORTED" : "RAW MOUSE NOT SUPPORTED", -.95f, -.8f, .1f, { 1.f, .3f, 1.f});
    
    textureInfos.push_back(textures.at(0)->descriptorInfo());
    auto ibl_brdf_lut = textures.at(1)->descriptorInfo();

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

    // SKYBOX RENDERING
    std::unique_ptr<DescriptorPool> skyboxPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto skyboxSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    std::vector<VkDescriptorSet> skyboxDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < skyboxDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*skyboxSetLayout, *skyboxPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &environment)
            .build(skyboxDescriptorSets[i]);
    }
    
    RenderSystem skyboxSystem{
        device,
        renderer.getSwapChainRenderPass(),
        skyboxSetLayout->getDescriptorSetLayout(),
        shaderPath+"skybox"
    };
    
    // GLOBAL RENDER SYSTEM
    globalPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)textureInfos.size() * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();

    auto globalSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                (uint32_t)textureInfos.size())
            .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &irradiance) // Irradiance
            .writeImage(2, &prefiltered)  // Reflection
            .writeImage(3, &ibl_brdf_lut)
            .writeImage(4, textureInfos.data())
            .build(globalDescriptorSets[i]);
    }
    
    RenderSystem renderSystem{
        device,
        renderer.getSwapChainRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        shaderPath+"shader"
    };

    while(!window.shouldClose()) {
        glfwPollEvents();
        
        cnt = (cnt + 4) % 628;
        
        // Compute frame latency and store the value
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTimes.push_back(frameTime);
        frameTime = glm::min(frameTime, .05f); // Prevent movement glitches when resizing
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window.getGLFWwindow(), VK_TRUE);
        }
        
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
        
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_R) == GLFW_PRESS) {
            if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_1) == GLFW_PRESS) {
                solidObjects.at(0).roughness -= .01f;
                if (solidObjects.at(0).roughness < 0.01f) { solidObjects.at(0).roughness = 0.01f; }
            } else if (glfwGetKey(window.getGLFWwindow(), GLFW_KEY_2) == GLFW_PRESS) {
                solidObjects.at(0).roughness += .01f;
                if (solidObjects.at(0).roughness > 1) { solidObjects.at(0).roughness = 1.f; }
            }
        }
        if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_M) == GLFW_PRESS) {
            if(glfwGetKey(window.getGLFWwindow(), GLFW_KEY_1) == GLFW_PRESS) {
                solidObjects.at(0).metalness -= .01f;
                if (solidObjects.at(0).metalness < 0) { solidObjects.at(0).metalness = 0.f; }
            } else if (glfwGetKey(window.getGLFWwindow(), GLFW_KEY_2) == GLFW_PRESS) {
                solidObjects.at(0).metalness += .01f;
                if (solidObjects.at(0).metalness > 1) { solidObjects.at(0).metalness = 1.f; }
            }
        }
        
        if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
            cameraObj.transform.rotation += rotate * .05f;
            cameraObj.transform.rotation.x = glm::clamp(cameraObj.transform.rotation.x, -1.5f, 1.5f);
            cameraObj.transform.rotation.y = glm::mod(cameraObj.transform.rotation.y, glm::two_pi<float>());
            rotate = glm::vec3{.0f};
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
        
        
        //solidObjects.at(2).transform.rotation.y = glm::mod(solidObjects.at(2).transform.rotation.y + 0.02f, glm::two_pi<float>());
        //solidObjects.at(2).transform.translation.y += .02f * glm::cos((float)cnt / 100.f);
        
        
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
            
            FrameInfo skyboxInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                skyboxDescriptorSets[frameIndex],
                env
            };
            
            FrameInfo guiInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                guiDescriptorSets[frameIndex],
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
            
            
            skyboxSystem.renderSolidObjects(skyboxInfo);

            renderSystem.renderSolidObjects(frameInfo);

            guiSystem.renderSolidObjects(guiInfo);

            
            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
        
        // Compute average of the last n frames
        if ((cnt % 628) == 0) {
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
    auto sphere = SolidObject::createSolidObject();
    sphere.model = Model::createModelFromFile(device, shaderPath+"cube.obj");
    
    sphere.color = {1.f, 1.f, 1.f};
    sphere.metalness = 1.f;
    sphere.roughness = .01f;
    sphere.transform.translation = {.0f, .0f, .0f};
    sphere.transform.scale = {.5f, .5f, .5f};
    
    solidObjects.emplace(sphere.getId(), std::move(sphere));
    
    /*
    auto volvo = SolidObject::createSolidObject();
    volvo.model = Model::createModelFromFile(device, shaderPath+"bmw.obj");
    
    volvo.color = {1.f, 1.f, 1.f};
    volvo.textureIndex = 0;
    volvo.metalness = .0f;
    volvo.roughness = .5f;
    volvo.transform.translation = {.0f, .0f, .0f};
    volvo.transform.scale = {1.f, 1.f, 1.f};
    
    solidObjects.emplace(volvo.getId(), std::move(volvo));
    
    auto sphere2 = SolidObject::createSolidObject();
    sphere2.model = Model::createModelFromFile(device, shaderPath+"pistol.obj");
    
    sphere2.color = {1.f, 1.f, 1.f};
    sphere2.textureIndex = 0;
    sphere2.metalness = 1.f;
    sphere2.roughness = .25f;
    sphere2.transform.translation = {.0f, -3.f, .0f};
    sphere2.transform.scale = {2.f, 2.f, 2.f};
    
    solidObjects.emplace(sphere2.getId(), std::move(sphere2));

    auto plane = SolidObject::createSolidObject();
    plane.model = Model::createModelFromFile(device, shaderPath+"piano.obj");
    
    plane.color = {1.f, 1.f, 1.f};
    plane.textureIndex = 1;
    plane.transform.translation = {.0f, .0f, .0f};
    plane.transform.scale = {3.f, 3.f, 3.f};
    plane.transform.rotation = {.0f, .0f, .0f};
    
    //solidObjects.emplace(plane.getId(), std::move(plane));
    */
    
    auto light = SolidObject::createSolidObject();
    light.model = Model::createModelFromFile(device, shaderPath+"cube.obj");
    
    light.color = {1.f, .2f, .1f};
    light.textureIndex = 0;
    light.transform.translation = {.0f, .0f, .0f};
    light.transform.scale = {.01f, .01f, .01f};
    light.transform.rotation = {.0f, .0f, .0f};
    
    solidObjects.emplace(light.getId(), std::move(light));
    
    auto cube = SolidObject::createSolidObject();
    cube.model = Model::createModelFromFile(device, shaderPath+"cube.obj");
    
    cube.color = {1.f, .2f, .1f};
    cube.textureIndex = 0;
    cube.transform.translation = {.0f, .0f, .0f};
    cube.transform.scale = {1.f, 1.f, 1.f};
    cube.transform.rotation = {.0f, .0f, .0f};
    
    env.emplace(cube.getId(), std::move(cube));
}

