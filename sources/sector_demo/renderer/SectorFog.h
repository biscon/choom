#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

namespace game {

struct SectorFogRenderContext {
    SectorTopologyFogSettings settings;
    Vector3 cameraPosition = {};
    bool volumetricHandoffEnabled = false;
    float volumetricMaximumDistanceWorld = 0.0f;
};

struct SectorFogShaderLocations {
    int enabled = -1;
    int color = -1;
    int cameraPosition = -1;
    int startDistanceWorld = -1;
    int density = -1;
    int maxOpacity = -1;
    int referenceHeightWorld = -1;
    int heightFalloff = -1;
    int volumetricHandoffEnabled = -1;
    int volumetricMaximumDistanceWorld = -1;
};

SectorFogRenderContext BuildSectorFogRenderContext(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        bool volumetricHandoffEnabled = false,
        float volumetricMaximumDistanceWorld = 0.0f);
SectorFogShaderLocations GetSectorFogShaderLocations(Shader shader);
void UploadSectorFogShaderValues(
        Shader shader,
        const SectorFogShaderLocations& locations,
        const SectorFogRenderContext& context);

} // namespace game
