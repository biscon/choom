#include "sector_editor/selection/SectorEditorSelectionService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cstdio>

namespace game {

namespace {

void CancelSpotLightPilot(SectorEditorSelectionServiceContext& context, const char* message)
{
    if (context.cancelSpotLightPilot != nullptr) {
        context.cancelSpotLightPilot(context.userData, message);
    }
}

void SetStatusText(SectorEditorSelectionServiceContext& context, const std::string& status)
{
    if (context.statusText != nullptr) {
        *context.statusText = status;
    }
}

void ResetRuntimeObjectUiState(SectorEditorUiState& uiState)
{
    uiState.runtimeObjectXInput = engine::UIFloatInputState{};
    uiState.runtimeObjectYInput = engine::UIFloatInputState{};
    uiState.runtimeObjectZInput = engine::UIFloatInputState{};
    uiState.runtimeObjectYawInput = engine::UIFloatInputState{};
    uiState.runtimeObjectWidthInput = engine::UIFloatInputState{};
    uiState.runtimeObjectHeightInput = engine::UIFloatInputState{};
    uiState.runtimeObjectThicknessInput = engine::UIFloatInputState{};
    uiState.runtimeObjectNormalOffsetInput = engine::UIFloatInputState{};
    uiState.runtimeObjectOpenDistanceInput = engine::UIFloatInputState{};
    uiState.runtimeObjectSpeedInput = engine::UIFloatInputState{};
    uiState.runtimeObjectInitialOpenFractionInput = engine::UIFloatInputState{};
    uiState.runtimeObjectAutoOpenDistanceInput = engine::UIFloatInputState{};
    uiState.runtimeObjectInteractionDistanceInput = engine::UIFloatInputState{};
    uiState.runtimeObjectOriginXInput = engine::UIFloatInputState{};
    uiState.runtimeObjectOriginYInput = engine::UIFloatInputState{};
}

} // namespace

SectorTopologySector* SelectedSectorEditorTopologySector(SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(context.state.topologyMap, context.state.selectedTopologySectorId);
}

const SectorTopologySector* SelectedSectorEditorTopologySector(const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(context.state.topologyMap, context.state.selectedTopologySectorId);
}

SectorTopologyVertex* SelectedSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(context.state.topologyMap, context.state.selectedTopologyVertexId);
}

const SectorTopologyVertex* SelectedSectorEditorTopologyVertex(const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(context.state.topologyMap, context.state.selectedTopologyVertexId);
}

SectorTopologySideDef* SelectedSectorEditorTopologySideDef(SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(context.state.topologyMap, context.state.selectedTopologySideDefId);
}

const SectorTopologySideDef* SelectedSectorEditorTopologySideDef(const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(context.state.topologyMap, context.state.selectedTopologySideDefId);
}

SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::LineDef
            && context.state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(context.state.topologyMap, context.state.selectedTopologyLineDefId);
}

const SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::LineDef
            && context.state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(context.state.topologyMap, context.state.selectedTopologyLineDefId);
}

SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(context.state.topologyMap, context.state.selectedTopologyLightId);
}

const SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(context.state.topologyMap, context.state.selectedTopologyLightId);
}

SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::StaticSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticSpotLight(
            context.state.topologyMap,
            context.state.selectedTopologyStaticSpotLightId);
}

const SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::StaticSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticSpotLight(
            context.state.topologyMap,
            context.state.selectedTopologyStaticSpotLightId);
}

SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(context.state.topologyMap, context.state.selectedTopologyDynamicLightId);
}

const SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(context.state.topologyMap, context.state.selectedTopologyDynamicLightId);
}

SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::DynamicSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicSpotLight(
            context.state.topologyMap,
            context.state.selectedTopologyDynamicSpotLightId);
}

const SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        const SectorEditorSelectionServiceContext& context)
{
    if (context.state.topologySelectionKind != TopologySelectionKind::DynamicSpotLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicSpotLight(
            context.state.topologyMap,
            context.state.selectedTopologyDynamicSpotLightId);
}

SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context)
{
    return FindSectorPlacedRuntimeObject(context.state.topologyMap, context.state.selectedRuntimeObjectId);
}

const SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(const SectorEditorSelectionServiceContext& context)
{
    return FindSectorPlacedRuntimeObject(context.state.topologyMap, context.state.selectedRuntimeObjectId);
}

void ClearStaleSectorEditorTopologySelection(SectorEditorSelectionServiceContext& context)
{
    SectorEditorState& state = context.state;
    bool stale = false;
    if (state.topologySelectionKind == TopologySelectionKind::Sector) {
        stale = state.selectedTopologySectorId < 0
                || FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex) {
        stale = state.selectedTopologyVertexId < 0
                || FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                state.selectedTopologySideDefId);
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                state.topologyMap,
                state.selectedTopologyLineDefId);
        stale = sideDef == nullptr
                || lineDef == nullptr
                || sideDef->lineDefId != lineDef->id;
        if (!stale) {
            state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                    state.topologyMap,
                    sideDef,
                    state.selectedTopologyWallPart);
        }
    } else if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        stale = state.selectedTopologyLineDefId < 0
                || FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId) == nullptr;
        if (!stale && state.selectedTopologyWallPart == TopologyWallPart::Middle) {
            state.selectedTopologyWallPart = TopologyWallPart::Wall;
        }
    } else if (state.topologySelectionKind == TopologySelectionKind::StaticLight) {
        stale = state.selectedTopologyLightId < 0
                || FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
        stale = state.selectedTopologyStaticSpotLightId < 0
                || FindSectorTopologyStaticSpotLight(
                        state.topologyMap,
                        state.selectedTopologyStaticSpotLightId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        stale = state.selectedTopologyDynamicLightId < 0
                || FindSectorTopologyDynamicLight(state.topologyMap, state.selectedTopologyDynamicLightId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
        stale = state.selectedTopologyDynamicSpotLightId < 0
                || FindSectorTopologyDynamicSpotLight(
                        state.topologyMap,
                        state.selectedTopologyDynamicSpotLightId) == nullptr;
    }
    if (state.selectedRuntimeObjectId >= 0
            && FindSectorPlacedRuntimeObject(state.topologyMap, state.selectedRuntimeObjectId) == nullptr) {
        state.selectedRuntimeObjectId = -1;
    }

    if (stale) {
        CancelSpotLightPilot(context, nullptr);
        state.topologySelectionKind = TopologySelectionKind::None;
        state.selectedTopologySectorId = -1;
        state.selectedTopologyVertexId = -1;
        state.selectedTopologySideDefId = -1;
        state.selectedTopologyLineDefId = -1;
        state.selectedTopologyLightId = -1;
        state.selectedTopologyStaticSpotLightId = -1;
        state.selectedTopologyDynamicLightId = -1;
        state.selectedTopologyDynamicSpotLightId = -1;
        state.selectedTopologySideKind = SectorTopologySideKind::Front;
        context.uiState.idBufferSectorIndex = -1;
        context.uiState.idBufferLightIndex = -1;
        SyncSectorEditorSelectedSectorIdBuffer(context);
        SyncSectorEditorSelectedLightIdBuffer(context);
    }
}

void SyncSectorEditorSelectedSectorIdBuffer(SectorEditorSelectionServiceContext& context)
{
    const SectorTopologySector* sector = SelectedSectorEditorTopologySector(context);
    if (sector == nullptr) {
        context.uiState.selectedSectorIdBuffer[0] = '\0';
        context.uiState.idBufferSectorIndex = -1;
        context.uiState.idEditError.clear();
        return;
    }

    if (context.uiState.idBufferSectorIndex == context.state.selectedTopologySectorId) {
        return;
    }

    std::snprintf(
            context.uiState.selectedSectorIdBuffer,
            sizeof(context.uiState.selectedSectorIdBuffer),
            "%s",
            sector->name.c_str());
    context.uiState.idBufferSectorIndex = context.state.selectedTopologySectorId;
    context.uiState.idEditError.clear();
}

void SyncSectorEditorSelectedLightIdBuffer(SectorEditorSelectionServiceContext& context)
{
    const SectorTopologyStaticPointLight* light = SelectedSectorEditorTopologyLight(context);
    const SectorTopologyStaticSpotLight* staticSpotLight = SelectedSectorEditorTopologyStaticSpotLight(context);
    const SectorTopologyDynamicPointLight* dynamicLight = SelectedSectorEditorTopologyDynamicLight(context);
    const SectorTopologyDynamicSpotLight* dynamicSpotLight = SelectedSectorEditorTopologyDynamicSpotLight(context);
    if (light == nullptr && staticSpotLight == nullptr && dynamicLight == nullptr && dynamicSpotLight == nullptr) {
        context.uiState.selectedLightIdBuffer[0] = '\0';
        context.uiState.idBufferLightIndex = -1;
        if (context.state.topologySelectionKind == TopologySelectionKind::None) {
            context.uiState.idEditError.clear();
        }
        return;
    }

    const int lightId = light != nullptr
            ? light->id
            : (staticSpotLight != nullptr
                    ? staticSpotLight->id
                    : (dynamicLight != nullptr ? dynamicLight->id : dynamicSpotLight->id));
    if (context.uiState.idBufferLightIndex == lightId) {
        return;
    }

    std::snprintf(context.uiState.selectedLightIdBuffer, sizeof(context.uiState.selectedLightIdBuffer), "%d", lightId);
    context.uiState.idBufferLightIndex = lightId;
    context.uiState.idEditError.clear();
}

void ResetSectorEditorSurface3DUiState(SectorEditorSelectionServiceContext& context)
{
    context.uiState.surface3DUvScaleUInput = engine::UIFloatInputState{};
    context.uiState.surface3DUvScaleVInput = engine::UIFloatInputState{};
    context.uiState.surface3DUvOffsetUInput = engine::UIFloatInputState{};
    context.uiState.surface3DUvOffsetVInput = engine::UIFloatInputState{};
    context.uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
    context.uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
}

void ClearSectorEditorTopologySelectionOnly(SectorEditorSelectionServiceContext& context)
{
    CancelSpotLightPilot(context, nullptr);
    context.state.selectDragArm = SelectDragArmState{};
    context.state.lightDrag = LightDragState{};
    context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    context.state.topologySelectionKind = TopologySelectionKind::None;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSectorEditorSurface3DUiState(context);
    ResetRuntimeObjectUiState(context.uiState);
    context.uiState.idBufferSectorIndex = -1;
    context.uiState.idBufferLightIndex = -1;
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void ClearSectorEditorSelection(SectorEditorSelectionServiceContext& context)
{
    CancelSpotLightPilot(context, nullptr);
    context.state.selectDragArm = SelectDragArmState{};
    context.state.authoringVertexDrag = AuthoringVertexDragState{};
    context.state.topologySelectionKind = TopologySelectionKind::None;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.ambientIntensityInput = engine::UIFloatInputState{};
    context.uiState.ambientRedInput = engine::UIIntInputState{};
    context.uiState.ambientGreenInput = engine::UIIntInputState{};
    context.uiState.ambientBlueInput = engine::UIIntInputState{};
    ResetRuntimeObjectUiState(context.uiState);
    context.uiState.inspectorScroll.offset = Vector2{};
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologySector(SectorEditorSelectionServiceContext& context, int sectorId)
{
    if (FindSectorTopologySector(context.state.topologyMap, sectorId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.topologySelectionKind = TopologySelectionKind::Sector;
    context.state.selectedTopologySectorId = sectorId;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.idBufferLightIndex = -1;
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.floorInput = engine::UIFloatInputState{};
    context.uiState.ceilingInput = engine::UIFloatInputState{};
    context.uiState.ambientIntensityInput = engine::UIFloatInputState{};
    context.uiState.ambientRedInput = engine::UIIntInputState{};
    context.uiState.ambientGreenInput = engine::UIIntInputState{};
    context.uiState.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : context.uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context, int vertexId)
{
    const SectorTopologyVertex* vertex = FindSectorTopologyVertex(context.state.topologyMap, vertexId);
    if (vertex == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.topologySelectionKind = TopologySelectionKind::Vertex;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = vertex->id;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = vertex->id;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.idBufferLightIndex = -1;
    context.uiState.inspectorScroll.offset = Vector2{};
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologySideDef(
        SectorEditorSelectionServiceContext& context,
        int sideDefId,
        TopologyWallPart wallPart)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(context.state.topologyMap, sideDefId);
    if (sideDef == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(context.state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.topologySelectionKind = TopologySelectionKind::SideDef;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = sideDef->id;
    context.state.selectedTopologyLineDefId = lineDef->id;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = sideDef->side;
    context.state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
            context.state.topologyMap,
            sideDef,
            wallPart);
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.idBufferLightIndex = -1;
    context.uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : context.uiState.topologySideDefUvInputs) {
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
    if (FindSectorTopologyLineDef(context.state.topologyMap, lineDefId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.topologySelectionKind = TopologySelectionKind::LineDef;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = lineDefId;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = side;
    context.state.selectedTopologyWallPart = wallPart == TopologyWallPart::Middle
            ? TopologyWallPart::Wall
            : wallPart;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.idBufferLightIndex = -1;
    context.uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : context.uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSectorEditorSelectedSectorIdBuffer(context);
    SyncSectorEditorSelectedLightIdBuffer(context);
}

void SelectSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyStaticLight(context.state.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.selectedTopologyLightId = topologyLightId;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.topologySelectionKind = TopologySelectionKind::StaticLight;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.lightXInput = engine::UIFloatInputState{};
    context.uiState.lightYInput = engine::UIFloatInputState{};
    context.uiState.lightZInput = engine::UIFloatInputState{};
    context.uiState.lightTargetXInput = engine::UIFloatInputState{};
    context.uiState.lightTargetYInput = engine::UIFloatInputState{};
    context.uiState.lightTargetZInput = engine::UIFloatInputState{};
    context.uiState.lightIntensityInput = engine::UIFloatInputState{};
    context.uiState.lightRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightInnerConeInput = engine::UIFloatInputState{};
    context.uiState.lightOuterConeInput = engine::UIFloatInputState{};
    context.uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightRedInput = engine::UIIntInputState{};
    context.uiState.lightGreenInput = engine::UIIntInputState{};
    context.uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyStaticSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyStaticSpotLight(context.state.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.state.spotLightPilot.active
            && (context.state.spotLightPilot.kind != SpotLightPilotKind::Static
                    || context.state.spotLightPilot.lightId != topologyLightId)) {
        CancelSpotLightPilot(context, nullptr);
    }
    context.state.selectedTopologyStaticSpotLightId = topologyLightId;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.topologySelectionKind = TopologySelectionKind::StaticSpotLight;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.lightXInput = engine::UIFloatInputState{};
    context.uiState.lightYInput = engine::UIFloatInputState{};
    context.uiState.lightZInput = engine::UIFloatInputState{};
    context.uiState.lightTargetXInput = engine::UIFloatInputState{};
    context.uiState.lightTargetYInput = engine::UIFloatInputState{};
    context.uiState.lightTargetZInput = engine::UIFloatInputState{};
    context.uiState.lightIntensityInput = engine::UIFloatInputState{};
    context.uiState.lightRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightInnerConeInput = engine::UIFloatInputState{};
    context.uiState.lightOuterConeInput = engine::UIFloatInputState{};
    context.uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightRedInput = engine::UIIntInputState{};
    context.uiState.lightGreenInput = engine::UIIntInputState{};
    context.uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyDynamicLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyDynamicLight(context.state.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    CancelSpotLightPilot(context, nullptr);
    context.state.selectedTopologyDynamicLightId = topologyLightId;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicSpotLightId = -1;
    context.state.topologySelectionKind = TopologySelectionKind::DynamicLight;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.lightXInput = engine::UIFloatInputState{};
    context.uiState.lightYInput = engine::UIFloatInputState{};
    context.uiState.lightZInput = engine::UIFloatInputState{};
    context.uiState.lightTargetXInput = engine::UIFloatInputState{};
    context.uiState.lightTargetYInput = engine::UIFloatInputState{};
    context.uiState.lightTargetZInput = engine::UIFloatInputState{};
    context.uiState.lightIntensityInput = engine::UIFloatInputState{};
    context.uiState.lightRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightInnerConeInput = engine::UIFloatInputState{};
    context.uiState.lightOuterConeInput = engine::UIFloatInputState{};
    context.uiState.lightRedInput = engine::UIIntInputState{};
    context.uiState.lightGreenInput = engine::UIIntInputState{};
    context.uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorTopologyDynamicSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId)
{
    if (FindSectorTopologyDynamicSpotLight(context.state.topologyMap, topologyLightId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    if (context.state.spotLightPilot.active
            && (context.state.spotLightPilot.kind != SpotLightPilotKind::Dynamic
                    || context.state.spotLightPilot.lightId != topologyLightId)) {
        CancelSpotLightPilot(context, nullptr);
    }
    context.state.selectedTopologyDynamicSpotLightId = topologyLightId;
    context.state.selectedTopologyLightId = -1;
    context.state.selectedTopologyStaticSpotLightId = -1;
    context.state.selectedTopologyDynamicLightId = -1;
    context.state.topologySelectionKind = TopologySelectionKind::DynamicSpotLight;
    context.state.selectedTopologySectorId = -1;
    context.state.selectedTopologyVertexId = -1;
    context.state.selectedTopologySideDefId = -1;
    context.state.selectedTopologyLineDefId = -1;
    context.state.selectedRuntimeObjectId = -1;
    context.state.selectedTopologySideKind = SectorTopologySideKind::Front;
    context.state.inspectedTopologyVertexId = -1;
    context.state.selectedSurface3D = SectorSurfaceRef{};
    context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(context.state);
    ResetSectorEditorSurface3DUiState(context);
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.lightXInput = engine::UIFloatInputState{};
    context.uiState.lightYInput = engine::UIFloatInputState{};
    context.uiState.lightZInput = engine::UIFloatInputState{};
    context.uiState.lightTargetXInput = engine::UIFloatInputState{};
    context.uiState.lightTargetYInput = engine::UIFloatInputState{};
    context.uiState.lightTargetZInput = engine::UIFloatInputState{};
    context.uiState.lightIntensityInput = engine::UIFloatInputState{};
    context.uiState.lightRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    context.uiState.lightInnerConeInput = engine::UIFloatInputState{};
    context.uiState.lightOuterConeInput = engine::UIFloatInputState{};
    context.uiState.lightFlickerSpeedInput = engine::UIFloatInputState{};
    context.uiState.lightFlickerAmountInput = engine::UIFloatInputState{};
    context.uiState.lightRedInput = engine::UIIntInputState{};
    context.uiState.lightGreenInput = engine::UIIntInputState{};
    context.uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSectorEditorSelectedLightIdBuffer(context);
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SelectSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context, int objectId)
{
    if (FindSectorPlacedRuntimeObject(context.state.topologyMap, objectId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    context.state.selectedRuntimeObjectId = objectId;
    ResetSectorEditorSurface3DUiState(context);
    ResetRuntimeObjectUiState(context.uiState);
    context.uiState.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringLineTarget(SectorEditorSelectionServiceContext& context, int lineId)
{
    if (FindSectorAuthoringLine(context.state.authoringGraph, lineId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringLine(context.state, lineId);
    context.uiState.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringVertexTarget(SectorEditorSelectionServiceContext& context, int vertexId)
{
    if (FindSectorAuthoringVertex(context.state.authoringGraph, vertexId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringVertex(context.state, vertexId);
    context.uiState.inspectorScroll.offset = Vector2{};
}

void SelectSectorEditorAuthoringFaceAnchorTarget(SectorEditorSelectionServiceContext& context, int faceAnchorId)
{
    if (FindSectorAuthoringFaceAnchor(context.state.authoringGraph, faceAnchorId) == nullptr) {
        ClearSectorEditorSelection(context);
        return;
    }

    ClearSectorEditorSelection(context);
    SelectSectorEditorAuthoringFaceAnchor(context.state, faceAnchorId);
    context.uiState.inspectorScroll.offset = Vector2{};
    context.uiState.floorInput = engine::UIFloatInputState{};
    context.uiState.ceilingInput = engine::UIFloatInputState{};
    context.uiState.ambientIntensityInput = engine::UIFloatInputState{};
    context.uiState.ambientRedInput = engine::UIIntInputState{};
    context.uiState.ambientGreenInput = engine::UIIntInputState{};
    context.uiState.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : context.uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
}

void SelectSectorEditorSurface3D(SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface)
{
    const TopologySurfaceEditTarget target = SectorEditorTopologyEditTargetForSurface(surface);
    if (!IsValidSectorEditorSurfaceRef(context, surface)
            || !IsValidSectorEditorTopologySurfaceEditTarget(context, target)) {
        context.state.selectedSurface3D = SectorSurfaceRef{};
        context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    SectorEditorAuthoringSurfaceTarget authoringTarget;
    const bool hasAuthoringGraph = HasAuthoringGraphData(context.state);
    if (hasAuthoringGraph) {
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    context.state,
                    surface,
                    authoringTarget,
                    &unavailableStatus)) {
            context.state.selectedSurface3D = SectorSurfaceRef{};
            context.state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            SetStatusText(context, unavailableStatus);
            return;
        }
    }

    if (!SameSectorEditorSurfaceRef(context.state.selectedSurface3D, surface)) {
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
            SelectSectorEditorAuthoringLine(context.state, authoringSelection.lineId);
        } else if (authoringSelection.kind == SectorAuthoringSelectionKind::FaceAnchor) {
            SelectSectorEditorAuthoringFaceAnchor(context.state, authoringSelection.faceAnchorId);
        }
    }
    context.state.selectedSurface3D = surface;
    context.state.selectedTopologySurface3D = target;
}

bool IsValidSectorEditorSurfaceRef(const SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface)
{
    if (surface.kind == SectorSurfaceKind::None) {
        return false;
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                context.state.topologyMap,
                surface.topologySideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != surface.topologyLineDefId
                || sideDef->side != surface.topologySide) {
            return false;
        }
        return surface.kind != SectorSurfaceKind::Middle
                || IsTopologyMiddleEligible(context.state.topologyMap, sideDef);
    }
    return FindSectorTopologySector(context.state.topologyMap, surface.topologySectorId) != nullptr;
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
    return game::IsValidMaterialSurfaceTarget(context.state.topologyMap, target);
}

} // namespace game
