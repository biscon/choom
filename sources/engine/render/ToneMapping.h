#pragma once

#include <raylib.h>

namespace engine {

// Neutral max-channel Reinhard mapping. A single scale is applied to all RGB
// channels, preserving linear RGB ratios while bounding the brightest channel.
Vector3 ToneMapNeutralMaxChannel(Vector3 linearRgb);

} // namespace engine
