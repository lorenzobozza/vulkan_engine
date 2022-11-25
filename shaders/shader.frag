
#version 450

#extension GL_EXT_nonuniform_qualifier : require

#define PI 3.1415926535897932384626433832795

layout(location = 0) in VertexShader {
    vec3 color;
    vec3 worldPos;
    vec3 tangentPos;
    vec3 tangentViewPos;
    vec2 texcoord;
    mat3 TBN;
} vert;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(binding = 1) uniform samplerCube irradianceMap;
layout(binding = 2) uniform samplerCube prefilterMap;
layout(binding = 3) uniform sampler2D brdfLUT;
layout(binding = 4) uniform sampler2D PBRMaterial[];

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureIndex;
    float metalness;
    float roughness;
} push;

vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

void main() {
    float texScale = 1.0;
    const int texOffset = 4 * push.textureIndex;
    // PBR Material Stack
    vec3 albedo = (push.textureIndex < 0) ? vert.color : texture(PBRMaterial[0 + texOffset], vert.texcoord * texScale).rgb;
    vec3 normal = (push.textureIndex < 0) ? vec3(0.0, 0.0, 1.0) : normalize(texture(PBRMaterial[1 + texOffset], vert.texcoord * texScale).rgb * 2.0 - 1.0);
    float metalness = (push.textureIndex < 0) ? push.metalness : texture(PBRMaterial[2 + texOffset], vert.texcoord * texScale).r;
    float roughness = (push.textureIndex < 0) ? push.roughness : 5.0 * texture(PBRMaterial[3 + texOffset], vert.texcoord * texScale).r;
    
    mat3 invTBN = inverse(vert.TBN);
    vec3 N = invTBN * normal;
    vec3 tangentLightPos = vert.TBN * ubo.lightPosition;

    vec3 viewPos = ubo.invViewMatrix[3].xyz;
    vec3 viewDir = normalize(viewPos - vert.worldPos);
    vec3 tangentViewDir = normalize(vert.tangentViewPos - vert.tangentPos);
    vec3 tangentLightDir = normalize(tangentLightPos - vert.tangentPos);
    vec3 R = reflect(-viewDir, N);

    // Lighting directions in tangent space
    vec3 lightDir = normalize(tangentLightPos - vert.tangentPos);
    vec3 halfDir = normalize(lightDir + tangentViewDir);
    float lightDist = length(tangentLightPos - vert.tangentPos);
    float attenuation = ubo.lightColor.w / (lightDist * lightDist);
    vec3 radiance = ubo.lightColor.xyz * attenuation;

    //Physically Based Rendering (Metalness-Roughness Workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    vec3  F   = fresnelSchlick(max(dot(halfDir, tangentViewDir), 0.0), F0);
    float NDF = DistributionGGX(normal, halfDir, roughness);
    float G   = GeometrySmith(normal, tangentViewDir, tangentLightDir, roughness);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, tangentViewDir), 0.0) * max(dot(normal, tangentLightDir), 0.0)  + 0.0001;
    vec3 specular     = numerator / denominator;
    
    float NdotL = max(dot(normal, tangentLightDir), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(normal, tangentViewDir), 0.0), roughness)).rg;
    brdf = vec2(1.0,0.0);
    
    // NON TANGENT SPACE
    kS = fresnelSchlickRoughness(max(dot(N, viewDir), 0.0), F0, roughness);
    kD = 1.0 - kS;
    kD *= 1.0 - metalness;
    
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse      = irradiance * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
    specular = prefilteredColor * (kS * brdf.x + brdf.y);
    vec3 ambient = kD * diffuse + specular;
    
    vec3 pbr = ambient + Lo;
    
    outColor = vec4(pbr, 1.0);
}



vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

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

//Specular Workflow
/*  // Ambient lighting
    vec3 ambientLight = color * ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;

    // Diffuse lighting
    vec3 diffuseLight = color * radiance * max(dot(lightDir, normal),0.0);
    
    // Specular lighting
    float spec = 0.0;
    const float kShininess = 16.0;
    if(obj.blinn) {
       const float kEnergyConservation = ( 8.0 + kShininess ) / ( 8.0 * PI );
       spec = kEnergyConservation * pow(max(dot(normal, halfDir), 0.0), kShininess);
    } else {
       const float kEnergyConservation = ( 2.0 + kShininess ) / ( 2.0 * PI );
       spec = kEnergyConservation * pow(max(dot(viewDir, reflectDir), 0.0), kShininess);
    }
    vec3 specularLight = radiance * spec;
    
    //outColor = vec4(ambientLight + diffuseLight + specularLight, 1.0);*/
