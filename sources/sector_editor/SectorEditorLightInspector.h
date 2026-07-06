#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

namespace game {

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError);
float StaticSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError);
float DynamicLightInspectorContentHeight(float rowH, float gap, bool hasIdError);
float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, float shadowNoteHeight);

bool DrawSelectedStaticLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyStaticPointLight& light,
        SectorEditorUiState& uiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested);

bool DrawSelectedStaticSpotLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyStaticSpotLight& light,
        SectorEditorUiState& uiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested);

bool DrawSelectedDynamicLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyDynamicPointLight& light,
        SectorEditorUiState& uiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested);

bool DrawSelectedDynamicSpotLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyDynamicSpotLight& light,
        SectorEditorUiState& uiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested);

} // namespace game
