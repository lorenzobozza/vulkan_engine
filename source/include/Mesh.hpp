//
//  Mesh.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 08/11/21.
//

#ifndef Mesh_hpp
#define Mesh_hpp

#include "Device.hpp"
#include "Buffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <memory>

class Mesh {
public:
    struct Data {
        struct Vertex {
            glm::vec3 position{0.f};
            glm::vec3 color{1.f};
            glm::vec3 normal{};
            glm::vec4 tangent{};
            glm::vec2 uv{};
            glm::vec2 uv1{};
            
            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            
            bool operator==(const Vertex &other) const {
                return position == other.position && color == other.color && normal == other.normal
                && tangent == other.tangent && uv == other.uv && uv1 == other.uv1;
            }
        };
        
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        
        void loadModel(const std::string &filePath, bool allUniqueVertices);
        
        static Data makeSimpleCube(bool invert = false);
        static void computeTangentBasis(Vertex &v0, Vertex &v1, Vertex &v2, glm::vec3 *tanOut);
    };
    
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    
    Mesh(const Device& dev, const Data& data);
    ~Mesh();
    
    static std::unique_ptr<Mesh> createModelFromObj(const Device& device, const std::string &filePath, bool allUniqueVertices = VK_FALSE);
    
    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
    
private:
    void createVertexBuffer(const std::vector<Data::Vertex> &vertices);
    void createIndexBuffer(const std::vector<uint32_t> &indices);
    
    const Device& m_Device;
    
    std::unique_ptr<Buffer> m_VertexBuffer;
    uint32_t m_VertexCount;
    std::unique_ptr<Buffer> m_IndexBuffer;
    uint32_t m_IndexCount;
    
    bool m_HasIndexBuffer = false;
};

#endif /* Mesh_hpp */
