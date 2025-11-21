//
//  importGLTF.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#ifndef importGLTF_hpp
#define importGLTF_hpp

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include "Device.hpp"
#include "Model.hpp"
#include "Primitive.hpp"
#include "Texture.hpp"
#include "Material.hpp"
#include "Light.hpp"

struct Node {
    
    struct Tree {
        Tree(size_t reserve) { nodes.reserve(reserve); }
        
        std::vector<Node> nodes;
        
        void add(Node& n, uint32_t parent) {
            if (parent < nodes.size()) {
                n.parent = parent;
                nodes.push_back(n);
                nodes[parent].children.push_back((uint32_t)nodes.size() - 1);
            }
        }
        void pop(uint32_t node) {
            if (node < nodes.size()) {
                int32_t parent = nodes[node].parent;
                uint32_t child = 0;
                for (auto it = nodes[parent].children.begin(); it < nodes[parent].children.end(); ++it, ++child) {
                    if (nodes[parent].children[child] == node) { nodes[parent].children.erase(it); break; }
                }
                nodes.erase(nodes.begin() + node);
            }
        }
    };
    
    Node(std::string name) : name(name) {}
    
    int32_t parent = -1;
    std::vector<uint32_t> children;
    
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
    std::string name;
};

class NodeSet {
public:
    struct InitStruct {
        Device& device;
        Image& image;
        Primitive::Map& primitives;
        std::vector<std::unique_ptr<Texture>>& textures;
        std::unordered_map<std::string, Material>& materials;
        std::vector<Light>& lights;
    };
    
    NodeSet(InitStruct& init, std::string filePath);
    NodeSet(NodeSet &&) = default;
    
    ~NodeSet();
    
    std::shared_ptr<Node::Tree> getNodes(void) { return m_NodeTree; }
    
private:
    // member variables
    std::string m_FilePath;
    tinygltf::Model m_gltfModel;
    
    Device& m_Device;
    Image& m_Image;
    Primitive::Map& m_Primitives;
    std::vector<std::unique_ptr<Texture>>& m_Textures;
    std::unordered_map<std::string, Material>& m_Materials;
    std::vector<Light>& m_Lights;
    
    std::shared_ptr<Node::Tree> m_NodeTree;
    
    // private methods
    void parseGLTF(void);
    void loadNodeFromModel(int gltfIndex, uint32_t parentIndex, uint32_t& actualIndex);
    void parseMeshFromNode(const tinygltf::Node& node, glm::mat4 transform);
    void parseLightFromNode(const tinygltf::Node& node, glm::mat4 transform);
    void loadMaterialsToVRAM(void);
    void fillSamplerInfo(int textureIndex, VkSamplerCreateInfo *samplerInfo);
    
};

/*
    bool rightHand = true;
    for (auto& node : gltfModel.nodes) {
        if (node.matrix.size() != 16)
            continue;
            
        float D = glm::determinant( glm::make_mat4x4( node.matrix.data() ) );
        if (D < 0) {
            rightHand = false;
            break;
        }
    }
    
    std::cout << (rightHand ? "RIGHT HANDED" : "LEFT HANDED") << std::endl;
*/

#endif /* importGLTF_hpp */
