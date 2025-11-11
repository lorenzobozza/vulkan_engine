
#version 450

layout(location = 0) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
    vec4 lightVector[8];
    vec4 lightChroma[8];
    uint lightInfo;
    uint debugMode;
} ubo;

layout(binding = 1) uniform samplerCube environmentMap;

void main() {
    vec3 envColor;
    
    if ((ubo.debugMode & 0x100) == 0x100) {
        envColor = texture(environmentMap, fragPos).rgb;
    } else {
        envColor = vec3(0.01);
    }
    
    outColor = vec4(envColor, 1.0);
}
