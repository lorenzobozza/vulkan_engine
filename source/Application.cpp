//
//  Application.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "include/Application.hpp"
#include "include/RenderSystem.hpp"
#include "include/CompositionPipeline.hpp"
#include "include/Buffer.hpp"
#include "UI.hpp"

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
    glm::vec3 lightPosition{.0f,-1.f,.0f};
    alignas(16) glm::vec4 lightColor{1.f, 1.f, 1.f, 10.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

Application::Application(const char* binaryPath) : binaryDir{binaryPath} {
    while(binaryDir.back() != '/' && !binaryDir.empty()) binaryDir.pop_back();
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
    float aspect = renderer.getAspectRatio();
    camera.setProjection.perspective(aspect, glm::radians(75.f), .01f, 100.f);
    bool orth = false;
    // Create object without model for the 3D camera position
    SolidObject cameraObj = SolidObject::createSolidObject();
    cameraObj.transform.translation = {.0f, .0f, -2.f};
    
    TextRender font{device, textMeshes, "fonts/Disket-Mono-Regular.ttf"};
    auto faces = font.getDescriptor();
    
    // 2D Layer Pipeline
    std::unique_ptr<DescriptorPool> guiPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    auto guiSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
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
        binaryDir+"font"
    };
    
    // Composition Pipeline
    std::unique_ptr<DescriptorPool> compositionPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto compositionSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    std::vector<VkDescriptorSet>compositionDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < compositionDescriptorSets.size(); i++) {
        auto descriptorInfo = renderer.getOffscreenImageInfo();
        DescriptorWriter(*compositionSetLayout, *compositionPool)
            .writeImage(0, &descriptorInfo)
            .build(compositionDescriptorSets[i]);
    }
    
    CompositionPipeline composition{
        device,
        renderer.getSwapChainRenderPass(),
        compositionSetLayout->getDescriptorSetLayout(),
        binaryDir+"composition"
    };
    
    UI imgui(device);
    
    auto imguiDescriptorInfo = imgui.loadFontTexture();
    
    std::unique_ptr<DescriptorPool> imguiPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto imguiSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

    std::vector<VkDescriptorSet>imguiDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < imguiDescriptorSets.size(); i++) {
        DescriptorWriter(*imguiSetLayout, *imguiPool)
            .writeImage(0, &imguiDescriptorInfo)
            .build(imguiDescriptorSets[i]);
    }
    
    imgui.createPipeline(imguiSetLayout->getDescriptorSetLayout(), renderer.getSwapChainRenderPass(), binaryDir+"imgui");

    // Load heavy assets on a separate thread
    std::thread([this]() {
        this->load_phase = 1;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_A.tga"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_N.tga", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_M.tga"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_R.tga"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_AO.tga"));
        
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_A.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_N.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_M.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_R.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_AO.png"));
        this->load_phase = 2;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/hdri/spiaggia_di_mondello_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT));
        this->load_phase = 3;
        this->assetsLoaded = true;
    }).detach();
    
    // Loading Screen Rendering
    font.renderText("Vulkan Engine v0.7 Beta, MSAA x" + std::to_string(device.msaaSamples) + ", V-Sync: " + (renderer.isVSyncEnabled() ? "On" : "Off") , -.65f, -.9f, .4f, {.2f, .8f, 1.f}, aspect);
    font.renderText("Loading Vulkan Engine", .0f, .0f, 1.2f, { .7f, .0f, .0f}, aspect);
    textHolder.insert(textMeshes.begin(), textMeshes.end());
    font.renderText("Lorenzo Bozza's Works", .0f, .2f, .4f, { .8f, .8f, .8f}, aspect);

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
                camera,
                guiDescriptorSets[frameIndex],
                textMeshes
            };
            
            //Render
            renderer.beginOffscreenRenderPass(commandBuffer);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            composition.renderSceneToSwapChain(commandBuffer, compositionDescriptorSets[frameIndex]);
            guiSystem.renderSolidObjects(guiInfo);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();
            
            if(nextIsLast) { break; }
            switch(load_phase) {
                case 1:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Materials...", .0f, .2f, .4f, { .8f, .8f, .8f}, aspect);
                    load_phase = 0;
                    break;
                case 2:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Environment...", .0f, .2f, .4f, { .8f, .8f, .8f}, aspect);
                    load_phase = 0;
                    break;
                case 3:
                    vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
                    textMeshes.clear();
                    textMeshes.insert(textHolder.begin(), textHolder.end());
                    font.renderText("Loading Geometries...", .0f, .2f, .4f, { .8f, .8f, .8f}, aspect);
                    load_phase = 0;
                    nextIsLast = true;
                    break;
                default:
                    break;
            }
        }
    }
    vkDeviceWaitIdle(device.device());
    textMeshes.clear();
    
    loadSolidObjects();

    //HDRi Pre-Processing
    auto equitangular = textures.at(10)->descriptorInfo();
    
    HDRi environmentMap{device, equitangular, {1024, 1024}, "equirectangular", 9};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{device, environment, {256, 256}, "prefiltering", 9};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{device, environment, {32, 32}, "irradiance"};
    auto irradiance = irradianceMap.descriptorInfo();
    
    font.renderText("Assets load time: " + std::to_string(std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - startTime).count()).substr(0, 5) + " s", .0f, -.9f, .6f, { .5f, .0f, .0f}, aspect);
    
    // Uniform Buffer Object (GlobalUbo)
    std::unique_ptr<Buffer> uboBuffer;
    uboBuffer = std::make_unique<Buffer>(
        device,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    uboBuffer->map();
    auto bufferInfo = uboBuffer->descriptorInfo();

    // SKYBOX RENDERING
    std::unique_ptr<DescriptorPool> skyboxPool =
       DescriptorPool::Builder(device)
           .setMaxSets(1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
           .build();
    
    auto skyboxSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    VkDescriptorSet skyboxDescriptorSet;
    DescriptorWriter(*skyboxSetLayout, *skyboxPool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &environment)
        .build(skyboxDescriptorSet);
    
    RenderSystem skyboxSystem{
        device,
        renderer.getOffscreenRenderPass(),
        skyboxSetLayout->getDescriptorSetLayout(),
        binaryDir+"skybox",
        device.msaaSamples
    };
    
    // Load Descriptors for PBR Textures
    for (int i = 0; i < 10; i++) {
        textureInfos.push_back(textures.at(i)->descriptorInfo());
    }
    
    // GLOBAL RENDER SYSTEM
    globalPool =
       DescriptorPool::Builder(device)
           .setMaxSets(1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)textureInfos.size())
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

    VkDescriptorSet globalDescriptorSet;
    DescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &irradiance) // Irradiance
        .writeImage(2, &prefiltered)  // Reflection
        .writeImage(3, renderer.getBrdfLutInfo())
        .writeImage(4, textureInfos.data())
        .build(globalDescriptorSet);
    
    RenderSystem renderSystem{
        device,
        renderer.getOffscreenRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        binaryDir+"shader",
        device.msaaSamples
    };
    ImGuiIO& io = ImGui::GetIO();
    bool running = true;
    auto counter1Hz = std::chrono::high_resolution_clock::now();
    bool mouseLeft = false;
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
                case SDL_MOUSEBUTTONDOWN:
                    io.MouseDown[0] = sdl_event.button.state;
                    mouseLeft = true;
                    break;
                case SDL_MOUSEBUTTONUP:
                    io.MouseDown[0] = sdl_event.button.state;
                    mouseLeft = false;
                    break;
                case SDL_MOUSEMOTION:
                    io.AddMousePosEvent(sdl_event.motion.x, sdl_event.motion.y);
                    if (mouseLeft) {
                        rotate.x = .05f*sdl_event.motion.yrel;
                        rotate.y = -.05f*sdl_event.motion.xrel;
                    }
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
        
        if (std::chrono::duration<float, std::chrono::seconds::period>(newTime -  counter1Hz).count() > 1.f) {
            counter1Hz = newTime;
            //std::cout << frameTimes.back() << " - " << frameTimes.front() << '\n';
            float avg = 0;
            for (int k = 0; k < frameTimes.size(); k++) { avg += frameTimes[k]; }
            avg = avg / frameTimes.size();
            vkWaitForFences(device.device(), 1, renderer.getSwapChainImageFence(frameIndex), VK_TRUE, UINT64_MAX);
            textMeshes.clear();
            font.renderText(std::to_string((int)(1.f / avg)) + " FPS", .9f, -.9f, .7f, {1.f, 1.f, 1.f}, aspect);
            font.renderText(std::to_string(avg * 1000.f).substr(0, 4) + " MS", .9f, -.8f, .7f, {1.f, 1.f, 1.f}, aspect);
            frameTimes.clear();
        }
        
        int height,width;
        SDL_GetWindowSize(window.getWindow(), &width, &height);
        io.DisplaySize = {(float)width, (float)height};
        imgui.newFrame();
        vkDeviceWaitIdle(device.device());
        imgui.updateBuffers();
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                globalDescriptorSet,
                solidObjects
            };
            
            FrameInfo skyboxInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                skyboxDescriptorSet,
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
            uboBuffer->writeToBuffer(&ubo);
            uboBuffer->flush();
            
            //RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer);
            skyboxSystem.renderSolidObjects(skyboxInfo);
            renderSystem.renderSolidObjects(frameInfo);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            composition.renderSceneToSwapChain(commandBuffer, compositionDescriptorSets[frameIndex]);
            guiSystem.renderSolidObjects(guiInfo);
            imgui.draw(commandBuffer, imguiDescriptorSets[frameIndex]);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();
        }
    }

    vkDeviceWaitIdle(device.device());
}

void Application::loadSolidObjects() {
    auto sphere = SolidObject::createSolidObject();
    sphere.model = Model::createModelFromFile(device, binaryDir+"Cerberus.obj");
    sphere.textureIndex = 0;
    sphere.transform.translation = {.65f, .1f, -.9f};
    sphere.transform.rotation.y = 3.9f;
    sphere.transform.rotation.x = .04f;
    sphere.transform.scale = {1.f, 1.f, 1.f};
    solidObjects.emplace(sphere.getId(), std::move(sphere));
    
    auto block = SolidObject::createSolidObject();
    block.model = Model::createModelFromFile(device, binaryDir+"plane.obj");
    block.textureIndex = 1;
    block.transform.translation = {.0f, 1.0f, .0f};
    block.transform.scale = {.4f, .4f, .4f};
    block.transform.rotation = {.0f, .0f, .0f};
    solidObjects.emplace(block.getId(), std::move(block));
    
    // Cubemap 3D canvas
    auto cube = SolidObject::createSolidObject();
    cube.model = Model::createModelFromFile(device, binaryDir+"cube.obj");
    cube.transform.translation = {.0f, .0f, .0f};
    cube.transform.scale = {1.f, 1.f, 1.f};
    cube.transform.rotation = {.0f, .0f, .0f};
    env.emplace(cube.getId(), std::move(cube));
}
