#pragma once

namespace engine {

// Canonical GLSL 330 transfer-function snippet. It intentionally omits a
// #version directive so future shaders can compose it after their own version
// line without introducing a shader-preprocessor framework.
inline constexpr const char* ColorTransferGlsl = R"glsl(
float SrgbNormalizedChannelToLinear(float srgb)
{
    srgb = clamp(srgb, 0.0, 1.0);
    return srgb <= 0.04045
        ? srgb / 12.92
        : pow((srgb + 0.055) / 1.055, 2.4);
}

vec3 SrgbColorToLinearScene(vec3 srgb)
{
    return vec3(
        SrgbNormalizedChannelToLinear(srgb.r),
        SrgbNormalizedChannelToLinear(srgb.g),
        SrgbNormalizedChannelToLinear(srgb.b));
}

vec4 SrgbColorToLinearScene(vec4 srgb)
{
    return vec4(SrgbColorToLinearScene(srgb.rgb), srgb.a);
}

float LinearNormalizedChannelToSrgb(float linear)
{
    linear = clamp(linear, 0.0, 1.0);
    return linear <= 0.0031308
        ? linear * 12.92
        : 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
}

vec3 LinearSceneToDisplaySrgb(vec3 linearRgb)
{
    return vec3(
        LinearNormalizedChannelToSrgb(linearRgb.r),
        LinearNormalizedChannelToSrgb(linearRgb.g),
        LinearNormalizedChannelToSrgb(linearRgb.b));
}

vec4 LinearSceneToDisplaySrgb(vec4 linearRgba)
{
    return vec4(LinearSceneToDisplaySrgb(linearRgba.rgb), linearRgba.a);
}
)glsl";

} // namespace engine
