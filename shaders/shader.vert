
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec2 uv;

layout(location = 0) out VertexShader {
    vec3 color;
    vec3 worldPos;
    vec3 tangentPos;
    vec3 tangentViewPos;
    vec2 texcoord;
    mat3 TBN;
} frag;

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
    int textureIndex;
    float metalness;
    float roughness;
} push;

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    
    vec3 T = normalize( vec3(push.modelMatrix * vec4(tangent, 0.0)) );
    vec3 N = normalize( vec3(push.modelMatrix * vec4(normal, 0.0)) );
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = transpose( mat3(T, B, N) );

    frag.color = color;
    frag.worldPos = positionWorld.xyz;
    frag.tangentPos = TBN * positionWorld.xyz;
    frag.tangentViewPos = TBN * ubo.invViewMatrix[3].xyz;
    frag.texcoord = uv;
    frag.texcoord.t = 1.0 - uv.t;
    frag.TBN = TBN;

    gl_Position = ubo.projectionViewMatrix * ubo.viewMatrix * positionWorld;
}
