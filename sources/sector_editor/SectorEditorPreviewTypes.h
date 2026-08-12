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
    Pbr,
    Objects,
    Probes,
    Viewmodel,
    Controls
};

struct LightPilotPreviewRestoreState {
    SectorViewPose originalPreviewPose = {};
    bool originalMouseLookEnabled = true;
};

} // namespace game
