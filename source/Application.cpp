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

static void updateCamera(Camera& camera, Primitive& cameraHandle, uint8_t move, glm::vec3 rotate, float newAspect, float frameTime);

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
    
		auto cubeCanvas = Primitive::new_primitive();
		cubeCanvas.setModel(std::make_shared<Model>(vulkanDevice, Model::Data::makeSimpleCube(true)));
		env.emplace(cubeCanvas.getId(), std::move(cubeCanvas));
		
		Camera camera{};
		camera.setProjection.perspective(renderer.getAspectRatio(), glm::radians(75.f), .01f, 100.f);
		Primitive cameraHandle = Primitive::new_primitive();
		cameraHandle.transform.translation = {-5.f, -2.f, .0f};
		cameraHandle.transform.rotation.y = glm::half_pi<float>();
    
    std::thread([this, &widgets]() {
    
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
        auto equirectangular = textures.back()->descriptorInfo();
        
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
				
				widgets.view->loading = 3;
				
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
								if (light.m_type == Light::Type::Directional) ubo.lightSpaceMatrix = light.m_data.lightSpaceMatrix;
                ubo.lightVector[index] = glm::vec4(light.m_data.pos, 0.f);
                ubo.lightChroma[index] = light.m_data.color;
                ubo.lightInfo |= (light.m_type & 0x1) << (index + 8);
                ++index;
            }
        }
        
        widgets.view->loading = 2;
				
				/**** HDRi, IBL, SkyBox  */
				m_Environment.instance = std::make_unique<HDRi>(vulkanDevice, &equirectangular, VkExtent2D(1024, 1024), "equirectangular", binaryDir, 9);
				m_Environment.descriptor = m_Environment.instance->getImageDescriptor();
				
				m_Prefiltered.instance = std::make_unique<HDRi>(vulkanDevice, m_Environment.descriptor, VkExtent2D(512, 512), "prefiltering", binaryDir, 8);
				m_Prefiltered.descriptor = m_Prefiltered.instance->getImageDescriptor();
				
				m_Irradiance.instance = std::make_unique<HDRi>(vulkanDevice, m_Environment.descriptor, VkExtent2D(32, 32), "irradiance", binaryDir);
				m_Irradiance.descriptor = m_Irradiance.instance->getImageDescriptor();
				
				
				widgets.view->loading = 1;
				
				renderer.integrateBrdfLut(binaryDir);
				
				/**** Shadow Pipeline */
				m_Pipelines.shadow = std::make_unique<ShadowPipeline>(
						vulkanDevice,
						renderer.getOffscreenRenderPass(RenderPass::ShadowPass),
						"shadow",
						ShadowPipeline::FrameData {
								.primitives = primitives,
								.uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()}
						}
				);
				
				/**** Scene Pipeline */
				m_Pipelines.scene = std::make_unique<ScenePipeline>(
						vulkanDevice,
						renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
						"shader",
						ScenePipeline::FrameData {
								.primitives = primitives,
								.materials = materials,
								.uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()},
								.imageDescriptors = {
										.brdf = renderer.getBrdfLutInfo(),
										.reflection = m_Prefiltered.descriptor,
										.irradiance = m_Irradiance.descriptor,
										.shadow = renderer.getImageDescriptor(RenderPass::ShadowPass)
								}
						}
				);
				
				/**** Skybox Pipeline */
				m_Pipelines.skybox = std::make_unique<SkyboxPipeline>(
						vulkanDevice,
						renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
						"skybox",
						SkyboxPipeline::FrameData {
								.primitives = env,
								.uboDescriptors = {uboBuffers[0]->descriptorInfo(), uboBuffers[1]->descriptorInfo(), uboBuffers[2]->descriptorInfo()},
								.envImageDescriptor = m_Environment.descriptor
						}
				);
				
				/**** Composition Pipeline */
				m_Pipelines.composit = std::make_unique<CompositionPipeline>(
						vulkanDevice,
						renderer.getOffscreenRenderPass(RenderPass::ScreenSpace),
						renderer.getDescriptorSetLayout(RenderPass::WorldSpace),
						"composition"
				);
    
				widgets.settings->recreatePipelinesCallback([this](void){
						m_Pipelines.shadow->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::ShadowPass));
						m_Pipelines.scene->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
						m_Pipelines.skybox->recreatePipeline(renderer.getOffscreenRenderPass(RenderPass::WorldSpace), vulkanDevice.msaaSamples);
				});
				
        widgets.view->loading = 0;
				assetsLoaded = true;
        
    }).detach();


    while(window.isWindowOpen())
    {
        
        m_Perf.startFrame();
        
        // Prepare next GUI Frame
        ui.newFrame();
        
        window.pollWindowEvents([this, widgets](){ renderer.recreateSwapChain(); widgets.view->setExtent(renderer.getSwapChainExtent().width * 0.8f, renderer.getSwapChainExtent().height * 0.8f); });
        
        updateCamera(camera, cameraHandle, window.getMovement(), window.getRotation(), renderer.getAspectRatio(), m_Perf.cpuTime + m_Perf.gpuTime);
        
        m_Perf.cpuEnd();
        
        if (auto commandBuffer = renderer.beginFrame()) {
            frameIndex = renderer.getFrameIndex();
            
            // Update Uniform Buffer Object
            ubo.projectionView = camera.getProjection();
            ubo.viewMatrix = camera.getView();
            ubo.invViewMatrix = camera.getInverseView();
            ubo.debugMode = widgets.settings->debugMode;
            
            if (assetsLoaded) {
								m_Pipelines.composit->exposure = widgets.settings->otherData.exposure;
								m_Pipelines.composit->gamma = widgets.settings->otherData.gamma;
								m_Pipelines.composit->peak_brightness = widgets.settings->otherData.peak_brightness;
								m_Pipelines.composit->debugMode = widgets.settings->otherData.debugMode;

								uboBuffers[frameIndex]->writeToBuffer(&ubo);
								uboBuffers[frameIndex]->flush();
            }
            
            // Update UI Buffer
            ui.updateBuffers(frameIndex);
            
            // RenderPass
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ShadowPass);
            if (assetsLoaded) {
								m_Pipelines.shadow->render(commandBuffer, frameIndex);
						}
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            if (assetsLoaded) {
								m_Pipelines.scene->render(commandBuffer, frameIndex);
								m_Pipelines.skybox->render(commandBuffer, frameIndex);
            }
            renderer.endRenderPass(commandBuffer);
            
            renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            if (assetsLoaded) {
								m_Pipelines.composit->renderSceneToSwapChain(commandBuffer, renderer.getDescriptorSets(RenderPass::WorldSpace)->at(frameIndex));
            }
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


static void updateCamera(Camera& camera, Primitive& cameraHandle, uint8_t move, glm::vec3 rotate, float newAspect, float frameTime) {
		static float oldAspect = newAspect;

		if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
				cameraHandle.transform.rotation += rotate * .05f;
				cameraHandle.transform.rotation.x = glm::clamp(cameraHandle.transform.rotation.x, -1.5f, 1.5f);
				cameraHandle.transform.rotation.y = glm::mod(cameraHandle.transform.rotation.y, glm::two_pi<float>());
		}
		
		if (move) {
				float yaw = cameraHandle.transform.rotation.y;
				const glm::vec3 forwardDir{glm::sin(yaw), .0f, glm::cos(yaw)};
				const glm::vec3 rightDir{forwardDir.z, .0f, -forwardDir.x};
				const glm::vec3 upDir{.0f, -1.f, .0f};
				glm::vec3 moveDir{0.f};
				if (move & 0x01) { moveDir += forwardDir; }
				if (move & 0x02) { moveDir -= rightDir; }
				if (move & 0x04) { moveDir -= forwardDir; }
				if (move & 0x08) { moveDir += rightDir; }
				if (move & 0x10) { moveDir -= upDir; }
				if (move & 0x20) { moveDir += upDir; }
				if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
						cameraHandle.transform.translation += 8.f * frameTime * glm::normalize(moveDir);
				}
		}
		
		// Fix camera projection if the viewport's aspect ratio changes
		if (oldAspect != newAspect) {
				oldAspect = newAspect;
				camera.setProjection.perspective(newAspect);
				//camera.setOrthographicProjection(-newAspect, newAspect, -1.f, 1.f, -10.f, 100.f);
		}
		
		// Polling keystrokes and adjusting the camera position/rotation
		camera.setViewYXZ(cameraHandle.transform.translation, cameraHandle.transform.rotation);
}
