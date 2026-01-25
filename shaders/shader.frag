#version 450
#extension GL_GOOGLE_include_directive : require
#include "shader_functions.glsl"

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D colorTexture;

void main() {
    vec3 baseColor = vec3(1.0);

    if (MASK_COMPARE(push.textureBitmap, COLOR_TEXTURE)) {
        vec4 colorSample = SRGBtoLINEAR(texture(colorTexture, (push.textureBitmap & COLOR_UV) == 0 ? vert.texcoord : vert.texcoord1));
        baseColor = colorSample.rgb;
    }
    baseColor *= push.color.rgb;

    vec3 fragmentColor = BRDF(baseColor);
    
    outColor = vec4(fragmentColor, 1.0);
}
