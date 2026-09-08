#pragma once

#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include <array>
#include <string>

namespace game
{

// Cubemaps use units 12..15, outside raylib's twelve material map slots.
// No sampler arrays or GL 4 cubemap arrays are required.
struct SectorReflectionShaderLocations
{
    std::array<int, 4> textures{{-1, -1, -1, -1}};
    int capture = -1, center = -1, extents = -1, parameters = -1;
    int transition = -1, widths = -1, plane = -1, mode = -1;
    int aperture = -1, heights = -1;
};

SectorReflectionShaderLocations LoadSectorReflectionShaderLocations(Shader shader);
void UploadSectorReflectionBlend(Shader shader, const SectorReflectionShaderLocations &locations,
                                 const SectorPbrEnvironmentBlend &blend,
                                 engine::AssetManager &assets, float skyExposure = 0.15f);

} // namespace game
