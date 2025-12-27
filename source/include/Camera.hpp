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
#include <glm/gtc/quaternion.hpp>


class Camera {
public:
    void setPerspectiveProjection(float aspect, float fovy, float near, float far);
    const float getAspectRatio(void) const { return m_AspectRatio; }
    void changeAspectRatio(float aspect);

    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    
    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewTarget(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setView(glm::mat4 view) { viewMatrix = view; }
    void setInverseView(glm::mat4 invView) { inverseViewMatrix = invView; }

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }
    const glm::mat4& getInverseView() const { return inverseViewMatrix; }
    const float getYaw(void) const { return yaw; }
    const float getFov(void) const { return m_FovY; }
    
    void setViewYXZDelta(glm::vec3 deltaP, glm::vec3 deltaR);
    
    size_t ghostObject;
    bool noClip = false;
    
private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 inverseViewMatrix{1.f};
    
    float yaw, pitch, roll;
    glm::vec3 position;
    
    float m_AspectRatio{1.f};
    float m_FovY{0.f};
};

#endif /* Camera_hpp */
