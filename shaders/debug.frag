#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in VertexShader {
    vec3 color;
    vec3 worldPos;
    vec4 lightSpacePos;
    vec2 texcoord;
    vec2 texcoord1;
    mat3 TBN;
} vert;

void main() { 
    outColor = vec4(vert.color, 1.0);
}
