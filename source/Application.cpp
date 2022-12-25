//
//  Application.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "include/Application.hpp"
#include "include/RenderSystem.hpp"
#include "include/UI.hpp"
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
    int cnt{0};
    auto startTime = currentTime;
    
    // 3D CAMERA
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

    postProcessing = std::make_unique<CompositionPipeline>(
        device,
        renderer.getSwapChainRenderPass(),
        renderer.getPostProcessingDescriptorSetLayout(),
        binaryDir+"composition"
    );
    
    UI imgui(device, renderer.getSwapChainRenderPass(), binaryDir+"imgui");

    // Load heavy assets on a separate thread
    std::thread([this]() {
        this->load_phase = 1;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_A.tga"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_N.tga", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_M.tga", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_R.tga", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Cerberus_AO.tga", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_A.png"));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_N.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_M.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_R.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/Waffle_AO.png", VK_FORMAT_R8G8B8A8_UNORM));
        this->load_phase = 2;
        this->textures.push_back( std::make_unique<Texture>(this->device, vulkanImage, "texture/hdri/spiaggia_di_mondello_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT));
        this->load_phase = 3;
        this->assetsLoaded = true;
    }).detach();
    
    // Loading Screen Rendering
    font.renderText("Vulkan Engine v0.7 Beta, MSAA up to " + std::to_string(device.msaaSamples) + "X, V-Sync: " + (renderer.isVSyncEnabled() ? "On" : "Off") , -.65f, -.9f, .4f, {.2f, .8f, 1.f}, aspect);
    font.renderText("Loading Vulkan Engine", .0f, .0f, 1.2f, { .7f, .0f, .0f}, aspect);
    textHolder.insert(textMeshes.begin(), textMeshes.end());
    font.renderText("Lorenzo Bozza's Works", .0f, .2f, .4f, { .8f, .8f, .8f}, aspect);

    bool nextIsLast = false;
    while (!assetsLoaded || load_phase > 0 || nextIsLast) {
        SDL_PollEvent(&sdl_event);
        
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
            postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getPostProcessingDescriptorSets()->at(frameIndex));
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
    
    font.renderText("Assets took " + std::to_string(std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - startTime).count()).substr(0, 4) + " seconds to load", .0f, -.9f, .6f, { .8f, .8f, .8f}, aspect);
    
    // Uniform Buffer Objects (GlobalUbo)
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

    // SKYBOX RENDERING DESCRIPTORS AND PIPELINE
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

    skyboxSystem = std::make_unique<RenderSystem>(
        device,
        renderer.getOffscreenRenderPass(),
        skyboxSetLayout->getDescriptorSetLayout(),
        binaryDir+"skybox",
        device.msaaSamples
    );
    
    // Load Descriptors for PBR Textures
    for (int i = 0; i < 10; i++) {
        textureInfos.push_back(textures.at(i)->descriptorInfo());
    }
    
    // GLOBAL RENDER SYSTEM DESCRIPTORS AND PIPELINE
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
            .writeImage(3, renderer.getBrdfLutInfo())
            .writeImage(4, textureInfos.data())
            .build(globalDescriptorSets[i]);
     }
    
    renderSystem = std::make_unique<RenderSystem>(
        device,
        renderer.getOffscreenRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        binaryDir+"shader",
        device.msaaSamples
    );
    
    SDL_Vulkan_GetDrawableSize(window.getWindow(), &surfaceExtent.width, &surfaceExtent.height);
    SDL_GetWindowSize(window.getWindow(), &windowExtent.width, &windowExtent.height);
    float dpi_scale_fact = surfaceExtent.width / windowExtent.width;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {(float)surfaceExtent.width, (float)surfaceExtent.height};
    io.FontGlobalScale = dpi_scale_fact;
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameBorderSize = 0.0f;
        style.WindowBorderSize = 0.0f;
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2f, 0.0f, 0.4f, 0.6f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.0f, 0.4f, 0.8f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.2f, 0.0f, 0.4f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.0f, 0.4f, 0.4f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        style.WindowRounding = 10.f;
        style.FrameRounding = 5.f;
        style.ScaleAllSizes(dpi_scale_fact);
    }
    
    bool running = true;
    auto counter4Hz = std::chrono::high_resolution_clock::now();
    bool mouseLeft = false;
    while(running)
    {
        cnt = ++cnt % 628;
        // Compute frame latency and store the value
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTimes.push_back(frameTime);
        if (frameTimes.size() > 50) {
            frameTimes.erase(frameTimes.begin());
        }
        frameTime = glm::min(frameTime, .05f); // Prevent movement glitches when resizing
        if (std::chrono::duration<float, std::chrono::seconds::period>(newTime -  counter4Hz).count() > .25f) {
            counter4Hz = newTime;
            float avg = 0;
            for (int k = 0; k < frameTimes.size(); k++) { avg += frameTimes[k]; }
            avg = avg / frameTimes.size();
            avg = 1.f / avg;
            framesPerSecond.push_back(avg);
            if (framesPerSecond.size() > 75) {
                framesPerSecond.erase(framesPerSecond.begin());
            }
        }

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
                    int wx, wy, mx, my;
                    SDL_GetWindowPosition(window.getWindow(), &wx, &wy);
                    SDL_GetGlobalMouseState(&mx, &my);
                    io.AddMousePosEvent((mx - wx) * dpi_scale_fact, (my - wy) * dpi_scale_fact);
                    if (mouseLeft && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
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

        // Update ImGui Buffer
        imgui.newFrame(this);
        
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
            
            imgui.updateBuffers(frameIndex);
            
            //RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer);
            skyboxSystem->renderSolidObjects(skyboxInfo);
            renderSystem->renderSolidObjects(frameInfo);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getPostProcessingDescriptorSets()->at(frameIndex));
            guiSystem.renderSolidObjects(guiInfo);
            imgui.draw(commandBuffer, frameIndex);
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
    block.transform.translation = {.0f, 1.f, .0f};
    //block.transform.scale = {.01f, .01f, .01f};
    //block.transform.rotation = {3.14f, 1.57f, .0f};
    solidObjects.emplace(block.getId(), std::move(block));
    
    // Cubemap 3D canvas
    auto cube = SolidObject::createSolidObject();
    cube.model = Model::createModelFromFile(device, binaryDir+"cube.obj");
    cube.transform.translation = {.0f, .0f, .0f};
    cube.transform.scale = {1.f, 1.f, 1.f};
    cube.transform.rotation = {.0f, .0f, .0f};
    env.emplace(cube.getId(), std::move(cube));
}

void Application::renderImguiContent() {
    ImGui::TextUnformatted(device.properties.deviceName);
    
    ImGui::SetNextWindowPos(ImVec2(20, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 1000), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoTitleBar);
    
    std::string label = std::to_string((int)framesPerSecond.rbegin()[0]) + " FPS";
    ImGui::PlotLines(label.c_str(), framesPerSecond.data(), (int)framesPerSecond.size(), 0, NULL, 0, FLT_MAX, {0, 100});
    ImGui::NewLine();
    ImGui::Text("Peak White Brightness:");
    ImGui::SliderFloat("##", &postProcessing->peak_brightness, 80.f, 1000.f);
    ImGui::Text("Gamma Correction:");
    ImGui::SliderFloat("##", &postProcessing->gamma, 1.f, 3.f);
    ImGui::NewLine();
    static bool vsync = SwapChain::enableVSync;
    ImGui::Checkbox(vsync ? "VSync Enabled" : "VSync Disabled", &vsync);
    if (SwapChain::enableVSync != vsync) {
        SwapChain::enableVSync = vsync;
        renderer.recreateSwapChain();
    }
    ImGui::NewLine();
    static int msaa = device.msaaSamples;
    ImGui::Text("Anti-Aliasing:");
    ImGui::RadioButton("No AA", &msaa, 1);
    for (int i = 2; i <= device.maxSampleCount; i *= 2) {
        ImGui::RadioButton(("MSAA "+std::to_string(i)+"X").c_str(), &msaa, i);
    }
    if (msaa != device.msaaSamples) {
        device.msaaSamples = (VkSampleCountFlagBits)msaa;
        renderer.recreateOffscreenFlag = true;
        renderer.recreateSwapChain();
        renderSystem->recreatePipeline(renderer.getOffscreenRenderPass(), device.msaaSamples);
        skyboxSystem->recreatePipeline(renderer.getOffscreenRenderPass(), device.msaaSamples);
    }
    ImGui::NewLine();
    float ddpi;
    SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr);
    ImGui::Text("Window size: %i x %i", WIDTH, HEIGHT);
    ImGui::Text("Actual window: %i x %i", windowExtent.width, windowExtent.height);
    ImGui::Text("Vulkan surface: %i x %i", surfaceExtent.width, surfaceExtent.height);
    ImGui::Text("Display DPI: %i", (int)ddpi);

    ImGui::End();
}
