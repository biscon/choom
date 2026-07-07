#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace game {
namespace {

bool SameVector3(Vector3 lhs, Vector3 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool SameColor(Color lhs, Color rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

template<typename Value>
bool SetValue(Value& target, Value value)
{
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool SetVector3(Vector3& target, Vector3 value)
{
    if (SameVector3(target, value)) {
        return false;
    }
    target = value;
    return true;
}

bool SetColorValue(Color& target, Color value)
{
    value.a = 255;
    if (SameColor(target, value)) {
        return false;
    }
    target = value;
    return true;
}

float ClampConeDegrees(float value)
{
    return std::clamp(value, 0.0f, 179.0f);
}

void ResetLightInspectorUiState(SectorEditorLightEditingServiceContext::UiRefs& uiState)
{
    uiState.inspectorScroll.offset = Vector2{};
    uiState.lightXInput = engine::UIFloatInputState{};
    uiState.lightYInput = engine::UIFloatInputState{};
    uiState.lightZInput = engine::UIFloatInputState{};
    uiState.lightTargetXInput = engine::UIFloatInputState{};
    uiState.lightTargetYInput = engine::UIFloatInputState{};
    uiState.lightTargetZInput = engine::UIFloatInputState{};
    uiState.lightIntensityInput = engine::UIFloatInputState{};
    uiState.lightRadiusInput = engine::UIFloatInputState{};
    uiState.lightInnerConeInput = engine::UIFloatInputState{};
    uiState.lightOuterConeInput = engine::UIFloatInputState{};
    uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    uiState.lightFlickerSpeedInput = engine::UIFloatInputState{};
    uiState.lightFlickerAmountInput = engine::UIFloatInputState{};
    uiState.lightShadowPriorityInput = engine::UIIntInputState{};
    uiState.lightShadowBiasInput = engine::UIFloatInputState{};
    uiState.lightShadowStrengthInput = engine::UIFloatInputState{};
    uiState.lightShadowSoftnessInput = engine::UIFloatInputState{};
    uiState.lightRedInput = engine::UIIntInputState{};
    uiState.lightGreenInput = engine::UIIntInputState{};
    uiState.lightBlueInput = engine::UIIntInputState{};
}

void ClearLightSelection(
        SectorEditorLightEditingServiceContext::SelectionRefs& state,
        SectorEditorLightEditingServiceContext::UiRefs& uiState)
{
    state.manipulationState.selectDragArm = SelectDragArmState{};
    state.manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
    state.runtimeObjectDrag = RuntimeObjectDragState{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyStaticSpotLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologyDynamicSpotLightId = -1;
    state.selectedRuntimeObjectId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    state.selectedAuthoring = SectorAuthoringSelectionTarget{};
    ResetLightInspectorUiState(uiState);
    uiState.inspectorIdUiState.idBufferSectorIndex = -1;
    uiState.inspectorIdUiState.idBufferLightIndex = -1;
    uiState.inspectorIdUiState.selectedSectorIdBuffer[0] = '\0';
    uiState.inspectorIdUiState.selectedLightIdBuffer[0] = '\0';
}

void SelectLight(
        SectorEditorLightEditingServiceContext::SelectionRefs& state,
        SectorEditorLightEditingServiceContext::UiRefs& uiState,
        TopologySelectionKind kind,
        int lightId)
{
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedRuntimeObjectId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    state.selectedAuthoring = SectorAuthoringSelectionTarget{};
    ResetLightInspectorUiState(uiState);

    state.topologySelectionKind = kind;
    state.selectedTopologyLightId = kind == TopologySelectionKind::StaticLight ? lightId : -1;
    state.selectedTopologyStaticSpotLightId = kind == TopologySelectionKind::StaticSpotLight ? lightId : -1;
    state.selectedTopologyDynamicLightId = kind == TopologySelectionKind::DynamicLight ? lightId : -1;
    state.selectedTopologyDynamicSpotLightId = kind == TopologySelectionKind::DynamicSpotLight ? lightId : -1;
    std::snprintf(uiState.inspectorIdUiState.selectedLightIdBuffer, sizeof(uiState.inspectorIdUiState.selectedLightIdBuffer), "%d", lightId);
    uiState.inspectorIdUiState.idBufferLightIndex = lightId;
    uiState.inspectorIdUiState.idBufferSectorIndex = -1;
    uiState.inspectorIdUiState.selectedSectorIdBuffer[0] = '\0';
    uiState.inspectorIdUiState.idEditError.clear();
}

SectorEditorLightMutationResult FinishLightMutationResult(bool changed)
{
    SectorEditorLightMutationResult result;
    result.changed = changed;
    return result;
}

} // namespace

SectorEditorLightEditingService::SectorEditorLightEditingService(
        SectorEditorLightEditingServiceContext context)
    : context_(std::move(context))
{
}

void SectorEditorLightEditingService::MarkEdited(const char* status)
{
    context_.topologyDocumentDirty = true;
    context_.hasUnsavedChanges = true;
    ++context_.topologyRenderRevision;
    context_.topologyRenderCache.valid = false;
    if (status != nullptr && status[0] != '\0') {
        context_.statusText = status;
    }
}

bool SectorEditorLightEditingService::FinishTopologyActionResult(
        const SectorEditorTopologyActionResult& result)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            context_.statusText = result.status;
        }
        return false;
    }

    MarkEdited(result.status.c_str());
    return true;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddStaticLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddStaticLightResult add = AddStaticLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddStaticSpotLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddStaticSpotLightResult add = AddStaticSpotLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddDynamicLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddDynamicLightResult add = AddDynamicLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddDynamicSpotLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddDynamicSpotLightResult add = AddDynamicSpotLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::DeleteSelectedLightConfirmed()
{
    SectorEditorTopologyActionResult deleteResult;
    int lightId = -1;
    TopologySelectionKind kind = context_.selection.topologySelectionKind;

    if (kind == TopologySelectionKind::StaticLight && context_.selection.selectedTopologyLightId >= 0) {
        lightId = context_.selection.selectedTopologyLightId;
        deleteResult = DeleteStaticLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::StaticSpotLight
            && context_.selection.selectedTopologyStaticSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyStaticSpotLightId;
        deleteResult = DeleteStaticSpotLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::DynamicLight
            && context_.selection.selectedTopologyDynamicLightId >= 0) {
        lightId = context_.selection.selectedTopologyDynamicLightId;
        deleteResult = DeleteDynamicLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::DynamicSpotLight
            && context_.selection.selectedTopologyDynamicSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyDynamicSpotLightId;
        deleteResult = DeleteDynamicSpotLight(context_.map, lightId);
    } else {
        return {};
    }

    SectorEditorLightMutationResult result;
    if (!deleteResult.changed) {
        FinishTopologyActionResult(deleteResult);
        return result;
    }

    if ((kind == TopologySelectionKind::StaticLight && context_.selection.selectedTopologyLightId == lightId)
            || (kind == TopologySelectionKind::StaticSpotLight
                    && context_.selection.selectedTopologyStaticSpotLightId == lightId)
            || (kind == TopologySelectionKind::DynamicLight
                    && context_.selection.selectedTopologyDynamicLightId == lightId)
            || (kind == TopologySelectionKind::DynamicSpotLight
                    && context_.selection.selectedTopologyDynamicSpotLightId == lightId)) {
        if (context_.lightState.spotLightPilot.active
                && ((kind == TopologySelectionKind::StaticSpotLight
                             && context_.lightState.spotLightPilot.kind == SpotLightPilotKind::Static)
                        || (kind == TopologySelectionKind::DynamicSpotLight
                                && context_.lightState.spotLightPilot.kind == SpotLightPilotKind::Dynamic))
                && context_.lightState.spotLightPilot.lightId == lightId) {
            result.restoredSpotLightPilot = context_.lightState.spotLightPilot;
            result.previewPoseRestoreNeeded = true;
            context_.lightState.spotLightPilot = SpotLightPilotLightState{};
        }
        ClearLightSelection(context_.selection, context_.ui);
    }

    if (context_.selection.hoveredTopologyLightId == lightId) {
        context_.selection.hoveredTopologyLightId = -1;
    }
    if (context_.selection.hoveredTopologyStaticSpotLightId == lightId) {
        context_.selection.hoveredTopologyStaticSpotLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicLightId == lightId) {
        context_.selection.hoveredTopologyDynamicLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicSpotLightId == lightId) {
        context_.selection.hoveredTopologyDynamicSpotLightId = -1;
    }
    if (context_.lightState.lightDrag.topologyLightId == lightId) {
        context_.lightState.lightDrag = LightDragState{};
    }
    if (context_.lightState.lightEdit.topologyLightId == lightId) {
        context_.lightState.lightEdit = LightEditTransactionState{};
    }

    result.changed = FinishTopologyActionResult(deleteResult);
    return result;
}

bool SectorEditorLightEditingService::BeginLightDrag(
        TopologySelectionKind kind,
        int topologyLightId,
        SpotLightHandle spotHandle)
{
    LightEditTransactionState edit;
    edit.active = true;
    edit.kind = kind;
    edit.topologyLightId = topologyLightId;
    edit.spotHandle = spotHandle;

    if (kind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming static spot %d", light->id)
                : TextFormat("Moving static spot %d", light->id);
    } else if (kind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming dynamic spot %d", light->id)
                : TextFormat("Moving dynamic spot %d", light->id);
    } else if (kind == TopologySelectionKind::DynamicLight) {
        const SectorTopologyDynamicPointLight* light =
                FindSectorTopologyDynamicLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        context_.statusText = TextFormat("Moving dynamic light %d", light->id);
    } else {
        const SectorTopologyStaticPointLight* light =
                FindSectorTopologyStaticLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.kind = TopologySelectionKind::StaticLight;
        edit.originalPosition = light->position;
        context_.statusText = TextFormat("Moving static light %d", light->id);
    }

    context_.lightState.lightEdit = edit;
    return true;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ApplyLightDragToSnappedPosition(
        Vector3 snappedPosition)
{
    const LightEditTransactionState& edit = context_.lightState.lightEdit;
    if (!edit.active) {
        return {};
    }

    if (edit.kind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light =
                FindSectorTopologyDynamicLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        context_.statusText = TextFormat("Moving dynamic light %d", light->id);
        return {};
    }

    if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
        SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        if (edit.spotHandle == SpotLightHandle::Target) {
            light->target.x = snappedPosition.x;
            light->target.y = edit.originalTarget.y;
            light->target.z = snappedPosition.z;
            context_.statusText = TextFormat("Aiming dynamic spot %d", light->id);
            return {};
        }
        const float dx = snappedPosition.x - edit.originalPosition.x;
        const float dz = snappedPosition.z - edit.originalPosition.z;
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        light->target.x = edit.originalTarget.x + dx;
        light->target.y = edit.originalTarget.y;
        light->target.z = edit.originalTarget.z + dz;
        context_.statusText = TextFormat("Moving dynamic spot %d", light->id);
        return {};
    }

    if (edit.kind == TopologySelectionKind::StaticSpotLight) {
        SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        if (edit.spotHandle == SpotLightHandle::Target) {
            light->target.x = snappedPosition.x;
            light->target.y = edit.originalTarget.y;
            light->target.z = snappedPosition.z;
            context_.statusText = TextFormat("Aiming static spot %d", light->id);
            return {};
        }
        const float dx = snappedPosition.x - edit.originalPosition.x;
        const float dz = snappedPosition.z - edit.originalPosition.z;
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        light->target.x = edit.originalTarget.x + dx;
        light->target.y = edit.originalTarget.y;
        light->target.z = edit.originalTarget.z + dz;
        context_.statusText = TextFormat("Moving static spot %d", light->id);
        return {};
    }

    SectorTopologyStaticPointLight* light =
            FindSectorTopologyStaticLight(context_.map, edit.topologyLightId);
    if (light == nullptr) {
        return {};
    }
    light->position.x = snappedPosition.x;
    light->position.y = edit.originalPosition.y;
    light->position.z = snappedPosition.z;
    context_.statusText = TextFormat("Moving static light %d", light->id);
    return {};
}

SectorEditorLightMutationResult SectorEditorLightEditingService::FinishLightDrag()
{
    const LightEditTransactionState edit = context_.lightState.lightEdit;
    if (!edit.active) {
        return {};
    }
    context_.lightState.lightEdit = LightEditTransactionState{};

    if (edit.kind == TopologySelectionKind::DynamicLight) {
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(
                FinishMoveDynamicLight(context_.map, edit.topologyLightId, edit.originalPosition)));
    }

    if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, edit.topologyLightId);
        const bool movedOrigin = std::fabs(light->position.x - edit.originalPosition.x) > GeometryEpsilon
                || std::fabs(light->position.z - edit.originalPosition.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - edit.originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - edit.originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult finish;
        finish.changed = edit.spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        finish.status = finish.changed
                ? TextFormat("Moved dynamic spot %d", edit.topologyLightId)
                : TextFormat("Dynamic spot %d unchanged", edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(finish));
    }

    if (edit.kind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, edit.topologyLightId);
        const bool movedOrigin = std::fabs(light->position.x - edit.originalPosition.x) > GeometryEpsilon
                || std::fabs(light->position.z - edit.originalPosition.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - edit.originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - edit.originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult finish;
        finish.changed = edit.spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        finish.status = finish.changed
                ? TextFormat("Moved static spot %d", edit.topologyLightId)
                : TextFormat("Static spot %d unchanged", edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(finish));
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, edit.topologyLightId);
    return FinishLightMutationResult(FinishTopologyActionResult(
            FinishMoveStaticLight(context_.map, edit.topologyLightId, edit.originalPosition)));
}

SectorEditorLightMutationResult SectorEditorLightEditingService::CancelLightDragData(const char* message)
{
    const LightEditTransactionState edit = context_.lightState.lightEdit;
    if (edit.active) {
        if (edit.kind == TopologySelectionKind::DynamicLight) {
            if (SectorTopologyDynamicPointLight* light =
                        FindSectorTopologyDynamicLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
            if (SectorTopologyDynamicSpotLight* light =
                        FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::StaticSpotLight) {
            if (SectorTopologyStaticSpotLight* light =
                        FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, light->id);
            }
        } else if (SectorTopologyStaticPointLight* light =
                       FindSectorTopologyStaticLight(context_.map, edit.topologyLightId)) {
            light->position = edit.originalPosition;
            SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, light->id);
        }
    }
    context_.lightState.lightEdit = LightEditTransactionState{};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
    return {};
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ApplySpotLightPilot(
        Vector3 position,
        Vector3 target)
{
    const SpotLightPilotLightState pilot = context_.lightState.spotLightPilot;
    if (!pilot.active) {
        return {};
    }

    SectorTopologyStaticSpotLight* staticLight = pilot.kind == SpotLightPilotKind::Static
            ? FindSectorTopologyStaticSpotLight(context_.map, pilot.lightId)
            : nullptr;
    SectorTopologyDynamicSpotLight* dynamicLight = pilot.kind == SpotLightPilotKind::Dynamic
            ? FindSectorTopologyDynamicSpotLight(context_.map, pilot.lightId)
            : nullptr;
    if (staticLight == nullptr && dynamicLight == nullptr) {
        return CancelSpotLightPilotData("Spotlight pilot cancelled: light missing");
    }

    const int lightId = staticLight != nullptr ? staticLight->id : dynamicLight->id;
    if (staticLight != nullptr) {
        staticLight->position = position;
        staticLight->target = target;
    } else {
        dynamicLight->position = position;
        dynamicLight->target = target;
    }
    context_.lightState.spotLightPilot = SpotLightPilotLightState{};
    MarkEdited(staticLight != nullptr
            ? TextFormat("Piloted static spot %d", lightId)
            : TextFormat("Piloted dynamic spot %d", lightId));
    context_.statusText = staticLight != nullptr
            ? TextFormat("Applied static spot %d pilot pose", lightId)
            : TextFormat("Applied dynamic spot %d pilot pose", lightId);

    SectorEditorLightMutationResult result;
    result.changed = true;
    result.dynamicLightRendererRefreshNeeded = dynamicLight != nullptr;
    return result;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::CancelSpotLightPilotData(const char* message)
{
    if (!context_.lightState.spotLightPilot.active) {
        return {};
    }

    SectorEditorLightMutationResult result;
    const SpotLightPilotLightState pilot = context_.lightState.spotLightPilot;
    result.restoredSpotLightPilot = pilot;
    result.previewPoseRestoreNeeded = true;
    context_.lightState.spotLightPilot = SpotLightPilotLightState{};
    if (pilot.kind == SpotLightPilotKind::Static) {
        if (SectorTopologyStaticSpotLight* light =
                    FindSectorTopologyStaticSpotLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition;
            light->target = pilot.originalTarget;
        }
    } else if (SectorTopologyDynamicSpotLight* light =
                       FindSectorTopologyDynamicSpotLight(context_.map, pilot.lightId)) {
        light->position = pilot.originalPosition;
        light->target = pilot.originalTarget;
    }
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
    return result;
}

bool SectorEditorLightEditingService::SetStaticLightPosition(
        SectorTopologyStaticPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightIntensity(
        SectorTopologyStaticPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightRadius(
        SectorTopologyStaticPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.radius);
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightSourceRadius(
        SectorTopologyStaticPointLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.radius);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited("Updated light source radius");
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightColor(
        SectorTopologyStaticPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightPosition(
        SectorTopologyStaticSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightTarget(
        SectorTopologyStaticSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightRange(
        SectorTopologyStaticSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.range);
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightSourceRadius(
        SectorTopologyStaticSpotLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.range);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d source radius", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightInnerCone(
        SectorTopologyStaticSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightOuterCone(
        SectorTopologyStaticSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightIntensity(
        SectorTopologyStaticSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightColor(
        SectorTopologyStaticSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightEnabled(
        SectorTopologyDynamicPointLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d enabled", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlicker(
        SectorTopologyDynamicPointLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerSpeed(
        SectorTopologyDynamicPointLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerAmount(
        SectorTopologyDynamicPointLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightPosition(
        SectorTopologyDynamicPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightIntensity(
        SectorTopologyDynamicPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightRadius(
        SectorTopologyDynamicPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightColor(
        SectorTopologyDynamicPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightEnabled(
        SectorTopologyDynamicSpotLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d enabled", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlicker(
        SectorTopologyDynamicSpotLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerSpeed(
        SectorTopologyDynamicSpotLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerAmount(
        SectorTopologyDynamicSpotLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightCastsShadow(
        SectorTopologyDynamicSpotLight& light,
        bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow request", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowPriority(
        SectorTopologyDynamicSpotLight& light,
        int shadowPriority)
{
    shadowPriority = ClampDynamicSpotLightShadowPriority(shadowPriority);
    if (!SetValue(light.shadowPriority, shadowPriority)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow priority", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowBias(
        SectorTopologyDynamicSpotLight& light,
        float shadowBias)
{
    shadowBias = ClampDynamicSpotLightShadowBias(shadowBias);
    if (!SetValue(light.shadowBias, shadowBias)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow bias", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowStrength(
        SectorTopologyDynamicSpotLight& light,
        float shadowStrength)
{
    shadowStrength = ClampDynamicSpotLightShadowStrength(shadowStrength);
    if (!SetValue(light.shadowStrength, shadowStrength)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow strength", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowSoftness(
        SectorTopologyDynamicSpotLight& light,
        float shadowSoftness)
{
    shadowSoftness = ClampDynamicSpotLightShadowSoftness(shadowSoftness);
    if (!SetValue(light.shadowSoftness, shadowSoftness)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow softness", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightPosition(
        SectorTopologyDynamicSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightTarget(
        SectorTopologyDynamicSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightIntensity(
        SectorTopologyDynamicSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightRange(
        SectorTopologyDynamicSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightInnerCone(
        SectorTopologyDynamicSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightOuterCone(
        SectorTopologyDynamicSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightColor(
        SectorTopologyDynamicSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d color", light.id));
    return true;
}

} // namespace game
