#pragma once

namespace engine {

inline constexpr const char* ToneMappingGlsl = R"glsl(
// Khronos PBR Neutral reference operator:
// https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
vec3 ToneMapKhronosPbrNeutral(vec3 linearRgb)
{
    vec3 color = max(linearRgb, vec3(0.0));
    float darkest = min(color.r, min(color.g, color.b));
    float offset = darkest < 0.08
            ? darkest - 6.25 * darkest * darkest
            : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    const float startCompression = 0.8 - 0.04;
    if (peak < startCompression) {
        return color;
    }

    const float compressionDistance = 1.0 - startCompression;
    float newPeak = 1.0
            - compressionDistance * compressionDistance
                    / (peak + compressionDistance - startCompression);
    color *= newPeak / peak;
    const float desaturation = 0.15;
    float amount = 1.0
            - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0), amount);
}

// Krzysztof Narkowicz's real-time ACES filmic fit, not a full ACES output
// transform.
vec3 ToneMapAcesFilmicFitted(vec3 linearRgb)
{
    vec3 color = max(linearRgb, vec3(0.0));
    return clamp(
            (color * (2.51 * color + 0.03))
                    / (color * (2.43 * color + 0.59) + 0.14),
            0.0,
            1.0);
}

vec3 ApplyToneMapping(vec3 linearRgb, int toneMapper)
{
    if (toneMapper == 1) {
        return ToneMapAcesFilmicFitted(linearRgb);
    }
    return ToneMapKhronosPbrNeutral(linearRgb);
}
)glsl";

} // namespace engine
