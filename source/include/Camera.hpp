//
//  Camera.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/11/21.
//

#ifndef Camera_hpp
#define Camera_hpp

//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class Camera {
public:
    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    
    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewTarget(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{.0f, -1.f, .0f});
    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);
    
    const glm::mat4 &getProjection() const { return projectionMatrix; }
    const glm::mat4 &getView() const { return viewMatrix; }
    const glm::mat4 &getInverseView() const { return inverseViewMatrix; }
    
    void setView(glm::mat4 view) { viewMatrix = view; }

private:
    glm::mat4 projectionMatrix{1.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 inverseViewMatrix{1.f};
    
    void setPerspectiveProjection(float aspect, float fovy, float near, float far);

    class SetProjection {
        public:
        SetProjection(Camera *camera) : camera{camera} {};
        
        SetProjection& operator[](int offset) { paramOffset = offset; return *this; }
        
        template <typename ...floats>
        void perspective(floats ...args) {
            (setPerspectiveParameters(args, paramOffset++), ...);
            paramOffset = 0;
            camera->setPerspectiveProjection(aspect, fovy, near, far);
        }
        
        private:
        void setPerspectiveParameters(float param, int index);
        
        Camera *camera;
        int paramOffset{0};
        float aspect{1.f};
        float fovy{1.f};
        float near{.01f};
        float far{100.f};
    };
    
public:
    SetProjection setProjection{this};
};

#endif /* Camera_hpp */
