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
#include "Widgets.hpp"

//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <imgui_internal.h>

#include <ft2build.h>
#include FT_FREETYPE_H

//std
#include <thread>

struct WidgetStruct {
    std::shared_ptr<Viewport> view;
    std::shared_ptr<LogView> log;
    std::shared_ptr<AssetTree> assets;
    std::shared_ptr<MeterialViewer> material;
    std::shared_ptr<Settings> settings;
    std::shared_ptr<Menu> menu;
};


Application::Application(const char* binaryPath) : binaryDir{binaryPath} {
    while(binaryDir.back() != '/' && !binaryDir.empty()) binaryDir.pop_back();
}

void Application::run() {
    UI ui(vulkanDevice, renderer);
    
    WidgetStruct widgets {
        .view = std::make_shared<Viewport>(),
        .log = std::make_shared<LogView>(),
        .assets = std::make_shared<AssetTree>(),
        .material = std::make_shared<MeterialViewer>(materials),
        .settings = std::make_shared<Settings>(vulkanDevice,window,renderer,m_Perf)
    };
    widgets.menu = std::make_shared<Menu>(widgets.log->getVisibility(), widgets.material->getVisibility());
    ui.addWidgets(widgets.view, widgets.log, widgets.assets, widgets.material, widgets.settings, widgets.menu);
    
    widgets.log->getVisibility() = false;
    widgets.material->getVisibility() = false;
    widgets.view->setExtent(renderer.getSwapChainExtent().width * 0.66f, renderer.getSwapChainExtent().height * 0.66f);
    widgets.view->addFlags(ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground);

        Camera camera{};
        float aspectRatio = renderer.getAspectRatio();
        camera.setProjection.perspective(aspectRatio, glm::radians(75.f), .01f, 100.f);
        
        Primitive cameraObj = Primitive::new_primitive();
        cameraObj.transform.translation = {.0f, -2.f, .0f};
        cameraObj.transform.rotation.y = glm::half_pi<float>();
        bool orth = false;
    
    renderSystems.composit = std::make_unique<CompositionPipeline>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::ScreenSpace),
        renderer.getDescriptorSetLayout(RenderPass::WorldSpace),
        "composition"
    );
    
    
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
        
        ui.newFrame();
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            ui.updateBuffers(frameIndex);
            
            //Render
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::DepthPass);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            ui.draw(commandBuffer, frameIndex);
            renderer.endRenderPass(commandBuffer);
            
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
    renderSystems.skybox = std::make_unique<RenderSystem>(
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
    
    
    //Depth only pass
    DescriptorSetLayout depthSetLayout = DescriptorSetLayout::Builder(vulkanDevice.device())
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();
            
    // Pipeline
    renderSystems.depth = std::make_unique<RenderSystem>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::DepthPass),
        depthSetLayout.getDescriptorSetLayout(),
        "depth",
        vulkanDevice.msaaSamples
    );
    
    DescriptorPool depthPool = DescriptorPool::Builder(vulkanDevice.device())
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    // Create a Descriptor Map for each frame in flight
    VkDescriptorSet depthDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
                                    
        DescriptorWriter(depthSetLayout, depthPool)
            .writeBuffer(0, &bufferInfo)
            .build(depthDescriptorSets[i]);
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
    renderSystems.pbr = std::make_unique<RenderSystem>(
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
    
    widgets.settings->recreatePipelinesCallback([this](void){
        renderSystems.pbr->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
        renderSystems.skybox->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
    });

    
    while(window.isWindowOpen())
    {
        
        m_Perf.startFrame();
        
        window.pollWindowEvents([this](){ renderer.recreateSwapChain(); });
        
        // Prepare next GUI Frame
        ui.newFrame();
        
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
                cameraObj.transform.translation += 8.f * m_Perf.gpuTime * glm::normalize(moveDir);
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
            FrameInfoNoMaterials depthInfo{
                frameIndex,
                m_Perf.gpuTime,
                commandBuffer,
                camera,
                depthDescriptorSets[frameIndex],
                primitives
            };
            
            FrameInfo frameInfo{
                frameIndex,
                m_Perf.gpuTime,
                commandBuffer,
                camera,
                inFlightDescriptorSets[frameIndex],
                primitives,
                materials
            };
            
            FrameInfo skyboxInfo{
                frameIndex,
                m_Perf.gpuTime,
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
            ui.updateBuffers(frameIndex);
            
            // RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::DepthPass);
            renderSystems.depth->renderSolidObjects(depthInfo);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            renderSystems.skybox->renderSolidObjects(skyboxInfo);
            renderSystems.pbr->renderSolidObjects(frameInfo);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            renderSystems.composit->renderSceneToSwapChain(commandBuffer, renderer.getDescriptorSets(RenderPass::WorldSpace)->at(frameIndex));
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginSwapChainRenderPass(commandBuffer);
            ui.draw(commandBuffer, frameIndex);
            renderer.endRenderPass(commandBuffer);
            
            renderer.endFrame();
        }
        
        m_Perf.gpuEnd();
    
    }
    vkDeviceWaitIdle(vulkanDevice.device());
    
}
