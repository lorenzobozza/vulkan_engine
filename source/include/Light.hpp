//
//  Light.hpp
//
//
//  Created by Lorenzo Bozza on 09/11/25.
//

#ifndef Light_h
#define Light_h

#include <glm/glm.hpp>

class Light {
public:
    Light() = delete;
    ~Light() = default;
    
    enum Type {
        Point = 0,
        Directional,
        Spot // currently not supported
    };
    static inline const std::string gltfTypes[] = {"point", "directional", "spot"};
    
    static Light makePoint(glm::vec3 position, glm::vec4 color) { return Light{Type::Point, position, color}; }
    static Light makeDirectional(glm::vec3 direction, glm::vec4 color) { return Light{Type::Directional, direction, color}; }
    
    Type m_Type;
    
    struct {
        union {
            glm::vec3 dir;
            glm::vec3 pos;
        };
        glm::vec4 color{1.f};
        glm::mat4 lightSpaceMatrix{1.f};
    } m_Data;
    
private:
    Light(Type type, glm::vec3 dir_pos, glm::vec4 color) : m_Type((Type)type), m_Data{dir_pos, color} {};
};

#endif /* Light_h */
