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
#include "include/importGLTF.hpp"

#include "include/Material.hpp"

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
    

    Camera camera{};
    float aspectRatio = renderer.getAspectRatio();
    camera.setProjection.perspective(aspectRatio, glm::radians(75.f), .01f, 100.f);
    
    Primitive cameraObj = Primitive::new_primitive();
    {
        cameraObj.transform.translation = {.0f, -2.f, -2.f};
    }
    bool orth = false;
    
    postProcessing = std::make_unique<CompositionPipeline>(
        vulkanDevice,
        renderer.getSwapChainRenderPass(),
        renderer.getPostProcessingDescriptorSetLayout(),
        binaryDir + "composition"
    );
    
    //TextRender font{vulkanDevice, renderer.getSwapChainRenderPass(), "fonts/Disket-Mono-Regular.ttf"};
    UI imgui(vulkanDevice, renderer.getSwapChainRenderPass(), binaryDir);
    
    // GUI Style and Sizes definition
    SDL_Vulkan_GetDrawableSize(window.getWindow(), &surfaceExtent.width, &surfaceExtent.height);
    SDL_GetWindowSize(window.getWindow(), &windowExtent.width, &windowExtent.height);
    dpi_scale_fact = surfaceExtent.width / windowExtent.width;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {(float)surfaceExtent.width, (float)surfaceExtent.height};
    io.FontGlobalScale = dpi_scale_fact * (windowExtent.width / 1920.f);
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameBorderSize = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.8f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        style.WindowRounding = 4.f;
        style.FrameRounding = 5.f;
        style.ScaleAllSizes(dpi_scale_fact * 0.8f);
    }

    // Load heavy assets on a separate thread
    std::thread([this]() {
        this->load_phase = 1;
        
        this->testure.push_back(std::make_unique<Texture>(this->vulkanDevice, vulkanImage, binaryDir+"texture/hdri/symmetrical_garden_02_8k.hdr", false, VK_FORMAT_R32G32B32A32_SFLOAT));
        //this->testure.back()->moveBuffer();
        
        Material globalMaterial(&testure);
        globalMaterial.color = {1.f, 0.f, 1.f, 1.f};
        materials.emplace("Global_Default_Material", globalMaterial);
        
        NodeSet::InitStruct initNodeStruct{vulkanDevice, vulkanImage, primitives, testure, materials};
        
        //NodeSet(initNodeStruct, binaryDir + "chess.glb");
        NodeSet(initNodeStruct, binaryDir + "Sponza.glb");
        

        // Cubemap 3D canvas
        auto cube = Primitive::new_primitive();
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
        cube.setModel(std::make_shared<Model>(vulkanDevice, cubeData));
        cube.textureIndex = 1;
        cube.material = "SKY";
        cube.transform.translation = {.0f, .0f, .0f};
        cube.transform.scale = {1.f, 1.f, 1.f};
        cube.transform.rotation = {.0f, .0f, .0f};
        env.emplace(cube.getId(), std::move(cube));
        
        this->load_phase = 2;
        this->assetsLoaded = true;
    }).detach();
    
    // Loading Screen Rendering
    //font.renderText("Vulkan Engine V0.8", .0f, -.85f, 1.2f, { .7f, .0f, .0f}, aspectRatio);
    //font.renderText("github.com/lorenzobozza/vulkan_engine", .0f, -.8f, .35f, { .8f, .8f, .8f}, aspectRatio);

    bool nextIsLast = false;
    auto loadTimer = std::chrono::high_resolution_clock::now();
    while (!assetsLoaded || load_phase > 0 || nextIsLast) {
        //SDL_PollEvent(&sdl_event);
        
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, .05f);

        pollWindowEvents();
        
        imgui.newFrame(this);
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            imgui.updateBuffers(frameIndex);
            
            //Render
            renderer.beginOffscreenRenderPass(commandBuffer);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            //postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getPostProcessingDescriptorSets()->at(frameIndex));
            //font.render(commandBuffer, frameIndex);
            imgui.draw(commandBuffer, frameIndex);
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
    
    for (auto& t : testure) {
        t->moveBuffer();
    }
    
    vkDeviceWaitIdle(vulkanDevice.device());
    renderer.integrateBrdfLut(binaryDir);
    //loadSolidObjects();
    DEBUG_MESSAGE('\t' << std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - loadTimer).count());
    
    /****
    Global Uniform Buffer Objects
    */
    std::unique_ptr<Buffer> uboBuffers[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
         uboBuffers[i] = std::make_unique<Buffer>(
            vulkanDevice,
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
    auto equitangular = testure.at(0)->descriptorInfo();
    
    HDRi environmentMap{vulkanDevice, equitangular, {1024, 1024}, "equirectangular", binaryDir, 9};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{vulkanDevice, environment, {512, 512}, "prefiltering", binaryDir, 9};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{vulkanDevice, environment, {32, 32}, "irradiance", binaryDir};
    auto irradiance = irradianceMap.descriptorInfo();

    // SkyBox Descriptors
    std::unique_ptr<DescriptorPool> skyboxPool =
       DescriptorPool::Builder(vulkanDevice)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    auto skyboxSetLayout =
        DescriptorSetLayout::Builder(vulkanDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
            
    std::unordered_map<std::string, VkDescriptorSet> skyboxDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorSet descriptorSet;
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*skyboxSetLayout, *skyboxPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &environment)
            .build(descriptorSet);
            
        skyboxDescriptorSets[i].emplace("SKY", descriptorSet);
    }

    // SkyBox Pipeline
    skyboxSystem = std::make_unique<RenderSystem>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(),
        skyboxSetLayout->getDescriptorSetLayout(),
        binaryDir+"skybox",
        vulkanDevice.msaaSamples
    );
    
    /****
    Global Scene
    */
    //for (int i = 1; i < textures.size(); i++) { textureInfos.push_back(textures.at(i)->descriptorInfo()); }
    
    const uint32_t numOfMaterials = (uint32_t)materials.size();
    //(uint32_t)textureInfos.size() / 3;
    
    // Global Scene Descriptors
    globalPool =
       DescriptorPool::Builder(vulkanDevice)
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
        DescriptorSetLayout::Builder(vulkanDevice)
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
    
    // Create a Descriptor Map for each frame in flight
    std::unordered_map<std::string, VkDescriptorSet> inFlightDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        for (auto& kv : materials) {
            auto& name = kv.first;
            auto& material = kv.second;
            VkDescriptorSet descriptorSet;
            VkDescriptorImageInfo   emissive = material.getColorTextureIF(),
                                    normal = material.getNormalTextureIF(),
                                    occlusion = material.getOcclusionTextureIF(),
                                    metalRough = material.getMetalRoughTextureIF(),
                                    def = testure.at(0)->descriptorInfo();
                                    
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &irradiance)                 // Irradiance
                .writeImage(2, &prefiltered)                // Reflection
                .writeImage(3, renderer.getBrdfLutInfo())   // BRDF Lut
                .writeImage(4, &emissive)       // Diffuse
                .writeImage(5, &normal)         // Normal
                .writeImage(6, &metalRough)     // Metallic
                .writeImage(7, &def)            // Roughness
                .writeImage(8, &occlusion)      // Occlusion
                .build(descriptorSet);
                
            inFlightDescriptorSets[i].emplace(name, descriptorSet);
        }
    }
     
    // Global Scene Pipeline
    renderSystem = std::make_unique<RenderSystem>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        binaryDir+"shader",
        vulkanDevice.msaaSamples
    );
    
    
    
    // Hide not supported anti-aliasing presets from GUI
    aaPresets.resize(1 + ctz(vulkanDevice.maxSampleCount));
    
    auto counter4Hz = std::chrono::high_resolution_clock::now();
    
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
        
        m_Perf.startFrame();
        
        // Prepare next GUI Frame
        imgui.newFrame(this);
        
        pollWindowEvents();
        
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
        if (aspectRatio != renderer.getAspectRatio()) {
            aspectRatio = renderer.getAspectRatio();
            if (orth) {
                camera.setOrthographicProjection(-aspectRatio, aspectRatio, -1.f, 1.f, -10.f, 100.f);
            } else {
                camera.setProjection.perspective(aspectRatio);
            }
        }
        
        // Polling keystrokes and adjusting the camera position/rotation
        camera.setViewYXZ(cameraObj.transform.translation, cameraObj.transform.rotation);
        
        m_Perf.cpuEnd();
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                inFlightDescriptorSets[frameIndex],
                primitives,
                materials
            };
            
            FrameInfo skyboxInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                skyboxDescriptorSets[frameIndex],
                env,
                materials
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
        
        m_Perf.gpuEnd();
    
    }
    vkDeviceWaitIdle(vulkanDevice.device());
    
}

void Application::pollWindowEvents(void) {
    ImGuiIO& io = ImGui::GetIO();
    bool mouseLeft = false;
    while(SDL_PollEvent(&sdl_event))
    {
        switch (sdl_event.type) {
            case SDL_WINDOWEVENT:
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    DEBUG_MESSAGE("Window resize event detected!");
                    SDL_Vulkan_GetDrawableSize(window.getWindow(), &surfaceExtent.width, &surfaceExtent.height);
                    SDL_GetWindowSize(window.getWindow(), &windowExtent.width, &windowExtent.height);
                    renderer.recreateSwapChain();
                    dpi_scale_fact = surfaceExtent.width / windowExtent.width;
                    io.DisplaySize = {(float)surfaceExtent.width, (float)surfaceExtent.height};
                    io.FontGlobalScale = dpi_scale_fact * (windowExtent.width / 1920.f);
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
}

void Application::loadSolidObjects() {
    
}

void Application::renderImguiContent() {
    static auto counter10Hz = std::chrono::high_resolution_clock::now();
    
    {   // Load model window
        char charbuf[64];
        if (ImGui::Begin("Load Model")) {
            ImGui::InputText("GLB Filename", charbuf, 64);
            ImGui::End();
        }
    }
    
    static bool showMaterials = false;
    ImGui::Checkbox("Show Materials Table", &showMaterials);
    if (showMaterials && ImGui::BeginTable("material_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Name");
        ImGui::TableNextColumn(); ImGui::Text("Color");
        ImGui::TableNextColumn(); ImGui::Text("Normal");
        ImGui::TableNextColumn(); ImGui::Text("Occlusion");
        ImGui::TableNextColumn(); ImGui::Text("Metal/Rough");
        for (auto& kv: materials) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", kv.first.c_str());
            ImGui::TableNextColumn();
            if ((kv.second.getTextureBitmap() & 0x1) == 0) {ImGui::ColorButton("", ImVec4(kv.second.color.r,kv.second.color.g,kv.second.color.b,kv.second.color.a));}
            else { ImGui::Text("%s","Texture"); }
            ImGui::TableNextColumn();
            ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x2) == 0 ? "NO" : "Texture");
            ImGui::TableNextColumn();
            ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x4) == 0 ? "NO" : "Texture");
            ImGui::TableNextColumn();
            ImGui::Text("%s", (kv.second.getTextureBitmap() & 0x8) == 0 ? ( "M: " + std::to_string(kv.second.metalness) + ", R: " + std::to_string(kv.second.roughness) ).c_str() : "Texture");
        }
        ImGui::EndTable();
    }
    
    ImGui::TextUnformatted(vulkanDevice.properties.deviceName);
    float ddpi;
    SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr);
    ImGui::Text("Actual window size\t %i x %i", windowExtent.width, windowExtent.height);
    ImGui::Text("Vulkan surface size\t %i x %i", surfaceExtent.width, surfaceExtent.height);
    ImGui::Text("Display DPI\t %i", (int)ddpi);
    
    /**** SETTINGS WINDOW **/
    
    ImGui::SetNextWindowPos(ImVec2(20, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(windowExtent.width*.25f, windowExtent.height*.75f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoTitleBar);
    
    std::string label = std::to_string((int)framesPerSecond.rbegin()[0]) + " FPS";
    ImGui::PlotLines(label.c_str(), framesPerSecond.data(), (int)framesPerSecond.size(), 0, NULL, 0, FLT_MAX, {0, 100});
    
    static float frameTime = frameTimes.back() * 1000.f;
    if(std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - counter10Hz).count() > 100.f) {
        frameTime = frameTimes.back() * 1000.f;
        counter10Hz = std::chrono::high_resolution_clock::now();
    }
    ImGui::Text("CPU Time %.2fms", m_Perf.cpuTime * 1000.f);
    ImGui::Text("GPU Time %.2fms", m_Perf.gpuTime * 1000.f);
    
    ImGui::NewLine();
    ImGui::Text("FIF: %i", SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    ImGui::NewLine();
    static int windowMode = 0;
    if (ImGui::Combo("##fullscreen", &windowMode, "Windowed\0Windowed Borderless\0Full Screen\0")) {
        switch (windowMode) {
            case 0:
            window.setWindowFullScreen(0);
            renderer.recreateOffscreenFlag = VK_TRUE;
                break;
            case 1:
            window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            case 2:
            window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN);
                break;
        }
    }
    static int res = 0;
    if (ImGui::Combo("##resolution", &res, window.supportedResNames.c_str())) {
        window.setWindowExtent(window.supportedModes[res].w, window.supportedModes[res].h);
        if (windowMode == 2) {
            window.setWindowFullScreen(SDL_WINDOW_FULLSCREEN);
        }
    }
    
    ImGui::NewLine();
    static bool vsync = SwapChain::enableVSync;
    ImGui::Checkbox(vsync ? "VSync Enabled" : "VSync Disabled", &vsync);
    if (SwapChain::enableVSync != vsync) {
        SwapChain::enableVSync = vsync;
        renderer.recreateSwapChain();
    }
    
    ImGui::NewLine();
    static int aaIndex = ctz(vulkanDevice.msaaSamples);
    ImGui::Text("Anti-Aliasing");
    if (ImGui::Combo("##antialiasing", &aaIndex, aaPresets.data(), (int)aaPresets.size())) {
        vulkanDevice.msaaSamples = static_cast<VkSampleCountFlagBits>(1 << aaIndex);
        renderer.recreateOffscreenFlag = true;
        renderer.recreateSwapChain();
        renderSystem->recreatePipeline(renderer.getOffscreenRenderPass(), vulkanDevice.msaaSamples);
        skyboxSystem->recreatePipeline(renderer.getOffscreenRenderPass(), vulkanDevice.msaaSamples);
    }
    
    ImGui::NewLine();
    ImGui::Text("Exposure");
    ImGui::SliderFloat("##exposure", &postProcessing->exposure, 1.f, 5.f);
    ImGui::Text("Peak White Brightness");
    ImGui::SliderFloat("##brightness", &postProcessing->peak_brightness, 1.f, 15.f);
    ImGui::Text("Gamma Correction");
    ImGui::SliderFloat("##gamma", &postProcessing->gamma, 1.f, 3.f);
    
    ImGui::NewLine();
    float color[4] = {ubo.lightColor.r, ubo.lightColor.g, ubo.lightColor.b, ubo.lightColor.a};
    ImGui::ColorEdit3("Light Color", color);
    ImGui::SliderFloat("##strength", &color[3], 1.f, 100.f);
    ubo.lightColor = {color[0], color[1], color[2], color[3]};
    
    glm::vec4 lightPos = ubo.lightPosition[0];
    ImGui::SliderFloat("LPosX", &lightPos.x, -3.f, 3.f);
    ImGui::SliderFloat("LPosY", &lightPos.y, -.5f, -5.f);
    ImGui::SliderFloat("LPosZ", &lightPos.z, -4.f, 4.f);
    ubo.lightPosition[0] = lightPos;
    ubo.lightPosition[1].z = - lightPos.z;

    ImGui::End();
}
