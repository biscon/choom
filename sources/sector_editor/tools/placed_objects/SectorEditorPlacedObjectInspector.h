#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"

namespace game {

class SectorEditorTextureCatalogService;
struct SectorRuntimeObjectState;

struct SectorEditorPlacedObjectInspectorMeasureContext {
    engine::AssetManager& assets;
    engine::FontHandle smallFont;
    const engine::UIConfig& smallConfig;
    SectorTopologyMap& topologyMap;
    SectorRuntimeObjectState& runtimeObjects;
    engine::EngineContext* engineContext = nullptr;
    SectorEditorRuntimeObjectEditingService& editing;
    RuntimeObjectEditingState& editingState;
    SectorEditorTextureCatalogService& textureCatalog;
    float contentW = 0.0f;
    float rowH = 0.0f;
    float gap = 0.0f;
};

struct SectorEditorPlacedObjectInspectorContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    engine::UIScrollAreaResult scroll;
    SectorEditorState& state;
    SectorAuthoringGraph& authoringGraph;
    SectorTopologyMap& topologyMap;
    SectorRuntimeObjectState& runtimeObjects;
    RuntimeObjectEditingUiState& uiState;
    engine::EngineContext* engineContext = nullptr;
    SectorEditorRuntimeObjectEditingService& editing;
    RuntimeObjectEditingState& editingState;
    SectorEditorStaticModelPickerService& staticModelPicker;
    std::string& statusText;
    bool& deleteRequested;
    SectorEditorTextureCatalogService& textureCatalog;
    float contentW = 0.0f;
    float rowH = 0.0f;
    float gap = 0.0f;
};

float MeasureSectorEditorPlacedObjectInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context);

void DrawSectorEditorPlacedObjectInspector(
        SectorEditorPlacedObjectInspectorContext& context);

} // namespace game
