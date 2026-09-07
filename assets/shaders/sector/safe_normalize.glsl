#ifndef SECTOR_SAFE_NORMALIZE_GLSL
#define SECTOR_SAFE_NORMALIZE_GLSL

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

#endif
