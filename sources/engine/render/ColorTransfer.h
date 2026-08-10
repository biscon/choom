#pragma once

#include <raylib.h>

namespace engine {

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
