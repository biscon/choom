#pragma once

#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

namespace game {

struct SectorFpsControllerConfig {
    float walkSpeed = 6.0f;
    float runSpeed = 12.0f;
    float mouseSensitivity = 1.0f;
    float eyeHeight = 5.0f;
};

struct SectorFpsControllerState {
    Vector3 feetPosition = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

struct SectorFpsControllerInput {
    bool moveForward = false;
    bool moveBackward = false;
    bool strafeLeft = false;
    bool strafeRight = false;
    bool run = false;
    bool mouseLookEnabled = false;
    Vector2 mouseDelta = {};
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
void UpdateSectorFpsController(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt);

} // namespace game
