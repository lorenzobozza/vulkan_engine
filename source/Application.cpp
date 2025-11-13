//
//  Application.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "include/Application.hpp"
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


void Application::run() {

/**** User Interface Setup */
    UI ui(vulkanDevice, renderer);
    
    UI::setBessDarkColors();
    window.updateUiScaling();
    
    WidgetStruct widgets {
        .view = std::make_shared<Viewport>(),
        .log = std::make_shared<LogView>(),
        .assets = std::make_shared<AssetTree>(),
        .material = std::make_shared<MeterialViewer>(materials),
        .settings = std::make_shared<Settings>(vulkanDevice,window,renderer,m_Perf)
    };
    widgets.menu = std::make_shared<Menu>(widgets.log->getVisibility(), widgets.material->getVisibility());
    widgets.log->getVisibility() = false;
    widgets.material->getVisibility() = false;
    widgets.view->setExtent(renderer.getSwapChainExtent().width * 0.8f, renderer.getSwapChainExtent().height * 0.8f);
    widgets.view->addFlags(ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground);
    
    ui.addWidgets(widgets.view, widgets.log, widgets.assets, widgets.material, widgets.settings, widgets.menu);
    
    std::thread([this]() {
    
				/**** Fallback Material */
        Material globalMaterial(&textures);
        globalMaterial.color = {1.f, 1.f, 1.f, 1.f};
        materials.emplace("Global_Default_Material", globalMaterial);
        
        /**** Load HDRi Texture */
        textures.push_back(std::make_unique<Texture>(
            this->vulkanDevice,
            vulkanImage,
            "../../../assets/textures/mondello_4k.hdr",
            false,
            VK_FORMAT_R32G32B32A32_SFLOAT
        ));
        
				/**** Allocate Uniform Buffer Object Buffers */
				for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
						uboBuffers[i] = std::make_unique<Buffer>(
																vulkanDevice,
																sizeof(ScenePipeline::UniformBuffer),
																1,
																VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
																VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
														);
						uboBuffers[i]->map();
				}
				
				/**** Load Scene from glTF file */
        // TODO: better task-set creation
        NodeSet::InitStruct initNodeStruct{vulkanDevice, vulkanImage, primitives, textures, materials, lights};
        NodeSet _gltf(initNodeStruct, binaryDir + "../../../assets/models/Sponza.glb");
        
        /**** Load Point-Light Nodes from scene */
        // TODO: clean this mess
        ubo.lightInfo = (uint8_t)lights.size();
        uint8_t index = 0;
        for (auto& light : lights) {
            if (index < 8 && light.m_type < Light::Type::Spot) {
                ubo.lightSpaceMatrix = light.m_data.lightSpaceMatrix;
                ubo.lightVector[index] = glm::vec4(light.m_data.pos, 0.f);
                ubo.lightChroma[index] = light.m_data.color;
                ubo.lightInfo |= (light.m_type & 0x1) << (index + 8);
                ++index;
            }
        }
                
        assetsLoaded = true;
        
    }).detach();
        
    while (!assetsLoaded) {
        
        ui.newFrame();

        window.pollWindowEvents([this, widgets](){ renderer.recreateSwapChain(); widgets.view->setExtent(renderer.getSwapChainExtent().width * 0.8f, renderer.getSwapChainExtent().height * 0.8f); });
        
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

    
    vkDeviceWaitIdle(vulkanDevice.device());
    renderer.integrateBrdfLut(binaryDir);
		
/**** HDRi, IBL, SkyBox  */
    auto equitangular = textures.at(0)->descriptorInfo();
    
    HDRi environmentMap{vulkanDevice, equitangular, {1024, 1024}, "equirectangular", binaryDir, 9};
    auto environment = environmentMap.descriptorInfo();
    
    HDRi prefilteredMap{vulkanDevice, environment, {512, 512}, "prefiltering", binaryDir, 9};
    auto prefiltered = prefilteredMap.descriptorInfo();
    
    HDRi irradianceMap{vulkanDevice, environment, {32, 32}, "irradiance", binaryDir};
    auto irradiance = irradianceMap.descriptorInfo();
    
    
/**** Shadow Pipeline */
    ShadowPipeline::FrameData shadowData {
				.primitives = primitives,
				.uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()}
		};
		m_Pipelines.shadow = std::make_unique<ShadowPipeline>(vulkanDevice, renderer.getOffscreenRenderPass(RenderPass::DepthPass), "shadow", shadowData);
    
/**** Scene Pipeline */
        ScenePipeline::FrameData sceneData{
                .primitives = primitives,
                .materials = materials,
                .uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()},
                .imageDescriptors = {
                    .brdf = renderer.getBrdfLutInfo(),
                    .irradiance = &irradiance,
                    .reflection = &prefiltered,
                    .shadow = renderer.getImageDescriptor(RenderPass::DepthPass)
                }
		};
		m_Pipelines.scene = std::make_unique<ScenePipeline>(vulkanDevice, renderer.getOffscreenRenderPass(RenderPass::WorldSpace), "shader", sceneData);
		
/**** Skybox Pipeline */
		SkyboxPipeline::FrameData skyboxData {
				.primitives = env,
				.uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()},
				.envImageDescriptor = environment
		};
		m_Pipelines.skybox = std::make_unique<SkyboxPipeline>(vulkanDevice, renderer.getOffscreenRenderPass(RenderPass::WorldSpace), "skybox", skyboxData);
		
/**** Composition Pipeline */
		m_Pipelines.composit = std::make_unique<CompositionPipeline>(
        vulkanDevice,
        renderer.getOffscreenRenderPass(RenderPass::ScreenSpace),
        renderer.getDescriptorSetLayout(RenderPass::WorldSpace),
        "composition"
    );
    
    
    widgets.settings->recreatePipelinesCallback([this](void){
				m_Pipelines.shadow->recreatePipeline();
				m_Pipelines.scene->recreatePipeline();
				m_Pipelines.skybox->recreatePipeline();
    });
    
// Misc
		auto cube = Primitive::new_primitive();
		cube.setModel(std::make_shared<Model>(vulkanDevice, Model::Data::makeSimpleCube(true)));
		env.emplace(cube.getId(), std::move(cube));
		
		Camera camera{};
		float aspectRatio = renderer.getAspectRatio();
		camera.setProjection.perspective(aspectRatio, glm::radians(75.f), .01f, 100.f);
		
		Primitive cameraObj = Primitive::new_primitive();
		cameraObj.transform.translation = {-5.f, -2.f, .0f};
		cameraObj.transform.rotation.y = glm::half_pi<float>();
		bool orth = false;

    while(window.isWindowOpen())
    {
        
        m_Perf.startFrame();
        
        // Prepare next GUI Frame
        ui.newFrame();
        
        window.pollWindowEvents([this, widgets](){ renderer.recreateSwapChain(); widgets.view->setExtent(renderer.getSwapChainExtent().width * 0.8f, renderer.getSwapChainExtent().height * 0.8f); });
        
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
            
            // Update Uniform Buffer Object
            ubo.projectionView = camera.getProjection();
            ubo.viewMatrix = camera.getView();
            ubo.invViewMatrix = camera.getInverseView();
            ubo.debugMode = widgets.settings->debugMode;
            
            m_Pipelines.composit->exposure = widgets.settings->otherData.exposure;
            m_Pipelines.composit->gamma = widgets.settings->otherData.gamma;
            m_Pipelines.composit->peak_brightness = widgets.settings->otherData.peak_brightness;
            m_Pipelines.composit->debugMode = widgets.settings->otherData.debugMode;

            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            
            // Update UI Buffer
            ui.updateBuffers(frameIndex);
            
            // RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::DepthPass);
            m_Pipelines.shadow->render(commandBuffer, frameIndex);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            m_Pipelines.scene->render(commandBuffer, frameIndex);
            m_Pipelines.skybox->render(commandBuffer, frameIndex);
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            m_Pipelines.composit->renderSceneToSwapChain(commandBuffer, renderer.getDescriptorSets(RenderPass::WorldSpace)->at(frameIndex));
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
