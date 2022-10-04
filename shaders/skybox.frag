
#version 450

layout(location = 0) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(binding = 1) uniform samplerCube environmentMap;

void main() {
    vec3 envColor = texture(environmentMap, fragPos).rgb;
    
    envColor = envColor / (envColor + vec3(1.0));
    
    outColor = vec4(envColor, 1.0);
}
