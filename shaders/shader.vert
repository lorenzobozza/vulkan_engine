
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 tangentFragPos;
layout(location = 2) out vec3 tangentViewPos;
layout(location = 3) out vec3 tangentLightPos;
layout(location = 4) out vec2 fragUv;

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
    
    vec3 T = normalize( vec3(push.modelMatrix * vec4(tangent, 0.0)) );
    vec3 N = normalize( vec3(push.modelMatrix * vec4(normal, 0.0)) );
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    mat3 TBN = transpose( mat3(T, B, N) );
    
    fragColor = color;
    tangentFragPos  = TBN * positionWorld.xyz;
    tangentViewPos  = TBN * ubo.invViewMatrix[3].xyz;
    tangentLightPos = TBN * ubo.lightPosition;
    fragUv = uv;

    gl_Position = ubo.projectionViewMatrix * ubo.viewMatrix * positionWorld;
}
