#ifndef SECTOR_SAFE_NORMALIZE_LOWER_GLSL
#define SECTOR_SAFE_NORMALIZE_LOWER_GLSL

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.00000001 ? value * inversesqrt(lengthSquared) : fallback;
}

#endif
