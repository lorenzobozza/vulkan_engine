//
//  SolidObject.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef SolidObject_hpp
#define SolidObject_hpp

#include "Model.hpp"

//lib
#include <glm/gtc/matrix_transform.hpp>

//std
#include <memory>
#include <unordered_map>

struct TransformComponent {
    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{};
    
    // Rotation is EULER YXZ
    glm::mat4 mat4();
    glm::mat3 normalMatrix();
};

// GravityPhysicsSystem
struct RigidBody2dComponent {
    glm::vec2 velocity;
    float mass{1.0f};
};

class SolidObject {
public:
    using id_t = unsigned int;
    using Map = std::unordered_map<id_t, SolidObject>;
    
    static SolidObject createSolidObject() {
        static id_t currentId = 0;
        return SolidObject{currentId++};
    }
    
    SolidObject(const SolidObject &) = delete;
    SolidObject &operator=(const SolidObject &) = delete;
    SolidObject(SolidObject &&) = default;
    SolidObject &operator=(SolidObject &&) = default;
    
    id_t getId() { return id; }
    
    std::shared_ptr<Model> model{};
    glm::vec3 color{};
    id_t textureIndex{0};
    TransformComponent transform{};
    
    RigidBody2dComponent rigidBody2d{};
    
private:
    SolidObject(id_t objId) : id{objId} {}
    
    id_t id;
};

#endif /* SolidObject_hpp */
