//
//  Model.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 08/11/21.
//

#include "Mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny-obj/tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>

template <typename T, typename... Rest>
static void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hashCombine(seed, rest), ...);
};

namespace std {
template <>
struct hash<Mesh::Data::Vertex> {
    size_t operator()(Mesh::Data::Vertex const &vertex) const {
        size_t seed = 0;
        hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
        return seed;
    }
};
}


std::vector<VkVertexInputBindingDescription> Mesh::Data::Vertex::getBindingDescriptions(void) {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Mesh::Data::Vertex::getAttributeDescriptions(void) {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    
    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)});
    attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    attributeDescriptions.push_back({5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv1)});
    
    return attributeDescriptions;
}

void Mesh::Data::computeTangentBasis(Vertex &v0, Vertex &v1, Vertex &v2, glm::vec3 *tanOut) {
    // Edges of the triangle : position delta
    glm::vec3 deltaPos1 = v1.position - v0.position;
    glm::vec3 deltaPos2 = v2.position - v0.position;
    
    // UV delta
    glm::vec2 deltaUV1 = v1.uv - v0.uv;
    glm::vec2 deltaUV2 = v2.uv - v0.uv;
    
    if (v1.uv == v0.uv && v2.uv == v0.uv) {
        deltaUV1 = {1.f, .0f};
        deltaUV2 = {0.f, 1.f};
    }
    
    float denom = (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
    float r = denom == 0.f ? 0.f : 1.f / denom;
    
    tanOut[0] = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
    tanOut[1] = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;
}

void Mesh::Data::loadModel(const std::string &filePath, bool allUniqueVertices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        throw std::runtime_error(warn + err);
    }
    
    vertices.clear();
    indices.clear();
    
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto &shape: shapes) {
        
        for (const auto &index: shape.mesh.indices) {
            Vertex vertex{};
            
            if (index.vertex_index >= 0) {
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2]
                };
            }
            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            
            // allUniqueVertices == True treats ALL vertices as unique (bypass overlapping UV bug)
            if (uniqueVertices.count(vertex) == 0 || allUniqueVertices) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
        
    }
    
    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.f));
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.f));
    glm::vec3 tanBasis[2];
    
    // Compute Tangent Basis for each triangle
    for (size_t i = 0; i < indices.size(); i+=3) {
        computeTangentBasis(vertices.at(indices[i]), vertices.at(indices[i +1]), vertices.at(indices[i +2]), tanBasis);
        tangents.at(indices[i]) += tanBasis[0];
        tangents.at(indices[i +1]) += tanBasis[0];
        tangents.at(indices[i +2]) += tanBasis[0];
        bitangents.at(indices[i]) += tanBasis[1];
        bitangents.at(indices[i +1]) += tanBasis[1];
        bitangents.at(indices[i +2]) += tanBasis[1];
    }
    
    // Assign oriented Tangent Basis to each vertex
    for (size_t i = 0; i < vertices.size(); i++) {
        glm::vec3 N = vertices.at(i).normal;
        glm::vec3 T = tangents.at(i);
        // Re-Orthogonalize, then Normalize
        T = glm::normalize(T - (glm::dot(T, N) * N));
        float w = glm::dot(glm::cross(N, T), bitangents.at(i)) < 0.f ? -1.f : 1.f;
        
        vertices.at(i).tangent = {T, w};
    }
    
}

Mesh::Data Mesh::Data::makeSimpleCube(bool invert) {
    Mesh::Data cubeData;
    cubeData.vertices = {
        {{-1.f, -1.f, 1.f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, 1.f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, 1.f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, 1.f}, {}, {}, {}, {0.f, 1.f}},
        {{-1.f, -1.f, -1.f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, -1.f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, -1.f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, -1.f}, {}, {}, {}, {0.f, 1.f}}
    };
    if (invert) {
        cubeData.indices = {
            0,2,1,2,0,3,
            4,5,6,6,7,4,
            1,6,5,6,1,2,
            0,4,7,7,3,0,
            4,1,5,1,4,0,
            3,6,2,6,3,7
        };
    } else {
        cubeData.indices = {
            2,0,1,0,2,3,
            5,4,6,7,6,4,
            6,1,5,1,6,2,
            4,0,7,3,7,0,
            1,4,5,4,1,0,
            6,3,2,3,6,7
        };
    }
    return cubeData;
}

Mesh::Mesh(const Device& dev, const Data &data) : m_Device(dev) {
    createVertexBuffer(data.vertices);
    createIndexBuffer(data.indices);
}

Mesh::~Mesh() {}

std::unique_ptr<Mesh> Mesh::createModelFromObj(const Device& device, const std::string &filePath, bool allUniqueVertices) {
    Data data{};
    data.loadModel(filePath, allUniqueVertices);
    return std::make_unique<Mesh>(device, data);
}

void Mesh::bind(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {m_VertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    
    if (m_HasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void Mesh::draw(VkCommandBuffer commandBuffer) {
    if (m_HasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, m_VertexCount, 1, 0, 0);
    }
}

void Mesh::createVertexBuffer(const std::vector<Data::Vertex> &vertices) {
    m_VertexCount = static_cast<uint32_t>(vertices.size());
    assert(m_VertexCount >= 3 && "createVertexBuffer(): Vertex count must be at least 3");
    
    uint32_t vertexSize = sizeof(vertices[0]);
    VkDeviceSize bufferSize = vertexSize * m_VertexCount;
    
    Buffer stagingBuffer{
        m_Device,
        vertexSize,
        m_VertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)vertices.data());
    
    
    m_VertexBuffer = std::make_unique<Buffer>(m_Device, vertexSize, m_VertexCount,
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    m_Device.copyBuffer(stagingBuffer.getBuffer(), m_VertexBuffer->getBuffer(), bufferSize);
}

void Mesh::createIndexBuffer(const std::vector<uint32_t> &indices) {
    m_IndexCount = static_cast<uint32_t>(indices.size());
    m_HasIndexBuffer = m_IndexCount > 0;
    
    if (!m_HasIndexBuffer) { return; }
    
    uint32_t indexSize = sizeof(indices[0]);
    VkDeviceSize bufferSize = indexSize * m_IndexCount;
    
    Buffer stagingBuffer{
        m_Device,
        indexSize,
        m_IndexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)indices.data());
    
    m_IndexBuffer = std::make_unique<Buffer>(m_Device, indexSize, m_IndexCount,
                                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    m_Device.copyBuffer(stagingBuffer.getBuffer(), m_IndexBuffer->getBuffer(), bufferSize);
}
