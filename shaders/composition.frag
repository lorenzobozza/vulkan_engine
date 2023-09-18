
#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D frame;

layout(push_constant) uniform Push {
    float exposure;
    float peak_brightness;
    float gamma;
} push;

vec3 aces_approx(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

void main() {
    vec3 color = texture(frame, texCoord).rgb;
    if (push.peak_brightness < 10.0) {
        float lum = 0.2126f * color.r + 0.7152 * color.g + 0.0722 * color.b;
        vec3 mappedLum = aces_approx(push.exposure * vec3(lum));
        color *= mappedLum / lum;
    } else {
        color = aces_approx(push.exposure * color);
    }
    color *= 1.0 / aces_approx(vec3(push.peak_brightness));
    color = pow(color, vec3(1.0) / push.gamma);
    outColor = vec4(color, 1.0);
}
