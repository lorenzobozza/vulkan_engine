
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
layout(binding = 2) uniform samplerCube prefilteredMap;
layout(binding = 3) uniform sampler2D brdfLUT;
layout(binding = 4) uniform sampler2D PBRMaterial[];

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureIndex;
    float metalness;
    float roughness;
} push;

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0;
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Fresnel Roughness function ----------------------------------------------------
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    float texScale = 1.0;
    const int texOffset = 5 * push.textureIndex;
    // PBR Material Stack
    vec3 albedo = (push.textureIndex < 0) ? vec3(0.0,0.6,0.0) : texture(PBRMaterial[0 + texOffset], vert.texcoord * texScale).rgb;
    vec3 normal = (push.textureIndex < 0) ? vec3(0.0, 0.0, 1.0) : normalize(texture(PBRMaterial[1 + texOffset], vert.texcoord * texScale).rgb * 2.0 - 1.0);
    float metalness = (push.textureIndex < 0) ? push.metalness : texture(PBRMaterial[2 + texOffset], vert.texcoord * texScale).r;
    float roughness = (push.textureIndex < 0) ? push.roughness : texture(PBRMaterial[3 + texOffset], vert.texcoord * texScale).r;
    float occlusion = (push.textureIndex < 0) ? 1.0 : texture(PBRMaterial[4 + texOffset], vert.texcoord * texScale).r;
    
    mat3 invTBN = transpose(vert.TBN);
    vec3 N = invTBN * normal;
    vec3 tangentLightPos = vert.TBN * ubo.lightPosition;

    vec3 viewPos = ubo.invViewMatrix[3].xyz;
    vec3 viewDir = normalize(viewPos - vert.worldPos);
    vec3 tangentViewDir = normalize(vert.tangentViewPos - vert.tangentPos);
    vec3 tangentLightDir = normalize(tangentLightPos - vert.tangentPos);
    vec3 R = reflect(-viewDir, N);

    // Lighting directions in tangent space
    vec3 halfDir = normalize(tangentLightDir + tangentViewDir);
    float lightDist = length(tangentLightPos - vert.tangentPos);
    float attenuation = ubo.lightColor.w / (lightDist * lightDist);
    vec3 radiance = ubo.lightColor.xyz * attenuation;
    

//Physically Based Rendering (Metalness-Roughness Workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    
    // Specular Light contribution
    vec3 Lo = vec3(0.0);
	float dotNH = clamp(dot(normal, halfDir), 0.0, 1.0);
	float dotNV = clamp(dot(normal, tangentViewDir), 0.0, 1.0);
	float dotNL = clamp(dot(normal, tangentLightDir), 0.0, 1.0);
	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);
		Lo = (kD * albedo / PI + spec) * dotNL * radiance;
	}
    
// IBL Part (Non-Tangent Space)
    vec3 reflection = prefilteredReflection(R, roughness);
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(normal, tangentViewDir), 0.0), roughness)).rg;
    
    vec3 F = F_SchlickR(max(dot(N, viewDir), 0.0), F0, roughness);
    
    // Diffuse irradiance
    vec3 diffuse = irradiance * albedo;
    
    // Specular reflectance
    vec3 specular = reflection * (F * brdf.x + brdf.y);
    
    // Ambient
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metalness;
    vec3 ambient = kD * diffuse + specular;
    
// Ambient + Light
    vec3 pbr = ambient + Lo;
    
// Apply occlusion map
    pbr = mix(pbr, pbr * occlusion, 1.0);
    
    outColor = vec4(pbr, 1.0);
}
