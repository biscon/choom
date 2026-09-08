#ifndef SECTOR_SAFE_NORMALIZE_ATMOSPHERE_GLSL
#define SECTOR_SAFE_NORMALIZE_ATMOSPHERE_GLSL

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.0000001 ? value * inversesqrt(lengthSquared) : fallback;
}

#endif
