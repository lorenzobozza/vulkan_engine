//
//  Nodes.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#ifndef Nodes_hpp
#define Nodes_hpp

#include "Device.hpp"
#include "Texture.hpp"
#include "Primitive.hpp"
#include "Physics.hpp"
#include "Material.hpp"
#include "Light.hpp"
#include "Camera.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include <string>

struct Node {
    struct Tree {
        // glTF -> Vulkan, PI rotation about X axis
        Tree() { Node root("_root"); root.matrix = glm::toMat4(glm::quat(0.f, 1.f, 0.f, 0.f)); add(root); }
        uint32_t add(const Node& n, uint32_t parent = UINT32_MAX);
        void pop(uint32_t node);
        std::vector<Node> nodes;
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
    
    enum Flags {
        MATRIX = 0x1,
        QUAT = 0x2,
        SCALE = 0x4,
        TRANSL = 0x8,
        MESH = 0x10,
        LIGHT = 0x20
    };
    int8_t flags = 0;
};

class NodeSet {
public:
    struct InitStruct {
        const Device& device;
        const Image& image;
        Primitive::Map& primitives;
        Physics& physics;
        Assets& assets;
        Camera::Collection& cameras;
        std::vector<Light>& lights;
        Node::Tree& nodeTree;
    };
    
    NodeSet(const NodeSet&) = delete;
    NodeSet& operator=(const NodeSet&) = delete;
    
    NodeSet(InitStruct& init, std::string filePath);
    ~NodeSet();
        
private:
    void parseGLTF(void);
    void loadNodeFromModel(int gltfIndex, uint32_t parentIndex);
    void parseMeshFromNode(const tinygltf::Node& node, glm::mat4 transform, uint32_t thisIndex);
    void parseLightFromNode(const tinygltf::Node& node, glm::mat4 transform);
    void parseCameraFromNode(const tinygltf::Node& node, glm::mat4 transform);
    void loadMaterialsToVRAM(void);
    void fillSamplerInfo(int textureIndex, VkSamplerCreateInfo *samplerInfo);
    void parsePhysicsMaterialsAndShapes(void);
    
    
    std::string m_FilePath;
    tinygltf::Model m_gltfModel;
    
    const Device& m_Device;
    const Image& m_Image;
    Primitive::Map& m_Primitives;
    Assets& m_Assets;
    Camera::Collection& m_Cameras;
    std::vector<Light>& m_Lights;
    Physics& m_Physics;
    Node::Tree& m_NodeTree;
};

#endif /* Nodes_hpp */
