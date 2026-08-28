#include "engine/render/ToneMapping.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

float NonNegativeFinite(float value)
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

float AcesFilmicFittedChannel(float value)
{
    value = NonNegativeFinite(value);
    const float numerator = value * (2.51f * value + 0.03f);
    const float denominator = value * (2.43f * value + 0.59f) + 0.14f;
    return std::clamp(numerator / denominator, 0.0f, 1.0f);
}

} // namespace

const char* ToneMappingOperatorName(ToneMappingOperator toneMapper)
{
    switch (toneMapper) {
        case ToneMappingOperator::KhronosPbrNeutral:
            return "khronosPbrNeutral";
        case ToneMappingOperator::AcesFilmicFitted:
            return "acesFilmicFitted";
    }
    return "khronosPbrNeutral";
}

const char* ToneMappingOperatorDisplayName(ToneMappingOperator toneMapper)
{
    switch (toneMapper) {
        case ToneMappingOperator::KhronosPbrNeutral:
            return "Khronos PBR Neutral";
        case ToneMappingOperator::AcesFilmicFitted:
            return "ACES Filmic (fitted)";
    }
    return "Khronos PBR Neutral";
}

ToneMappingSettings NormalizeToneMappingSettings(ToneMappingSettings settings)
{
    switch (settings.toneMapper) {
        case ToneMappingOperator::KhronosPbrNeutral:
        case ToneMappingOperator::AcesFilmicFitted:
            break;
        default:
            settings.toneMapper = ToneMappingOperator::KhronosPbrNeutral;
            break;
    }
    if (!std::isfinite(settings.exposureCompensationEv)) {
        settings.exposureCompensationEv = 0.0f;
    }
    settings.exposureCompensationEv = std::clamp(
            settings.exposureCompensationEv,
            MinimumToneMappingExposureEv,
            MaximumToneMappingExposureEv);
    return settings;
}

float ToneMappingExposureMultiplier(float exposureCompensationEv)
{
    ToneMappingSettings settings;
    settings.exposureCompensationEv = exposureCompensationEv;
    return std::exp2(
            NormalizeToneMappingSettings(settings).exposureCompensationEv);
}

Vector3 ToneMapKhronosPbrNeutral(Vector3 linearRgb)
{
    // Khronos PBR Neutral reference operator:
    // https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
    Vector3 color{
            NonNegativeFinite(linearRgb.x),
            NonNegativeFinite(linearRgb.y),
            NonNegativeFinite(linearRgb.z)};
    const float darkest = std::min({color.x, color.y, color.z});
    const float offset = darkest < 0.08f
            ? darkest - 6.25f * darkest * darkest
            : 0.04f;
    color.x -= offset;
    color.y -= offset;
    color.z -= offset;
    const float peak = std::max({color.x, color.y, color.z});
    constexpr float StartCompression = 0.8f - 0.04f;
    if (peak < StartCompression) {
        return color;
    }

    constexpr float CompressionDistance = 1.0f - StartCompression;
    const float newPeak = 1.0f
            - CompressionDistance * CompressionDistance
                    / (peak + CompressionDistance - StartCompression);
    const float scale = newPeak / peak;
    color.x *= scale;
    color.y *= scale;
    color.z *= scale;
    constexpr float Desaturation = 0.15f;
    const float desaturation = 1.0f
            - 1.0f / (Desaturation * (peak - newPeak) + 1.0f);
    return Vector3{
            color.x + (newPeak - color.x) * desaturation,
            color.y + (newPeak - color.y) * desaturation,
            color.z + (newPeak - color.z) * desaturation};
}

Vector3 ToneMapAcesFilmicFitted(Vector3 linearRgb)
{
    // Krzysztof Narkowicz's real-time ACES filmic fit, not a full ACES
    // output transform: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    return Vector3{
            AcesFilmicFittedChannel(linearRgb.x),
            AcesFilmicFittedChannel(linearRgb.y),
            AcesFilmicFittedChannel(linearRgb.z)};
}

Vector3 ApplyToneMapping(
        Vector3 linearRgb,
        const ToneMappingSettings& sourceSettings)
{
    const ToneMappingSettings settings =
            NormalizeToneMappingSettings(sourceSettings);
    const float exposure = ToneMappingExposureMultiplier(
            settings.exposureCompensationEv);
    const Vector3 exposed{
            NonNegativeFinite(linearRgb.x) * exposure,
            NonNegativeFinite(linearRgb.y) * exposure,
            NonNegativeFinite(linearRgb.z) * exposure};
    switch (settings.toneMapper) {
        case ToneMappingOperator::AcesFilmicFitted:
            return ToneMapAcesFilmicFitted(exposed);
        case ToneMappingOperator::KhronosPbrNeutral:
            return ToneMapKhronosPbrNeutral(exposed);
    }
    return ToneMapKhronosPbrNeutral(exposed);
}

} // namespace engine
