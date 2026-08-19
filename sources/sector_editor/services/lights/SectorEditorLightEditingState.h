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

struct HaloPlacementState {
    bool active = false;
    bool dragging = false;
    LightPilotKind kind = LightPilotKind::None;
    int lightId = -1;
    Vector3 originalOffsetWorld = {};
    Vector3 dragPlanePointWorld = {};
    Vector3 dragPlaneNormalWorld = {0.0f, 0.0f, 1.0f};
    Vector3 dragStartIntersectionWorld = {};
    Vector3 dragStartCenterWorld = {};
};

struct LightEditingState {
    LightDragState lightDrag;
    LightEditTransactionState lightEdit;
    LightPilotLightState lightPilot;
    HaloPlacementState haloPlacement;
};

} // namespace game
