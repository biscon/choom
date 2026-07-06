#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>

namespace game {

class SectorEditorTextureCatalogService;

struct SectorEditorPlacedObjectInspectorCallbacks {
    std::function<const SectorPlacedRuntimeObject*()> selectedRuntimeObject;
    std::function<bool(const char*, const std::function<bool(SectorPlacedRuntimeObject&)>&)>
            mutateSelectedRuntimeObject;
    std::function<void()> openBillboardSpritePicker;
    std::function<void()> openDoorTexturePicker;
    std::function<void()> openDoorTextureSettingsModal;
    std::function<bool()> deleteSelectedRuntimeObject;
    std::function<bool(bool&)> selectedDoorRuntimeTargetOpen;
    std::function<void(bool)> setSelectedDoorRuntimeTargetOpen;
};

struct SectorEditorPlacedObjectInspectorMeasureContext {
    engine::AssetManager& assets;
    engine::FontHandle smallFont;
    const engine::UIConfig& smallConfig;
    SectorEditorState& state;
    engine::EngineContext* engineContext = nullptr;
    const SectorEditorPlacedObjectInspectorCallbacks& callbacks;
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
    SectorEditorUiState& uiState;
    engine::EngineContext* engineContext = nullptr;
    const SectorEditorPlacedObjectInspectorCallbacks& callbacks;
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
