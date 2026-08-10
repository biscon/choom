#include "engine/render/ToneMapping.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

float NonNegativeFinite(float value)
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

} // namespace

Vector3 ToneMapNeutralMaxChannel(Vector3 linearRgb)
{
    const Vector3 color{
            NonNegativeFinite(linearRgb.x),
            NonNegativeFinite(linearRgb.y),
            NonNegativeFinite(linearRgb.z)};
    const float peak = std::max({color.x, color.y, color.z});
    const float scale = 1.0f / (1.0f + peak);
    return Vector3{color.x * scale, color.y * scale, color.z * scale};
}

} // namespace engine
