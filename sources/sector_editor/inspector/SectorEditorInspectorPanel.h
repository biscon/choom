#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"
#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"
#include "sector_editor/services/fog_volumes/SectorEditorFogVolumeEditingState.h"
#include "sector_editor/services/footsteps/SectorEditorFootstepService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingState.h"

#include <array>
#include <string>

#include <raylib.h>

namespace game {

class SectorEditorTextureCatalogService;
class SectorEditorSoundService;

enum class SectorEditorInspectorPanelRequestKind {
    RebuildSectorCollisionWorld,
    BeginAuthoringInsertVertex,
    DeleteSelectedRuntimeObject,
    OpenDeleteSelectedLightConfirmation,
    OpenDeleteSelectedFogVolumeConfirmation,
    OpenDeleteSelectedLevelMarkerConfirmation,
    BakeLightmaps,
    RefreshPreviewLightSources
};

struct SectorEditorInspectorPanelRequest {
    SectorEditorInspectorPanelRequestKind kind = SectorEditorInspectorPanelRequestKind::BakeLightmaps;
    std::string status;
    int lineId = -1;
};

struct SectorEditorInspectorPanelResult {
    std::array<SectorEditorInspectorPanelRequest, 8> requests{};
    int requestCount = 0;
};

struct SectorEditorInspectorPanelContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    Rectangle panelRect;

    SectorEditorState& state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState& selectionState;
    SectorEditorUiState& uiState;
    RuntimeObjectEditingState& runtimeObjectEditingState;
    RuntimeObjectEditingUiState& runtimeObjectEditingUiState;
    SectorRuntimeObjectState& runtimeObjects;
    InspectorIdUiState& inspectorIdUiState;
    MaterialEditingUiState& materialUiState;
    FogVolumeEditingUiState& fogVolumeUiState;
    LevelMarkerEditingUiState& levelMarkerUiState;
    std::string& statusText;

    SectorEditorSelectionServiceContext& selection;
    SectorEditorRuntimeObjectEditingService& runtimeObjectEditing;
    SectorEditorStaticModelPickerService& staticModelPicker;
    SectorEditorMaterialEditingService& materialEditing;
    SectorEditorFootstepService& footsteps;
    SectorEditorTextureCatalogService& textureCatalog;
    SectorEditorSoundService& sounds;
    SectorEditorLightEditingService& lightEditing;
    SectorEditorAuthoringFogVolumeEditingService& fogVolumeEditing;
    SectorEditorLevelMarkerEditingService& levelMarkerEditing;
    engine::EngineContext* engineContext = nullptr;
};

SectorEditorInspectorPanelResult DrawSectorEditorInspectorPanel(
        SectorEditorInspectorPanelContext& context);

} // namespace game
