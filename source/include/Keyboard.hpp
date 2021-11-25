//
//  Keyboard.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/11/21.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

#include "Window.hpp"
#include "SolidObject.hpp"

 class Keyboard {
 public:
    struct KeyMappings {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;
    };
    
    void moveInPlaneXZ(GLFWwindow * window, float dt, SolidObject &solidObject);
    
    KeyMappings keys{};
    float moveSpeed{3.f};
    float lookSpeed{1.5f};
 };

#endif /* Keyboard_hpp */
