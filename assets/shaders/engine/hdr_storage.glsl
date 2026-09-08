#ifndef ENGINE_HDR_STORAGE_GLSL
#define ENGINE_HDR_STORAGE_GLSL

const float kRgba16fMaximumFinite = 65504.0;
float SanitizeLinearHdrChannelForRgba16f(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? kRgba16fMaximumFinite : 0.0;
    return min(max(value, 0.0), kRgba16fMaximumFinite);
}
vec3 SanitizeLinearHdrForRgba16f(vec3 value) {
    return vec3(
        SanitizeLinearHdrChannelForRgba16f(value.r),
        SanitizeLinearHdrChannelForRgba16f(value.g),
        SanitizeLinearHdrChannelForRgba16f(value.b));
}
float SanitizeBoundedHdrAlpha(float value, float fallbackValue) {
    return (isnan(value) || isinf(value))
        ? clamp(fallbackValue, 0.0, 1.0)
        : clamp(value, 0.0, 1.0);
}
vec4 StoreLinearHdrRgba16f(vec3 rgb, float alpha, float alphaFallback) {
    return vec4(
        SanitizeLinearHdrForRgba16f(rgb),
        SanitizeBoundedHdrAlpha(alpha, alphaFallback));
}

#endif
