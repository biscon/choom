#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingState.h"

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

    SectorEditorState& state;
    SelectionState& selectionState;
    ManipulationState& manipulationState;
    SectorEditorUiState& uiState;
    InspectorIdUiState& inspectorIdUiState;
    MaterialEditingUiState& materialUiState;
    LightEditingState& lightState;
    std::string& statusText;
    SectorMeshRenderer& preview;
};

struct SectorEditorPreviewOverlayResult {
    bool requestStartSpotLightPilot = false;
    bool requestApplySpotLightPilot = false;
    bool requestCancelSpotLightPilot = false;
    bool openPreviewSettings = false;
    bool markTopologyDocumentEdited = false;
    const char* topologyDocumentEditStatus = nullptr;
};

Rectangle BuildSectorEditorPreviewOverlayInteractionRect(PreviewDebugOverlayTab activeTab);

void DrawSectorEditorPreviewSurfaceHighlights(
        SectorEditorState& state,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        MaterialEditingUiState& materialUiState,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewSpotLightOverlay(
        const SectorEditorState& state,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview);
void DrawSectorEditorPreviewObjectProbeOverlay(
        const SectorEditorState& state,
        const SectorMeshRenderer& preview);

SectorEditorPreviewOverlayResult DrawSectorEditorPreviewOverlay(
        SectorEditorPreviewOverlayContext& context);

} // namespace game
