//
//  Keyboard.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/11/21.
//

/*
void Keyboard::moveInPlaneXZ(GLFWwindow * window, float dt, Primitive &Primitive) {
    glm::vec3 rotate{0};
    if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
    
    if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
    if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;
    
    if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
        Primitive.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
    }
    Primitive.transform.rotation.x = glm::clamp(Primitive.transform.rotation.x, -1.5f, 1.5f);
    Primitive.transform.rotation.y = glm::mod(Primitive.transform.rotation.y, glm::two_pi<float>());
    
    float yaw = Primitive.transform.rotation.y;
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
        Primitive.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
}
*/
