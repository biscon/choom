#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace engine {

inline constexpr float MinimumDisplayGamma = 0.5f;
inline constexpr float MaximumDisplayGamma = 2.0f;

// User adjustment of display-encoded RGB, separate from the sRGB transfer.
// 1 is neutral; larger values brighten midtones without lifting black.
inline float NormalizeDisplayGamma(float gamma)
{
    return std::isfinite(gamma)
            ? std::clamp(gamma, MinimumDisplayGamma, MaximumDisplayGamma)
            : 1.0f;
}
Vector3 ApplyDisplayGamma(Vector3 displayRgb, float gamma);

// Exact IEC 61966-2-1 sRGB transfer functions for normalized color channels.
float SrgbNormalizedChannelToLinear(float srgb);
float LinearNormalizedChannelToSrgb(float linear);

Vector3 SrgbNormalizedRgbToLinearScene(Vector3 srgb);
Vector4 SrgbNormalizedRgbaToLinearScene(Vector4 srgb);

// Color bytes are visible sRGB scene swatches. RGB is decoded for scene use;
// alpha is normalized but is never gamma transformed.
Vector3 SrgbColorBytesToLinearSceneRgb(Color color);
Vector4 SrgbColorBytesToLinearSceneRgba(Color color);
Color SrgbColorBytesToLinearSceneUnorm(Color color);

// Encode linear scene RGB for display-referred sRGB output. Alpha is clamped
// to the normalized domain but is never gamma transformed.
Vector3 LinearSceneRgbToDisplaySrgb(Vector3 linearRgb);
Vector4 LinearSceneRgbaToDisplaySrgb(Vector4 linearRgba);

} // namespace engine
