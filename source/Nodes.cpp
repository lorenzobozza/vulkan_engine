//
//  Nodes.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#include "Nodes.hpp"
#include "Log.hpp"

#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include "mikktspace.h"
#include "TaskScheduler.h"

#include <random>

enki::TaskScheduler g_TS;

static void convertImageData(tinygltf::Image& image);
static bool tinygltf_LoadImageDataCallback(tinygltf::Image *image, const int image_idx, std::string *err,
                                           std::string *warn, int req_width, int req_height,
                                           const unsigned char *bytes, int size, void *user_data);
// MikkTSpace callbacks
int giveNumFaces(const SMikkTSpaceContext * pContext);
int giveNumVerticesOfFace(const SMikkTSpaceContext * pContext, const int iFace);
void givePosition(const SMikkTSpaceContext * pContext, float fvPosOut[], const int iFace, const int iVert);
void giveNormal(const SMikkTSpaceContext * pContext, float fvNormOut[], const int iFace, const int iVert);
void giveTexCoord(const SMikkTSpaceContext * pContext, float fvTexcOut[], const int iFace, const int iVert);
void takeTSpaceBasic(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert);


uint32_t Node::Tree::add(const Node& n, uint32_t parent) {
    if (parent == UINT32_MAX) {
        nodes.emplace_back(n);
        if (n.parent >= 0 && (uint32_t)n.parent < nodes.size())
            nodes[n.parent].children.push_back((uint32_t)nodes.size() - 1);
        return (uint32_t)nodes.size() - 1;
    } else if (parent < nodes.size()) {
        nodes.emplace_back(n);
        nodes.back().parent = parent;
        nodes[parent].children.push_back((uint32_t)nodes.size() - 1);
        return (uint32_t)nodes.size() - 1;
    }
    return UINT32_MAX;
}
void Node::Tree::pop(uint32_t node) {
    if (node < nodes.size()) {
        int32_t parent = nodes[node].parent;
        uint32_t child = 0;
        for (auto it = nodes[parent].children.begin(); it < nodes[parent].children.end(); ++it, ++child) {
            if (nodes[parent].children[child] == node) { nodes[parent].children.erase(it); break; }
        }
        nodes.erase(nodes.begin() + node);
    }
}

struct ImageParseTaskSet : enki::ITaskSet {
    ImageParseTaskSet(std::vector<tinygltf::Image>& images) : m_Images(images) { m_SetSize = (uint32_t)m_Images.size(); }
    
    std::vector<tinygltf::Image>& m_Images;
    
    void ExecuteRange( enki::TaskSetPartition range_, uint32_t threadnum_ ) override {
        for(unsigned int i = range_.start; i < range_.end; ++i ) {
            convertImageData(m_Images.at(i));
        }
    }
};

NodeSet::NodeSet(InitStruct& init, std::string filePath) : m_Device(init.device), m_Image(init.image),
m_Primitives(init.primitives), m_Assets(init.assets), m_Lights(init.lights), m_FilePath(filePath) {
    parseGLTF();
    
    const unsigned int vCpu = enki::GetNumHardwareThreads();
    g_TS.Initialize(vCpu);
    
    ImageParseTaskSet task(m_gltfModel.images);
    g_TS.AddTaskSetToPipe( &task );
    
    g_TS.WaitforTask( &task );
    
    g_TS.ShutdownNow();
    
    // glTF -> Vulkan, unit quaternion along X to rotate 180° about X
    m_NodeTree = std::make_shared<Node::Tree>(m_gltfModel.nodes.size() + 1);
    Node root("_root");
    root.matrix = glm::toMat4(glm::quat{0.f, 1.f, 0.f, 0.f});
    m_NodeTree->add(root);
    
    for(int gltfIndex: m_gltfModel.scenes[m_gltfModel.defaultScene].nodes) {
        loadNodeFromModel(gltfIndex, 0);
    }
    
    loadMaterialsToVRAM();
}

NodeSet::~NodeSet() {}

void NodeSet::parseGLTF(void) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    loader.SetImageLoader(tinygltf_LoadImageDataCallback, nullptr);
    
    bool ret = false;
    auto fileExt = m_FilePath.substr(m_FilePath.size() - 4, m_FilePath.size() - 1);
    
    if (fileExt == ".glb") {
        ret = loader.LoadBinaryFromFile(&m_gltfModel, &err, &warn, m_FilePath);
    }
    else if (fileExt == "gltf") {
        ret = loader.LoadASCIIFromFile(&m_gltfModel, &err, &warn, m_FilePath);
    }
    
    if (!warn.empty()) {
        std::println("glTF Warning: {}", warn.c_str());
    }
    
    if (!err.empty()) {
        std::println("glTF Error: {}", err.c_str());
    }
    
    if (!ret) {
        std::println("Failed to parse glTF");
    }
}

void NodeSet::loadNodeFromModel(int gltfIndex, uint32_t parentIndex) {
    const tinygltf::Node& gltfNode = m_gltfModel.nodes[gltfIndex];
    
    uint32_t thisIndex = m_NodeTree->add(Node(gltfNode.name), parentIndex);
    
    {
    Node& thisNode = m_NodeTree->nodes.back();
    
    if (gltfNode.scale.size() == 3) {
        thisNode.flags |= Node::Flags::SCALE;
        thisNode.scale = glm::make_vec3(gltfNode.scale.data());
    }
    if (gltfNode.translation.size() == 3) {
        thisNode.flags |= Node::Flags::TRANSL;
        thisNode.transl = glm::make_vec3(gltfNode.translation.data());
    }
    if (gltfNode.rotation.size() == 4) {
        thisNode.flags |= Node::Flags::QUAT;
        // by default glm uses w,x,y,z while glTF uses x,y,z,w
        thisNode.quat = glm::quat((float)gltfNode.rotation[3], (float)gltfNode.rotation[0], (float)gltfNode.rotation[1], (float)gltfNode.rotation[2]);
    }
    if (gltfNode.matrix.size() == 16) {
        thisNode.flags |= Node::Flags::MATRIX;
        thisNode.matrix = glm::make_mat4x4(gltfNode.matrix.data());
    }
    if (gltfNode.mesh > -1) thisNode.flags |= Node::Flags::MESH;
    if (gltfNode.light > -1) thisNode.flags |= Node::Flags::LIGHT;
    
    thisNode.matrix *= (glm::translate(glm::mat4(1.0), thisNode.transl) * glm::toMat4(thisNode.quat) * glm::scale(glm::mat4(1.0), thisNode.scale));
    }
    
    // Tree propagation
    if (gltfNode.children.size() > 0) {
        for (int gltfChild : gltfNode.children) {
            loadNodeFromModel(gltfChild, thisIndex);
        }
    }
    
    // MAYBE we can determine if the transformation is needed to save time
    glm::mat4 transform = m_NodeTree->nodes[thisIndex].matrix;
    
    // Move backwards to build the correct transformation matrix
    int32_t parent = m_NodeTree->nodes[thisIndex].parent;
    
    while (parent > -1) {
        transform = m_NodeTree->nodes[parent].matrix * transform;
        parent = m_NodeTree->nodes[parent].parent;
    }
    
    parseLightFromNode(gltfNode, transform);
    parseMeshFromNode(gltfNode, transform);
}

void NodeSet::parseLightFromNode(const tinygltf::Node& node, glm::mat4 transform) {
    int lightIdx = node.light;
    if (lightIdx > -1) {
        auto& light = m_gltfModel.lights[lightIdx];
        
        if (light.type == Light::gltfTypes[Light::Type::Point]) {
            m_Lights.emplace_back(Light::makePoint(glm::vec3(transform[3].x, transform[3].y, transform[3].z),
                                                   glm::vec4(glm::make_vec3(light.color.data()), light.intensity * 0.0184f)));
            
        } else if (light.type == Light::gltfTypes[Light::Type::Directional]) {
            m_Lights.emplace_back(Light::makeDirectional(glm::vec3(-transform[2].x, -transform[2].y, -transform[2].z),
                                                         glm::vec4(glm::make_vec3(light.color.data()), light.intensity * 0.00146f)));
            m_Lights.back().m_Data.lightSpaceMatrix =
            glm::orthoLH_ZO(-20.0f, 20.0f, -25.0f, 20.0f, -20.f, 10.f)
            * glm::lookAt(m_Lights.back().m_Data.dir, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
            
        } else if (light.type == Light::gltfTypes[Light::Type::Spot]) {
            Log::getInstance()->warn("Node {} contains a Spot-Type light source, which is currently not supported.", light.name);
        }
    }
}

void NodeSet::parseMeshFromNode(const tinygltf::Node& node, glm::mat4 transform) {
    if (node.mesh > -1) {
        const tinygltf::Mesh& mesh = m_gltfModel.meshes[node.mesh];
        
        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            Mesh::Data data{};
            uint32_t indexOffset = 0;
            
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            glm::vec3 posMin{};
            glm::vec3 posMax{};
            
            const float* bufferPos = nullptr;
            const float* bufferNormals = nullptr;
            const float* bufferTangents = nullptr;
            const float* bufferColor = nullptr;
            const float* bufferTexCoordSet0 = nullptr;
            const float* bufferTexCoordSet1 = nullptr;
            
            int posByteStride;
            int normByteStride{0};
            int tangByteStride{0};
            int colorByteStride{0};
            int uv0ByteStride{0};
            int uv1ByteStride{0};
            
            bool hasIndices = primitive.indices > -1;
            
            // Position is required
            const tinygltf::Accessor& posAccessor = m_gltfModel.accessors[primitive.attributes.find("POSITION")->second];
            const tinygltf::BufferView& posView = m_gltfModel.bufferViews[posAccessor.bufferView];
            
            bufferPos = reinterpret_cast<const float *>(&(m_gltfModel.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
            posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
            posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
            vertexCount = static_cast<uint32_t>(posAccessor.count);
            posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
            
            // Normal
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& normAccessor = m_gltfModel.accessors[primitive.attributes.find("NORMAL")->second];
                const tinygltf::BufferView& normView = m_gltfModel.bufferViews[normAccessor.bufferView];
                
                bufferNormals = reinterpret_cast<const float *>(&(m_gltfModel.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
                normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
            }
            
            // Tangent
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                const tinygltf::Accessor& tangAccessor = m_gltfModel.accessors[primitive.attributes.find("TANGENT")->second];
                const tinygltf::BufferView& tangView = m_gltfModel.bufferViews[tangAccessor.bufferView];
                
                bufferTangents = reinterpret_cast<const float *>(&(m_gltfModel.buffers[tangView.buffer].data[tangAccessor.byteOffset + tangView.byteOffset]));
                tangByteStride = tangAccessor.ByteStride(tangView) ? (tangAccessor.ByteStride(tangView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
            }
            
            // Vertex colors
            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = m_gltfModel.accessors[primitive.attributes.find("COLOR_0")->second];
                const tinygltf::BufferView& view = m_gltfModel.bufferViews[accessor.bufferView];
                
                bufferColor = reinterpret_cast<const float*>(&(m_gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                colorByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
            }
            
            // UVs
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& uvAccessor = m_gltfModel.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                const tinygltf::BufferView& uvView = m_gltfModel.bufferViews[uvAccessor.bufferView];
                
                bufferTexCoordSet0 = reinterpret_cast<const float *>(&(m_gltfModel.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
            }
            
            if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
                const tinygltf::Accessor& uvAccessor = m_gltfModel.accessors[primitive.attributes.find("TEXCOORD_1")->second];
                const tinygltf::BufferView& uvView = m_gltfModel.bufferViews[uvAccessor.bufferView];
                
                bufferTexCoordSet1 = reinterpret_cast<const float *>(&(m_gltfModel.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                uv1ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
            }
            
            for (size_t v = 0; v < posAccessor.count; v++) {
                Mesh::Data::Vertex vertex{};
                vertex.position = glm::make_vec3(&bufferPos[v * posByteStride]);
                vertex.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
                
                vertex.tangent = glm::vec4(1.f, glm::normalize(glm::vec3(bufferTangents ? glm::make_vec3(&bufferTangents[v * tangByteStride]) : glm::vec3(0.0f))));
                
                vertex.color = bufferColor ? glm::make_vec4(&bufferColor[v * colorByteStride]) : glm::vec4(1.0f);
                vertex.uv = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
                vertex.uv1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec3(0.0f);
                
                data.vertices.push_back(vertex);
            }
            
            if (hasIndices) {
                const tinygltf::Accessor& accessor = m_gltfModel.accessors[primitive.indices > -1 ? primitive.indices : 0];
                const tinygltf::BufferView& bufferView = m_gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = m_gltfModel.buffers[bufferView.buffer];
                
                indexCount = static_cast<uint32_t>(accessor.count);
                const void *dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                
                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                    const uint32_t *buf = static_cast<const uint32_t*>(dataPtr);
                    for (size_t i = 0; i < indexCount; i++)
                        data.indices.push_back(static_cast<uint32_t>(buf[i]) + indexOffset);
                    }
                    break;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                    const uint16_t *buf = static_cast<const uint16_t*>(dataPtr);
                    for (size_t i = 0; i < indexCount; i++)
                        data.indices.push_back(static_cast<uint32_t>(buf[i]) + indexOffset);
                    }
                    break;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                    const uint8_t *buf = static_cast<const uint8_t*>(dataPtr);
                    for (size_t i = 0; i < indexCount; i++)
                        data.indices.push_back(static_cast<uint32_t>(buf[i]) + indexOffset);
                    }
                    break;
                default:
                    std::println("Index component type {} not supported!", accessor.componentType);
                    return;
                }
                
                indexOffset = static_cast<uint32_t>(data.vertices.size());
            }
            
            
            SMikkTSpaceInterface myMikkInterface {
                .m_getNumFaces = giveNumFaces,
                .m_getNumVerticesOfFace = giveNumVerticesOfFace,
                .m_getPosition = givePosition,
                .m_getNormal = giveNormal,
                .m_getTexCoord = giveTexCoord,
                .m_setTSpaceBasic = takeTSpaceBasic
            };
            
            SMikkTSpaceContext tSpaceContext {
                .m_pInterface = &myMikkInterface,
                .m_pUserData = (void*)&data,
            };
            
            genTangSpaceDefault(&tSpaceContext);
            
            
            int materialID = primitive.material;
            
            Primitive p = Primitive::new_primitive();
            
            {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.f, .5f);
            const glm::vec3 color = {dis(gen), dis(gen), dis(gen)};
            
            Mesh::Data cubeData;
            cubeData.vertices = {
                {posMin,                         color, {}, {}, {0.f, 0.f}},
                {{posMax.x, posMin.y, posMin.z}, color, {}, {}, {1.f, 0.f}},
                {{posMax.x, posMax.y, posMin.z}, color, {}, {}, {1.f, 1.f}},
                {{posMin.x, posMax.y, posMin.z}, color, {}, {}, {0.f, 1.f}},
                
                {{posMin.x, posMin.y, posMax.z}, color, {}, {}, {0.f, 0.f}},
                {{posMax.x, posMin.y, posMax.z}, color, {}, {}, {1.f, 0.f}},
                {posMax,                         color, {}, {}, {1.f, 1.f}},
                {{posMin.x, posMax.y, posMax.z}, color, {}, {}, {0.f, 1.f}},
            };
            cubeData.indices = {
                0,1, 1,2, 2,3, 3,0,
                4,5, 5,6, 6,7, 7,4,
                0,4, 1,5, 2,6, 3,7,
            };
            p.aabb = std::make_shared<Mesh>(m_Device, cubeData);
            }
            {
            Mesh::Data normals;
            normals.vertices.resize(data.vertices.size() * 2);
            size_t i = 0;
            for (auto& v : data.vertices) {
                normals.vertices[i].color = (v.normal + 1.f) * 0.5f;
                normals.vertices[i++].position = v.position;
                normals.vertices[i].color = (v.normal + 1.f) * 0.5f;
                normals.vertices[i++].position = v.position + (glm::normalize(v.normal) * .2f / p.transform.scale);
            }
            p.normals = std::make_shared<Mesh>(m_Device, normals);
            }
            
            p.model = std::make_shared<Mesh>(m_Device, data);
            
            p.transform.hasMatrix = true;
            p.transform.matrix = transform;
            
            // TODO: Make the whole node transform hierarchy always affect the final matrix (needs cache system)
            
            if (materialID > -1) {
                if (m_gltfModel.materials[materialID].alphaMode != "OPAQUE") continue;
                p.material = m_gltfModel.materials[materialID].name + "_" + std::to_string(materialID);
            } else {
                p.material = "Global_Default_Material";
            }
            
            m_Primitives.emplace(p.getId(), std::move(p));
        }
        
    }
}


void NodeSet::loadMaterialsToVRAM(void) {
    size_t index = m_Assets.textures.size();
    size_t materialID{0};
    bool mipMapping = true;
    
    const VkFormat default_rgb_format = VK_FORMAT_R8G8B8_UNORM;
    const VkFormat default_rgba_format = VK_FORMAT_R8G8B8A8_UNORM;
    
    m_Assets.textures.reserve(index + m_gltfModel.textures.size());
    m_Assets.materials.reserve(m_Assets.materials.size() + m_gltfModel.materials.size());
    for (const tinygltf::Material &gltfMaterial : m_gltfModel.materials) {
        
        int colorTextureIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        int normalTextureIndex = gltfMaterial.normalTexture.index;
        int metalRoughTextureIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        int occlusionTextureIndex = gltfMaterial.occlusionTexture.index;
        
        Material material;
        
        VkSamplerCreateInfo samplerInfo = {};
        
        if (gltfMaterial.alphaMode == "BLEND") {
            material.alphaMode = Material::ALPHAMODE_BLEND;
        } else if (gltfMaterial.alphaMode == "MASK") {
            material.alphaMode = Material::ALPHAMODE_MASK;
            material.alphaCutoff = (float)gltfMaterial.alphaCutoff;
        }
        
        material.color = glm::make_vec4(gltfMaterial.pbrMetallicRoughness.baseColorFactor.data());
        if (colorTextureIndex > -1) {
            const tinygltf::Image& color = m_gltfModel.images[m_gltfModel.textures[colorTextureIndex].source];
            
            fillSamplerInfo(colorTextureIndex, &samplerInfo);
            
            m_Assets.textures.emplace_back(std::make_unique<Texture>(m_Device, m_Image, (void*)color.image.data(),
                                                                     color.width, color.height, color.component, mipMapping,
                                                                     (color.component > 3 ? default_rgba_format : default_rgb_format), &samplerInfo));
            material.setColorTexture(index++);
            material.setNormalTexCoordSet(gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord);
            
        }
        
        if (normalTextureIndex > -1) {
            const tinygltf::Image& normal = m_gltfModel.images[m_gltfModel.textures[normalTextureIndex].source];
            
            fillSamplerInfo(normalTextureIndex, &samplerInfo);
            
            m_Assets.textures.emplace_back(std::make_unique<Texture>(m_Device, m_Image, (void*)normal.image.data(),
                                                                     normal.width, normal.height, normal.component, mipMapping,
                                                                     (normal.component > 3 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM), &samplerInfo));
            material.setNormalTexture(index++);
            material.setNormalTexCoordSet(gltfMaterial.normalTexture.texCoord);
        }
        
        if (occlusionTextureIndex > -1) {
            const tinygltf::Image& occlusion = m_gltfModel.images[m_gltfModel.textures[occlusionTextureIndex].source];
            
            fillSamplerInfo(occlusionTextureIndex, &samplerInfo);
            
            m_Assets.textures.emplace_back(std::make_unique<Texture>(m_Device, m_Image, (void*)occlusion.image.data(),
                                                                     occlusion.width, occlusion.height, occlusion.component, mipMapping,
                                                                     (occlusion.component > 3 ? default_rgba_format : default_rgb_format), &samplerInfo));
            material.setOcclusionTexture(index++);
            material.setOcclusionTexCoordSet(gltfMaterial.occlusionTexture.texCoord);
        }
        
        material.metalness = (float)gltfMaterial.pbrMetallicRoughness.metallicFactor;
        material.roughness = (float)gltfMaterial.pbrMetallicRoughness.roughnessFactor;
        if (metalRoughTextureIndex > -1) {
            const tinygltf::Image& metalRough = m_gltfModel.images[m_gltfModel.textures[metalRoughTextureIndex].source];
            
            fillSamplerInfo(metalRoughTextureIndex, &samplerInfo);
            
            m_Assets.textures.emplace_back(std::make_unique<Texture>(m_Device, m_Image, (void*)metalRough.image.data(),
                                                                     metalRough.width, metalRough.height, metalRough.component, mipMapping,
                                                                     (metalRough.component > 3 ? default_rgba_format : default_rgb_format), &samplerInfo));
            material.setRoughMetalTexture(index++);
            material.setMetalRoughTexCoordSet(gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord);
        }
        
        m_Assets.materials.emplace(gltfMaterial.name + "_" + std::to_string(materialID++), std::move(material));
    }
}

static VkSamplerAddressMode getVkWrapMode(int32_t wrapMode) {
    switch (wrapMode) {
    case -1:
    case 10497:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case 33071:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 33648:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkFilter getVkFilterMode(int32_t filterMode) {
    switch (filterMode) {
    case -1:
    case 9728:
        return VK_FILTER_NEAREST;
    case 9729:
        return VK_FILTER_LINEAR;
    case 9984:
        return VK_FILTER_NEAREST;
    case 9985:
        return VK_FILTER_NEAREST;
    case 9986:
        return VK_FILTER_LINEAR;
    case 9987:
        return VK_FILTER_LINEAR;
    }
    
    return VK_FILTER_NEAREST;
}

void NodeSet::fillSamplerInfo(int textureIndex, VkSamplerCreateInfo *samplerInfo) {
    if ((int32_t)m_gltfModel.samplers.size() > textureIndex) {
        samplerInfo->magFilter = getVkFilterMode(m_gltfModel.samplers[textureIndex].magFilter);
        samplerInfo->minFilter = getVkFilterMode(m_gltfModel.samplers[textureIndex].minFilter);
        
        samplerInfo->addressModeU = getVkWrapMode(m_gltfModel.samplers[textureIndex].wrapS);
        samplerInfo->addressModeV = getVkWrapMode(m_gltfModel.samplers[textureIndex].wrapT);
        samplerInfo->addressModeW = samplerInfo->addressModeV;
    } else {
        samplerInfo->magFilter = VK_FILTER_LINEAR;
        samplerInfo->minFilter = VK_FILTER_LINEAR;
        
        samplerInfo->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}


static bool tinygltf_LoadImageDataCallback(tinygltf::Image *image, const int image_idx, std::string *err,
                                           std::string *warn, int req_width, int req_height,
                                           const unsigned char *bytes, int size, void *user_data) {
    image->image.resize(size);
    std::copy(bytes, bytes + size, image->image.begin());
    return true;
}

/*
 Copy raw unconverted data directly to destination
 it will be converted later using multithreading
 */
static void convertImageData(tinygltf::Image& image) {
    int w = 0, h = 0, comp = 0, req_comp = 4;
    
    unsigned char *data = nullptr;
    
    int bits = 8;
    int pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    
    if (stbi_is_16_bit_from_memory(image.image.data(), (int)image.image.size())) {
        data = reinterpret_cast<unsigned char *>(
                                                 stbi_load_16_from_memory(image.image.data(), (int)image.image.size(), &w, &h, &comp, req_comp));
        if (data) {
            bits = 16;
            pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        }
    }
    
    if (!data) data = stbi_load_from_memory(image.image.data(), (int)image.image.size(), &w, &h, &comp, req_comp);
    
    if ((w < 1) || (h < 1)) {
        stbi_image_free(data);
        return;
    }
    
    if (req_comp != 0) {
        comp = req_comp;
    }
    
    image.width = w;
    image.height = h;
    image.component = comp;
    image.bits = bits;
    image.pixel_type = pixel_type;
    image.image.resize(static_cast<size_t>(w * h * comp) * size_t(bits / 8));
    std::copy(data, data + w * h * comp * (bits / 8), image.image.begin());
    
    stbi_image_free(data);
}

int giveNumFaces(const SMikkTSpaceContext * pContext) {
    Mesh::Data const * data = (Mesh::Data*)pContext->m_pUserData;
    
    return static_cast<int>( std::floor(data->indices.size() / 3) );
}

int giveNumVerticesOfFace(const SMikkTSpaceContext * pContext, const int iFace) { return 3; }

void givePosition(const SMikkTSpaceContext * pContext, float fvPosOut[], const int iFace, const int iVert) {
    Mesh::Data * const data = (Mesh::Data*)pContext->m_pUserData;
    
    const int offset = iFace * 3;
    const int index = iVert + offset;
    
    glm::vec3 pos = data->vertices.at( data->indices.at(index) ).position;
    
    fvPosOut[0] = (float)pos.x;
    fvPosOut[1] = (float)pos.y;
    fvPosOut[2] = (float)pos.z;
}

void giveNormal(const SMikkTSpaceContext * pContext, float fvNormOut[], const int iFace, const int iVert) {
    Mesh::Data * const data = (Mesh::Data*)pContext->m_pUserData;
    
    const int offset = iFace * 3;
    const int index = iVert + offset;
    
    glm::vec3 norm = data->vertices.at( data->indices.at(index) ).normal;
    
    fvNormOut[0] = (float)norm.x;
    fvNormOut[1] = (float)norm.y;
    fvNormOut[2] = (float)norm.z;
}

void giveTexCoord(const SMikkTSpaceContext * pContext, float fvTexcOut[], const int iFace, const int iVert) {
    Mesh::Data * const data = (Mesh::Data*)pContext->m_pUserData;
    
    const int offset = iFace * 3;
    const int index = iVert + offset;
    
    glm::vec2 uv = data->vertices.at( data->indices.at(index) ).uv;
    
    fvTexcOut[0] = (float)uv.x;
    fvTexcOut[1] = 1.f - (float)uv.y;
}

void takeTSpaceBasic(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
    Mesh::Data * const data = (Mesh::Data*)pContext->m_pUserData;
    
    const int offset = iFace * 3;
    const int index = iVert + offset;
    
    data->vertices.at( data->indices.at(index) ).tangent = glm::vec4{fvTangent[0], fvTangent[1], fvTangent[2], fSign};
}

/*
 std::vector<glm::vec3> tangents(data.vertices.size(), glm::vec3(0.f));
 std::vector<glm::vec3> bitangents(data.vertices.size(), glm::vec3(0.f));
 glm::vec3 tanBasis[2];
 
 // Compute Tangent Basis for each triangle
 for (size_t i = 0; i < data.indices.size(); i+=3) {
 data.computeTangentBasis(data.vertices.at(data.indices[i]), data.vertices.at(data.indices[i +1]), data.vertices.at(data.indices[i +2]), tanBasis);
 tangents.at(data.indices[i]) += tanBasis[0];
 tangents.at(data.indices[i +1]) += tanBasis[0];
 tangents.at(data.indices[i +2]) += tanBasis[0];
 bitangents.at(data.indices[i]) += tanBasis[1];
 bitangents.at(data.indices[i +1]) += tanBasis[1];
 bitangents.at(data.indices[i +2]) += tanBasis[1];
 }
 
 // Assign oriented Tangent Basis to each vertex
 for (size_t i = 0; i < data.vertices.size(); i++) {
 glm::vec3 N = glm::normalize(data.vertices.at(i).normal);
 glm::vec3 T = glm::normalize(tangents.at(i));
 // Re-Orthogonalize, then Normalize
 T = glm::normalize(T - (glm::dot(N, T) * N));
 float w = glm::dot(glm::cross(N, T), glm::normalize(bitangents.at(i))) < 0.f ? -1.f : 1.f;
 
 data.vertices.at(i).tangent = {T, w};
 }
 
 // Show distribution of indices
 {
 //for (size_t i = 0; i < data.indices.size(); i++) {
 //    data.vertices.at(data.indices.at(i)).tangent = glm::vec4(( (float)i / (float)data.indices.size() ), 0.0, 0.0, 0.0);
 //}
 }
 */
