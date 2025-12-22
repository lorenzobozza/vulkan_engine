//
//  Primitive.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef Physics_hpp
#define Physics_hpp

#include "Primitive.hpp"

#include <btBulletDynamicsCommon.h>

#include <memory>
#include <vector>

class Physics {
public:
    Physics(const Physics&) = delete;
    Physics& operator=(const Physics&) = delete;
    Physics(Physics&&) = delete;
    Physics& operator=(Physics&&) = delete;
    
    Physics();
    ~Physics() = default;
    
    void addCompoundShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material);
    void addTriangleMeshShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material);
    void addBasicShape(int shape, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material);
    
    void setStaticRigidBodyFlag(Primitive::id_t id);
    void setKinematicRigidBodyFlag(Primitive::id_t id);
    void setVelocity(Primitive::id_t id, const glm::vec3& linear, const glm::vec3& angular);
    void setGravityFactor(Primitive::id_t id, float factor);
    
    std::unique_ptr<btDiscreteDynamicsWorld>& getWorldHandle(void) { return m_DynamicsWorld; }
    std::unordered_map<Primitive::id_t, std::unique_ptr<btRigidBody>>& getRigidBodyMap(void) { return m_RigidBodies; }
    
    struct ImplicitShape {
        enum Type {
            Plane = 0,
            Sphere,
            Box,
            Cylinder,
            Capsule
        };
        Type type;
        float value1;
        float value2;
        float value3;
    };
    void appendPhysicsMaterial(float friction, float restitution) { m_PhysicsMaterials.emplace_back(PhysicsMaterial(friction, restitution)); }
    void appendImplicitShape(ImplicitShape shape) { m_ImplicitShapes.emplace_back(shape); }
    
private:
    void makeRigidBody(btCollisionShape* collisionShape, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material);
    
    std::unique_ptr<btBroadphaseInterface> m_BroadPhaseIface;
    std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfig;
    std::unique_ptr<btCollisionDispatcher> m_CollisionDispatcher;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;
    
    struct TriangleMesh {
        std::unique_ptr<btTriangleMesh> mesh;
        std::unique_ptr<btBvhTriangleMeshShape> shape;
    };
    std::vector<TriangleMesh> m_TriangleMeshes;
    
    struct ConvexHull {
        std::vector<std::unique_ptr<btConvexHullShape>> singleShapes;
        std::unique_ptr<btCompoundShape> compoundShape;
    };
    std::vector<ConvexHull> m_ConvexHulls;
    
    struct PhysicsMaterial {
        float friction;
        float restitution;
    };
    std::vector<PhysicsMaterial> m_PhysicsMaterials;
    std::vector<ImplicitShape> m_ImplicitShapes;
    std::vector<std::unique_ptr<btCollisionShape>> m_CollisionShapes;
    
    std::vector<std::unique_ptr<btDefaultMotionState>> m_MotionStates;
    std::unordered_map<Primitive::id_t, std::unique_ptr<btRigidBody>> m_RigidBodies;
};

#endif /* Physics_hpp */
