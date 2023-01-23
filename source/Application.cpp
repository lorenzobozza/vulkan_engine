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
#include <cassert>
#include <chrono>
#include <iostream>
#include <future>

#define ENHANCED_MT

// Count Trailing Zeros
unsigned ctz(int n) {
    unsigned bits = 0, x = n;
    if (x) {
        /* mask the 8 low order bits, add 8 and shift them out if they are all 0 */
        if (!(x & 0x000000FF)) { bits +=  8; x >>=  8; }
        /* mask the 4 low order bits, add 4 and shift them out if they are all 0 */
        if (!(x & 0x0000000F)) { bits +=  4; x >>=  4; }
        /* mask the 2 low order bits, add 2 and shift them out if they are all 0 */
        if (!(x & 0x00000003)) { bits +=  2; x >>=  2; }
        /* mask the low order bit and add 1 if it is 0 */
        bits += (x & 1) ^ 1;
    }
    return bits;
}

Application::Application(const char* binaryPath) : binaryDir{binaryPath} {
    while(binaryDir.back() != '/' && !binaryDir.empty()) binaryDir.pop_back();
}

Application::~Application() {}

glm::vec3 rotate{.0f};

void Application::run() {

    // GAMELOOP TIMING
    auto currentTime = std::chrono::high_resolution_clock::now();
    int cnt{0};
    
    // 3D CAMERA
    Camera camera{};
    float aspect = renderer.getAspectRatio();
    camera.setProjection.perspective(aspect, glm::radians(75.f), .01f, 100.f);
    bool orth = false;
    // Create object without model for the 3D camera position
    SolidObject cameraObj = SolidObject::createSolidObject();
    cameraObj.transform.translation = {.0f, -2.f, -2.f};
    
    postProcessing = std::make_unique<CompositionPipeline>(
        device,
        renderer.getSwapChainRenderPass(),
        renderer.getPostProcessingDescriptorSetLayout(),
        binaryDir+"composition"
    );
    
    //TextRender font{device, renderer.getSwapChainRenderPass(), "fonts/Disket-Mono-Regular.ttf"};
    UI imgui(device, renderer.getSwapChainRenderPass(), binaryDir);

    // Load heavy assets on a separate thread
    std::thread([this]() {
        this->load_phase = 1;
        this->textures.emplace(0, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"texture/hdri/spiaggia_di_mondello_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT));
        textures.at(0)->moveBuffer();

        std::vector<std::string> materials = {
            "Arches", "Bricks", "Ceiling", "Column_A", "Column_B", "Column_C", "Details", "Fabric_Curtain_Blue",
            "Fabric_Curtain_Green", "Fabric_Curtain_Red", "Fabric_Round_Blue", "Fabric_Round_Green",
            "Fabric_Round_Red", "Flagpoles", "Floor", "Ivy", "Lion_Head", "Lion_Shield", "Roof", "Vase_Hanging",
            "Vase_Hanging_Chain", "Vase_Octagonal", "Vase_Round", "Vase_Round_Plants"
        };
        
        const size_t nTex = materials.size() * 5;
        textures.reserve(nTex + 1);
        
        #ifndef ENHANCED_MT
        
        for (int i = 0; i < materials.size(); i++) {
            uint16_t tex = i * 5;
            this->textures.emplace(++tex, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Diffuse.tif"));
            this->textures.at(tex)->moveBuffer();
            this->textures.emplace(++tex, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Normal.tif", VK_FORMAT_R8G8B8A8_UNORM));
            this->textures.at(tex)->moveBuffer();
            this->textures.emplace(++tex, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Metallic.tif"));
            this->textures.at(tex)->moveBuffer();
            this->textures.emplace(++tex, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Smoothness.tif"));
            this->textures.at(tex)->moveBuffer();
            this->textures.emplace(++tex, std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_AO.tif"));
            this->textures.at(tex)->moveBuffer();
        }
        
        #else
        
        // Load textures to host visible memory
        std::unique_ptr<Texture> staged[nTex];
        for (int i = 0; i < materials.size(); i++)
            std::thread([this, i, &materials, &staged]() {
            uint16_t tex = i * 5;
                staged[tex++] = std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Diffuse.tif");
                staged[tex++] = std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Normal.tif", VK_FORMAT_R8G8B8A8_UNORM);
                staged[tex++] = std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Metallic.tif");
                staged[tex++] = std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_Smoothness.tif");
                staged[tex] = std::make_unique<Texture>(this->device, vulkanImage, binaryDir+"sponza/textures/"+materials[i]+"/"+materials[i]+"_AO.tif");
            }).detach();
        
        // Move staged texture-buffers to VRAM as they get loaded (while also moving each associated pointer to the heap)
        int loaded = 0, i = 0;
        while(loaded < nTex) {
            if (staged[i] != nullptr) {
                staged[i]->moveBuffer(); // Host -> Device
                textures.emplace(i + 1, std::move(staged[i]));
                loaded++;
            }
            i = ++i % nTex;
        }
        
        #endif
        
        this->load_phase = 2;
        this->assetsLoaded = true;
    }).detach();
    
    // Loading Screen Rendering
    //font.renderText("Vulkan Engine V0.8", .0f, -.85f, 1.2f, { .7f, .0f, .0f}, aspect);
    //font.renderText("github.com/lorenzobozza/vulkan_engine", .0f, -.8f, .35f, { .8f, .8f, .8f}, aspect);

    bool nextIsLast = false;
    auto loadTimer = std::chrono::high_resolution_clock::now();
    while (!assetsLoaded || load_phase > 0 || nextIsLast) {
        SDL_PollEvent(&sdl_event);
        
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, .05f);

        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            //Render
            renderer.beginOffscreenRenderPass(commandBuffer);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            //postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getPostProcessingDescriptorSets()->at(frameIndex));
            //font.render(commandBuffer, frameIndex);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();

            if(nextIsLast) { nextIsLast = false; }
            switch(load_phase) {
                case 1:
                    DEBUG_MESSAGE("Loading Materials");
                    load_phase = 0;
                    break;
                case 2:
                    DEBUG_MESSAGE('\t' << std::chrono::duration<float, std::chrono::seconds::period>(newTime - loadTimer).count());
                    loadTimer = newTime;
                    DEBUG_MESSAGE("Loading Geometries");
                    load_phase = 0;
                    nextIsLast = true;
                    break;
                default:
                    break;
            }
        }
    }
    vkDeviceWaitIdle(device.device());
    renderer.integrateBrdfLut(binaryDir);
    loadSolidObjects();
    DEBUG_MESSAGE('\t' << std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - loadTimer).count());
    
    /****
    Global Uniform Buffer Objects
    */
    std::unique_ptr<Buffer> uboBuffers[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
         uboBuffers[i] = std::make_unique<Buffer>(
            device,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboBuffers[i]->map();
    }

    /****
    HDRi, IBL, SkyBox
    */
    auto equitangular = textures.at(0)->descriptorInfo();
    
    HDRi environmentMap{device, equitangular, {1024, 1024}, "equirectangular", binaryDir, 9};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{device, environment, {512, 512}, "prefiltering", binaryDir, 9};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{device, environment, {32, 32}, "irradiance", binaryDir};
    auto irradiance = irradianceMap.descriptorInfo();

    // SkyBox Descriptors
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
            
    std::vector<VkDescriptorSet> skyboxDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        skyboxDescriptorSets[i] = std::vector<VkDescriptorSet>(1);
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*skyboxSetLayout, *skyboxPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &environment)
            .build(skyboxDescriptorSets[i][0]);
    }

    // SkyBox Pipeline
    skyboxSystem = std::make_unique<RenderSystem>(
        device,
        renderer.getOffscreenRenderPass(),
        skyboxSetLayout->getDescriptorSetLayout(),
        binaryDir+"skybox",
        device.msaaSamples
    );
    
    /****
    Global Scene
    */
    for (int i = 1; i < textures.size(); i++) { textureInfos.push_back(textures.at(i)->descriptorInfo()); }
    
    const uint32_t numOfMaterials = (uint32_t)textureInfos.size() / 5;
    
    // Global Scene Descriptors
    globalPool =
       DescriptorPool::Builder(device)
           .setMaxSets(numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();

    auto globalSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    std::vector<VkDescriptorSet> inFlightDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<VkDescriptorSet> descriptorSets(numOfMaterials);
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        int matCnt = 0;
        for (int j = 0; j < numOfMaterials; j++) {
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &irradiance)                 // Irradiance
                .writeImage(2, &prefiltered)                // Reflection
                .writeImage(3, renderer.getBrdfLutInfo())   // BRDF Lut
                .writeImage(4, &textureInfos[matCnt++])     // Diffuse
                .writeImage(5, &textureInfos[matCnt++])     // Normal
                .writeImage(6, &textureInfos[matCnt++])     // Metallic
                .writeImage(7, &textureInfos[matCnt++])     // Roughness
                .writeImage(8, &textureInfos[matCnt++])     // Occlusion
                .build(descriptorSets[j]);
        }
        inFlightDescriptorSets[i] = descriptorSets;
    }
     
    // Global Scene Pipeline
    renderSystem = std::make_unique<RenderSystem>(
        device,
        renderer.getOffscreenRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        binaryDir+"shader",
        device.msaaSamples
    );
    
    // GUI Style and Sizes definition
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
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.8f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        style.WindowRounding = 10.f;
        style.FrameRounding = 5.f;
        style.ScaleAllSizes(dpi_scale_fact * 0.8f);
    }
    
    // Hide not supported anti-aliasing presets from GUI
    aaPresets.resize(1 + ctz(device.maxSampleCount));
    
    bool running = true;
    auto counter4Hz = std::chrono::high_resolution_clock::now();
    bool mouseLeft = false;
    uint8_t movement{0x00};
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
                case SDL_WINDOWEVENT:
                    if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        SDL_Vulkan_GetDrawableSize(window.getWindow(), &surfaceExtent.width, &surfaceExtent.height);
                        SDL_GetWindowSize(window.getWindow(), &windowExtent.width, &windowExtent.height);
                        renderer.recreateSwapChain();
                    }
                    break;
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    switch (sdl_event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                        case SDLK_w:
                            movement |= 0x01;
                            break;
                        case SDLK_a:
                            movement |= 0x02;
                            break;
                        case SDLK_s:
                            movement |= 0x04;
                            break;
                        case SDLK_d:
                            movement |= 0x08;
                            break;
                        case SDLK_LSHIFT:
                            movement |= 0x10;
                            break;
                        case SDLK_SPACE:
                            movement |= 0x20;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (sdl_event.key.keysym.sym) {
                        case SDLK_w:
                            movement &= 0xFE;
                            break;
                        case SDLK_a:
                            movement &= 0xFD;
                            break;
                        case SDLK_s:
                            movement &= 0xFB;
                            break;
                        case SDLK_d:
                            movement &= 0xF7;
                            break;
                        case SDLK_LSHIFT:
                            movement &= 0xEF;
                            break;
                        case SDLK_SPACE:
                            movement &= 0xDF;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    SDL_GameControllerOpen(0);
                    //font.renderText(SDL_GameControllerNameForIndex(0), -.1f, -.95f, .1f);
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
                case SDL_MOUSEWHEEL:
                    io.AddMouseWheelEvent(sdl_event.wheel.preciseX, sdl_event.wheel.preciseX);
                    break;
            }
        }
        
        if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
            cameraObj.transform.rotation += rotate * .05f;
            cameraObj.transform.rotation.x = glm::clamp(cameraObj.transform.rotation.x, -1.5f, 1.5f);
            cameraObj.transform.rotation.y = glm::mod(cameraObj.transform.rotation.y, glm::two_pi<float>());
            rotate = glm::vec3{.0f};
        }
        
        if (movement) {
            float yaw = cameraObj.transform.rotation.y;
            const glm::vec3 forwardDir{glm::sin(yaw), .0f, glm::cos(yaw)};
            const glm::vec3 rightDir{forwardDir.z, .0f, -forwardDir.x};
            const glm::vec3 upDir{.0f, -1.f, .0f};
            glm::vec3 moveDir{0.f};
            if (movement & 0x01) { moveDir += forwardDir; }
            if (movement & 0x02) { moveDir -= rightDir; }
            if (movement & 0x04) { moveDir -= forwardDir; }
            if (movement & 0x08) { moveDir += rightDir; }
            if (movement & 0x10) { moveDir -= upDir; }
            if (movement & 0x20) { moveDir += upDir; }
            if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
                cameraObj.transform.translation += 8.f * frameTime * glm::normalize(moveDir);
            }
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

        // Prepare next GUI Frame
        imgui.newFrame(this);
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                inFlightDescriptorSets[frameIndex],
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
            
            // Update UBO
            ubo.projectionView = frameInfo.camera.getProjection();
            ubo.viewMatrix = frameInfo.camera.getView();
            ubo.invViewMatrix = frameInfo.camera.getInverseView();
            //ubo.lightPosition = pos;
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            
            // Update UI Buffer
            imgui.updateBuffers(frameIndex);
            
            // RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer);
            skyboxSystem->renderSolidObjects(skyboxInfo);
            renderSystem->renderSolidObjects(frameInfo);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getPostProcessingDescriptorSets()->at(frameIndex));
            //font.render(commandBuffer, frameIndex);
            imgui.draw(commandBuffer, frameIndex);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();
        }
    }

    vkDeviceWaitIdle(device.device());
}

void Application::loadSolidObjects() {
    std::vector<std::string> meshNames = {
        "arches", "brickwalls", "ceilings", "columns_a", "columns_b", "columns_c",
        "details", "fabric_curtains_blue", "fabric_curtains_green", "fabric_curtains_red",
        "fabric_rounds_blue", "fabric_rounds_green", "fabric_rounds_red", "flagpoles",
        "floors", "ivys", "lion_heads", "lion_shields", "roofs", "vases_hanging",
        "vases_hanging_chain", "vases_octagonal", "vases_round", "vases_round_plants"
    };

    for (int i = 0; i < meshNames.size(); i++) {
        auto group = SolidObject::createSolidObject();
        group.model = Model::createModelFromFile(device, binaryDir + "sponza/sponza_" + meshNames[i] + ".obj");
        group.textureIndex = i;
        solidObjects.emplace(group.getId(), std::move(group));
    }
    
    // Cubemap 3D canvas
    auto cube = SolidObject::createSolidObject();
    Model::Data cubeData;
    cubeData.vertices = {
        {{-1.f, -1.f, 1.f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, 1.f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, 1.f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, 1.f}, {}, {}, {}, {0.f, 1.f}},
        {{-1.f, -1.f, -1.f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, -1.f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, -1.f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, -1.f}, {}, {}, {}, {0.f, 1.f}}
    };
    cubeData.indices = {
        0,2,1,2,0,3,
        4,5,6,6,7,4,
        1,6,5,6,1,2,
        0,4,7,7,3,0,
        4,1,5,1,4,0,
        3,6,2,6,3,7
    };
    cube.model = std::make_unique<Model>(device, cubeData);
    cube.textureIndex = 0;
    cube.transform.translation = {.0f, .0f, .0f};
    cube.transform.scale = {1.f, 1.f, 1.f};
    cube.transform.rotation = {.0f, .0f, .0f};
    env.emplace(cube.getId(), std::move(cube));
}

void Application::renderImguiContent() {
    static auto counter10Hz = std::chrono::high_resolution_clock::now();
    
    ImGui::TextUnformatted(device.properties.deviceName);
    float ddpi;
    SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr);
    ImGui::Text("Window size: %i x %i", WIDTH, HEIGHT);
    ImGui::Text("Actual window: %i x %i", windowExtent.width, windowExtent.height);
    ImGui::Text("Vulkan surface: %i x %i", surfaceExtent.width, surfaceExtent.height);
    ImGui::Text("Display DPI: %i", (int)ddpi);
    
    /**** SETTINGS WINDOW **/
    
    ImGui::SetNextWindowPos(ImVec2(20, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 1000), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoTitleBar);
    
    std::string label = std::to_string((int)framesPerSecond.rbegin()[0]) + " FPS";
    ImGui::PlotLines(label.c_str(), framesPerSecond.data(), (int)framesPerSecond.size(), 0, NULL, 0, FLT_MAX, {0, 100});
    
    static float frameTime = frameTimes.back() * 1000.f;
    if(std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - counter10Hz).count() > 100.f) {
        frameTime = frameTimes.back() * 1000.f;
        counter10Hz = std::chrono::high_resolution_clock::now();
    }
    ImGui::Text("Frametime %.2f", frameTime);
    
    ImGui::NewLine();
    ImGui::Text("FIF: %i", SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    ImGui::NewLine();
    static bool vsync = SwapChain::enableVSync;
    ImGui::Checkbox(vsync ? "VSync Enabled" : "VSync Disabled", &vsync);
    if (SwapChain::enableVSync != vsync) {
        SwapChain::enableVSync = vsync;
        renderer.recreateSwapChain();
    }
    
    ImGui::NewLine();
    static int aaIndex = ctz(device.msaaSamples);
    ImGui::Text("Anti-Aliasing");
    if (ImGui::Combo("##antialiasing", &aaIndex, aaPresets.data(), (int)aaPresets.size())) {
        device.msaaSamples = static_cast<VkSampleCountFlagBits>(1 << aaIndex);
        renderer.recreateOffscreenFlag = true;
        renderer.recreateSwapChain();
        renderSystem->recreatePipeline(renderer.getOffscreenRenderPass(), device.msaaSamples);
        skyboxSystem->recreatePipeline(renderer.getOffscreenRenderPass(), device.msaaSamples);
    }
    
    ImGui::NewLine();
    ImGui::Text("Peak White Brightness");
    ImGui::SliderFloat("##brightness", &postProcessing->peak_brightness, 80.f, 1000.f);
    ImGui::Text("Gamma Correction");
    ImGui::SliderFloat("##gamma", &postProcessing->gamma, 1.f, 3.f);
    
    ImGui::NewLine();
    float color[4] = {ubo.lightColor.r, ubo.lightColor.g, ubo.lightColor.b, ubo.lightColor.a};
    ImGui::ColorEdit3("Light Color", color);
    ImGui::SliderFloat("##strength", &color[3], 1.f, 100.f);
    ubo.lightColor = {color[0], color[1], color[2], color[3]};
    
    glm::vec3 lightPos = ubo.lightPosition;
    ImGui::SliderFloat("LPosX", &lightPos.x, -3.f, 3.f);
    ImGui::SliderFloat("LPosY", &lightPos.y, -.5f, -5.f);
    ImGui::SliderFloat("LPosZ", &lightPos.z, -4.f, 4.f);
    ubo.lightPosition = lightPos;

    ImGui::End();
}
