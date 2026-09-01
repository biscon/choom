#pragma once

#include "game/PlayerFlashlightSettings.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include <raylib.h>

namespace game {

inline constexpr int PlayerFlashlightRuntimeLightId = -2;

struct PlayerFlashlightState {
    bool enabled = false;
    bool directionValid = false;
    Vector3 smoothedDirection = {0.0f, 0.0f, -1.0f};
};

void SetPlayerFlashlightEnabled(
        PlayerFlashlightState& state,
        bool enabled);
void TogglePlayerFlashlight(PlayerFlashlightState& state);

bool UpdatePlayerFlashlight(
        PlayerFlashlightState& state,
        const PlayerFlashlightApplicationSettings& settings,
        const Camera3D& camera,
        int ownerSectorId,
        float dt,
        SectorPreviewDynamicPointLightSource& outLight,
        SectorLightAtmosphereSource& outAtmosphere);

} // namespace game
