#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"

#include <raylib.h>

namespace game {

enum class SpotLightPilotKind {
    None,
    Static,
    Dynamic
};

struct LightDragState {
    bool active = false;
    int topologyLightId = -1;
    SpotLightHandle spotHandle = SpotLightHandle::Origin;
    Vector3 snappedPosition = {};
};

struct LightEditTransactionState {
    bool active = false;
    TopologySelectionKind kind = TopologySelectionKind::None;
    int topologyLightId = -1;
    SpotLightHandle spotHandle = SpotLightHandle::Origin;
    Vector3 originalPosition = {};
    Vector3 originalTarget = {};
};

struct SpotLightPilotLightState {
    bool active = false;
    SpotLightPilotKind kind = SpotLightPilotKind::None;
    int lightId = -1;
    Vector3 originalPosition = {};
    Vector3 originalTarget = {};
    float targetDistanceWorld = 4.0f;
};

struct LightEditingState {
    LightDragState lightDrag;
    LightEditTransactionState lightEdit;
    SpotLightPilotLightState spotLightPilot;
};

} // namespace game
