/*
"Stable Geometric Specular Antialiasing with Projected-Space NDF Filtering"
Yusuke Tokuyoshi, Anton S. Kaplanyan
*/

mat2 NonAxisAlignedNDFFiltering(vec3 halfvectorTS, vec2 alpha_roughness) {
    // Compute the derivatives of the halfvector in the projected space.
    vec2 halfvector2D = halfvectorTS.xy; // 2016-> / abs(halfvectorTS.z);
    vec2 deltaU = dFdx(halfvector2D);
    vec2 deltaV = dFdy(halfvector2D);
    // Compute 2 * covariance matrix for the filter kernel (Eq. (3)).
    float SIGMA2 = 0.15915494;
    mat2 delta = {deltaU, deltaV};
    mat2 kernelRoughnessMat = 2.0 * SIGMA2 * (transpose(delta), delta);
    // Approximate NDF filtering (Eq. (9)).
    mat2 roughnessMat = mat2(alpha_roughness.x, 0.0, 0.0, alpha_roughness.y);
    mat2 filteredRoughnessMat = roughnessMat + kernelRoughnessMat;
    return filteredRoughnessMat;
}

mat2 AxisAlignedNDFFiltering(vec3 halfvectorTS, vec2 alpha_roughness) {
	// Compute the bounding rectangle of halfvector derivatives.
	vec2 halfvector2D = halfvectorTS.xy; // 2016-> / abs(halfvectorTS.z);
	vec2 bounds = fwidth(halfvector2D);
	// Compute an axis-aligned filter kernel from the bounding rectangle.
	float SIGMA2 = 0.15915494;
	vec2 kernelRoughness2 = 2.0 * SIGMA2 * (bounds * bounds);
	// Approximate NDF filtering (Eq. (9)).
	// We clamp the kernel size to avoid overfiltering.
	float KAPPA = 0.18;
	vec2 clampedKernelRoughness2 = min(kernelRoughness2, KAPPA);
	vec2 filteredRoughness2 = clamp(alpha_roughness + clampedKernelRoughness2, 0.0, 1.0);
	return mat2(filteredRoughness2.x, 0.0, 0.0, filteredRoughness2.y);
}

mat2 FullNonAxisAlignedNDFFiltering(vec3 halfvectorTS, vec2 alpha_roughness) {
	// Compute the derivatives of the halfvector in the projected space.
	vec2 halfvector2D = halfvectorTS.xy;
	vec2 deltaU = dFdx(halfvector2D);
	vec2 deltaV = dFdy(halfvector2D);
	// Compute 2 * covariance matrix for the filter kernel (Eq. (3)).
	float SIGMA2 = 0.15915494;
	mat2 delta = {deltaU, deltaV};
	mat2 kernelRoughnessMat = 2.0 * SIGMA2 * (transpose(delta) * delta);
	// Convert the roughness from slope space to the projected space (Eq. (4)).
	vec2 projRoughness2 = alpha_roughness / (1.0 - alpha_roughness);
	mat2 projRoughnessMat = mat2(projRoughness2.x, 0.0, 0.0, projRoughness2.y);
	// NDF filtering in the projected space (Eq. (6)).
	mat2 filteredProjRoughnessMat = projRoughnessMat + kernelRoughnessMat;
	// Convert the roughness from the projected space to slope space (Eq. (7)).
	// This implementation is optimized based on Appendix D.
	// For numerical stability, the determinant is clamped with the lower bound.
	float detMin = projRoughness2.x * projRoughness2.y;
	float det = max(determinant(filteredProjRoughnessMat), detMin);
	mat2 m = filteredProjRoughnessMat / det + mat2(1.0, 0.0, 0.0, 1.0);
	mat2 filteredRoughnessMat = m / max(determinant(m), 1.0);
	return filteredRoughnessMat;
}