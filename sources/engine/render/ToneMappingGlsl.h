#pragma once

namespace engine {

inline constexpr const char* ToneMappingGlsl = R"glsl(
vec3 ToneMapNeutralMaxChannel(vec3 linearRgb)
{
    vec3 color = max(linearRgb, vec3(0.0));
    float peak = max(color.r, max(color.g, color.b));
    return color / (1.0 + peak);
}
)glsl";

} // namespace engine
