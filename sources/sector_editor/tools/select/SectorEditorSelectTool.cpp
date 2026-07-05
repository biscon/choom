#include "sector_editor/tools/select/SectorEditorSelectTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <vector>

namespace game {

namespace {

std::vector<SectorEditorPickCandidate> BuildPickCandidates(
        SectorEditorToolContext& context,
        Vector2 screenPoint)
{
    return context.buildSelectPickCandidates
            ? context.buildSelectPickCandidates(screenPoint)
            : std::vector<SectorEditorPickCandidate>{};
}

SectorEditorPickTarget CurrentPickSelectionTarget(SectorEditorToolContext& context)
{
    return context.currentPickSelectionTarget
            ? context.currentPickSelectionTarget()
            : SectorEditorPickTarget{};
}

bool SelectPickTarget(SectorEditorToolContext& context, SectorEditorPickTarget target)
{
    if (!context.buildSelectionServiceContext) {
        return false;
    }

    context.state.selectDragArm = SelectDragArmState{};
    SectorEditorSelectionServiceContext selectionContext = context.buildSelectionServiceContext();
    switch (target.kind) {
        case SectorEditorPickKind::RuntimeObject:
            SelectSectorEditorRuntimeObject(selectionContext, target.id);
            return context.state.selectedRuntimeObjectId == target.id;
        case SectorEditorPickKind::DynamicSpotLight:
            SelectSectorEditorTopologyDynamicSpotLight(selectionContext, target.id);
            return context.state.selectedTopologyDynamicSpotLightId == target.id;
        case SectorEditorPickKind::DynamicLight:
            SelectSectorEditorTopologyDynamicLight(selectionContext, target.id);
            return context.state.selectedTopologyDynamicLightId == target.id;
        case SectorEditorPickKind::StaticSpotLight:
            SelectSectorEditorTopologyStaticSpotLight(selectionContext, target.id);
            return context.state.selectedTopologyStaticSpotLightId == target.id;
        case SectorEditorPickKind::StaticLight:
            SelectSectorEditorTopologyLight(selectionContext, target.id);
            return context.state.selectedTopologyLightId == target.id;
        case SectorEditorPickKind::AuthoringVertex:
            SelectSectorEditorAuthoringVertexTarget(selectionContext, target.id);
            return context.state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
                    && context.state.selectedAuthoring.vertexId == target.id;
        case SectorEditorPickKind::AuthoringLine:
            SelectSectorEditorAuthoringLineTarget(selectionContext, target.id);
            return context.state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
                    && context.state.selectedAuthoring.lineId == target.id;
        case SectorEditorPickKind::AuthoringFaceAnchor:
            SelectSectorEditorAuthoringFaceAnchorTarget(selectionContext, target.id);
            return context.state.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
                    && context.state.selectedAuthoring.faceAnchorId == target.id;
        case SectorEditorPickKind::None:
            ClearSectorEditorSelection(selectionContext);
            return true;
    }
    return false;
}

void UpdateSelectHover(SectorEditorToolContext& context, Vector2)
{
    if (context.input == nullptr) {
        return;
    }

    const std::vector<SectorEditorPickCandidate> candidates =
            BuildPickCandidates(context, context.input->MousePosition());
    if (!candidates.empty()) {
        const SectorEditorPickTarget target = candidates.front().target;
        if (target.kind == SectorEditorPickKind::AuthoringVertex) {
            SetHoveredSectorEditorAuthoringVertex(context.state, target.id);
        } else if (target.kind == SectorEditorPickKind::AuthoringLine) {
            SetHoveredSectorEditorAuthoringLine(context.state, target.id);
        } else if (target.kind == SectorEditorPickKind::StaticLight) {
            context.state.hoveredTopologyLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::StaticSpotLight) {
            context.state.hoveredTopologyStaticSpotLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::DynamicLight) {
            context.state.hoveredTopologyDynamicLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::DynamicSpotLight) {
            context.state.hoveredTopologyDynamicSpotLightId = target.id;
        }
    }
    context.state.inspectedTopologyVertexId = -1;
}

bool UpdateSelectToolEarly(SectorEditorToolContext& context)
{
    if (context.input == nullptr || !context.buildManipulationServiceContext) {
        return false;
    }

    SectorEditorManipulationServiceContext manipulationContext =
            context.buildManipulationServiceContext();
    const bool wasArmed = context.state.selectDragArm.active;
    UpdateSectorEditorSelectDragArm(manipulationContext, *context.input);
    return wasArmed && !context.state.selectDragArm.active;
}

bool HandleSelectMousePress(SectorEditorToolContext& context, const engine::InputEvent& event)
{
    if (event.mouseButton.button != MOUSE_LEFT_BUTTON
            || !CheckCollisionPointRec(event.mouseButton.position, context.canvasRect)
            || !context.buildManipulationServiceContext) {
        return false;
    }

    SectorEditorManipulationServiceContext manipulationContext =
            context.buildManipulationServiceContext();
    ArmSectorEditorSelectedDrag(manipulationContext, event.mouseButton.position);
    return context.state.selectDragArm.active;
}

bool UpdateSelectTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr) {
        return false;
    }

    bool handled = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [&context, &handled](engine::InputEvent& event) {
                if (handled
                        || event.mouseClick.button != MOUSE_LEFT_BUTTON
                        || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) {
                    return;
                }

                const std::vector<SectorEditorPickCandidate> candidates =
                        BuildPickCandidates(context, event.mouseClick.releasePosition);
                int cycleIndex = -1;
                int cycleCount = 0;
                const SectorEditorPickTarget target = ChooseSectorEditorPickTarget(
                        candidates,
                        CurrentPickSelectionTarget(context),
                        &cycleIndex,
                        &cycleCount);
                if (target.kind == SectorEditorPickKind::None) {
                    if (context.buildSelectionServiceContext) {
                        SectorEditorSelectionServiceContext selectionContext =
                                context.buildSelectionServiceContext();
                        ClearSectorEditorSelection(selectionContext);
                    } else if (context.clearSelection) {
                        context.clearSelection();
                    }
                    context.statusText = "Selection cleared";
                } else if (SelectPickTarget(context, target)) {
                    const char* kindName = SectorEditorPickKindName(target.kind);
                    context.statusText = cycleCount > 1 && cycleIndex >= 0
                            ? TextFormat(
                                    "Selected %s %d (%d/%d)",
                                    kindName,
                                    target.id,
                                    cycleIndex + 1,
                                    cycleCount)
                            : TextFormat("Selected %s %d", kindName, target.id);
                }
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

const SectorEditorToolModule SelectModule{
        SectorEditorTool::Select,
        "Select",
        UpdateSelectHover,
        UpdateSelectToolEarly,
        HandleSelectMousePress,
        UpdateSelectTool,
        nullptr,
        nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorSelectToolModule()
{
    return SelectModule;
}

} // namespace game
