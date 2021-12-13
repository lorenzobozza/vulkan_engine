
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2DArray tex;

layout(push_constant) uniform PushInfo {
    layout(offset = 128)
    uint texIndex;
} obj;

void main() {

    float bitAlpha = texture(tex, vec3(fragUv, obj.texIndex)).r;

    outColor = vec4(1.0, 1.0, 1.0, bitAlpha) * vec4(fragColor, 1.0);
    
}

