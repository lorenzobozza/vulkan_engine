
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;
layout(location = 4) in vec2 uv;

layout(location = 0) out vec2 fragUv;

void main() {
    fragUv = uv;
    gl_Position = vec4(position, 1.0);
}
