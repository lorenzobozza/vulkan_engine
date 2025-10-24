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

#include <imgui_internal.h>

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

void Application::run() {

    // GAMELOOP TIMING
    auto currentTime = std::chrono::high_resolution_clock::now();

    Camera camera{};
    float aspectRatio = renderer.getAspectRatio();
    camera.setProjection.perspective(aspectRatio, glm::radians(75.f), .01f, 100.f);
    
    Primitive cameraObj = Primitive::new_primitive();
    cameraObj.transform.translation = {.0f, -2.f, .0f};
    cameraObj.transform.rotation.y = glm::half_pi<float>();
    bool orth = false;
    
    postProcessing = std::make_unique<CompositionPipeline>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::ScreenSpace),
        renderer.getDescriptorSetLayout(RenderPass::WorldSpace),
        "composition"
    );
    
    //TextRender font{vulkanDevice, renderer.getSwapChainRenderPass(), "fonts/Disket-Mono-Regular.ttf"};
    UI imgui(vulkanDevice, renderer);
    
    // GUI Style and Sizes definition
    {
        SDL_Vulkan_GetDrawableSize(window.getWindow(), &surfaceExtent.width, &surfaceExtent.height);
        SDL_GetWindowSize(window.getWindow(), &windowExtent.width, &windowExtent.height);
        float dpi_scale_fact = surfaceExtent.width / windowExtent.width;
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = {(float)surfaceExtent.width, (float)surfaceExtent.height};
        io.FontGlobalScale = dpi_scale_fact * (windowExtent.width / 1920.f);
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(dpi_scale_fact * 0.8f);
        UI::setBessDarkColors();
    }
    
    
    std::thread([this]() {
    
        textures.push_back(std::make_unique<Texture>(
            this->vulkanDevice,
            vulkanImage,
            "../../../assets/textures/symmetrical_garden_02_8k.hdr",
            false,
            VK_FORMAT_R32G32B32A32_SFLOAT
        ));
        
        Material globalMaterial(&textures);
        globalMaterial.color = {1.f, 0.f, 1.f, 1.f};
        materials.emplace("Global_Default_Material", globalMaterial);
    
        // Multithreaded job, migliorare la creazione dei task-sets
        NodeSet::InitStruct initNodeStruct{vulkanDevice, vulkanImage, primitives, textures, materials};
        NodeSet(initNodeStruct, binaryDir + "Sponza.glb");

        // Cubemap 3D canvas
        auto cube = Primitive::new_primitive();
        cube.setModel(std::make_shared<Model>(vulkanDevice, Model::Data::makeSimpleCube()));
        cube.textureIndex = 1;
        cube.material = "SKY";
        env.emplace(cube.getId(), std::move(cube));
        
        assetsLoaded = true;
        
    }).detach();
        
    while (!assetsLoaded) {

        window.pollWindowEvents([this](){ renderer.recreateSwapChain(); });
        
        imgui.newFrame(this);
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            imgui.updateBuffers(frameIndex);
            
            //Render
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            imgui.draw(commandBuffer, frameIndex);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();
        }
    }
    
    for (auto& t : textures) {
        t->moveBuffer();
    }
    
    vkDeviceWaitIdle(vulkanDevice.device());
    renderer.integrateBrdfLut(binaryDir);
    
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

/**** HDRi, IBL, SkyBox  */
    auto equitangular = textures.at(0)->descriptorInfo();
    
    HDRi environmentMap{vulkanDevice, equitangular, {1024, 1024}, "equirectangular", binaryDir, 9};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{vulkanDevice, environment, {512, 512}, "prefiltering", binaryDir, 9};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{vulkanDevice, environment, {32, 32}, "irradiance", binaryDir};
    auto irradiance = irradianceMap.descriptorInfo();

/**** SkyBox Descriptors */
    DescriptorSetLayout skyboxSetLayout = DescriptorSetLayout::Builder(vulkanDevice.device())
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    // SkyBox Pipeline
    skyboxSystem = std::make_unique<RenderSystem>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
        skyboxSetLayout.getDescriptorSetLayout(),
        "skybox",
        vulkanDevice.msaaSamples
    );
            
    DescriptorPool skyboxPool = DescriptorPool::Builder(vulkanDevice.device())
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
            
    std::unordered_map<std::string, VkDescriptorSet> skyboxDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorSet descriptorSet;
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(skyboxSetLayout, skyboxPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &environment)
            .build(descriptorSet);
            
        skyboxDescriptorSets[i].emplace("SKY", std::move(descriptorSet));
    }

    
/**** Global Pipeline */
    DescriptorSetLayout globalSetLayout = DescriptorSetLayout::Builder(vulkanDevice.device())
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
            
    // Pipeline
    renderSystem = std::make_unique<RenderSystem>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
        globalSetLayout.getDescriptorSetLayout(),
        "shader",
        vulkanDevice.msaaSamples
    );
    
    const uint32_t numOfMaterials = (uint32_t)materials.size();
    
    DescriptorPool globalPool = DescriptorPool::Builder(vulkanDevice.device())
           .setMaxSets(numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
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
                                    metalRough = material.getMetalRoughTextureIF();
                                    
            DescriptorWriter(globalSetLayout, globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &irradiance)                 // Irradiance
                .writeImage(2, &prefiltered)                // Reflection
                .writeImage(3, renderer.getBrdfLutInfo())   // BRDF Lut
                .writeImage(4, &emissive)       // Diffuse
                .writeImage(5, &normal)         // Normal
                .writeImage(6, &metalRough)     // Metallic-Roughness
                .writeImage(7, &occlusion)      // Occlusion
                .build(descriptorSet);
                
            inFlightDescriptorSets[i].emplace(name, std::move(descriptorSet));
        }
    }
    
    
    
    // Hide not supported anti-aliasing presets from GUI
    aaPresets.resize(1 + ctz(vulkanDevice.maxSampleCount));
    
    auto counter4Hz = std::chrono::high_resolution_clock::now();
    
    while(window.isWindowOpen())
    {
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
        
        window.pollWindowEvents([this](){ renderer.recreateSwapChain(); });
        
        // Prepare next GUI Frame
        imgui.newFrame(this);
        
        glm::vec3 rotate = window.getRotation();
        if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
            cameraObj.transform.rotation += rotate * .05f;
            cameraObj.transform.rotation.x = glm::clamp(cameraObj.transform.rotation.x, -1.5f, 1.5f);
            cameraObj.transform.rotation.y = glm::mod(cameraObj.transform.rotation.y, glm::two_pi<float>());
        }
        
        uint8_t movement = window.getMovement();
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
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            skyboxSystem->renderSolidObjects(skyboxInfo);
            renderSystem->renderSolidObjects(frameInfo);
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            postProcessing->renderSceneToSwapChain(commandBuffer, renderer.getDescriptorSets(RenderPass::WorldSpace)->at(frameIndex));
            renderer.endOffscreenRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            imgui.draw(commandBuffer, frameIndex);
            renderer.endSwapChainRenderPass(commandBuffer);
            
            renderer.endFrame();
        }
        
        m_Perf.gpuEnd();
    
    }
    vkDeviceWaitIdle(vulkanDevice.device());
    
}

void Application::renderImguiContent() {
    static auto counter10Hz = std::chrono::high_resolution_clock::now();
    
    int flags = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ? 0 : ImGuiWindowFlags_NoMouseInputs;
    flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;
    float height = (float)renderer.getSwapChainExtent().height * .66f;
    float width = (float)renderer.getSwapChainExtent().width * .66f;
    ImGui::SetNextWindowContentSize(ImVec2(width, height));
    if (ImGui::Begin("Viewport", nullptr, flags)) {
        ImGuiDockNode* id = ImGui::GetWindowDockNode();
        id->LocalFlags |= ImGuiDockNodeFlags_NoResize;
        
        ImGui::Image( (void*)(intptr_t) 1, ImVec2(width, height) );

        ImGui::End();
    }

    
    UI::OnImGui(binaryDir);
    
    static bool showMaterials = false;
    if (ImGui::Begin("Materials")) {
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
        if (ImGui::Button("Open...")) {
            std::string file = window.openFileDialog("/");
        }
        ImGui::End();
    }
    
    float ddpi;
    SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr);
    
    /**** SETTINGS WINDOW **/
    if (ImGui::Begin("Settings")) {
    
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
            renderSystem->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
            skyboxSystem->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
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
}
