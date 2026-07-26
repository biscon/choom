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

    SectorEditorSelectionServiceContext selectionContext = context.buildSelectionServiceContext();
    selectionContext.manipulationState.selectDragArm = SelectDragArmState{};
    switch (target.kind) {
        case SectorEditorPickKind::RuntimeObject:
            SelectSectorEditorRuntimeObject(selectionContext, target.id);
            return context.selectionState.selectedRuntimeObjectId == target.id;
        case SectorEditorPickKind::DynamicSpotLight:
            SelectSectorEditorTopologyDynamicSpotLight(selectionContext, target.id);
            return context.selectionState.selectedTopologyDynamicSpotLightId == target.id;
        case SectorEditorPickKind::DynamicLight:
            SelectSectorEditorTopologyDynamicLight(selectionContext, target.id);
            return context.selectionState.selectedTopologyDynamicLightId == target.id;
        case SectorEditorPickKind::StaticSpotLight:
            SelectSectorEditorTopologyStaticSpotLight(selectionContext, target.id);
            return context.selectionState.selectedTopologyStaticSpotLightId == target.id;
        case SectorEditorPickKind::StaticLight:
            SelectSectorEditorTopologyLight(selectionContext, target.id);
            return context.selectionState.selectedTopologyLightId == target.id;
        case SectorEditorPickKind::AuthoringVertex:
            SelectSectorEditorAuthoringVertexTarget(selectionContext, target.id);
            return context.selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
                    && context.selectionState.selectedAuthoring.vertexId == target.id;
        case SectorEditorPickKind::AuthoringLine:
            SelectSectorEditorAuthoringLineTarget(selectionContext, target.id);
            return context.selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
                    && context.selectionState.selectedAuthoring.lineId == target.id;
        case SectorEditorPickKind::AuthoringFaceAnchor:
            SelectSectorEditorAuthoringFaceAnchorTarget(selectionContext, target.id);
            return context.selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
                    && context.selectionState.selectedAuthoring.faceAnchorId == target.id;
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
            SetHoveredSectorEditorAuthoringVertex(
                    context.authoringGraph,
                    context.selectionState,
                    target.id);
        } else if (target.kind == SectorEditorPickKind::AuthoringLine) {
            SetHoveredSectorEditorAuthoringLine(
                    context.authoringGraph,
                    context.selectionState,
                    target.id);
        } else if (target.kind == SectorEditorPickKind::StaticLight) {
            context.selectionState.hoveredTopologyLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::StaticSpotLight) {
            context.selectionState.hoveredTopologyStaticSpotLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::DynamicLight) {
            context.selectionState.hoveredTopologyDynamicLightId = target.id;
        } else if (target.kind == SectorEditorPickKind::DynamicSpotLight) {
            context.selectionState.hoveredTopologyDynamicSpotLightId = target.id;
        }
    }
    context.selectionState.inspectedTopologyVertexId = -1;
}

bool UpdateSelectToolEarly(SectorEditorToolContext& context)
{
    if (context.input == nullptr || !context.buildManipulationServiceContext) {
        return false;
    }

    SectorEditorManipulationServiceContext manipulationContext =
            context.buildManipulationServiceContext();
    const bool wasArmed = manipulationContext.manipulationState.selectDragArm.active;
    UpdateSectorEditorSelectDragArm(manipulationContext, *context.input);
    return wasArmed && !manipulationContext.manipulationState.selectDragArm.active;
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
    return manipulationContext.manipulationState.selectDragArm.active;
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
