//
//  Application.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "Application.hpp"

#include "UI.hpp"
#include "Buffer.hpp"
#include "Nodes.hpp"
#include "Widgets.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <thread>


struct WidgetStruct {
    std::shared_ptr<Viewport> view;
    std::shared_ptr<LogView> log;
    std::shared_ptr<AssetTree> assets;
    std::shared_ptr<NodeTreeViewer> nodes;
    std::shared_ptr<MeterialViewer> material;
    std::shared_ptr<Settings> settings;
    std::shared_ptr<Menu> menu;
};

void Application::run() {
    
    /**** User Interface Setup */
    UI ui(m_Device, m_Renderer);
    
    UI::setBessDarkColors();
    m_Window.updateUiScaling();
    
    WidgetStruct widgets {
        .view = std::make_shared<Viewport>(),
        .log = std::make_shared<LogView>(),
        .assets = std::make_shared<AssetTree>(),
        .nodes = std::make_shared<NodeTreeViewer>(m_Primitives),
        .material = std::make_shared<MeterialViewer>(m_Assets),
        .settings = std::make_shared<Settings>(m_Device, m_Window, m_Renderer, m_Perf, m_MSAASampleCount)
    };
    widgets.menu = std::make_shared<Menu>(widgets.log->getVisibility(), widgets.material->getVisibility());
    widgets.log->getVisibility() = false;
    widgets.material->getVisibility() = false;
    widgets.view->setExtent((float)m_Renderer.getSwapChainExtent().width, (float)m_Renderer.getSwapChainExtent().height);
    widgets.view->addFlags(ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground);
    
    ui.addWidgets(widgets.view, widgets.log, widgets.assets, widgets.material, widgets.settings, widgets.menu, widgets.nodes);
    
    std::shared_ptr<Camera> camera;
    
    std::thread([this, &widgets, &camera]() {
        
        /**** Load HDRi Texture */
        m_Assets.textures.push_back(std::make_unique<const Texture>(
            this->m_Device,
            m_Image,
            "../../../assets/textures/puresky_4k.hdr",
            false,
            VK_FORMAT_R32G32B32A32_SFLOAT
        ));
        auto equirectangular = m_Assets.textures.back()->descriptorInfo();
        
        /**** Allocate Uniform Buffer Object Buffers */
        for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            m_UboBuffers[i] = std::make_unique<Buffer>(
                m_Device,
                sizeof(ScenePipeline::UniformBuffer),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            m_UboBuffers[i]->map();
        }
        
        widgets.view->loading = 3;
        
        /**** Load Scene from glTF file */
        // TODO: better task-set creation
        NodeSet::InitStruct initNodeStruct{m_Device, m_Image, m_Primitives, m_Physics, m_Assets, m_Lights};
        NodeSet _gltf(initNodeStruct, "../../../assets/models/Billiard.glb");
        camera = _gltf.camera;
        
        /**** Load Point-Light Nodes from scene */
        // TODO: clean this mess
        m_Ubo.lightInfo = (uint8_t)m_Lights.size();
        uint8_t index = 0;
        for (auto& light : m_Lights) {
            if (index < 8 && light.m_Type < Light::Type::Spot) {
                if (light.m_Type == Light::Type::Directional) m_Ubo.lightSpaceMatrix = light.m_Data.lightSpaceMatrix;
                m_Ubo.lightVector[index] = glm::vec4(light.m_Data.pos, 0.f);
                m_Ubo.lightChroma[index] = light.m_Data.color;
                m_Ubo.lightInfo |= (light.m_Type & 0x1) << (index + 8);
                ++index;
            }
        }
        
        widgets.view->loading = 2;
        
        /**** HDRi, IBL, SkyBox  */
        m_Environment.instance = std::make_unique<CubeMap>(m_Device, &equirectangular, VkExtent2D(1024, 1024), "equirectangular", 9);
        m_Environment.descriptor = m_Environment.instance->getImageDescriptor();
        
        m_Prefiltered.instance = std::make_unique<CubeMap>(m_Device, m_Environment.descriptor, VkExtent2D(512, 512), "prefiltering", 8);
        m_Prefiltered.descriptor = m_Prefiltered.instance->getImageDescriptor();
        
        m_Irradiance.instance = std::make_unique<CubeMap>(m_Device, m_Environment.descriptor, VkExtent2D(32, 32), "irradiance");
        m_Irradiance.descriptor = m_Irradiance.instance->getImageDescriptor();
        
        
        widgets.view->loading = 1;
        
        m_Renderer.integrateBrdfLut();
        
        /**** Shadow Pipeline */
        m_Pipes.shadow.ptr = std::make_unique<ShadowPipeline>(
            m_Device,
            m_Renderer.getOffscreenRenderPass(RenderPass::ShadowPass),
            "shadow",
            ShadowPipeline::FrameData {
              .primitives = m_Primitives,
              .uboDescriptors = {m_UboBuffers[0]->descriptorInfo(), m_UboBuffers[1]->descriptorInfo(), m_UboBuffers[2]->descriptorInfo()}
            }
        );
        
        /**** Scene Pipeline */
        m_Pipes.scene.ptr = std::make_unique<ScenePipeline>(
            m_Device,
            m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
            "shader",
            ScenePipeline::FrameData {
            .primitives = m_Primitives,
            .assets = m_Assets,
            .uboDescriptors = {m_UboBuffers[0]->descriptorInfo(), m_UboBuffers[1]->descriptorInfo(), m_UboBuffers[2]->descriptorInfo()},
                .imageDescriptors = {
                    .brdf = m_Renderer.getBrdfLutInfo(),
                    .reflection = m_Prefiltered.descriptor,
                    .irradiance = m_Irradiance.descriptor,
                    .shadow = m_Renderer.getImageDescriptor(RenderPass::ShadowPass)
                }
            }
        );
        
        /**** Debug Pipeline */
        m_Pipes.debug.ptr = std::make_unique<DebugPipeline>(
            m_Device,
            m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
            "debug",
            DebugPipeline::FrameData {
                .primitives = m_Primitives,
                .uboDescriptors = {m_UboBuffers[0]->descriptorInfo(), m_UboBuffers[1]->descriptorInfo(), m_UboBuffers[2]->descriptorInfo()}
            }
        );
        
        /**** Skybox Pipeline */
        m_Pipes.skybox.ptr = std::make_unique<SkyboxPipeline>(
            m_Device,
            m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace),
            "skybox",
            SkyboxPipeline::FrameData {
                .uboDescriptors = {m_UboBuffers[0]->descriptorInfo(), m_UboBuffers[1]->descriptorInfo(), m_UboBuffers[2]->descriptorInfo()},
                .envImageDescriptor = m_Environment.descriptor
            }
        );
        
        /**** Composition Pipeline */
        m_Pipes.composit.ptr = std::make_unique<CompositingPipeline>(
            m_Device,
            m_Renderer.getOffscreenRenderPass(RenderPass::ScreenSpace),
            "composition",
            m_Renderer.getDescriptorSets(RenderPass::WorldSpace)
        );
        
        widgets.nodes->setTree(_gltf.getNodes());
        
        widgets.settings->recreatePipelinesCallback([this](void){
            if (m_Pipes.shadow.ptr && m_Pipes.scene.ptr && m_Pipes.skybox.ptr) {
                m_Pipes.shadow.ptr->recreatePipeline(m_Renderer.getOffscreenRenderPass(RenderPass::ShadowPass));
                m_Pipes.scene.ptr->recreatePipeline(m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace), m_MSAASampleCount);
                m_Pipes.skybox.ptr->recreatePipeline(m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace), m_MSAASampleCount);
                m_Pipes.debug.ptr->recreatePipeline(m_Renderer.getOffscreenRenderPass(RenderPass::WorldSpace), m_MSAASampleCount);
            }
        });
        
        widgets.view->loading = 0;
        m_AssetsLoaded = true;
        
    }).detach();
    
    
    while(m_Window.isWindowOpen()) {
        
        m_Perf.startFrame();
        
        // Prepare next GUI Frame
        if (!m_PreviewMode) ui.newFrame();
        
        m_Window.pollWindowEvents([this, widgets]() {
            m_Renderer.recreateSwapChain();
            widgets.view->setExtent((float)m_Renderer.getSwapChainExtent().width, (float)m_Renderer.getSwapChainExtent().height);
        });
        
        controlCamera(camera, m_Window.getMovement(), m_Window.getRotation(), m_Renderer.getAspectRatio(), m_Perf.cpuTime + m_Perf.gpuTime);
        
        shortcutCallback(m_Window.getShortcut());
        
        
        btQuaternion qrot;
        qrot.setRotation(btVector3(0,1.f,0), .01f);
        
        if (m_AssetsLoaded && m_RunSimulation) {
            m_Physics.getWorldHandle()->stepSimulation(m_Perf.cpuTime + m_Perf.gpuTime, 20, m_Perf.cpuTime + m_Perf.gpuTime);
            for (auto& bt : m_Physics.getRigidBodyMap()) {
                btTransform t;
                bt.second->getMotionState()->getWorldTransform(t);
                float m[16];
                t.getOpenGLMatrix(m);
                m_Primitives.at(bt.first).transform.matrix = glm::make_mat4(m);
            }
        }
        
        m_Perf.cpuEnd();
        
        if (auto commandBuffer = m_Renderer.beginFrame()) {
            m_FrameIndex = m_Renderer.getFrameIndex();
            
            // Update Uniform Buffer Object
            if (camera) {
                m_Ubo.projectionView = camera->getProjection();
                m_Ubo.viewMatrix = camera->getView();
                m_Ubo.invViewMatrix = camera->getInverseView();
                m_Ubo.debugMode = widgets.settings->debugMode;
            }
            
            if (m_Pipes.composit.ptr) {
                auto p = m_Pipes.composit.ptr_cast<CompositingPipeline>();
                p->exposure = widgets.settings->otherData.exposure;
                p->gamma = widgets.settings->otherData.gamma;
                p->peak_brightness = widgets.settings->otherData.peak_brightness;
                p->debugMode = widgets.settings->otherData.debugMode;
                
                m_UboBuffers[m_FrameIndex]->writeToBuffer(&m_Ubo);
                m_UboBuffers[m_FrameIndex]->flush();
            }
            
            // Update UI Buffer
            ui.updateBuffers(m_FrameIndex);
            
            // RenderPass
            m_Renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ShadowPass);
            m_Pipes.shadow.render(commandBuffer, m_FrameIndex);
            m_Renderer.endRenderPass(commandBuffer);
            
            m_Renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::WorldSpace);
            m_Pipes.scene.render(commandBuffer, m_FrameIndex);
            m_Pipes.skybox.render(commandBuffer, m_FrameIndex);
            if (m_DebugMode) m_Pipes.debug.render(commandBuffer, m_FrameIndex);
            m_Renderer.endRenderPass(commandBuffer);
            
            m_Renderer.beginOffscreenRenderPass(commandBuffer, RenderPass::ScreenSpace);
            if (!m_PreviewMode) {
                m_Pipes.composit.render(commandBuffer, m_FrameIndex);
            }
            m_Renderer.endRenderPass(commandBuffer);
            
            m_Renderer.beginSwapChainRenderPass(commandBuffer);
            if (m_PreviewMode) {
                m_Pipes.composit.render(commandBuffer, m_FrameIndex);
            } else {
                ui.draw(commandBuffer, m_FrameIndex);
            }
            m_Renderer.endRenderPass(commandBuffer);
            
            m_Renderer.endFrame();
        }
        
        m_Perf.gpuEnd();
        
    }
    vkDeviceWaitIdle(m_Device.device());
    
}

void Application::shortcutCallback(Shortcut shortcut) {
    if (m_AssetsLoaded && shortcut == CTRL_F) {
        vkDeviceWaitIdle(m_Device.device());
        if (m_Pipes.composit.ptr && m_PreviewMode) {
            m_Pipes.composit.ptr->recreatePipeline(m_Renderer.getOffscreenRenderPass(RenderPass::ScreenSpace));
            m_PreviewMode = false;
        } else if (m_Pipes.composit.ptr) {
            m_Pipes.composit.ptr->recreatePipeline(m_Renderer.getSwapChainRenderPass());
            m_PreviewMode = true;
        }
    }
    if (shortcut == CTRL_D) {
        m_DebugMode ^= true;
    }
    if (shortcut == CTRL_R) {
        m_RunSimulation ^= true;
    }
}

void Application::controlCamera(std::shared_ptr<Camera>& camera, uint8_t move, glm::vec3 rotate, float newAspect, float frameTime) {
    if (!camera) return;
    
    static float oldAspect = camera->getAspectRatio();
    bool changed = false;
    
    glm::vec3 position(0.f);
    glm::vec3 rotation(0.f);
    
    if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
        rotation = rotate * 0.02f * camera->getFov();
        changed = true;
    }
    
    if (move) {
        const glm::vec3 forwardDir{glm::sin(camera->getYaw()), .0f, glm::cos(camera->getYaw())};
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
            position += 8.f * frameTime * glm::normalize(moveDir);
            if (!camera->noClip) m_Physics.moveGhost((uint32_t)camera->ghostObject, camera->getInverseView(), position);
            changed = true;
        }
    }
    
    if (oldAspect != newAspect) {
        oldAspect = newAspect;
        camera->changeAspectRatio(newAspect);
        //camera.setOrthographicProjection(-newAspect, newAspect, -1.f, 1.f, -10.f, 100.f);
    }
    
    if (changed) {
        if (m_PreviewMode) {
            camera->setViewYXZDelta(position, rotation);
        } else {
            camera->pivotAroundOrigin(rotation);
        }
    }
}
