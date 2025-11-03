
#version 450

#include "shader.glsl"

//#extension GL_EXT_nonuniform_qualifier : require

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

layout(binding = 4) uniform sampler2D diffuseMap;
layout(binding = 5) uniform sampler2D normalMap;
layout(binding = 6) uniform sampler2D metalRoughnessMap;
layout(binding = 7) uniform sampler2D occlusionMap;

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


void main() {
    float viewDist = length(ubo.invViewMatrix[3].xyz - vert.worldPos);
    float lod = (pow(viewDist / 4.0, 2) / 6.0) + 0.5;
    if (lod > 5.0) lod = 5.0; // min texture size 32x32
    
    
    // PBR Material Stack
    vec3 baseColor;
    float alpha;
    float metallic;
    float perceptualRoughness;
    
    if ( (push.textureBitmap & COLOR_TEXTURE) == COLOR_TEXTURE ) {
        vec4 color4 = SRGBtoLINEAR(
            textureLod(diffuseMap, (push.textureBitmap & COLOR_UV) == 0 ? vert.texcoord : vert.texcoord1, lod)
        );
        color4 *= push.color;
        baseColor = color4.rgb;
        alpha = color4.w;
    }
    
    if ((push.alphaMode == ALPHAMODE_MASK) && (alpha < push.alphaCutoff)) { alpha = 0.0; }
    if (push.alphaMode == ALPHAMODE_OPAQUE) { alpha = 1.0; }
    
    vec3 normal = (push.textureBitmap & NORMAL_TEXTURE) == 0 ? vec3(0.0, 0.0, 1.0) : textureLod(normalMap, (push.textureBitmap & NORMAL_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).rgb * 2.0 - 1.0;
    

    if ( (push.textureBitmap & ROUGH_METAL_TEXTURE) == ROUGH_METAL_TEXTURE ) {
        vec4 mrx = textureLod(metalRoughnessMap, (push.textureBitmap & ROUGH_METAL_UV) == 0 ? vert.texcoord : vert.texcoord1, lod);
        metallic = clamp(mrx.b, 0.0, 1.0);
        perceptualRoughness = clamp(mrx.g, 0.04, 1.0);
    } else {
        metallic = clamp(push.metalness, 0.0, 1.0);
        perceptualRoughness = clamp(push.roughness, 0.04, 1.0);
    }
    
    float occlusion = (push.textureBitmap & OCCLUSION_TEXTURE) == 0 ? 1.0 : textureLod(occlusionMap, (push.textureBitmap & OCCLUSION_UV) == 0 ? vert.texcoord : vert.texcoord1, lod).r;
    
    if (push.backFace > 0) {
        normal.z *= -1.0;
    }
    
    vec3 f0 = vec3(0.04);
    
    vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
	diffuseColor *= 1.0 - metallic;
		
	float alphaRoughness = perceptualRoughness * perceptualRoughness;

	vec3 specularColor = mix(f0, baseColor.rgb, metallic);

	// Compute reflectance.
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec3 n = vert.TBN * normal;
	vec3 v = normalize(vert.tangentViewPos - vert.tangentPos);    // Vector from surface point to camera
    vec3 reflection = normalize(reflect(-v, n));
 
PBRInfo pbrInputs;
vec3 color = vec3(0);
for (int i = 0; i < 2; i++) {

	vec3 l = normalize(ubo.lightPosition[0].xyz - vert.tangentPos);     // Vector from surface point to light
	vec3 h = normalize(l+v);                        // Half vector between both l and v

	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);
    
    pbrInputs = PBRInfo(
		NdotL,
		NdotV,
		NdotH,
		LdotH,
		VdotH,
		perceptualRoughness,
		metallic,
		specularEnvironmentR0,
		specularEnvironmentR90,
		alphaRoughness,
		diffuseColor,
		specularColor
	);

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

    float lightDist = length(ubo.lightPosition[i].xyz - vert.tangentPos);
    float attenuation = ubo.lightColor.a / (lightDist * lightDist);
	const vec3 u_LightColor = ubo.lightColor.rgb * attenuation;

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	color += NdotL * u_LightColor * (diffuseContrib + specContrib);

}

	// Calculate lighting contribution from image based lighting source (IBL)
	color += getIBLContribution(pbrInputs, n, reflection);

	const float u_OcclusionStrength = 0.5f;
	// Apply optional PBR terms for additional (optional) shading
	if ((push.textureBitmap & OCCLUSION_TEXTURE) == OCCLUSION_TEXTURE) {
		color = mix(color, color * occlusion, u_OcclusionStrength);
	}
	
    outColor = vec4(color, alpha);
}
