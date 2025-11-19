
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec2 uv1;

layout(location = 0) out VertexShader {
    vec3 color;
    vec3 worldPos;
    vec4 lightSpacePos;
    vec2 texcoord0;
    vec2 texcoord1;
    mat3 TBN;
} frag;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;


void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);

    frag.color = color;

    gl_Position = ubo.projectionViewMatrix * ubo.viewMatrix * positionWorld;
}
