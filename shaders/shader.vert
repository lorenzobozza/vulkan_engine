
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 tangent;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec2 uv1;

layout(location = 0) out VertexShader {
    vec3 color;
    vec3 worldPos;
    vec4 lightSpacePos;
    vec2 texcoord0;
    vec2 texcoord1;
    vec3 N;
    vec3 T;
    float sign;
} frag;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
    mat4 lightSpaceMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

const mat4 zoMatrix = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    
    vec3 T = normalize( vec3(push.modelMatrix * vec4(tangent.xyz, 0.0)) );
    vec3 N = normalize( vec3(push.modelMatrix * vec4(normal, 0.0)) );

    frag.color = color;
    frag.worldPos = positionWorld.xyz;
    frag.lightSpacePos = ubo.lightSpaceMatrix * positionWorld;
    frag.lightSpacePos.xy = (zoMatrix * ubo.lightSpaceMatrix * (positionWorld + vec4(N * 0.1, 0.0))).xy;
    frag.texcoord0 = uv;
    frag.texcoord1 = uv1;
    frag.N = N;
    frag.T = T;
    frag.sign = tangent.w;

    gl_Position = ubo.projectionViewMatrix * ubo.viewMatrix * positionWorld;
}
