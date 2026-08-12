#include "engine/render/HdrEffectPolicy.h"

#include <algorithm>
#include <cmath>

namespace engine {

const char* const Rgba16fStoragePolicyGlsl = R"(
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
)";

float SanitizeLinearHdrChannelForRgba16f(float value)
{
    if (std::isnan(value)) return 0.0f;
    if (std::isinf(value)) return value > 0.0f ? Rgba16fMaximumFinite : 0.0f;
    return std::min(std::max(value, 0.0f), Rgba16fMaximumFinite);
}

Vector3 SanitizeLinearHdrForRgba16f(Vector3 value)
{
    return Vector3{
            SanitizeLinearHdrChannelForRgba16f(value.x),
            SanitizeLinearHdrChannelForRgba16f(value.y),
            SanitizeLinearHdrChannelForRgba16f(value.z)};
}

float SanitizeBoundedAlpha(float value, float fallback)
{
    return std::isfinite(value)
            ? std::clamp(value, 0.0f, 1.0f)
            : std::clamp(fallback, 0.0f, 1.0f);
}

Vector4 SanitizeLinearHdrForRgba16f(Vector4 value, float alphaFallback)
{
    const Vector3 rgb = SanitizeLinearHdrForRgba16f(
            Vector3{value.x, value.y, value.z});
    return Vector4{rgb.x, rgb.y, rgb.z,
            SanitizeBoundedAlpha(value.w, alphaFallback)};
}

HdrBloomSettings NormalizeHdrBloomSettings(HdrBloomSettings settings)
{
    const HdrBloomSettings defaults;
    settings.threshold = std::isfinite(settings.threshold)
            ? std::clamp(settings.threshold, 0.0f, Rgba16fMaximumFinite)
            : defaults.threshold;
    settings.softKnee = std::isfinite(settings.softKnee)
            ? std::clamp(settings.softKnee, 0.0f, 1.0f)
            : defaults.softKnee;
    settings.intensity = std::isfinite(settings.intensity)
            ? std::clamp(settings.intensity, 0.0f, 16.0f)
            : defaults.intensity;
    settings.radius = std::isfinite(settings.radius)
            ? std::clamp(settings.radius, 0.25f, 4.0f)
            : defaults.radius;
    return settings;
}

Vector3 EvaluateHdrBloomPrefilter(
        Vector3 linearSceneRgb,
        const HdrBloomSettings& settingsValue)
{
    const HdrBloomSettings settings = NormalizeHdrBloomSettings(settingsValue);
    const Vector3 color = SanitizeLinearHdrForRgba16f(linearSceneRgb);
    const float brightness = std::max({color.x, color.y, color.z});
    if (!(brightness > 0.0f)) return Vector3{};
    if (settings.threshold == 0.0f) return color;

    float excess = 0.0f;
    if (settings.softKnee == 0.0f) {
        excess = std::max(brightness - settings.threshold, 0.0f);
    } else {
        const float knee = settings.threshold * settings.softKnee;
        const float q = std::clamp(
                brightness - settings.threshold + knee,
                0.0f,
                2.0f * knee);
        const float softExcess = q * q / (4.0f * knee);
        const float hardExcess = std::max(
                brightness - settings.threshold, 0.0f);
        excess = std::max(softExcess, hardExcess);
    }
    const float weight = excess / brightness;
    return SanitizeLinearHdrForRgba16f(Vector3{
            color.x * weight,
            color.y * weight,
            color.z * weight});
}

Vector4 CompositeHdrPremultipliedAtmosphere(
        Vector4 linearScene,
        Vector4 premultipliedScattering)
{
    const Vector3 scene=SanitizeLinearHdrForRgba16f(
            Vector3{linearScene.x,linearScene.y,linearScene.z});
    const Vector3 scattering=SanitizeLinearHdrForRgba16f(
            Vector3{premultipliedScattering.x,premultipliedScattering.y,
                    premultipliedScattering.z});
    const float opacity=SanitizeBoundedAlpha(premultipliedScattering.w);
    const Vector3 composed=SanitizeLinearHdrForRgba16f(Vector3{
            scene.x*(1.0f-opacity)+scattering.x,
            scene.y*(1.0f-opacity)+scattering.y,
            scene.z*(1.0f-opacity)+scattering.z});
    return Vector4{composed.x,composed.y,composed.z,
            SanitizeBoundedAlpha(linearScene.w,1.0f)};
}

Vector4 CompositeHdrBloom(Vector4 linearScene, Vector3 bloomRgb, float intensity)
{
    const Vector3 scene=SanitizeLinearHdrForRgba16f(
            Vector3{linearScene.x,linearScene.y,linearScene.z});
    const Vector3 bloom=SanitizeLinearHdrForRgba16f(bloomRgb);
    const float safeIntensity=std::isfinite(intensity)
            ? std::max(intensity,0.0f) : 0.0f;
    const Vector3 composed=SanitizeLinearHdrForRgba16f(Vector3{
            scene.x+bloom.x*safeIntensity,
            scene.y+bloom.y*safeIntensity,
            scene.z+bloom.z*safeIntensity});
    return Vector4{composed.x,composed.y,composed.z,
            SanitizeBoundedAlpha(linearScene.w,1.0f)};
}

} // namespace engine
