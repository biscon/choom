#pragma once

#include <raylib.h>

namespace engine {

enum class ToneMappingOperator : int {
    KhronosPbrNeutral = 0,
    AcesFilmicFitted = 1
};

inline constexpr float MinimumToneMappingExposureEv = -8.0f;
inline constexpr float MaximumToneMappingExposureEv = 8.0f;

struct ToneMappingSettings {
    ToneMappingOperator toneMapper = ToneMappingOperator::KhronosPbrNeutral;
    float exposureCompensationEv = 0.0f;
};

const char* ToneMappingOperatorName(ToneMappingOperator toneMapper);
const char* ToneMappingOperatorDisplayName(ToneMappingOperator toneMapper);
ToneMappingSettings NormalizeToneMappingSettings(ToneMappingSettings settings);
float ToneMappingExposureMultiplier(float exposureCompensationEv);

Vector3 ToneMapKhronosPbrNeutral(Vector3 linearRgb);
Vector3 ToneMapAcesFilmicFitted(Vector3 linearRgb);
Vector3 ApplyToneMapping(Vector3 linearRgb, const ToneMappingSettings& settings);

} // namespace engine
