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
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

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
    
    void addCompoundShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, float mass, int32_t material);
    void addTriangleMeshShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, float mass, int32_t material);
    void addBasicShape(int shape, const Primitive::id_t id, const glm::mat4& transform, float mass, int32_t material);
    size_t addGhostObjectShape(ImplicitShape shape, const glm::mat4& transform);
    void moveGhost(uint32_t ghostIndex, const glm::mat4& from, glm::vec3& dir);
    
    void setStaticRigidBodyFlag(Primitive::id_t id);
    void setKinematicRigidBodyFlag(Primitive::id_t id);
    void setVelocity(Primitive::id_t id, const glm::vec3& linear, const glm::vec3& angular);
    void setGravityFactor(Primitive::id_t id, float factor);
    
    std::unique_ptr<btDiscreteDynamicsWorld>& getWorldHandle(void) { return m_DynamicsWorld; }
    std::unordered_map<Primitive::id_t, std::unique_ptr<btRigidBody>>& getRigidBodyMap(void) { return m_RigidBodies; }
    
    void appendPhysicsMaterial(float friction, float restitution) { m_PhysicsMaterials.emplace_back(PhysicsMaterial(friction, restitution)); }
    void appendImplicitShape(ImplicitShape shape) { m_ImplicitShapes.emplace_back(shape); }
    
private:
    void emplaceShape(ImplicitShape shape);
    void makeRigidBody(btCollisionShape* collisionShape, const Primitive::id_t id, const glm::mat4& transform, float mass, int32_t material);
    
    std::unique_ptr<btBroadphaseInterface> m_BroadPhaseIface;
    std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfig;
    std::unique_ptr<btCollisionDispatcher> m_CollisionDispatcher;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;
    
    // Mesh collision shapes
    struct TriangleMesh {
        std::unique_ptr<btTriangleMesh> mesh;
        std::unique_ptr<btBvhTriangleMeshShape> shape;
    };
    std::vector<TriangleMesh> m_TriangleMeshes;
    
    // CH collision shapes
    struct ConvexHull {
        std::vector<std::unique_ptr<btConvexHullShape>> singleShapes;
        std::unique_ptr<btCompoundShape> compoundShape;
    };
    std::vector<ConvexHull> m_ConvexHulls;
    
    // Basic collision shapes
    std::vector<ImplicitShape> m_ImplicitShapes;
    std::vector<std::unique_ptr<btCollisionShape>> m_CollisionShapes;
    
    // Physics materials definition
    struct PhysicsMaterial {
        float friction;
        float restitution;
    };
    std::vector<PhysicsMaterial> m_PhysicsMaterials;
    
    
    // Rigid collision objects
    std::vector<std::unique_ptr<btDefaultMotionState>> m_MotionStates;
    std::unordered_map<Primitive::id_t, std::unique_ptr<btRigidBody>> m_RigidBodies;
    
    // Ghost collision objects
    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_GhostObjects;
};

#endif /* Physics_hpp */
