//
//  ImportGLTF.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#ifndef ImportGLTF_hpp
#define ImportGLTF_hpp

#include "Device.hpp"
#include "Primitive.hpp"
#include "Physics.hpp"
#include "Assets.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "Nodes.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include <string>


class ImportGLTF {
public:
    struct InitStruct {
        const Device& device;
        const Image& image;
        Primitive::Map& primitives;
        Primitive::Map& primitivesAlpha;
        Physics& physics;
        Assets& assets;
        Camera::Collection& cameras;
        std::vector<Light>& lights;
        Node::Tree& nodeTree;
    };
    
    ImportGLTF(const ImportGLTF&) = delete;
    ImportGLTF& operator=(const ImportGLTF&) = delete;
    
    ImportGLTF(InitStruct& init, std::string filePath);
    ~ImportGLTF() = default;
        
private:
    void parseGLTF(void);
    void loadNodeFromModel(int gltfIndex, uint32_t parentIndex);
    void parseMeshFromNode(const tinygltf::Node& node, uint32_t nodeIndex);
    void parseLightFromNode(const tinygltf::Node& node, uint32_t nodeIndex);
    void parseCameraFromNode(const tinygltf::Node& node, uint32_t nodeIndex);
    void loadTextures(void);
    void loadMaterials(void);
    void fillSamplerInfo(int textureIndex, VkSamplerCreateInfo *samplerInfo);
    void parsePhysicsMaterialsAndShapes(void);
    
    
    std::string m_FilePath;
    tinygltf::Model m_gltfModel;
    
    const Device& m_Device;
    const Image& m_Image;
    Primitive::Map& m_Primitives;
    Primitive::Map& m_PrimitivesAlpha;
    Assets& m_Assets;
    Camera::Collection& m_Cameras;
    std::vector<Light>& m_Lights;
    Physics& m_Physics;
    Node::Tree& m_NodeTree;
    
    std::vector<void*> m_StbBuffers;
};

#endif /* ImportGLTF_hpp */
