
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;
layout(location = 4) in vec2 uv;

layout(location = 0) out vec3 fragPos;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

void main() {
    fragPos = position;
    
    mat4 rotView = mat4(mat3(ubo.viewMatrix));
    vec4 clipPos = ubo.projectionViewMatrix * rotView * vec4(position, 1.0);

    gl_Position = clipPos.xyww;
}
