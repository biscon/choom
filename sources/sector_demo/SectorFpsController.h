#pragma once

#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

namespace game {

struct SectorFpsControllerConfig {
    float walkSpeed = 6.0f;
    float runSpeed = 12.0f;
    float mouseSensitivity = 1.0f;
    float eyeHeight = 1.2f;
    float gravity = 25.0f;
    float playerRadius = 0.25f;
    float playerHeight = 1.6f;
    float stepHeight = 0.25f;
    float jumpHeight = 0.6f;
};

struct SectorFpsControllerState {
    Vector3 feetPosition = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    int currentSectorId = 0;
    bool grounded = false;
    float verticalVelocity = 0.0f;
};

struct SectorFpsControllerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool strafeLeft = false;
    bool strafeRight = false;
    bool run = false;
    bool jumpPressed = false;
    bool mouseLookEnabled = false;
    Vector2 mouseDelta = {};
};

struct SectorFpsVerticalContext {
    bool hasSector = false;
    float floorZ = 0.0f;
    float ceilingZ = 0.0f;
};

enum class SectorFpsVerticalTransition {
    None,
    StayedGrounded,
    SteppedUp,
    SnappedDown,
    StartedDrop,
    Landed,
    CeilingBonk,
    BlockedStep,
    CannotFit
};

struct SectorFpsVerticalResult {
    bool hasSector = false;
    bool cannotFit = false;
    float floorZ = 0.0f;
    float ceilingZ = 0.0f;
    SectorFpsVerticalTransition transition = SectorFpsVerticalTransition::None;
};

SectorFpsControllerConfig DefaultSectorFpsControllerConfig();
SectorFpsControllerConfig NormalizeSectorFpsControllerConfig(SectorFpsControllerConfig config);
SectorFpsControllerConfig SectorFpsControllerConfigFromPreviewSettings(
        SectorPreviewSettings settings);
SectorPreviewSettings SectorPreviewSettingsFromFpsControllerConfig(
        SectorFpsControllerConfig config);
float ClampSectorFpsPitch(float pitchRadians);
Vector3 SectorFpsControllerEyePosition(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config);
SectorMeshPreviewPose SectorFpsControllerPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config);
SectorFpsControllerState SectorFpsControllerStateFromCameraPose(
        const SectorMeshPreviewPose& pose,
        const SectorFpsControllerConfig& config);
Vector2 ComputeSectorFpsHorizontalMovementDelta(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt);
void UpdateSectorFpsMouseLook(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input);
void UpdateSectorFpsController(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt);
bool TryStartSectorFpsJump(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config);
SectorFpsVerticalResult UpdateSectorFpsVerticalPhysics(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsVerticalContext& context,
        float dt);

} // namespace game
