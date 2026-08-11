#include "sector_editor/selection/SectorEditorSelectionService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cstdio>

namespace game {

namespace {

void RequestCancelLightPilotWithPreviewRestore(SectorEditorSelectionServiceContext& context, const char* message)
{
    if (context.requestCancelLightPilotWithPreviewRestore != nullptr) {
        context.requestCancelLightPilotWithPreviewRestore(context.userData, message);
    }
}

void SetStatusText(SectorEditorSelectionServiceContext& context, const std::string& status)
{
    if (context.statusText != nullptr) {
        *context.statusText = status;
    }
}

void ResetRuntimeObjectUiState(SectorEditorSelectionUiDependencies& ui)
{
    ui.runtimeObjectXInput = engine::UIFloatInputState{};
    ui.runtimeObjectYInput = engine::UIFloatInputState{};
    ui.runtimeObjectZInput = engine::UIFloatInputState{};
    ui.runtimeObjectRotationXInput = engine::UIFloatInputState{};
    ui.runtimeObjectYawInput = engine::UIFloatInputState{};
    ui.runtimeObjectRotationZInput = engine::UIFloatInputState{};
    ui.runtimeObjectHeightOffsetInput = engine::UIFloatInputState{};
    ui.runtimeObjectScaleInput = engine::UIFloatInputState{};
    ui.runtimeObjectAnimationSpeedInput = engine::UIFloatInputState{};
    ui.runtimeObjectWidthInput = engine::UIFloatInputState{};
    ui.runtimeObjectHeightInput = engine::UIFloatInputState{};
    ui.runtimeObjectThicknessInput = engine::UIFloatInputState{};
    ui.runtimeObjectNormalOffsetInput = engine::UIFloatInputState{};
    ui.runtimeObjectOpenDistanceInput = engine::UIFloatInputState{};
    ui.runtimeObjectSpeedInput = engine::UIFloatInputState{};
    ui.runtimeObjectInitialOpenFractionInput = engine::UIFloatInputState{};
    ui.runtimeObjectAutoOpenDistanceInput = engine::UIFloatInputState{};
    ui.runtimeObjectInteractionDistanceInput = engine::UIFloatInputState{};
    ui.runtimeObjectOriginXInput = engine::UIFloatInputState{};
    ui.runtimeObjectOriginYInput = engine::UIFloatInputState{};
}

} // namespace

SectorTopologySector* SelectedSectorEditorTopologySector(SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(context.topologyMap, context.selectionState.selectedTopologySectorId);
}

const SectorTopologySector* SelectedSectorEditorTopologySector(const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(context.topologyMap, context.selectionState.selectedTopologySectorId);
}

SectorTopologyVertex* SelectedSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(context.topologyMap, context.selectionState.selectedTopologyVertexId);
}

const SectorTopologyVertex* SelectedSectorEditorTopologyVertex(const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(context.topologyMap, context.selectionState.selectedTopologyVertexId);
}

SectorTopologySideDef* SelectedSectorEditorTopologySideDef(SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(context.topologyMap, context.selectionState.selectedTopologySideDefId);
}

const SectorTopologySideDef* SelectedSectorEditorTopologySideDef(const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(context.topologyMap, context.selectionState.selectedTopologySideDefId);
}

SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::LineDef
            && context.selectionState.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(context.topologyMap, context.selectionState.selectedTopologyLineDefId);
}

const SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::LineDef
            && context.selectionState.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(context.topologyMap, context.selectionState.selectedTopologyLineDefId);
}

SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(context.topologyMap, context.selectionState.selectedTopologyLightId);
}

const SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(context.topologyMap, context.selectionState.selectedTopologyLightId);
}

SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::StaticSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticSpotLight(
            context.topologyMap,
            context.selectionState.selectedTopologyStaticSpotLightId);
}

const SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::StaticSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticSpotLight(
            context.topologyMap,
            context.selectionState.selectedTopologyStaticSpotLightId);
}

SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(context.topologyMap, context.selectionState.selectedTopologyDynamicLightId);
}

const SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(context.topologyMap, context.selectionState.selectedTopologyDynamicLightId);
}

SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::DynamicSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicSpotLight(
            context.topologyMap,
            context.selectionState.selectedTopologyDynamicSpotLightId);
}

const SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.selectionState.topologySelectionKind != TopologySelectionKind::DynamicSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicSpotLight(
            context.topologyMap,
            context.selectionState.selectedTopologyDynamicSpotLightId);
}

SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context)
{
    return FindSectorPlacedRuntimeObject(context.topologyMap, context.selectionState.selectedRuntimeObjectId);
}

const SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(const SectorEditorSelectionServiceContext& context)
{
    return FindSectorPlacedRuntimeObject(context.topologyMap, context.selectionState.selectedRuntimeObjectId);
}

void ClearStaleSectorEditorTopologySelection(SectorEditorSelectionServiceContext& context)
{
    SelectionState& selection = context.selectionState;
    bool stale = false;
    if (selection.topologySelectionKind == TopologySelectionKind::Sector) {
        stale = selection.selectedTopologySectorId < 0
                || FindSectorTopologySector(context.topologyMap, selection.selectedTopologySectorId) == nullptr;
    } else if (selection.topologySelectionKind == TopologySelectionKind::Vertex) {
        stale = selection.selectedTopologyVertexId < 0
                || FindSectorTopologyVertex(context.topologyMap, selection.selectedTopologyVertexId) == nullptr;
    } else if (selection.topologySelectionKind == TopologySelectionKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                context.topologyMap,
                selection.selectedTopologySideDefId);
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                context.topologyMap,
                selection.selectedTopologyLineDefId);
        stale = sideDef == nullptr
                || lineDef == nullptr
                || sideDef->lineDefId != lineDef->id;
        if (!stale) {
            selection.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                    context.topologyMap,
                    sideDef,
                    selection.selectedTopologyWallPart);
        }
    } else if (selection.topologySelectionKind == TopologySelectionKind::LineDef) {
        stale = selection.selectedTopologyLineDefId < 0
                || FindSectorTopologyLineDef(context.topologyMap, selection.selectedTopologyLineDefId) == nullptr;
        if (!stale && selection.selectedTopologyWallPart == TopologyWallPart::Middle) {
            selection.selectedTopologyWallPart = TopologyWallPart::Wall;
        }
    } else if (selection.topologySelectionKind == TopologySelectionKind::StaticLight) {
        stale = selection.selectedTopologyLightId < 0
                || FindSectorTopologyStaticLight(context.topologyMap, selection.selectedTopologyLightId) == nullptr;
    } else if (selection.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
        stale = selection.selectedTopologyStaticSpotLightId < 0
                || FindSectorTopologyStaticSpotLight(
                        context.topologyMap,
                        selection.selectedTopologyStaticSpotLightId) == nullptr;
    } else if (selection.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        stale = selection.selectedTopologyDynamicLightId < 0
                || FindSectorTopologyDynamicLight(
                        context.topologyMap,
                        selection.selectedTopologyDynamicLightId) == nullptr;
    } else if (selection.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
        stale = selection.selectedTopologyDynamicSpotLightId < 0
                || FindSectorTopologyDynamicSpotLight(
                        context.topologyMap,
                        selection.selectedTopologyDynamicSpotLightId) == nullptr;
    }
    if (selection.selectedRuntimeObjectId >= 0
            && FindSectorPlacedRuntimeObject(context.topologyMap, selection.selectedRuntimeObjectId) == nullptr) {
        selection.selectedRuntimeObjectId = -1;
    }

    if (stale) {
        RequestCancelLightPilotWithPreviewRestore(context, nullptr);
        selection.topologySelectionKind = TopologySelectionKind::None;
        selection.selectedTopologySectorId = -1;
        selection.selectedTopologyVertexId = -1;
        selection.selectedTopologySideDefId = -1;
        selection.selectedTopologyLineDefId = -1;
        selection.selectedTopologyLightId = -1;
        selection.selectedTopologyStaticSpotLightId = -1;
        selection.selectedTopologyDynamicLightId = -1;
        selection.selectedTopologyDynamicSpotLightId = -1;
        selection.selectedTopologySideKind = SectorTopologySideKind::Front;
        context.ui.inspectorIdUiState.idBufferSectorIndex = -1;
        context.ui.inspectorIdUiState.idBufferLightIndex = -1;
        SyncSectorEditorSelectedSectorIdBuffer(context);
        SyncSectorEditorSelectedLightIdBuffer(context);
    }
}

void SyncSectorEditorSelectedSectorIdBuffer(SectorEditorSelectionServiceContext& context)
{
    const SectorTopologySector* sector = SelectedSectorEditorTopologySector(context);
    if (sector == nullptr) {
        context.ui.inspectorIdUiState.selectedSectorIdBuffer[0] = '\0';
        context.ui.inspectorIdUiState.idBufferSectorIndex = -1;
        context.ui.inspectorIdUiState.idEditError.clear();
        return;
    }

    if (context.ui.inspectorIdUiState.idBufferSectorIndex == context.selectionState.selectedTopologySectorId) {
        return;
    }

    std::snprintf(
            context.ui.inspectorIdUiState.selectedSectorIdBuffer,
            static_cast<int>(sizeof(context.ui.inspectorIdUiState.selectedSectorIdBuffer)),
            "%s",
            sector->name.c_str());
    context.ui.inspectorIdUiState.idBufferSectorIndex = context.selectionState.selectedTopologySectorId;
    context.ui.inspectorIdUiState.idEditError.clear();
}

void SyncSectorEditorSelectedLightIdBuffer(SectorEditorSelectionServiceContext& context)
{
    const SectorTopologyStaticPointLight* light = SelectedSectorEditorTopologyLight(context);
    const SectorTopologyStaticSpotLight* staticSpotLight = SelectedSectorEditorTopologyStaticSpotLight(context);
    const SectorTopologyDynamicPointLight* dynamicLight = SelectedSectorEditorTopologyDynamicLight(context);
    const SectorTopologyDynamicSpotLight* dynamicSpotLight = SelectedSectorEditorTopologyDynamicSpotLight(context);
    if (light == nullptr && staticSpotLight == nullptr && dynamicLight == nullptr && dynamicSpotLight == nullptr) {
        context.ui.inspectorIdUiState.selectedLightIdBuffer[0] = '\0';
        context.ui.inspectorIdUiState.idBufferLightIndex = -1;
        if (context.selectionState.topologySelectionKind == TopologySelectionKind::None) {
            context.ui.inspectorIdUiState.idEditError.clear();
        }
        return;
    }

    const int lightId = light != nullptr
            ? light->id
            : (staticSpotLight != nullptr
                    ? staticSpotLight->id
                    : (dynamicLight != nullptr ? dynamicLight->id : dynamicSpotLight->id));
    if (context.ui.inspectorIdUiState.idBufferLightIndex == lightId) {
        return;
    }

    std::snprintf(context.ui.inspectorIdUiState.selectedLightIdBuffer, static_cast<int>(sizeof(context.ui.inspectorIdUiState.selectedLightIdBuffer)), "%d", lightId);
    context.ui.inspectorIdUiState.idBufferLightIndex = lightId;
    context.ui.inspectorIdUiState.idEditError.clear();
}

void ResetSectorEditorSurface3DUiState(SectorEditorSelectionServiceContext& context)
{
    context.materialUiState.surface3DUvScaleUInput = engine::UIFloatInputState{};
    context.materialUiState.surface3DUvScaleVInput = engine::UIFloatInputState{};
    context.materialUiState.surface3DUvOffsetUInput = engine::UIFloatInputState{};
    context.materialUiState.surface3DUvOffsetVInput = engine::UIFloatInputState{};
    context.materialUiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
    context.materialUiState.surface3DDecalEmissiveStrengthInput = engine::UIFloatInputState{};
}

void ClearSectorEditorTopologySelectionOnly(SectorEditorSelectionServiceContext& context)
{
    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.manipulationState.selectDragArm = SelectDragArmState{};
    if (context.lightState != nullptr) {
        context.lightState->lightDrag = LightDragState{};
        context.lightState->lightEdit = LightEditTransactionState{};
    }
    context.runtimeObjectDrag = RuntimeObjectDragState{};
    context.selectionState.topologySelectionKind = TopologySelectionKind::None;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSectorEditorSurface3DUiState(context);
    ResetRuntimeObjectUiState(context.ui);
    context.ui.inspectorIdUiState.idBufferSectorIndex = -1;
    context.ui.inspectorIdUiState.idBufferLightIndex = -1;
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void ClearSectorEditorSelection(SectorEditorSelectionServiceContext& context)
{
    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.manipulationState.selectDragArm = SelectDragArmState{};
    context.manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
    context.selectionState.topologySelectionKind = TopologySelectionKind::None;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.runtimeObjectDrag = RuntimeObjectDragState{};
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.ambientIntensityInput = engine::UIFloatInputState{};
    context.ui.ambientRedInput = engine::UIIntInputState{};
    context.ui.ambientGreenInput = engine::UIIntInputState{};
    context.ui.ambientBlueInput = engine::UIIntInputState{};
    ResetRuntimeObjectUiState(context.ui);
    context.ui.inspectorScroll.offset = Vector2{};
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologySector(SectorEditorSelectionServiceContext& context, int sectorId)
{
    if (FindSectorTopologySector(context.topologyMap, sectorId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.selectionState.topologySelectionKind = TopologySelectionKind::Sector;
    context.selectionState.selectedTopologySectorId = sectorId;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.runtimeObjectDrag = RuntimeObjectDragState{};
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorIdUiState.idBufferLightIndex = -1;
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.floorInput = engine::UIFloatInputState{};
    context.ui.ceilingInput = engine::UIFloatInputState{};
    context.ui.ambientIntensityInput = engine::UIFloatInputState{};
    context.ui.ambientRedInput = engine::UIIntInputState{};
    context.ui.ambientGreenInput = engine::UIIntInputState{};
    context.ui.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : context.materialUiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context, int vertexId)
{
    const SectorTopologyVertex* vertex = FindSectorTopologyVertex(context.topologyMap, vertexId);
    if (vertex == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.selectionState.topologySelectionKind = TopologySelectionKind::Vertex;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = vertex->id;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = vertex->id;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorIdUiState.idBufferLightIndex = -1;
    context.ui.inspectorScroll.offset = Vector2{};
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologySideDef(
        SectorEditorSelectionServiceContext& context,
        int sideDefId,
        TopologyWallPart wallPart)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(context.topologyMap, sideDefId);
    if (sideDef == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(context.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.selectionState.topologySelectionKind = TopologySelectionKind::SideDef;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = sideDef->id;
    context.selectionState.selectedTopologyLineDefId = lineDef->id;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = sideDef->side;
    context.selectionState.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
            context.topologyMap,
            sideDef,
            wallPart);
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorIdUiState.idBufferLightIndex = -1;
    context.ui.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : context.materialUiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologyLineDef(
        SectorEditorSelectionServiceContext& context,
        int lineDefId,
        SectorTopologySideKind side,
        TopologyWallPart wallPart)
{
    if (FindSectorTopologyLineDef(context.topologyMap, lineDefId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    context.selectionState.topologySelectionKind = TopologySelectionKind::LineDef;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = lineDefId;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = side;
    context.selectionState.selectedTopologyWallPart = wallPart == TopologyWallPart::Middle
            ? TopologyWallPart::Wall
            : wallPart;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorIdUiState.idBufferLightIndex = -1;
    context.ui.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : context.materialUiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyStaticLight(context.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.lightState != nullptr
            && context.lightState->lightPilot.active
            && (context.lightState->lightPilot.kind != LightPilotKind::StaticPoint
                    || context.lightState->lightPilot.lightId != topologyLightId)) {
        RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    }
    context.selectionState.selectedTopologyLightId = topologyLightId;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.topologySelectionKind = TopologySelectionKind::StaticLight;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.lightXInput = engine::UIFloatInputState{};
    context.ui.lightYInput = engine::UIFloatInputState{};
    context.ui.lightZInput = engine::UIFloatInputState{};
    context.ui.lightTargetXInput = engine::UIFloatInputState{};
    context.ui.lightTargetYInput = engine::UIFloatInputState{};
    context.ui.lightTargetZInput = engine::UIFloatInputState{};
    context.ui.lightIntensityInput = engine::UIFloatInputState{};
    context.ui.lightRadiusInput = engine::UIFloatInputState{};
    context.ui.lightInnerConeInput = engine::UIFloatInputState{};
    context.ui.lightOuterConeInput = engine::UIFloatInputState{};
    context.ui.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.ui.lightRedInput = engine::UIIntInputState{};
    context.ui.lightGreenInput = engine::UIIntInputState{};
    context.ui.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyStaticSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyStaticSpotLight(context.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.lightState != nullptr
            && context.lightState->lightPilot.active
            && (context.lightState->lightPilot.kind != LightPilotKind::StaticSpot
                    || context.lightState->lightPilot.lightId != topologyLightId)) {
        RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    }
    context.selectionState.selectedTopologyStaticSpotLightId = topologyLightId;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.topologySelectionKind = TopologySelectionKind::StaticSpotLight;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.lightXInput = engine::UIFloatInputState{};
    context.ui.lightYInput = engine::UIFloatInputState{};
    context.ui.lightZInput = engine::UIFloatInputState{};
    context.ui.lightTargetXInput = engine::UIFloatInputState{};
    context.ui.lightTargetYInput = engine::UIFloatInputState{};
    context.ui.lightTargetZInput = engine::UIFloatInputState{};
    context.ui.lightIntensityInput = engine::UIFloatInputState{};
    context.ui.lightRadiusInput = engine::UIFloatInputState{};
    context.ui.lightInnerConeInput = engine::UIFloatInputState{};
    context.ui.lightOuterConeInput = engine::UIFloatInputState{};
    context.ui.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.ui.lightRedInput = engine::UIIntInputState{};
    context.ui.lightGreenInput = engine::UIIntInputState{};
    context.ui.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyDynamicLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyDynamicLight(context.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.lightState != nullptr
            && context.lightState->lightPilot.active
            && (context.lightState->lightPilot.kind != LightPilotKind::DynamicPoint
                    || context.lightState->lightPilot.lightId != topologyLightId)) {
        RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    }
    context.selectionState.selectedTopologyDynamicLightId = topologyLightId;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicSpotLightId = -1;
    context.selectionState.topologySelectionKind = TopologySelectionKind::DynamicLight;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.lightXInput = engine::UIFloatInputState{};
    context.ui.lightYInput = engine::UIFloatInputState{};
    context.ui.lightZInput = engine::UIFloatInputState{};
    context.ui.lightTargetXInput = engine::UIFloatInputState{};
    context.ui.lightTargetYInput = engine::UIFloatInputState{};
    context.ui.lightTargetZInput = engine::UIFloatInputState{};
    context.ui.lightIntensityInput = engine::UIFloatInputState{};
    context.ui.lightRadiusInput = engine::UIFloatInputState{};
    context.ui.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.ui.lightInnerConeInput = engine::UIFloatInputState{};
    context.ui.lightOuterConeInput = engine::UIFloatInputState{};
    context.ui.lightRedInput = engine::UIIntInputState{};
    context.ui.lightGreenInput = engine::UIIntInputState{};
    context.ui.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyDynamicSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyDynamicSpotLight(context.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.lightState != nullptr
            && context.lightState->lightPilot.active
            && (context.lightState->lightPilot.kind != LightPilotKind::DynamicSpot
                    || context.lightState->lightPilot.lightId != topologyLightId)) {
        RequestCancelLightPilotWithPreviewRestore(context, nullptr);
    }
    context.selectionState.selectedTopologyDynamicSpotLightId = topologyLightId;
    context.selectionState.selectedTopologyLightId = -1;
    context.selectionState.selectedTopologyStaticSpotLightId = -1;
    context.selectionState.selectedTopologyDynamicLightId = -1;
    context.selectionState.topologySelectionKind = TopologySelectionKind::DynamicSpotLight;
    context.selectionState.selectedTopologySectorId = -1;
    context.selectionState.selectedTopologyVertexId = -1;
    context.selectionState.selectedTopologySideDefId = -1;
    context.selectionState.selectedTopologyLineDefId = -1;
    context.selectionState.selectedRuntimeObjectId = -1;
    context.selectionState.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.selectionState.inspectedTopologyVertexId = -1;
    context.selectedSurface3D = SectorSurfaceRef{};
    context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.selectionState);
    ResetSectorEditorSurface3DUiState(context);
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.lightXInput = engine::UIFloatInputState{};
    context.ui.lightYInput = engine::UIFloatInputState{};
    context.ui.lightZInput = engine::UIFloatInputState{};
    context.ui.lightTargetXInput = engine::UIFloatInputState{};
    context.ui.lightTargetYInput = engine::UIFloatInputState{};
    context.ui.lightTargetZInput = engine::UIFloatInputState{};
    context.ui.lightIntensityInput = engine::UIFloatInputState{};
    context.ui.lightRadiusInput = engine::UIFloatInputState{};
    context.ui.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.ui.lightInnerConeInput = engine::UIFloatInputState{};
    context.ui.lightOuterConeInput = engine::UIFloatInputState{};
    context.ui.lightFlickerSpeedInput = engine::UIFloatInputState{};
    context.ui.lightFlickerAmountInput = engine::UIFloatInputState{};
    context.ui.lightRedInput = engine::UIIntInputState{};
    context.ui.lightGreenInput = engine::UIIntInputState{};
    context.ui.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context, int objectId)
{
    if (FindSectorPlacedRuntimeObject(context.topologyMap, objectId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    context.selectionState.selectedRuntimeObjectId = objectId;
    ResetSectorEditorSurface3DUiState(context);
    ResetRuntimeObjectUiState(context.ui);
    context.ui.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringLineTarget(SectorEditorSelectionServiceContext& context, int lineId)
{
    if (FindSectorAuthoringLine(context.authoringGraph, lineId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringLine(context.authoringGraph, context.selectionState, lineId);
    context.ui.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringVertexTarget(SectorEditorSelectionServiceContext& context, int vertexId)
{
    if (FindSectorAuthoringVertex(context.authoringGraph, vertexId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringVertex(context.authoringGraph, context.selectionState, vertexId);
    context.ui.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringFaceAnchorTarget(SectorEditorSelectionServiceContext& context, int faceAnchorId)
{
    if (FindSectorAuthoringFaceAnchor(context.authoringGraph, faceAnchorId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringFaceAnchor(context.authoringGraph, context.selectionState, faceAnchorId);
    context.ui.inspectorScroll.offset = Vector2{};
    context.ui.floorInput = engine::UIFloatInputState{};
    context.ui.ceilingInput = engine::UIFloatInputState{};
    context.ui.ambientIntensityInput = engine::UIFloatInputState{};
    context.ui.ambientRedInput = engine::UIIntInputState{};
    context.ui.ambientGreenInput = engine::UIIntInputState{};
    context.ui.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : context.materialUiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
}

void SelectSectorEditorAuthoringFogVolumeTarget(
        SectorEditorSelectionServiceContext& context,
        int fogVolumeId)
{
    if (FindSectorAuthoringFogVolume(context.authoringGraph, fogVolumeId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }
    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringFogVolume(context.authoringGraph, context.selectionState, fogVolumeId);
    context.ui.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringLevelMarkerTarget(
        SectorEditorSelectionServiceContext& context,
        int levelMarkerId)
{
    if (FindSectorAuthoringLevelMarker(context.authoringGraph, levelMarkerId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }
    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringLevelMarker(
            context.authoringGraph,
            context.selectionState,
            levelMarkerId);
    context.ui.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorSurface3D(SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface)
{
    const TopologySurfaceEditTarget target = SectorEditorTopologyEditTargetForSurface(surface);
    if (!IsValidSectorEditorSurfaceRef(context, surface)
            || !IsValidSectorEditorTopologySurfaceEditTarget(context, target)) {
        context.selectedSurface3D = SectorSurfaceRef{};
        context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    SectorEditorAuthoringSurfaceTarget authoringTarget;
    const bool hasAuthoringGraph = HasAuthoringGraphData(context.authoringGraph);
    if (hasAuthoringGraph) {
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    context.topologyMap,
                    context.authoringGraph,
                    context.authoringDerivation,
                    context.authoringDerivationCurrent,
                    surface,
                    authoringTarget,
                    &unavailableStatus)) {
            context.selectedSurface3D = SectorSurfaceRef{};
            context.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            SetStatusText(context, unavailableStatus);
            return;
        }
    }

    if (!SameSectorEditorSurfaceRef(context.selectedSurface3D, surface)) {
        ResetSectorEditorSurface3DUiState(context);
    }

    if (IsWallSurface(surface.kind)) {
        SelectSectorEditorTopologySideDef(
                context,
                surface.topologySideDefId,
                SurfaceKindToTopologyWallPart(surface.kind));
    } else {
        SelectSectorEditorTopologySector(context, surface.topologySectorId);
    }
    if (hasAuthoringGraph) {
        const SectorAuthoringSelectionTarget authoringSelection =
                MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(authoringTarget);
        if (authoringSelection.kind == SectorAuthoringSelectionKind::Line) {
            SelectSectorEditorAuthoringLine(context.authoringGraph, context.selectionState, authoringSelection.lineId);
        } else if (authoringSelection.kind == SectorAuthoringSelectionKind::FaceAnchor) {
            SelectSectorEditorAuthoringFaceAnchor(context.authoringGraph, context.selectionState, authoringSelection.faceAnchorId);
        }
    }
    context.selectedSurface3D = surface;
    context.selectedTopologySurface3D = target;
}

bool IsValidSectorEditorSurfaceRef(const SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface)
{
    if (surface.kind == SectorSurfaceKind::None) {
        return false;
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                context.topologyMap,
                surface.topologySideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != surface.topologyLineDefId
                || sideDef->side != surface.topologySide) {
            return false;
        }
        return surface.kind != SectorSurfaceKind::Middle
                || IsTopologyMiddleEligible(context.topologyMap, sideDef);
    }
    return FindSectorTopologySector(context.topologyMap, surface.topologySectorId) != nullptr;
}

bool SameSectorEditorSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b)
{
    return a.kind == b.kind
            && a.topologySectorId == b.topologySectorId
            && a.topologyLineDefId == b.topologyLineDefId
            && a.topologySideDefId == b.topologySideDefId
            && a.topologySide == b.topologySide;
}

TopologySurfaceEditTarget SectorEditorTopologyEditTargetForSurface(SectorSurfaceRef surface)
{
    TopologySurfaceEditTarget target;
    target.kind = SurfaceKindToTopologyEditTargetKind(surface.kind);
    target.sectorId = surface.topologySectorId;
    target.lineDefId = surface.topologyLineDefId;
    target.sideDefId = surface.topologySideDefId;
    target.side = surface.topologySide;
    return target;
}

bool IsValidSectorEditorTopologySurfaceEditTarget(
        const SectorEditorSelectionServiceContext& context,
        TopologySurfaceEditTarget target)
{
    return game::IsValidMaterialSurfaceTarget(context.topologyMap, target);
}

} // namespace game
