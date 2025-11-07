
#version 450


layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec4 lightPosition[2];
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureBitmap;
    float metalness;
    float roughness;
    vec4 color;
    int alphaMode;
    float alphaCutoff;
    int backFace;
} push;


void main() {
}
