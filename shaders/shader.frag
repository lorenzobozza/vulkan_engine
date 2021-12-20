
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 tangentFragPos;
layout(location = 2) in vec3 tangentViewPos;
layout(location = 3) in vec3 tangentLightPos;
layout(location = 4) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(binding = 1) uniform sampler2D texNormal;
layout(binding = 2) uniform sampler2D texSampler[];

layout(push_constant) uniform PushInfo {
    layout(offset = 128)
    uint textureId;
} obj;

void main() {

    // Texture
    vec3 textureRGB = texture(texSampler[obj.textureId], fragUv).rgb;

    // Normal Map
    vec3 normalMap = normalize( texture(texNormal, fragUv).rgb * 2.0 - 1.0 );
    
    // Ambient lighting
    vec3 ambientLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;

    // Light
    vec3 viewDirection = normalize(tangentViewPos - tangentFragPos);
    vec3 lightDirection = normalize(tangentLightPos - tangentFragPos);
    float attenuation = ubo.lightColor.w / dot(lightDirection, lightDirection);
    float cosAngleIncidence = clamp( dot(normalMap, lightDirection) , 0 , 1 );
    
        // Diffuse lighting
        vec3 diffuseLight = ubo.lightColor.xyz * attenuation * cosAngleIncidence;
        
        // Specular lighting
        vec3 halfAngle = normalize(lightDirection + viewDirection);
        float blinnTerm = dot(normalMap, halfAngle);
        blinnTerm = clamp(blinnTerm, 0, 1);
        blinnTerm = cosAngleIncidence != 0.0 ? blinnTerm : 0.0;
        blinnTerm = pow(blinnTerm, 32.0);
        vec3 specularLighting = ubo.lightColor.xyz * attenuation * blinnTerm;

    outColor = vec4( textureRGB * (diffuseLight + ambientLight + specularLighting) * fragColor, 1.0);
} 
