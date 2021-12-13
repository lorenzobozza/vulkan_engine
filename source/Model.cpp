//
//  Model.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 08/11/21.
//

#include "include/Model.hpp"
#include "include/utils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny-obj/tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

//std
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <iostream>

namespace std {
    template <>
    struct hash<Model::Vertex> {
        size_t operator()(Model::Vertex const &vertex) const {
            size_t seed = 0;
            hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}


std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    
    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)});
    attributeDescriptions.push_back({5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bitangent)});
    
    return attributeDescriptions;
}

 void Model::Data::computeTangentBasis(Model::Vertex &v0, Model::Vertex &v1, Model::Vertex &v2) {
    // Edges of the triangle : position delta
    glm::vec3 deltaPos1 = v1.position - v0.position;
    glm::vec3 deltaPos2 = v2.position - v0.position;

    // UV delta
    glm::vec2 deltaUV1 = v1.uv - v0.uv;
    glm::vec2 deltaUV2 = v2.uv - v0.uv;
    
    float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
    glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
    glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

    if (v0.tangent != glm::vec3{.0f} || v0.bitangent != glm::vec3{.0f}) {
        v0.tangent = normalize(v0.tangent + bitangent);
        v0.bitangent = normalize(v0.bitangent + bitangent);
    } else {
        v0.tangent = tangent;
        v0.bitangent = bitangent;
    }
    
    if (v1.tangent != glm::vec3{.0f} || v1.bitangent != glm::vec3{.0f}) {
        v1.tangent = normalize(v1.tangent + bitangent);
        v1.bitangent = normalize(v1.bitangent + bitangent);
    } else {
        v1.tangent = tangent;
        v1.bitangent = bitangent;
    }
    
    if (v2.tangent != glm::vec3{.0f} || v2.bitangent != glm::vec3{.0f}) {
        v2.tangent = normalize(v2.tangent + bitangent);
        v2.bitangent = normalize(v2.bitangent + bitangent);
    } else {
        v2.tangent = tangent;
        v2.bitangent = bitangent;
    }

 }

void Model::Data::loadModel(const std::string &filePath) {
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
    
        uint32_t vertexCount{0};
        
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
            
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
            
            if (vertexCount++ == 2) {
                
                // compute tangent basis for every triangle
                computeTangentBasis(
                    vertices.at(indices.rbegin()[0]),
                    vertices.at(indices.rbegin()[1]),
                    vertices.at(indices.rbegin()[2])
                );
                vertexCount = 0;
            }
            
        }
        
    }
}

Model::Model(Device &dev, const Data &data) : device{dev} {
    createVertexBuffer(data.vertices);
    createIndexBuffer(data.indices);
}

Model::~Model() {}

std::unique_ptr<Model> Model::createModelFromFile(Device &device, const std::string &filePath) {
    Data data{};
    data.loadModel(filePath);
    return std::make_unique<Model>(device, data);
}

void Model::bind(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    
    if (hasIndexBuffer) {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void Model::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void Model::createVertexBuffer(const std::vector<Vertex> &vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    
    uint32_t vertexSize = sizeof(vertices[0]);
    VkDeviceSize bufferSize = vertexSize * vertexCount;
    
    Buffer stagingBuffer{
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)vertices.data());


    vertexBuffer = std::make_unique<Buffer>(
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

void Model::createIndexBuffer(const std::vector<uint32_t> &indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;
    
    if (!hasIndexBuffer) { return; }
    
    uint32_t indexSize = sizeof(indices[0]);
    VkDeviceSize bufferSize = indexSize * indexCount;
    
    Buffer stagingBuffer{
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)indices.data());

    indexBuffer = std::make_unique<Buffer>(
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}
