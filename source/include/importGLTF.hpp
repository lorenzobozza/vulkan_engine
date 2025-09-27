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

class NodeSet {
    struct Node {
        Node* p_Parent{nullptr};
        
        glm::mat4 Matrix{1.0f};
        glm::quat Quat{1.0f,0.0f,0.0f,0.0f};
        glm::vec3 Scale{1.0f};
        glm::vec3 Offset{0.0f};
    };
    
    // member variables
    std::vector<Node*> m_Nodes;
    std::string m_FilePath;
    tinygltf::Model m_gltfModel;
    
    Device& m_Device;
    Image& m_Image;
    Primitive::Map& m_Primitives;
    std::vector<std::unique_ptr<Texture>>& m_Textures;
    std::unordered_map<std::string, Material>& m_Materials;
    
    // private methods
    void loadBinaryGLB(void);
    void loadNodeFromModel(int nodeIndex, Node* parentNode);
    void parseMeshFromNode(const tinygltf::Node& node, glm::mat4 transform);
    void loadMaterialsToVRAM(void);
    void fillSamplerInfo(int textureIndex, VkSamplerCreateInfo *samplerInfo);
    
public:
    struct InitStruct {
        Device& device;
        Image& image;
        Primitive::Map& primitives;
        std::vector<std::unique_ptr<Texture>>& textures;
        std::unordered_map<std::string, Material>& materials;
    };
    
    NodeSet(InitStruct& init, std::string filePath);
    NodeSet(NodeSet &&) = default;
    
    ~NodeSet();
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
