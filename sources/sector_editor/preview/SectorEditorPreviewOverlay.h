#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "game/FpsViewmodel.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcRuntime.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingState.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingState.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>

#include <string>

namespace game {

class SectorMeshRenderer;

struct SectorEditorPreviewOverlayContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;

    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    const SectorAuthoringDerivationResult& authoringDerivation;
    bool authoringDerivationCurrent = false;
    bool topologyDocumentDirty = false;
    RuntimeObjectDragState& runtimeObjectDrag;
    RuntimeObjectEditingState& runtimeObjectEditingState;
    SectorEditorPreviewState& previewState;
    SectorRuntimeObjectState& runtimeObjects;
    SectorNavigationWorld& navigation;
    NpcNavigationRuntime& npcNavigation;
    FpsViewmodelRuntimeState& viewmodel;
    SelectionState& selectionState;
    ManipulationState& manipulationState;
    SectorEditorSelectionUiDependencies selectionUi;
    engine::UIFloatInputState& objectProbeDebugDrawMaxDistanceInput;
    MaterialEditingUiState& materialUiState;
    LightEditingState& lightState;
    std::string& statusText;
    SectorMeshRenderer& preview;
};

struct SectorEditorPreviewOverlayResult {
    bool requestStartLightPilot = false;
    bool requestApplyLightPilot = false;
    bool requestCancelLightPilot = false;
    LightProxyPlacementKind requestStartProxyPlacement = LightProxyPlacementKind::None;
    bool requestApplyProxyPlacement = false;
    bool requestCancelProxyPlacement = false;
    bool previewProxyOffsetChanged = false;
    Vector3 previewProxyOffsetWorld = {};
    bool openPreviewSettings = false;
    bool openWeaponEditor = false;
    bool markTopologyDocumentEdited = false;
    bool requestNavigationRebuild = false;
    bool requestBakeSelectedReflectionProbe = false;
    bool requestBakeAllReflectionProbes = false;
    bool requestApplyObjectAdjustment = false;
    bool requestCancelObjectAdjustment = false;
    const char* topologyDocumentEditStatus = nullptr;
};

Rectangle BuildSectorEditorPreviewOverlayInteractionRect(PreviewDebugOverlayTab activeTab);
Rectangle BuildSectorEditorPreviewObjectAdjustmentPanelRect();

void DrawSectorEditorPreviewSurfaceHighlights(
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        RuntimeObjectDragState& runtimeObjectDrag,
        SectorEditorPreviewSelectionState& previewSelectionState,
        const SectorEditorPreviewControllerState& previewControllerState,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorSelectionUiDependencies selectionUi,
        MaterialEditingUiState& materialUiState,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewSpotLightOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewControllerState& previewControllerState,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewObjectProbeOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewState& previewState,
        const SectorRuntimeObjectState& runtimeObjects,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewReflectionProbeOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewState& previewState,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewNavigationOverlay(
        const SectorEditorPreviewOverlayState& overlayState,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        int selectedRuntimeObjectId,
        const SectorMeshRenderer& preview);

SectorEditorPreviewOverlayResult DrawSectorEditorPreviewOverlay(
        SectorEditorPreviewOverlayContext& context);

} // namespace game
