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
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)});
    attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

    return attributeDescriptions;
}

void Model::Data::computeTangentBasis(Model::Vertex &v0, Model::Vertex &v1, Model::Vertex &v2, glm::vec3 *tanOut) {
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
    tanOut[1] = (deltaPos1 * deltaUV2.x - deltaPos2 * deltaUV1.x) * r;
 }

void Model::Data::loadModel(const std::string &filePath, bool allUniqueVertices) {
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

Model::Model(Device &dev, const Data &data) : device{dev} {
    createVertexBuffer(data.vertices);
    createIndexBuffer(data.indices);
}

Model::~Model() {}

std::unique_ptr<Model> Model::createModelFromFile(Device &device, const std::string &filePath, bool allUniqueVertices) {
    Data data{};
    data.loadModel(filePath, allUniqueVertices);
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
