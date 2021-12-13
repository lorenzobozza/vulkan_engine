
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec3 tangent;
layout(location = 5) in vec3 bitangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec2 fragUv;
layout(location = 3) out mat3 fragTBN;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat3 normalMatrix;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    
    gl_Position = ubo.projectionViewMatrix * ubo.viewMatrix * positionWorld;
    
    fragPosWorld = positionWorld.xyz;
    fragColor = color;
    fragUv = uv;
    fragTBN = mat3(
        normalize(mat3(push.normalMatrix) * tangent),
        normalize(mat3(push.normalMatrix) * bitangent),
        normalize(mat3(push.normalMatrix) * normal)
    );
    
}
