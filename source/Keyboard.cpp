//
//  Keyboard.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/11/21.
//

#include "include/Keyboard.hpp"

void Keyboard::moveInPlaneXZ(GLFWwindow * window, float dt, SolidObject &solidObject) {
    glm::vec3 rotate{0};
    if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
    
    if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
    if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;
    
    if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
        solidObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
    }
    solidObject.transform.rotation.x = glm::clamp(solidObject.transform.rotation.x, -1.5f, 1.5f);
    solidObject.transform.rotation.y = glm::mod(solidObject.transform.rotation.y, glm::two_pi<float>());
    
    float yaw = solidObject.transform.rotation.y;
    const glm::vec3 forwardDir{glm::sin(yaw), .0f, glm::cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, .0f, -forwardDir.x};
    const glm::vec3 upDir{.0f, -1.f, .0f};
    
    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
        solidObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
}
