#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"

#include <raylib.h>

namespace game {

enum class LightPilotKind {
    None,
    StaticPoint,
    StaticSpot,
    DynamicPoint,
    DynamicSpot
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

struct LightPilotLightState {
    bool active = false;
    LightPilotKind kind = LightPilotKind::None;
    int lightId = -1;
    Vector3 originalPosition = {};
    Vector3 originalTarget = {};
    float targetDistanceWorld = 4.0f;
};

struct LightEditingState {
    LightDragState lightDrag;
    LightEditTransactionState lightEdit;
    LightPilotLightState lightPilot;
};

} // namespace game
