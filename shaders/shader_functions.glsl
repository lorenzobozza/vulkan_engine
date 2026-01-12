#extension GL_GOOGLE_include_directive : require
#include "filters/NDF_filters.glsl"

#define M_PI 3.1415926535897932384626433832795

layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLUT;
layout(set = 0, binding = 4) uniform sampler2D shadowMap;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture(shadowMap, shadowCoord.st + off).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
		{
			shadow = 0.01;
		}
	}
	return shadow;
}

float shadowCast(vec4 sc) { return textureProj(sc, vec2(0)); }

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

struct PBRInfo
{
	float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector
	float VdotH;                  // cos angle between view direction and half vector
	float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
	float metalness;              // metallic value at the surface
	vec3 reflectance0;            // full reflectance color (normal incidence angle)
	vec3 reflectance90;           // reflectance color at grazing angle
	float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 diffuseColor;            // color contribution from diffuse lighting
	vec3 specularColor;           // color contribution from specular lighting
};

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#define MANUAL_SRGB 1
//#define SRGB_FAST_APPROXIMATION 1
 
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

//#define tonemap

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 getIBLContribution(vec3 n, vec3 v, vec3 reflection, float roughness, vec3 diffuse_color, vec3 specular_color)
{
    const int prefilteredCubeMipLevels = 8;
	float lod = (roughness * prefilteredCubeMipLevels);

	float NdotV = clamp(dot(n, v), 0.001, 1.0);
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
	vec3 diffuseLight = (texture(irradianceMap, n)).rgb;

	vec3 specularLight = (textureLod(prefilteredMap, reflection, lod)).rgb;

	vec3 diffuse = diffuseLight * diffuse_color;
	vec3 specular = specularLight * (specular_color * brdf.x + brdf.y);

	// For presentation, this allows us to tune IBL terms
	//diffuse *= attenuation.x;
	//specular *= attenuation.y;

	return diffuse + specular;
}

vec3 computeIBL(vec3 n, vec3 v, vec3 reflection, float roughness, vec3 diffuse_color, vec3 specular_color, bool multi_scatter) {
    float numEnvLevels = float(textureQueryLevels(prefilteredMap) - 1);
    float lodLevel = roughness * numEnvLevels;
    
    float NoV = clamp(dot(n, v), 0.001, 1.0);
    
    // Load env textures
    vec2 f_ab = texture(brdfLUT, vec2(NoV, roughness)).xy;
    vec3 radiance = textureLod(prefilteredMap, reflection, lodLevel).xyz;
    vec3 irradiance = texture(irradianceMap, n).xyz;
    
    vec3 Fr = max(vec3(1.0 - roughness), specular_color) - specular_color;
    vec3 k_S = specular_color + Fr * pow(1.0 - NoV, 5.0);
    
    vec3 FssEss = k_S * f_ab.x + f_ab.y;
    
    if (!multi_scatter) {
        return FssEss * radiance + diffuse_color * irradiance;
    }

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specular_color + (1.0 - specular_color) / 21.0;
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuse_color * (1.0 - FssEss - FmsEms);
    return FssEss * radiance + (FmsEms + k_D) * irradiance;
}


// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(PBRInfo pbrInputs)
{
	return pbrInputs.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
	return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
	float NdotL = pbrInputs.NdotL;
	float NdotV = pbrInputs.NdotV;
	float r = pbrInputs.alphaRoughness;

	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
	float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
	float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
	return roughnessSq / (M_PI * f * f);
}

/*
New Microfacet NDF Approach
				|
				|
				|
				|
				V
*/

vec2 computeAnisoRoughness(float alpha)
{
    float anisotropy = clamp(0.0, -1.0, 1.0);

    float alphaX = clamp(alpha * (1.0 + anisotropy), 0.001, 1.0);
    float alphaY = clamp(alpha * (1.0 - anisotropy), 0.001, 1.0);

    return vec2(alphaX, alphaY);
}

float DistributionGGX_Covariance(vec3 N, vec3 H, vec3 h_ts, vec2 alpha_roughness)
{
    // Compute normal-mapped World-Space Tangent and Bitangent
    vec3 T = normalize(vec3(N.y, -N.x, 0.0));
    vec3 B = normalize(cross(N, T));

    float NdotH = max(dot(N, H), 0.0);
    if (NdotH <= 0.0) return 0.0;

    // Project H into slope space: s = (Hx/Hn, Hy/Hn)
    float Hx = dot(H, T);
    float Hy = dot(H, B);
    vec2  s  = vec2(Hx, Hy) / NdotH;


    // Precompute inverse and determinant (cov2 must be positive definite)
    mat2 cov2 = mat2(alpha_roughness.x, 0.0, 0.0, alpha_roughness.y);
	//mat2 cov2 = AxisAlignedNDFFiltering(h_ts, alpha_roughness);
	//mat2 cov2 = NonAxisAlignedNDFFiltering(h_ts, alpha_roughness);
    //mat2 cov2 = FullNonAxisAlignedNDFFiltering(h_ts , alpha_roughness);
    float detC = cov2[0][0] * cov2[1][1] - cov2[0][1] * cov2[1][0];
    if (detC <= 0.0) return 0.0;

    mat2 invC = mat2( cov2[1][1], -cov2[0][1],
                     -cov2[1][0],  cov2[0][0]) / detC;

    // Quadratic form s^T invC s
    float q = dot(s, invC * s);

    // Anisotropic GGX in slope space
    // D = 1 / (pi * det(C) * (1 + q)^2)
    const float PI = 3.14159265358979323846;
    float D = 1.0 / (PI * detC * (1.0 + q) * (1.0 + q));

    return clamp(D, 0.0, 1.0);
}
