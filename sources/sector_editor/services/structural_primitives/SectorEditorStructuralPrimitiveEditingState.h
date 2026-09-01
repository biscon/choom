#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "engine/ui/UI.h"

#include <array>
#include <string>

namespace game {

enum class SectorEditorStructuralHandleKind {
    None,
    Move,
    Corner0,
    Corner1,
    Corner2,
    Corner3,
    Edge0,
    Edge1,
    Edge2,
    Edge3,
    Radius,
    Rotate
};

struct PendingStructuralPrimitivePlacement {
    bool active = false;
    SectorStructuralPrimitiveKind kind = SectorStructuralPrimitiveKind::Box;
    SectorTopologyCoordPoint start = {};
    SectorTopologyCoordPoint current = {};
    float seedFloor = 0.0f;
    std::string errorMessage;
};

struct StructuralPrimitiveManipulationState {
    bool active = false;
    int primitiveId = -1;
    SectorEditorStructuralHandleKind handle = SectorEditorStructuralHandleKind::None;
    SectorAuthoringStructuralPrimitive original;
    SectorAuthoringStructuralPrimitive preview;
    Vector2 pressMap = {};
    float pressYawDegrees = 0.0f;
    bool valid = false;
    std::string errorMessage;
};

struct PreviewStructuralPrimitiveAdjustmentState {
    bool active = false;
    bool changed = false;
    int primitiveId = -1;
    int preset = 1;
    SectorAuthoringGraph stagedGraph;
    SectorAuthoringDerivationResult stagedDerivation;
};

struct SectorEditorStructuralPrimitiveEditingState {
    SectorStructuralPrimitiveKind placementKind = SectorStructuralPrimitiveKind::Box;
    PendingStructuralPrimitivePlacement pendingPlacement;
    StructuralPrimitiveManipulationState manipulation;
    PreviewStructuralPrimitiveAdjustmentState previewAdjustment;
};

struct SectorEditorStructuralPrimitiveEditingUiState {
    int bufferedPrimitiveId = -1;
    std::string fieldError;
    std::array<engine::UIFloatInputState, 32> floatInputs{};
    std::array<engine::UIIntInputState, 3> intInputs{};
};

} // namespace game
