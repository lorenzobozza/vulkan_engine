
#version 450

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D equirectangularMap;

float luminance(vec3 v);

vec3 change_luminance(vec3 c_in, float l_out);

vec3 reinhard_extended_luminance(vec3 v, float max_white_l);

void main() {
    vec3 color = texture(equirectangularMap, texCoord).rgb;
    color = reinhard_extended_luminance(color, 400.0);
    outColor = vec4(color, 1.);
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
