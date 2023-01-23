//
//  Model.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 08/11/21.
//

#ifndef Model_hpp
#define Model_hpp

#include "Device.hpp"
#include "Buffer.hpp"

//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

//std
#include <vector>
#include <memory>

class Model {
public:
    struct Vertex {
        glm::vec3 position{0.f};
        glm::vec3 color{1.f};
        glm::vec3 normal{};
        glm::vec4 tangent{0.f};
        glm::vec2 uv{};

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        
        bool operator==(const Vertex &other) const {
            return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
        }
    };
    
    struct Data {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        
        void computeTangentBasis(Model::Vertex &v0, Model::Vertex &v1, Model::Vertex &v2, glm::vec3 *tanOut);
        
        void loadModel(const std::string &filePath);
    };
    
    Model(Device &dev, const Data &data);
    ~Model();
    
    // Prevent Obj copy
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
        
    static std::unique_ptr<Model> createModelFromFile(Device &device, const std::string &filePath);
    
    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
    
private:
    void createVertexBuffer(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<uint32_t> &indices);
    
    Device &device;
    
    std::unique_ptr<Buffer> vertexBuffer;
    uint32_t vertexCount;
    
    std::unique_ptr<Buffer> indexBuffer;
    uint32_t indexCount;
    bool hasIndexBuffer = false;
};

#endif /* Model_hpp */
