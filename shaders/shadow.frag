
#version 450


layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;


void main() {
}
