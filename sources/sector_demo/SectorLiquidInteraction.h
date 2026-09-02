#pragma once

#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorTopologyMap.h"

namespace game {

struct SectorLiquidContact {
    bool hasLiquid = false;
    int sectorId = 0;
    float bottomY = 0.0f;
    float surfaceY = 0.0f;
    float immersionFraction = 0.0f;
    bool eyeSubmerged = false;
    SectorLiquidSettings settings;
};

struct SectorLiquidMovementState {
    bool swimming = false;
    bool surfaceLatched = false;
    bool cameraSubmerged = false;
    SectorLiquidContact contact;
};

constexpr float SectorLiquidSwimEnterImmersion = 0.50f;
constexpr float SectorLiquidSwimExitImmersion = 0.40f;
constexpr float SectorLiquidSurfaceEyeOffsetWorld = 0.08f;
constexpr float SectorLiquidSubmersionHysteresisWorld = 0.03f;

SectorLiquidContact SampleSectorLiquidContact(
        const SectorTopologyMap& map,
        int sectorId,
        Vector3 feetPosition,
        const SectorFpsControllerConfig& config);
void UpdateSectorLiquidMovementState(
        SectorLiquidMovementState& state,
        const SectorLiquidContact& contact,
        bool diveHeld);
Vector3 ComputeSectorLiquidSwimMovementDelta(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        bool surfaceLatched,
        float dt);
bool UpdateSectorLiquidCameraSubmersion(
        bool wasSubmerged,
        const SectorLiquidContact& contact,
        float eyeY);

} // namespace game
