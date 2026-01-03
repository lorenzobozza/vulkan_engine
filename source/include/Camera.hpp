//
//  Camera.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/11/21.
//

#ifndef Camera_hpp
#define Camera_hpp

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <mutex>

class Camera {
public:
    Camera() = default;
    ~Camera() = default;
    
    void setPerspectiveProjection(float aspect, float fovy, float near, float far);
    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    const float getAspectRatio(void) const { return m_AspectRatio; }
    void changeAspectRatio(float aspect);

    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewTarget(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewYXZDelta(glm::vec3 deltaP, glm::vec3 deltaR);
    void pivotAroundOrigin(glm::vec3 deltaR = glm::vec3(.0f, .0f, .0f));
    
    void setView(glm::mat4 view) { viewMatrix = view; }
    void setInverseView(glm::mat4 invView) { inverseViewMatrix = invView; }
    void setPosition(glm::vec3 pos) { inverseViewMatrix[3] = glm::vec4(pos, 1.f); }

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }
    const glm::mat4& getInverseView() const { return inverseViewMatrix; }
    const float getYaw(void) const { return m_Fpv.yaw; }
    const float getFov(void) const { return m_FovY; }
    
    size_t ghostObject;
    bool noClip = true;
    
    struct Collection {
        Collection() {
            list.reserve(2);
            list.emplace_back(Camera());
            list.back().setPerspectiveProjection(1.77f, glm::radians(90.f), 0.1f, 100.f);
            list.back().setPosition(glm::vec3(0.f, -10.6f, 10.6f));
            list.back().pivotAroundOrigin();
        }
        std::vector<Camera> list;
        std::mutex mutex;
        
        size_t currentCamera{0};
    };
    
private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 inverseViewMatrix{1.f};
    
    struct {
        float yaw, pitch, roll;
        glm::vec3 position;
    } m_Fpv;
    
    float m_AspectRatio{1.f};
    float m_FovY{0.f};
};

#endif /* Camera_hpp */
