
#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D frame;

layout(push_constant) uniform Push {
    float peak_brightness;
    float gamma;
} push;

float luminance(vec3 v);

vec3 change_luminance(vec3 c_in, float l_out);

vec3 reinhard_extended_luminance(vec3 v, float max_white_l);

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
    if (push.peak_brightness > 800.0) {
        color = reinhard_extended_luminance(color, push.peak_brightness);
    } else {
        color = aces_approx(color);
    }
    color = pow(color, vec3(1.0) / push.gamma);
    outColor = vec4(color, 1.0);
}

float luminance(vec3 v)
{
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}
vec3 change_luminance(vec3 c_in, float l_out)
{
    float l_in = luminance(c_in);
    return c_in * (l_out / l_in);
}
vec3 reinhard_extended_luminance(vec3 v, float max_white_l)
{
    float l_old = luminance(v);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / (1.0f + l_old);
    return change_luminance(v, l_new);
}
