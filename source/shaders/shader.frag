
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat3 normalMatrix;
} push;

void main() {

    //Point Light
    vec3 directionToLight = ubo.lightPosition - fragPosWorld;
    float attenuation = ubo.lightColor.w / dot(directionToLight, directionToLight);
    float cosAngleIncidence = clamp( dot(normalize(fragNormalWorld), normalize(directionToLight)), 0 , 1 );
    
    vec3 diffuseLight = ubo.lightColor.xyz * attenuation * cosAngleIncidence;
    
    vec3 ambientLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    
    // specular lighting
    vec3 viewDirection = normalize(ubo.invViewMatrix[3].xyz - fragPosWorld);
    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(fragNormalWorld, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);
    blinnTerm = cosAngleIncidence != 0.0 ? blinnTerm : 0.0;
    blinnTerm = pow(blinnTerm, 32.0);
    vec3 specularLighting = ubo.lightColor.xyz * attenuation * blinnTerm;
    
    outColor = vec4( (diffuseLight + ambientLight + specularLighting) * fragColor, 1.0);
} 
