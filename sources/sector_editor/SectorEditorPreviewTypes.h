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

struct SpotLightPilotPreviewRestoreState {
    SectorViewPose originalPreviewPose = {};
    bool originalMouseLookEnabled = true;
};

} // namespace game
