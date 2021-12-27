
#version 450

#extension GL_EXT_nonuniform_qualifier : require

#define PI 3.14159265

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec3 viewPos;
layout(location = 3) in vec3 lightPos;
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

layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D metalnessMap;
layout(binding = 3) uniform sampler2D roughnessMap;
layout(binding = 4) uniform sampler2D colorMap[];

layout(push_constant) uniform PushInfo {
    layout(offset = 128)
    uint textureId;
    bool blinn;
} obj;

vec3 fresnelSchlick(float cosTheta, vec3 F0);
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

void main() {
    // G-Buffer
    vec3 color = texture(colorMap[obj.textureId], fragUv * 1.0).rgb;
    vec3 normal = normalize(texture(normalMap, fragUv * 1.0).rgb * 2.0 - 1.0);
    float metalness = texture(metalnessMap, fragUv * 1.0).r;
    float roughness = texture(roughnessMap, fragUv * 1.0).r;

    // Lighting directions
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 halfDirAngle = normalize(lightDir + viewDir);
    vec3 reflectDir = reflect(-lightDir, normal);
    
    float lightDist = length(lightPos - fragPos);
    float attenuation = ubo.lightColor.w / (lightDist * lightDist);
    vec3 radiance = ubo.lightColor.xyz * attenuation;
    
    //Specular Workflow
        // Ambient lighting
        vec3 ambientLight = color * ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    
        // Diffuse lighting
        vec3 diffuseLight = color * radiance * max(dot(lightDir, normal),0.0);
        
        // Specular lighting
        float spec = 0.0;
        const float kShininess = 16.0;
        if(obj.blinn) {
           const float kEnergyConservation = ( 8.0 + kShininess ) / ( 8.0 * PI );
           spec = kEnergyConservation * pow(max(dot(normal, halfDirAngle), 0.0), kShininess);
        } else {
           const float kEnergyConservation = ( 2.0 + kShininess ) / ( 2.0 * PI );
           spec = kEnergyConservation * pow(max(dot(viewDir, reflectDir), 0.0), kShininess);
        }
        vec3 specularLight = radiance * spec;

    //Physically Based Render
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, color, metalness);
    vec3  F   = fresnelSchlick(max(dot(halfDirAngle, viewDir), 0.0), F0);
    float NDF = DistributionGGX(normal, halfDirAngle, roughness);
    float G   = GeometrySmith(normal, viewDir, lightDir, roughness);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0)  + 0.0001;
    vec3 specular     = numerator / denominator;
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 o = (kD * color / PI + specular) * radiance * NdotL;
    
    vec3 ambient = vec3(0.1) * color;
    vec3 pbr = ambient + o;
    pbr = pbr / (pbr + vec3(1.0));
    outColor = vec4(pbr, 1.0);

    //outColor = vec4(ambientLight + diffuseLight + specularLight, 1.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
