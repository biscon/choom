#ifndef SECTOR_FINITE_HALF_RADIANCE_GLSL
#define SECTOR_FINITE_HALF_RADIANCE_GLSL

vec3 StoreFiniteHalfRadiance(vec3 value)
{
    vec3 result;
    result.r = isnan(value.r) ? 0.0 : (isinf(value.r) ? (value.r > 0.0 ? 65504.0 : 0.0) : min(max(value.r, 0.0), 65504.0));
    result.g = isnan(value.g) ? 0.0 : (isinf(value.g) ? (value.g > 0.0 ? 65504.0 : 0.0) : min(max(value.g, 0.0), 65504.0));
    result.b = isnan(value.b) ? 0.0 : (isinf(value.b) ? (value.b > 0.0 ? 65504.0 : 0.0) : min(max(value.b, 0.0), 65504.0));
    return result;
}

#endif
