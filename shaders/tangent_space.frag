
#version 450

//#extension GL_EXT_nonuniform_qualifier : require

#define PI 3.1415926535897932384626433832795

#define COLOR_TEXTURE       0x1
#define NORMAL_TEXTURE      0x2
#define OCCLUSION_TEXTURE   0x4
#define ROUGH_METAL_TEXTURE 0x8

#define COLOR_UV            0x10
#define NORMAL_UV           0x20
#define OCCLUSION_UV        0x40
#define ROUGH_METAL_UV      0x80

#define ALPHAMODE_OPAQUE 0
#define ALPHAMODE_MASK 1
#define ALPHAMODE_BLEND 2

layout(location = 0) in VertexShader {
    vec3 color;
    vec3 worldPos;
    vec3 tangentPos;
    vec3 tangentViewPos;
    vec2 texcoord;
    vec2 texcoord1;
    mat3 TBN;
} vert;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec4 lightPosition[2];
    vec4 lightColor;
    mat4 viewMatrix;
    mat4 invViewMatrix;
} ubo;

layout(binding = 1) uniform samplerCube irradianceMap;
layout(binding = 2) uniform samplerCube prefilteredMap;
layout(binding = 3) uniform sampler2D brdfLUT;
layout(binding = 4) uniform sampler2D diffuseMap;
layout(binding = 5) uniform sampler2D normalMap;
layout(binding = 6) uniform sampler2D metallicMap;
layout(binding = 7) uniform sampler2D roughnessMap;
layout(binding = 8) uniform sampler2D occlusionMap;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureBitmap;
    float metalness;
    float roughness;
    vec4 color;
    int alphaMode;
    float alphaCutoff;
    int backFace;
} push;

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#define MANUAL_SRGB 1
#define SRGB_FAST_APPROXIMATION 1
 
	#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
	#else //MANUAL_SRGB
	return srgbIn;
	#endif //MANUAL_SRGB
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 8.0;
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
	//return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return mix(F0, vec3(1.0), pow(1.01 - cosTheta, 5.0));
}

// Fresnel Roughness function ----------------------------------------------------
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    float viewDist = length(ubo.invViewMatrix[3].xyz - vert.worldPos);
    float lod = (pow(viewDist / 4.0, 2) / 6.0) + 0.5;
    if (lod > 5.0) lod = 5.0; // min texture size 32x32
    
    
    
    
    // PBR Material Stack
    vec3 albedo;
    float alpha;
    if ( (push.textureBitmap & COLOR_TEXTURE) == COLOR_TEXTURE ) {
        vec4 color4 = SRGBtoLINEAR(
            textureLod(diffuseMap, (push.textureBitmap & COLOR_UV) == 0 ? vert.texcoord : vert.texcoord1, lod)
        );
        albedo = color4.rgb;
        alpha = color4.w;
    }
    
    if ((push.alphaMode == ALPHAMODE_MASK) && (alpha < push.alphaCutoff)) { discard; }
    if (push.alphaMode == ALPHAMODE_OPAQUE) { alpha = 1.0; }
    
    vec3 normal = (push.textureBitmap & NORMAL_TEXTURE) == 0 ? vec3(0.0, 0.0, 1.0) : textureLod(normalMap, (push.textureBitmap & NORMAL_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).rgb * 2.0 - 1.0;
    float metalness = (push.textureBitmap & ROUGH_METAL_TEXTURE) == 0 ? push.metalness : textureLod(metallicMap, (push.textureBitmap & ROUGH_METAL_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).b;
    float roughness = (push.textureBitmap & ROUGH_METAL_TEXTURE) == 0 ? push.roughness : textureLod(metallicMap, (push.textureBitmap & ROUGH_METAL_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).g;
    float occlusion = (push.textureBitmap & OCCLUSION_TEXTURE) == 0 ? 1.0 : textureLod(occlusionMap, (push.textureBitmap & OCCLUSION_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).r;
    
    if (push.backFace > 0) {
        normal.z *= -1.0;
    }
    
    
    vec3 tangentViewDir = normalize(vert.tangentViewPos - vert.tangentPos);
    float dotNV = max(0.001, dot(normal, tangentViewDir));

//Physically Based Rendering (Metalness-Roughness Workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    
    // Specular Light contribution
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < 1; i++) {
        // Lighting coordinates in tangent space
        vec3 tangentLightPos = vert.TBN * ubo.lightPosition[i].xyz;
        vec3 tangentLightDir = normalize(tangentLightPos - vert.tangentPos);
        vec3 halfDir = normalize(tangentLightDir + tangentViewDir);
        float lightDist = length(tangentLightPos - vert.tangentPos);
        float attenuation = ubo.lightColor.w / (lightDist * lightDist);
        vec3 radiance = ubo.lightColor.xyz * attenuation;
        
        float dotNH = max(0.001, dot(normal, halfDir));
        float dotNL = max(0.001, dot(normal, tangentLightDir));
        float dotHV = max(0.001, dot(halfDir, tangentViewDir));
        if (dotNL > 0.0) {
            // D = Normal distribution (Distribution of the microfacets)
            float D = D_GGX(dotNH, roughness);
            // G = Geometric shadowing term (Microfacets shadowing)
            float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
            // F = Fresnel factor (Reflectance depending on angle of incidence)
            vec3 F = F_Schlick(dotHV, F0);
            vec3 spec = D * F * G / max(4.0 * dotNL * dotNV, 0.001);
            vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);
            Lo += (kD * albedo / PI + spec) * dotNL * radiance;
        }
    }
    
// IBL Part (Non-Tangent Space)
    mat3 notinvTBN = transpose(vert.TBN); // Sub-optimal
    vec3 worldN = notinvTBN * normal;
    vec3 viewDir = normalize(ubo.invViewMatrix[3].xyz - vert.worldPos);
    vec3 R = reflect(-viewDir, worldN);
    vec3 reflection = prefilteredReflection(R, roughness);
    vec3 irradiance = texture(irradianceMap, worldN).rgb;
    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(worldN, viewDir), 0.0), roughness)).rg;
    
    vec3 F = F_SchlickR(max(dot(worldN, viewDir), 0.0), F0, roughness);
    
    // Diffuse irradiance
    vec3 diffuse = irradiance * albedo;
    
    // Specular reflectance
    vec3 specular = reflection * (F * brdf.x + brdf.y);
    
    // Ambient
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metalness;
    vec3 ambient = kD * diffuse + specular;
    ambient = mix(ambient, ambient * occlusion, 0.7);
    
// Ambient + Light
    vec3 pbr = 0.5 * ambient + Lo;
    
    outColor = vec4(pbr, alpha);
}
