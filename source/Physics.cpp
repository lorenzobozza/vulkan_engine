//
//  Primitive.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/21.
//

#include "Physics.hpp"

#include <HACD/hacdHACD.h>

Physics::Physics() {
    // Broadphase: handles collision detection
    m_BroadPhaseIface = std::make_unique<btDbvtBroadphase>();
    
    // Collision configuration and dispatcher
    m_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    m_CollisionDispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfig.get());
    
    // Solver: resolves constraints
    m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    
    // Dynamics world: main physics environment
    m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(m_CollisionDispatcher.get(),
                                                                m_BroadPhaseIface.get(),
                                                                m_Solver.get(),
                                                                m_CollisionConfig.get());
    
    m_DynamicsWorld->setGravity(btVector3(0.f, 9.81f, 0.f)); // gravity along +Y-axis
}

void Physics::addCompoundShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material) {
    size_t nPoints = data.vertices.size();
    size_t nTriangles = data.indices.size() / 3;
    HACD::Vec3<HACD::Real>* points = new HACD::Vec3<HACD::Real>[nPoints];
    int i = 0;
    for (auto& v : data.vertices) {
        points[i++] = HACD::Vec3<HACD::Real>(v.position.x, v.position.y, v.position.z);
    }
    HACD::Vec3<long>* triangles = new HACD::Vec3<long>[nTriangles];
    for (size_t i = 0, j = 0; i < nTriangles; ++i, j+=3) {
        triangles[i] = HACD::Vec3<long>(data.indices[j], data.indices[j+1], data.indices[j+2]);
    }
    
    HACD::HACD hacd;
    hacd.SetPoints(points);
    hacd.SetNPoints(nPoints);
    hacd.SetTriangles(triangles);
    hacd.SetNTriangles(nTriangles);
    
    hacd.SetCompacityWeight(0.1);
    hacd.SetVolumeWeight(0.0);
    hacd.SetNClusters(0);           // 0 = auto
    hacd.SetNVerticesPerCH(100);    // max vertices per convex hull
    hacd.SetConcavity(0.0025);
    hacd.SetAddExtraDistPoints(true);
    hacd.SetAddFacesPoints(true);
    
    hacd.Compute();
    
    size_t nClusters = hacd.GetNClusters();
    
    ConvexHull convex;
    convex.singleShapes.resize(nClusters);
    convex.compoundShape = std::make_unique<btCompoundShape>();
    
    for (size_t c = 0; c < nClusters; ++c) {
        size_t nVertsCH = hacd.GetNPointsCH(c);
        size_t nTrisCH = hacd.GetNTrianglesCH(c);
        std::vector<HACD::Vec3<HACD::Real>> vertsCH(nVertsCH);
        std::vector<HACD::Vec3<long>> trisCH(nTrisCH);
        hacd.GetCH(c, vertsCH.data(), trisCH.data());
        convex.singleShapes.emplace_back(std::make_unique<btConvexHullShape>());
        for (size_t v=0; v<nVertsCH; ++v) {
            convex.singleShapes.back()->addPoint(btVector3(vertsCH[v].X(), vertsCH[v].Y(), vertsCH[v].Z()));
        }
        btTransform local; local.setIdentity();
        convex.compoundShape->addChildShape(local, convex.singleShapes.back().get());
    }
    
    makeRigidBody(convex.compoundShape.get(), id, transform, mass, material);
    
    m_ConvexHulls.emplace_back(std::move(convex));
}

void Physics::addTriangleMeshShape(Mesh::Data& data, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material) {
    TriangleMesh triangle;
    triangle.mesh = std::make_unique<btTriangleMesh>();
    
    for (auto& v : data.vertices) {
        triangle.mesh->findOrAddVertex(btVector3(v.position.x, v.position.y, v.position.z), false);
    }
    
    for (uint32_t i = 0; i < data.indices.size(); i += 3) {
        triangle.mesh->addTriangleIndices(data.indices[i], data.indices[i+1], data.indices[i+2]);
    }
    
    triangle.shape = std::make_unique<btBvhTriangleMeshShape>(triangle.mesh.get(), true);
    
    makeRigidBody(triangle.shape.get(), id, transform, mass, material);
    
    m_TriangleMeshes.emplace_back(std::move(triangle));
}

void Physics::addBasicShape(int shape, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material) {
    ImplicitShape s = m_ImplicitShapes.at(shape);
    switch (s.type) {
    case ImplicitShape::Plane:
        m_CollisionShapes.emplace_back(std::make_unique<btBoxShape>(btVector3(s.value1 * 0.5f, 0.001f, s.value2 * 0.5f)));
        break;
    case ImplicitShape::Sphere:
        m_CollisionShapes.emplace_back(std::make_unique<btSphereShape>(s.value1));
        break;
    case ImplicitShape::Box:
        m_CollisionShapes.emplace_back(std::make_unique<btBoxShape>(btVector3(s.value1 * 0.5f, s.value2 * 0.5f, s.value3 * 0.5f)));
        break;
    case ImplicitShape::Cylinder:
        if (s.value1 == 0)
            m_CollisionShapes.emplace_back(std::make_unique<btConeShape>(s.value3, s.value2));
        else
            m_CollisionShapes.emplace_back(std::make_unique<btCylinderShape>(btVector3(s.value1, s.value2 * 0.5f, s.value3)));
        break;
    case ImplicitShape::Capsule:
        m_CollisionShapes.emplace_back(std::make_unique<btCapsuleShape>(s.value3, s.value2));
        break;
    default:
        break;
    }

    makeRigidBody(m_CollisionShapes.back().get(), id, transform, mass, material);
}

void Physics::makeRigidBody(btCollisionShape* collisionShape, const Primitive::id_t id, const glm::mat4& transform, const float mass, int material) {
    if (collisionShape == nullptr)
        return;
    
    btTransform T;
    btVector3 inertia(0.f, 0.f, 0.f);
    if (mass > 0.f) {
        collisionShape->calculateLocalInertia(mass, inertia);
    }
    
    collisionShape->setMargin(0.04f);
    
    T.setFromOpenGLMatrix(glm::value_ptr(transform));
    m_MotionStates.emplace_back(std::make_unique<btDefaultMotionState>(T));
    
    btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(mass, m_MotionStates.back().get(), collisionShape, inertia);
    m_RigidBodies.emplace(id, std::make_unique<btRigidBody>(rigidBodyCI));
    
    m_DynamicsWorld->addRigidBody(m_RigidBodies.at(id).get());
    
    m_RigidBodies.at(id)->setFriction(m_PhysicsMaterials.at(material).friction);
    m_RigidBodies.at(id)->setRestitution(m_PhysicsMaterials.at(material).restitution);
    m_RigidBodies.at(id)->setRollingFriction(0.05f);
    m_RigidBodies.at(id)->setSpinningFriction(0.05f);
    //    m_RigidBodies.at(id)->setDamping(0.05f, 0.2f);
    //    m_RigidBodies.at(id)->setCcdMotionThreshold(1.f);
    //    m_RigidBodies.at(id)->setCcdSweptSphereRadius(1.f);
}

void Physics::setStaticRigidBodyFlag(Primitive::id_t id) {
    int flags = m_RigidBodies.at(id)->getCollisionFlags();
    flags |= btCollisionObject::CF_STATIC_OBJECT;
    m_RigidBodies.at(id)->setCollisionFlags(flags);
}
void Physics::setKinematicRigidBodyFlag(Primitive::id_t id) {
    int flags = m_RigidBodies.at(id)->getCollisionFlags();
    flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
    m_RigidBodies.at(id)->setCollisionFlags(flags);
}
void Physics::setVelocity(Primitive::id_t id, const glm::vec3& linear, const glm::vec3& angular) {
    m_RigidBodies.at(id)->setLinearVelocity(btVector3(linear.x, linear.y, linear.z));
    m_RigidBodies.at(id)->setAngularVelocity(btVector3(angular.x, angular.y, angular.z));
}
void Physics::setGravityFactor(Primitive::id_t id, float factor) {
    m_RigidBodies.at(id)->setGravity(btVector3(0.f, 9.81f * factor, 0.f));
}
