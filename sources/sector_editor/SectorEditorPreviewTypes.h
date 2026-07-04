#pragma once

#include "sector_demo/SectorViewPose.h"

#include <raylib.h>

namespace game {

enum class SectorPreviewControlMode {
    FreeFly,
    Gameplay
};

enum class PreviewDebugOverlayTab {
    None,
    View,
    Render,
    Visibility,
    Lighting,
    Objects,
    Probes,
    Controls
};

enum class SpotLightPilotKind {
    None,
    Static,
    Dynamic
};

struct SpotLightPilotState {
    bool active = false;
    SpotLightPilotKind kind = SpotLightPilotKind::None;
    int lightId = -1;
    Vector3 originalPosition = {};
    Vector3 originalTarget = {};
    SectorViewPose originalPreviewPose = {};
    bool originalMouseLookEnabled = true;
    float targetDistanceWorld = 4.0f;
};

} // namespace game
