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
    enum Type {
        Point = 0,
        Directional,
        Spot // currently not supported
    };
    static constexpr std::string gltfTypes[] = {"point", "directional", "spot"};
    
    static Light makePoint(glm::vec3 position, glm::vec4 color) { return Light{Type::Point, position, color}; }
    static Light makeDirectional(glm::vec3 direction, glm::vec4 color) { return Light{Type::Directional, direction, color}; }

    Light() = delete;
    ~Light() = default;
        
private:
    Light(Type type, glm::vec3 dir_pos, glm::vec4 color) : m_type((Type)type), m_data{dir_pos, color} {};

public:
    Type m_type;
    
    struct {
        union {
            glm::vec3 dir;
            glm::vec3 pos;
        };
        glm::vec4 color{1.f};
    } m_data;
};

#endif /* Light_h */
