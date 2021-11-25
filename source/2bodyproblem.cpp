//
//  2bodyproblem.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 21/11/21.
//

#include "include/Application.hpp"
#include "include/RenderSystem.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <chrono>

struct PushTwoObj {
    glm::mat4 modelMatrix{1.f};
    glm::vec3 color{1.f};
};

class TwoBodyRender : public RenderSystem {
public:
    using RenderSystem::RenderSystem;
    
    void renderSolidObjects(FrameInfo &frameInfo) {
      pipeline->bind(frameInfo.commandBuffer);
      
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr
        );

    for (auto &kv : frameInfo.solidObjects) {
        auto &obj = kv.second;
        
        PushTwoObj push{};
        push.modelMatrix = obj.transform.mat4();
        push.color = obj.color;

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushTwoObj),
            &push);
        obj.model->bind(frameInfo.commandBuffer);
        obj.model->draw(frameInfo.commandBuffer);
      }
    }

};


struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};
    glm::vec3 lightPosition{.0f, -2.f, .8f};
    alignas(16) glm::vec4 lightColor{1.f, 1.f, 1.f, .8f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
};

class GravityPhysicsSystem {
 public:
  GravityPhysicsSystem(float strength) : strengthGravity{strength} {}

  const float strengthGravity;

  // dt stands for delta time, and specifies the amount of time to advance the simulation
  // substeps is how many intervals to divide the forward time step in. More substeps result in a
  // more stable simulation, but takes longer to compute
  void update(std::vector<SolidObject>& objs, float dt, unsigned int substeps = 1) {
    const float stepDelta = dt / substeps;
    for (int i = 0; i < substeps; i++) {
      stepSimulation(objs, stepDelta);
    }
  }

  glm::vec2 computeForce(SolidObject& fromObj, SolidObject& toObj) const {
    auto offset = fromObj.transform.translation - toObj.transform.translation;
    float distanceSquared = glm::dot(offset, offset);

    // clown town - just going to return 0 if objects are too close together...
    if (glm::abs(distanceSquared) < 1e-10f) {
      return {.0f, .0f};
    }

    float force =
        strengthGravity * toObj.rigidBody2d.mass * fromObj.rigidBody2d.mass / distanceSquared;
    return force * offset / glm::sqrt(distanceSquared);
  }

 private:
  void stepSimulation(std::vector<SolidObject>& physicsObjs, float dt) {
    // Loops through all pairs of objects and applies attractive force between them
    for (auto iterA = physicsObjs.begin(); iterA != physicsObjs.end(); ++iterA) {
      auto& objA = *iterA;
      for (auto iterB = iterA; iterB != physicsObjs.end(); ++iterB) {
        if (iterA == iterB) continue;
        auto& objB = *iterB;

        auto force = computeForce(objA, objB);
        objA.rigidBody2d.velocity += dt * -force / objA.rigidBody2d.mass;
        objB.rigidBody2d.velocity += dt * force / objB.rigidBody2d.mass;
      }
    }

    // update each objects position based on its final velocity
    for (auto& obj : physicsObjs) {
      obj.transform.translation += glm::vec3(dt * obj.rigidBody2d.velocity, .0f);
    }
  }
};

class Vec2FieldSystem {
 public:
  void update(
      const GravityPhysicsSystem& physicsSystem,
      std::vector<SolidObject>& physicsObjs,
      std::vector<SolidObject>& vectorField) {
    // For each field line we caluclate the net graviation force for that point in space
    for (auto& vf : vectorField) {
      glm::vec3 direction{};
      for (auto& obj : physicsObjs) {
        direction += glm::vec3(physicsSystem.computeForce(obj, vf), .0f);
      }

      // This scales the length of the field line based on the log of the length
      // values were chosen just through trial and error based on what i liked the look
      // of and then the field line is rotated to point in the direction of the field
      vf.transform.scale.x =
          0.005f + 0.045f * glm::clamp(glm::log(glm::length(direction) + 1) / 3.f, 0.f, 1.f);
      vf.transform.rotation = glm::vec3(.0f, .0f, atan2(direction.y, direction.x));
    }
  }
};

std::unique_ptr<Model> createSquareModel(Device& device, glm::vec3 offset) {
  Model::Data model{};
  model.vertices = {
      {{-0.5f, -0.5f, .0f},{1.f, 1.f, 1.f}},
      {{0.5f, 0.5f, .0f},{1.f, 1.f, 1.f}},
      {{-0.5f, 0.5f, .0f},{1.f, 1.f, 1.f}},
      {{-0.5f, -0.5f, .0f},{1.f, 1.f, 1.f}},
      {{0.5f, -0.5f, .0f},{1.f, 1.f, 1.f}},
      {{0.5f, 0.5f, .0f},{1.f, 1.f, 1.f}},  //
  };
  for (auto& v : model.vertices) {
    v.position += offset;
  }
  return std::make_unique<Model>(device, model);
}

std::unique_ptr<Model> createCircleModel(Device& device, unsigned int numSides) {
  std::vector<Model::Vertex> uniqueVertices{};
  for (int i = 0; i < numSides; i++) {
    float angle = i * glm::two_pi<float>() / numSides;
    uniqueVertices.push_back({{glm::cos(angle), glm::sin(angle), .0f}});
  }
  uniqueVertices.push_back({});  // adds center vertex at 0, 0

    Model::Data model{};
  for (int i = 0; i < numSides; i++) {
    model.vertices.push_back(uniqueVertices[i]);
    model.vertices.push_back(uniqueVertices[(i + 1) % numSides]);
    model.vertices.push_back(uniqueVertices[numSides]);
  }
  return std::make_unique<Model>(device, model);
}

void Application::simulate() {
  // create some models
  std::shared_ptr<Model> squareModel = createSquareModel(
      device, {.5f, .0f, .0f});  // offset model by .5 so rotation occurs at edge rather than center of square
  std::shared_ptr<Model> circleModel = createCircleModel(device, 64);

  // create physics objects
  std::vector<SolidObject> physicsObjects{};
  auto red = SolidObject::createSolidObject();
  red.transform.scale = {.05f, .05f, .0f};
  red.transform.translation = {.2f, .2f, .0f};
  red.color = {1.f, 0.f, 0.f};
  red.rigidBody2d.velocity = {-.5f, .0f};
  red.model = circleModel;
  physicsObjects.push_back(std::move(red));
  auto blue = SolidObject::createSolidObject();
  blue.transform.scale = {.05f, .05f, .0f};
  blue.transform.translation = {-.45f, -.25f, .0f};
  blue.color = {0.f, 0.f, 1.f};
  blue.rigidBody2d.velocity = {.5f, .0f};
  blue.model = circleModel;
  physicsObjects.push_back(std::move(blue));

  // create vector field
  std::vector<SolidObject> vectorField{};
  int gridCount = 30;
  for (int i = 0; i < gridCount; i++) {
    for (int j = 0; j < gridCount; j++) {
      auto vf = SolidObject::createSolidObject();
      vf.transform.scale = {0.005f, 0.005f, .0f};
      vf.transform.translation = {
          -1.0f + (i + 0.5f) * 2.0f / gridCount,
          -1.0f + (j + 0.5f) * 2.0f / gridCount,
          .0f
      };
      vf.color = glm::vec3(1.0f);
      vf.model = squareModel;
      vectorField.push_back(std::move(vf));
    }
  }

  GravityPhysicsSystem gravitySystem{.81f};
  Vec2FieldSystem vecFieldSystem{};
  
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
    
    auto globalSetLayout =
       DescriptorSetLayout::Builder(device)
           .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
           .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
   for (int i = 0; i < globalDescriptorSets.size(); i++) {
     auto bufferInfo = uboBuffers[i]->descriptorInfo();
     DescriptorWriter(*globalSetLayout, *globalPool)
         .writeBuffer(0, &bufferInfo)
         .build(globalDescriptorSets[i]);
   }
    
    TwoBodyRender twoBodyRenderSystem{
        device,
        renderer.getSwapChainRenderPass(),
        globalSetLayout->getDescriptorSetLayout(),
        shaderPath+"2body"
    };
    
    // Set camera projection based on aspect ratio of the viewport
    Camera camera{};
    float aspect = renderer.getAspectRatio();
    camera.setOrthographicProjection(-aspect, aspect, -1.f, 1.f, -1.f, 10.f);
    
    // Gameloop sync
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<float> frameTimes;

  while (!window.shouldClose()) {
    glfwPollEvents();
    
    // Compute frame latency and store the value
    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime -  currentTime).count();
    currentTime = newTime;
    frameTimes.push_back(frameTime);
    frameTime = glm::min(frameTime, .05f); // Prevent movement glitches when resizing

    if (auto commandBuffer = renderer.beginFrame()) {
        // update systems
        gravitySystem.update(physicsObjects, 1.f / 60, 5);
        vecFieldSystem.update(gravitySystem, physicsObjects, vectorField);
        
        for (auto &obj : physicsObjects) {
            solidObjects.emplace(obj.getId(), std::move(obj));
        }
        for (auto &obj : vectorField) {
            solidObjects.emplace(obj.getId(), std::move(obj));
        }
      
        int frameIndex = renderer.getFrameIndex();
        FrameInfo frameInfo{
            frameIndex,
            frameTime,
            commandBuffer,
            camera,
            globalDescriptorSets[frameIndex],
            solidObjects
        };
        
        //Update
            GlobalUbo ubo{};
            ubo.projectionView = frameInfo.camera.getProjection();
            ubo.viewMatrix = frameInfo.camera.getView();
            ubo.invViewMatrix = frameInfo.camera.getInverseView();
            ubo.lightPosition = glm::vec3{1.f};
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

        // render system
        renderer.beginSwapChainRenderPass(commandBuffer);
        renderer.beginSwapChainRenderPass(commandBuffer);
        twoBodyRenderSystem.renderSolidObjects(frameInfo);
        renderer.endSwapChainRenderPass(commandBuffer);
        renderer.endFrame();
    }
  }

  vkDeviceWaitIdle(device.device());
}
