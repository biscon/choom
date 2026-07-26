#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>

namespace game {

struct SectorEditorSelectionUiDependencies {
    engine::UIFloatInputState& floorInput;
    engine::UIFloatInputState& ceilingInput;
    engine::UIFloatInputState& ambientIntensityInput;
    engine::UIIntInputState& ambientRedInput;
    engine::UIIntInputState& ambientGreenInput;
    engine::UIIntInputState& ambientBlueInput;
    engine::UIFloatInputState& lightXInput;
    engine::UIFloatInputState& lightYInput;
    engine::UIFloatInputState& lightZInput;
    engine::UIFloatInputState& lightTargetXInput;
    engine::UIFloatInputState& lightTargetYInput;
    engine::UIFloatInputState& lightTargetZInput;
    engine::UIFloatInputState& lightIntensityInput;
    engine::UIFloatInputState& lightRadiusInput;
    engine::UIFloatInputState& lightInnerConeInput;
    engine::UIFloatInputState& lightOuterConeInput;
    engine::UIFloatInputState& lightSourceRadiusInput;
    engine::UIFloatInputState& lightFlickerSpeedInput;
    engine::UIFloatInputState& lightFlickerAmountInput;
    engine::UIIntInputState& lightRedInput;
    engine::UIIntInputState& lightGreenInput;
    engine::UIIntInputState& lightBlueInput;
    engine::UIFloatInputState& runtimeObjectXInput;
    engine::UIFloatInputState& runtimeObjectYInput;
    engine::UIFloatInputState& runtimeObjectZInput;
    engine::UIFloatInputState& runtimeObjectYawInput;
    engine::UIFloatInputState& runtimeObjectWidthInput;
    engine::UIFloatInputState& runtimeObjectHeightInput;
    engine::UIFloatInputState& runtimeObjectThicknessInput;
    engine::UIFloatInputState& runtimeObjectNormalOffsetInput;
    engine::UIFloatInputState& runtimeObjectOpenDistanceInput;
    engine::UIFloatInputState& runtimeObjectSpeedInput;
    engine::UIFloatInputState& runtimeObjectInitialOpenFractionInput;
    engine::UIFloatInputState& runtimeObjectAutoOpenDistanceInput;
    engine::UIFloatInputState& runtimeObjectInteractionDistanceInput;
    engine::UIFloatInputState& runtimeObjectOriginXInput;
    engine::UIFloatInputState& runtimeObjectOriginYInput;
    engine::UIScrollState& inspectorScroll;
    InspectorIdUiState& inspectorIdUiState;
};

struct SectorEditorSelectionServiceContext {
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    const SectorAuthoringDerivationResult& authoringDerivation;
    bool authoringDerivationCurrent = false;
    SelectionState& selectionState;
    SectorSurfaceRef& selectedSurface3D;
    TopologySurfaceEditTarget& selectedTopologySurface3D;
    ManipulationState& manipulationState;
    RuntimeObjectDragState& runtimeObjectDrag;
    SectorEditorSelectionUiDependencies ui;
    MaterialEditingUiState& materialUiState;
    std::string* statusText = nullptr;
    void* userData = nullptr;
    void (*requestCancelSpotLightPilotWithPreviewRestore)(void* userData, const char* message) = nullptr;
    LightEditingState* lightState = nullptr;
};

SectorTopologySector* SelectedSectorEditorTopologySector(SectorEditorSelectionServiceContext& context);
const SectorTopologySector* SelectedSectorEditorTopologySector(const SectorEditorSelectionServiceContext& context);
SectorTopologyVertex* SelectedSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context);
const SectorTopologyVertex* SelectedSectorEditorTopologyVertex(const SectorEditorSelectionServiceContext& context);
SectorTopologySideDef* SelectedSectorEditorTopologySideDef(SectorEditorSelectionServiceContext& context);
const SectorTopologySideDef* SelectedSectorEditorTopologySideDef(const SectorEditorSelectionServiceContext& context);
SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(SectorEditorSelectionServiceContext& context);
const SectorTopologyLineDef* SelectedSectorEditorTopologyLineDef(const SectorEditorSelectionServiceContext& context);
SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context);
const SectorTopologyStaticPointLight* SelectedSectorEditorTopologyLight(const SectorEditorSelectionServiceContext& context);
SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(SectorEditorSelectionServiceContext& context);
const SectorTopologyStaticSpotLight* SelectedSectorEditorTopologyStaticSpotLight(
        const SectorEditorSelectionServiceContext& context);
SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(SectorEditorSelectionServiceContext& context);
const SectorTopologyDynamicPointLight* SelectedSectorEditorTopologyDynamicLight(
        const SectorEditorSelectionServiceContext& context);
SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        SectorEditorSelectionServiceContext& context);
const SectorTopologyDynamicSpotLight* SelectedSectorEditorTopologyDynamicSpotLight(
        const SectorEditorSelectionServiceContext& context);
SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context);
const SectorPlacedRuntimeObject* SelectedSectorEditorRuntimeObject(const SectorEditorSelectionServiceContext& context);

void ClearStaleSectorEditorTopologySelection(SectorEditorSelectionServiceContext& context);
void SyncSectorEditorSelectedSectorIdBuffer(SectorEditorSelectionServiceContext& context);
void SyncSectorEditorSelectedLightIdBuffer(SectorEditorSelectionServiceContext& context);
void ResetSectorEditorSurface3DUiState(SectorEditorSelectionServiceContext& context);
void ClearSectorEditorTopologySelectionOnly(SectorEditorSelectionServiceContext& context);
void ClearSectorEditorSelection(SectorEditorSelectionServiceContext& context);

void SelectSectorEditorTopologySector(SectorEditorSelectionServiceContext& context, int sectorId);
void SelectSectorEditorTopologyVertex(SectorEditorSelectionServiceContext& context, int vertexId);
void SelectSectorEditorTopologySideDef(
        SectorEditorSelectionServiceContext& context,
        int sideDefId,
        TopologyWallPart wallPart);
void SelectSectorEditorTopologyLineDef(
        SectorEditorSelectionServiceContext& context,
        int lineDefId,
        SectorTopologySideKind side,
        TopologyWallPart wallPart);
void SelectSectorEditorTopologyLight(SectorEditorSelectionServiceContext& context, int topologyLightId);
void SelectSectorEditorTopologyStaticSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId);
void SelectSectorEditorTopologyDynamicLight(SectorEditorSelectionServiceContext& context, int topologyLightId);
void SelectSectorEditorTopologyDynamicSpotLight(SectorEditorSelectionServiceContext& context, int topologyLightId);
void SelectSectorEditorRuntimeObject(SectorEditorSelectionServiceContext& context, int objectId);
void SelectSectorEditorAuthoringLineTarget(SectorEditorSelectionServiceContext& context, int lineId);
void SelectSectorEditorAuthoringVertexTarget(SectorEditorSelectionServiceContext& context, int vertexId);
void SelectSectorEditorAuthoringFaceAnchorTarget(SectorEditorSelectionServiceContext& context, int faceAnchorId);
void SelectSectorEditorSurface3D(SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface);

bool IsValidSectorEditorSurfaceRef(const SectorEditorSelectionServiceContext& context, SectorSurfaceRef surface);
bool SameSectorEditorSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b);
TopologySurfaceEditTarget SectorEditorTopologyEditTargetForSurface(SectorSurfaceRef surface);
bool IsValidSectorEditorTopologySurfaceEditTarget(
        const SectorEditorSelectionServiceContext& context,
        TopologySurfaceEditTarget target);

} // namespace game
