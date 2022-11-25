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
    alignas(16) glm::vec4 lightColor{1.f, 1.f, 1.f, 200.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

struct TextUbo {
    glm::mat4 projectionView{1.f};
};

Application::Application(const char* binaryPath) {
    // Shader binaries in the same folder as the application binary
    shaderPath = binaryPath;
    while(shaderPath.back() != '/' && !shaderPath.empty()) shaderPath.pop_back();
}

Application::~Application() {}

glm::vec3 rotate{.0f};

void Application::run() {
    // GAMELOOP TIMING
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<float> frameTimes;
    int cnt{0};
    auto startTime = currentTime;
    
    // 3D + 2D CAMERAS
    Camera camera{};
    Camera camera2D{};
    float aspect = renderer.getAspectRatio();
    camera.setProjection.perspective(aspect, glm::radians(75.f), .01f, 100.f);
    camera2D.setOrthographicProjection(-aspect, aspect, -1.f, 1.f, -10.f, 100.f);
    bool orth = false;

    // Create object without model for the 3D camera position
    SolidObject cameraObj = SolidObject::createSolidObject();
    cameraObj.transform.translation = {.0f, -.4f, -2.f};
    
    TextRender font{device, textMeshes, "fonts/Disket-Mono-Regular.ttf"};
    auto faces = font.getDescriptor();
    
    // Uniform Buffer Objects
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
    std::vector<std::unique_ptr<Buffer>> uboTextBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboTextBuffers.size(); i++) {
        uboTextBuffers[i] = std::make_unique<Buffer>(
            device,
            sizeof(TextUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboTextBuffers[i]->map();
    }
    
    // 2D LAYER RENDERING
    std::unique_ptr<DescriptorPool> guiPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto guiSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
            
    
    
    std::vector<VkDescriptorSet> guiDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < guiDescriptorSets.size(); i++) {
        auto bufferInfo = uboTextBuffers[i]->descriptorInfo();
        DescriptorWriter(*guiSetLayout, *guiPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &faces)
            .build(guiDescriptorSets[i]);
    }
    
    RenderSystem guiSystem{
        device,
        renderer.getSwapChainRenderPass(),
        guiSetLayout->getDescriptorSetLayout(),
        shaderPath+"font"
    };
    

    // Load heavy assets on a separate thread
    std::thread([this]() {
        this->load_phase = 1;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Marble_C.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/flat_normal.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Marble_M.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Marble_M.png"));
        this->load_phase = 2;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/hdri/neon_photostudio_8k.hdr"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/ibl_brdf_lut.png", VK_FORMAT_R8G8_UNORM));
        this->load_phase = 3;
        this->assetsLoaded = true;
    }).detach();
    
    // Loading Screen Rendering
    font.renderText("Vulkan Engine v0.6 Beta, MSAA x" + std::to_string(device.msaaSamples) + ", V-Sync: " + (renderer.isVSyncEnabled() ? "On" : "Off") , -.65f*aspect, -.9f, .4f, {.2f, .8f, 1.f});
    font.renderText("Loading Vulkan Engine", .0f, .0f, 1.2f, { .7f, .0f, .0f});
    textHolder.insert(textMeshes.begin(), textMeshes.end());
    font.renderText("Lorenzo Bozza's Works", .0f, .2f, .4f, { .8f, .8f, .8f});

    bool nextIsLast = false;
    while (!assetsLoaded || load_phase > 0 || nextIsLast) {
        SDL_PollEvent(&sdl_event);
        
        cnt = (cnt + 130) % 628;
        
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, .05f);

        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            FrameInfo guiInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera2D,
                guiDescriptorSets[frameIndex],
                textMeshes
            };
            
            //Update 2D
            TextUbo ubo{};
            ubo.projectionView = guiInfo.camera.getProjection();
            uboTextBuffers[frameIndex]->writeToBuffer(&ubo);
            uboTextBuffers[frameIndex]->flush();
            
            //Render
            renderer.beginSwapChainRenderPass(commandBuffer);
            
            guiSystem.renderSolidObjects(guiInfo);

            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
            
            if(nextIsLast) { break; }
            switch(load_phase) {
                case 1:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Materials...", .0f, .2f, .4f, { .8f, .8f, .8f});
                    load_phase = 0;
                    break;
                case 2:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Environment...", .0f, .2f, .4f, { .8f, .8f, .8f});
                    load_phase = 0;
                    break;
                case 3:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Geometries...", .0f, .2f, .4f, { .8f, .8f, .8f});
                    load_phase = 0;
                    nextIsLast = true;
                    break;
                default:
                    break;
            }
        }
    }
    
    loadSolidObjects();
    vkDeviceWaitIdle(device.device());
    textMeshes.clear();
    
    font.renderText("Assets load time: " + std::to_string(std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - startTime).count()).substr(0, 5) + " s", .0f * aspect, -.9f, .6f, { .5f, .0f, .0f});
    
    
    //HDRi Pre-Processing
    auto equitangular = textures.at(4)->descriptorInfo();
    
    HDRi environmentMap{device, equitangular, {2048, 2048}, "equirectangular", 5};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{device, environment, {480, 480}, "prefiltering", 5};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{device, environment, {32, 32}, "irradiance"};
    auto irradiance = irradianceMap.descriptorInfo();

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
    
    // PBR Textures
    textureInfos.push_back(textures.at(0)->descriptorInfo());
    textureInfos.push_back(textures.at(1)->descriptorInfo());
    textureInfos.push_back(textures.at(2)->descriptorInfo());
    textureInfos.push_back(textures.at(3)->descriptorInfo());
    auto ibl_brdf_lut = textures.at(5)->descriptorInfo();
    
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
    
    SDL_StartTextInput();
    bool running = true;
    while(running)
    {
        cnt = ++cnt % 628;
        // Compute frame latency and store the value
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTimes.push_back(frameTime);
        frameTime = glm::min(frameTime, .05f); // Prevent movement glitches when resizing

        while(SDL_PollEvent(&sdl_event))
        {
            switch (sdl_event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if(sdl_event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    SDL_GameControllerOpen(0);
                    font.renderText(SDL_GameControllerNameForIndex(0), -.1f, -.95f, .1f);
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    SDL_GameControllerClose(0);
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    running = false;
                    break;
                case SDL_MOUSEWHEEL:
                    rotate.x = -.5f*sdl_event.wheel.preciseY;
                    rotate.y = -.5f*sdl_event.wheel.preciseX;
                    break;
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
        camera.setViewYXZ(cameraObj.transform.translation, cameraObj.transform.rotation);
        //cameraCtrl.moveInPlaneXZ(window.getGLFWwindow(), frameTime, cameraObj);
        
        if (!(cnt % 314)) {
            //std::cout << frameTimes.back() << " - " << frameTimes.front() << '\n';
            float avg = 0;
            for (int k = 0; k < frameTimes.size(); k++) {
                avg += frameTimes[k];
            }
            avg = avg / frameTimes.size();
            int fps = (int)(1.f / avg);
            vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
            textMeshes.clear();
            font.renderText(std::to_string(fps) + " FPS", .9f * aspect, -.9f, .7f);
            frameTimes.clear();
        }
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
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
                camera2D,
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
    }

    vkDeviceWaitIdle(device.device());
}

void Application::loadSolidObjects() {
    auto sphere = SolidObject::createSolidObject();
    sphere.model = Model::createModelFromFile(device, shaderPath+"sphere.obj");
    
    sphere.color = {1.f, 1.f, 1.f};
    sphere.textureIndex = 0;
    sphere.metalness = .0f;
    sphere.roughness = 1.f;
    sphere.transform.translation = {.0f, .0f, .0f};
    sphere.transform.rotation.y = .0f;
    sphere.transform.scale = {1.f, 1.f, 1.f};
    
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

    auto light = SolidObject::createSolidObject();
    light.model = Model::createModelFromFile(device, shaderPath+"cube.obj");
    
    light.color = {1.f, .2f, .1f};
    light.textureIndex = 0;
    light.transform.translation = {.0f, .0f, .0f};
    light.transform.scale = {.01f, .01f, .01f};
    light.transform.rotation = {.0f, .0f, .0f};
    
    solidObjects.emplace(light.getId(), std::move(light));
    */
    
    auto cube = SolidObject::createSolidObject();
    cube.model = Model::createModelFromFile(device, shaderPath+"cube.obj");
    
    cube.color = {1.f, .2f, .1f};
    cube.transform.translation = {.0f, .0f, .0f};
    cube.transform.scale = {1.f, 1.f, 1.f};
    cube.transform.rotation = {.0f, .0f, .0f};
    
    env.emplace(cube.getId(), std::move(cube));
}

