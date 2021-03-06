
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec2 fragUv;

layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat3 normalMatrix;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    
    gl_Position = positionWorld;
    
    fragPosWorld = positionWorld.xyz;
    fragColor = color;
    fragUv = uv;
}
