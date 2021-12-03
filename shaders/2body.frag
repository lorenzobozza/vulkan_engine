
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec3 color;
} push;

void main() {
    outColor = vec4(fragColor, 1.0);
} 
