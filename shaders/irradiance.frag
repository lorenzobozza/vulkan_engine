
#version 450

#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform CubeUbo {
    mat4 projectionViewMatrix;
    mat4 viewMatrix;
    float roughness;
} ubo;

layout(binding = 1) uniform samplerCube environmentMap;

void main() {
    vec3 N = normalize(fragPos);
    N = vec3(N.x, N.y, -N.z);
    
    // tangent space calculation from origin point
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));
       
    float delta = 0.025;
    
    vec3 color = vec3(0.0);
	uint sampleCount = 0u;
	for (float phi = 0.0; phi < 2.0 * PI; phi += delta) {
		for (float theta = 0.0; theta < 0.5 * PI; theta += delta) {
			vec3 tempVec = cos(phi) * right + sin(phi) * up;
			vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
			color += texture(environmentMap, sampleVector).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}
	outColor = vec4(PI * color / float(sampleCount), 1.0);
}
