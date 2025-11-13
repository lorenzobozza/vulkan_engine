
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec2 uv1;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
    mat4 lightSpaceMatrix;
    vec4 lightVector[8];
    vec4 lightChroma[8];
    uint lightInfo;
    uint debugMode;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);

    gl_Position = ubo.lightSpaceMatrix * positionWorld;
}
