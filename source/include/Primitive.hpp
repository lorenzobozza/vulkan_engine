//
//  Primitive.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Primitive_hpp
#define Primitive_hpp

#include "Mesh.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>

struct TransformComponent {
    glm::vec3 translation{0.f};
    glm::vec3 scale{1.f};
    glm::vec3 rotation{0.f};
    
    bool hasMatrix = false;
    glm::mat4 matrix{};
    
    // Rotation is EULER YXZ
    glm::mat4 mat4();
    glm::mat3 normalMatrix();
};

class Primitive {
public:
    using id_t = unsigned int;
    using Map = std::unordered_map<id_t, Primitive>;
    
    Primitive(const Primitive &) = delete;
    Primitive& operator=(const Primitive &) = delete;
    
    Primitive(Primitive &&) = default;
    Primitive& operator=(Primitive &&) = default;
    
    static Primitive new_primitive() {
        static id_t currentId = 0;
        return Primitive{currentId++};
    }
    
    id_t getId() const { return m_Id; }
    
    std::shared_ptr<Mesh> model;
    
    bool showAABB = false;
    std::shared_ptr<Mesh> aabb;
    std::shared_ptr<Mesh> normals;
    
    glm::vec3 color{};
    int textureIndex{-1};
    float metalness{0.f};
    float roughness{.4f};
    TransformComponent transform{};
    
    std::string material{"Global_Default_Material"};
    
private:
    Primitive(id_t objId) : m_Id(objId) {}
    id_t m_Id;
};

#endif /* Primitive_hpp */
