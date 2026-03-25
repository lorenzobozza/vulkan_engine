//
//  Nodes.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#ifndef Nodes_hpp
#define Nodes_hpp

#include "Device.hpp"
#include "Primitive.hpp"

#include <string>

struct Node {
    class Tree {
    public:
        Tree(Primitive::Map& priMap);
        ~Tree() = default;
        uint32_t add(const Node& n, uint32_t parent = UINT32_MAX);
        void pop(uint32_t node);
        void invalidateCacheHierarchy(uint32_t node);
        void computeTransformMatrix(uint32_t node);
        std::vector<Node> nodes;
    private:
        Primitive::Map& m_Primitives;
    };
    
    Node(std::string name) : name(name) {}
    
    int32_t parent = -1;
    std::vector<uint32_t> children;
    std::vector<Primitive::id_t> primitives;
    bool aabb = false;
        
    std::string name;
    glm::mat4 matrix{1.0f};
    glm::quat quat{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 transl{0.0f};
    
    glm::mat4 cacheMatrix{1.0f};
    
    bool invalidateCache = true;
    // Would be nice to reference the vector inside each object to call tree affecting function from the node
    
    enum Flags {
        MATRIX = 0x1,
        QUAT = 0x2,
        SCALE = 0x4,
        TRANSL = 0x8,
        MESH = 0x10,
        LIGHT = 0x20,
        CAMERA = 0x40
    };
    int8_t flags = 0;
    
    struct tinyGLTF {
        int nodeID = -1;
        int meshID = -1;
        int lightID = -1;
        int cameraID = -1;
    } tinygltf;
};

#endif /* Nodes_hpp */
