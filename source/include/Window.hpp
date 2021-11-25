//
//  Window.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#ifndef Window_hpp
#define Window_hpp

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// std
#include <string>

class Window {
public:
    Window(int w, int h, std::string name);
    ~Window();
    
    // Prevent Obj copy
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    
    bool shouldClose() { return glfwWindowShouldClose(window); }
    
    VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    bool wasWindowResized() { return frameBufferResized; }
    void resetWindowResizeFlag() { frameBufferResized = false; }
    GLFWwindow *getGLFWwindow() const { return window; }
    
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
    
private:
    static void frameBufferResizeCallback(GLFWwindow *window, int width, int height);
    void initWindow();
    
    int width;
    int height;
    bool frameBufferResized = false;
    
    std::string windowName;
    GLFWwindow* window;
};

#endif /* Window_hpp */
