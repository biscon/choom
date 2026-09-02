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
    bool impactEntryActive = false;
    float impactEntryStartFeetY = 0.0f;
    float impactEntryTargetFeetY = 0.0f;
    float impactEntrySpeed = 0.0f;
    float impactEntryElapsedSeconds = 0.0f;
    float impactEntryDurationSeconds = 0.0f;
    bool exitingWater = false;
    Vector3 exitStartFeetPosition = {};
    Vector3 exitTargetFeetPosition = {};
    int exitTargetSectorId = 0;
    float exitElapsedSeconds = 0.0f;
    SectorLiquidContact contact;
};

struct SectorLiquidPhysicsConfig {
    float entrySlowdownSeconds = 0.20f;
    float waterDragPerSecond = 5.0f;
    float surfaceRecoveryFrequencyHz = 0.35f;
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
bool BeginSectorLiquidImpactEntry(
        SectorLiquidMovementState& liquid,
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorLiquidPhysicsConfig& physics,
        float minimumFeetY);
void UpdateSectorLiquidSwimmingVerticalMotion(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        SectorLiquidMovementState& liquid,
        const SectorLiquidPhysicsConfig& physics,
        float desiredVerticalVelocity,
        float minimumFeetY,
        float maximumFeetY,
        float dt);

} // namespace game
