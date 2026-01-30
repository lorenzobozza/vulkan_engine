#extension GL_GOOGLE_include_directive : require
#include "filters/NDF_filters.glsl"

#define M_PI (3.1415926535897932384626433832795)

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

#define DEBUG_SHOW_ENV_BIT      0x100
#define DEBUG_IBL_CONTRIB_BIT   0x200
#define DEBUG_MULTISCATTER_BIT  0x400
#define MASK_COMPARE(bitmap, mask)  ((bitmap & mask) == mask)


layout(location = 0) in VertexShader {
    vec3 color;
    vec3 worldPos;
    vec4 lightSpacePos;
    vec2 texcoord;
    vec2 texcoord1;
    vec3 N;
    vec3 T;
    float sign;
} vert;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    mat4 invViewMatrix;
    mat4 lightSpaceMatrix;
    vec4 lightVector[8];
    vec4 lightChroma[8];
    uint lightInfo;
    uint debugMode;
} ubo;
layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLUT;
layout(set = 0, binding = 4) uniform sampler2D shadowMap;

layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metalRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D occlusionMap;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    int textureBitmap;
    float metalness;
    float roughness;
    vec4 color;
    int alphaMode;
    float alphaCutoff;
    float coatWeight;
    float coatRoughness;
} push;


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

vec3 F_Schlick(const vec3 f0, float f90, float cosTheta) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return f0 + (f90 - f0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float F_Schlick(float f0, float f90, float cosTheta) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return f0 + (f90 - f0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

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

	reflection = mix(reflection, n, roughness * roughness * roughness * roughness);

	// mat3 Mr = mat3(cos(3.14), 0, sin(3.14),
    //                0,				  1,0,
    //                -sin(3.14), 0, cos(3.14));
    // if (!multi_scatter) {
	// 	n = Mr * n;
	// 	reflection = Mr * reflection;
	// }
    
    // Load env textures
    vec2 DFG = texture(brdfLUT, vec2(NoV, roughness)).xy;
    vec3 radiance = textureLod(prefilteredMap, reflection, lodLevel).xyz;
    vec3 irradiance = texture(irradianceMap, n).xyz;
    
	vec3 FssEss = mix(DFG.xxx, DFG.yyy, specular_color);

    if (!multi_scatter) {
        return FssEss * radiance + diffuse_color * irradiance;
    }

	FssEss *= radiance;
	diffuse_color *= irradiance;

	float Fc = F_Schlick(0.04, 1.0, NoV) * 0.0;
	diffuse_color  *= (1.0 - Fc);
	FssEss *= (1.0 - Fc);
	FssEss += radiance * Fc;
	return FssEss + diffuse_color;

    // Multiple scattering, from Fdez-Aguera
    // float Ems = (1.0 - (DFG.x + DFG.y));
    // vec3 F_avg = specular_color + (1.0 - specular_color) / 21.0;
    // vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    // vec3 k_D = diffuse_color * (1.0 - FssEss - FmsEms);
    // return FssEss * radiance + (FmsEms + k_D) * irradiance;
}

vec2 computeAnisoRoughness(float alpha) {
    float anisotropy = clamp(0.0, -1.0, 1.0);

    float alphaX = clamp(alpha * (1.0 + anisotropy), 0.001, 1.0);
    float alphaY = clamp(alpha * (1.0 - anisotropy), 0.001, 1.0);

    return vec2(alphaX, alphaY);
}

mat2 standardCovarianceMatrix(vec2 alpha, float phi) {
    float c = cos(phi), s = sin(phi);
    mat2 R = mat2(c, -s, s, c);
    mat2 S2 = mat2(alpha.x * alpha.x, 0.0, 0.0, alpha.y * alpha.y);
    return R * S2 * transpose(R);
}

float D_NDF_GGX_Covariance(mat2 C, float ToH, float BoH, float NoH) {
    // Project H into slope space: s = (ToH/NoH, BoH/NoH)
    vec2  s  = vec2(ToH, BoH) / NoH;

    float detC = C[0][0] * C[1][1] - C[0][1] * C[1][0];
    if (detC <= 0.0) return 0.0;

    mat2 invC = mat2( C[1][1], -C[0][1],
                     -C[1][0],  C[0][0]) / detC;

    // Quadratic form s^T invC s
    float q = dot(s, invC * s);

    // Anisotropic GGX in slope space
    // D = 1 / (pi * det(C) * (1 + q)^2)
    const float PI = 3.14159265358979323846;
    //float D = 1.0 / (PI * detC * (1.0 + q) * (1.0 + q));
	float D = 1.0 / (PI * sqrt(detC) * pow(1.0 + q, 2.0) * pow(NoH, 4));

    return clamp(D, 0.0, 1.0);
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (M_PI * f * f);
}

float G_SmithGGX(float NoV, float NoL, float alpha) {
	float k = (alpha * alpha) * 0.5;
	float attenuationL = NoL / (NoL * (1.0 - k) + k);
	float attenuationV = NoV / (NoV * (1.0 - k) + k);
	return attenuationL * attenuationV;
}


float D_GGX_Covariance(vec3 h, mat2 Sigma) {
    float hz = h.z;
    if (hz <= 0.0) return 0.0;

    vec2 m = h.xy / hz;
    float detS = determinant(Sigma);
    mat2 invS = inverse(Sigma);
    float q = dot(m, invS * m);  // m^T Σ^{-1} m
    return 1.0 / (M_PI * sqrt(detS) * pow(1.0 + q, 2.0) * pow(hz, 4.0));
}

float Lambda_dir(vec3 w, mat2 Sigma)
{
    if (w.z <= 0.0) return 1e9;  // fully masked

    vec2 t = w.xy;
    float q = dot(t, Sigma * t) / (w.z * w.z);
    float s = sqrt(1.0 + q);
    return 0.5 * (s - 1.0);
}

float G_Smith_Covariance(vec3 l, vec3 v, mat2 Sigma)
{
    float lambdaL = Lambda_dir(l, Sigma);
    float lambdaV = Lambda_dir(v, Sigma);
    return 1.0 / (1.0 + lambdaL + lambdaV);
}


float Fd_Burley(float roughness, float NoV, float NoL, float LoH) {
    // Burley 2012, "Physically-Based Shading at Disney"
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(1.0, f90, NoL);
    float viewScatter  = F_Schlick(1.0, f90, NoV);
    return lightScatter * viewScatter * (1.0 / M_PI);
}


vec3 disneySheen(float LoH, vec3 sheenColor, float sheen)
{
    if (sheen <= 0.0) return vec3(0.0);

    float FH = pow(1.0 - LoH, 5.0);

    return sheen * FH * sheenColor;
}



vec3 BRDF(vec3 baseColor) {
    vec3 N = normalize(vert.N);
    vec3 T = normalize(vert.T - dot(vert.T, N) * N);
    vec3 B = cross(N, T) * vert.sign;

    mat3 TBN_ws = mat3(T,B,N);
    mat3 TBN_ts = transpose(TBN_ws);

    vec3 color = vec3(0);

    // Normal Map
    vec3 n_ts = vec3(0.0, 0.0, 1.0);
    if (MASK_COMPARE(push.textureBitmap, NORMAL_TEXTURE)) {
        vec4 normalSample = texture(normalMap, (push.textureBitmap & NORMAL_UV) == 0 ? vert.texcoord : vert.texcoord1);
        n_ts = normalSample.rgb * 2.0 - 1.0;
    }
    
    // Metallic - Roughness - Occlusion
    float metallic, perceptualRoughness;
    float occlusion = (push.textureBitmap & OCCLUSION_TEXTURE) == 0 ? 1.0 : texture(occlusionMap, (push.textureBitmap & OCCLUSION_UV) == 0 ? vert.texcoord : vert.texcoord1).r;
    if (MASK_COMPARE(push.textureBitmap, ROUGH_METAL_TEXTURE)) {
        vec4 mro = texture(metalRoughnessMap, (push.textureBitmap & ROUGH_METAL_UV) == 0 ? vert.texcoord : vert.texcoord1);
        metallic = clamp(mro.b, 0.0, 1.0);
        perceptualRoughness = clamp(mro.g, 0.04, 1.0);
    } else {
        metallic = clamp(push.metalness, 0.0, 1.0);
        perceptualRoughness = clamp(push.roughness, 0.04, 1.0);
    }
    
    // Anisotropic linear roughness
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec2 alphaAniso = computeAnisoRoughness(alphaRoughness);
    
    // Separate diffuse and specular from metallic workflow textures
	float reflectance = 0.5;
    vec3 f0 = vec3(reflectance * reflectance * 0.16);
    vec3 diffuseColor = baseColor.rgb * (1.0 - metallic);
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

    vec3 n = normalize(TBN_ws * n_ts);
    vec3 v = normalize(ubo.invViewMatrix[3].xyz - vert.worldPos);
    vec3 reflection = normalize(reflect(-v, n));
    
    // Shade each light entity
    const float lightNum = ubo.lightInfo & 0xFF;
    for (int i = 0; i < lightNum; i++) {
        
        // Different interpretation for different light types
        vec3 l, u_LightColor; float shadow = 1.0;
        if (((ubo.lightInfo >> (8 + i)) & 0x1) == 0) {
            l = normalize(ubo.lightVector[i].xyz - vert.worldPos); // Vector from surface point to light

            vec3 posToLight = (ubo.lightVector[i].xyz - vert.worldPos) * 2.5;
            float distanceSquare = dot(posToLight, posToLight);
            float atten = 1.0 / max(distanceSquare, 1e-4);
            u_LightColor = ubo.lightChroma[i].rgb * ubo.lightChroma[i].a * atten;

        } else {
            l = -normalize(ubo.lightVector[i].xyz); // Vector from surface with direction of light
            u_LightColor = ubo.lightChroma[i].rgb * ubo.lightChroma[i].a;
            
            shadow = filterPCF(vert.lightSpacePos);
            if(dot(n, l) < 0.0) shadow = 0.01;
        }
        
        vec3 h = normalize(l + v);
        vec3 l_ts = normalize(TBN_ts * l);
        vec3 v_ts = normalize(TBN_ts * v);
        vec3 h_ts = normalize(l_ts + v_ts);
        
        float NoV = max(abs(dot(n, v)), 0.001);
        float NoL = max(dot(n, l), 0.001);
        float LoH = max(dot(l, h), 0.0);
        float NoVg = max(dot(N, v), 0.0);
        float energyComp = (NoVg > 0.0) ? (NoV / NoVg) : 1.0;

        // Cook-Torrance Anisotropic Microfacet BRDF using Covariance matrix in slope space
        mat2 cov2 = standardCovarianceMatrix(alphaAniso, 0.0);

        float D = D_GGX_Covariance(h_ts, cov2);
        float G = G_Smith_Covariance(l_ts, v_ts, cov2);
        vec3 F = F_Schlick(specularColor, 1.0, LoH);

        vec3 specContrib = energyComp * shadow * (D * G * F) / (4.0 * NoL * NoV);
        vec3 diffuseContrib = diffuseColor * Fd_Burley(alphaRoughness, NoV, NoL, LoH);

        // Simple Cook-Torrance Isotropic BRDF
        float weight = push.coatWeight;
        float Dcc = D_GGX(dot(N, h), pow(push.coatRoughness, 2.0));
        float Gcc = G_SmithGGX(NoV, NoL, pow(push.coatRoughness, 2.0));
        float Fcc = F_Schlick(0.04 * weight, 1.0, LoH) * weight;
        float DGFcc = shadow * shadow * (Dcc * Gcc * Fcc) / (4.0 * NoL * NoV);

        vec3 sheenContrib = disneySheen(LoH, vec3(1.0,0.0,0.0), 0.0);
        
        color += NoL * u_LightColor * ((diffuseContrib + specContrib * (1.0 - Fcc)) * (1.0 - Fcc) + DGFcc) * shadow;
    }
    
    // Calculate lighting contribution from image based lighting source (IBL)
    if (MASK_COMPARE(ubo.debugMode, DEBUG_IBL_CONTRIB_BIT)) {
        bool multi_scatter = MASK_COMPARE(ubo.debugMode, DEBUG_MULTISCATTER_BIT);
        color += computeIBL(n, v, reflection, perceptualRoughness, diffuseColor, specularColor, multi_scatter);
    }
    
    // Apply optional PBR terms for additional (optional) shading
    const float u_OcclusionStrength = 0.5f;
    if ((push.textureBitmap & OCCLUSION_TEXTURE) == OCCLUSION_TEXTURE) {
        color = mix(color, color * occlusion, u_OcclusionStrength);
    }
    
    switch (ubo.debugMode & 0xFF) {
    case 1:
        color = diffuseColor;
        break;
        
    case 2:
        color = (n + 1.0) * 0.5;
        break;
        
    case 3:
        color = vec3(perceptualRoughness);
        break;
        
    case 4:
        color = vec3(metallic);
        break;
        
    default:
        break;
    }
    
    return color;
}
