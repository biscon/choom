#include "engine/render/ColorTransfer.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

float ClampNormalized(float value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

} // namespace

float SrgbNormalizedChannelToLinear(float srgb)
{
    srgb = ClampNormalized(srgb);
    return srgb <= 0.04045f
            ? srgb / 12.92f
            : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}

float LinearNormalizedChannelToSrgb(float linear)
{
    linear = ClampNormalized(linear);
    return linear <= 0.0031308f
            ? linear * 12.92f
            : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

Vector3 SrgbNormalizedRgbToLinearScene(Vector3 srgb)
{
    return Vector3{
            SrgbNormalizedChannelToLinear(srgb.x),
            SrgbNormalizedChannelToLinear(srgb.y),
            SrgbNormalizedChannelToLinear(srgb.z)};
}

Vector4 SrgbNormalizedRgbaToLinearScene(Vector4 srgb)
{
    const Vector3 rgb = SrgbNormalizedRgbToLinearScene(
            Vector3{srgb.x, srgb.y, srgb.z});
    return Vector4{rgb.x, rgb.y, rgb.z, ClampNormalized(srgb.w)};
}

Vector3 SrgbColorBytesToLinearSceneRgb(Color color)
{
    constexpr float ByteToNormalized = 1.0f / 255.0f;
    return Vector3{
            SrgbNormalizedChannelToLinear(static_cast<float>(color.r) * ByteToNormalized),
            SrgbNormalizedChannelToLinear(static_cast<float>(color.g) * ByteToNormalized),
            SrgbNormalizedChannelToLinear(static_cast<float>(color.b) * ByteToNormalized)};
}

Vector4 SrgbColorBytesToLinearSceneRgba(Color color)
{
    const Vector3 rgb = SrgbColorBytesToLinearSceneRgb(color);
    return Vector4{
            rgb.x,
            rgb.y,
            rgb.z,
            static_cast<float>(color.a) / 255.0f};
}

Color SrgbColorBytesToLinearSceneUnorm(Color color)
{
    const Vector4 linear = SrgbColorBytesToLinearSceneRgba(color);
    const auto byte = [](float value) {
        return static_cast<unsigned char>(
                std::lround(ClampNormalized(value) * 255.0f));
    };
    return Color{byte(linear.x), byte(linear.y), byte(linear.z), color.a};
}

Vector3 LinearSceneRgbToDisplaySrgb(Vector3 linearRgb)
{
    return Vector3{
            LinearNormalizedChannelToSrgb(linearRgb.x),
            LinearNormalizedChannelToSrgb(linearRgb.y),
            LinearNormalizedChannelToSrgb(linearRgb.z)};
}

Vector4 LinearSceneRgbaToDisplaySrgb(Vector4 linearRgba)
{
    const Vector3 rgb = LinearSceneRgbToDisplaySrgb(
            Vector3{linearRgba.x, linearRgba.y, linearRgba.z});
    return Vector4{rgb.x, rgb.y, rgb.z, ClampNormalized(linearRgba.w)};
}

} // namespace engine
