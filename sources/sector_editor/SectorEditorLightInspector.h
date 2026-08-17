#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

namespace game {

inline float MeasureLightAtmosphereInspectorContentHeight(
        float rowH,
        float gap,
        const SectorLightAtmosphereSettings& atmosphere,
        bool showLegacyHaze)
{
    float height = 2.0f * (26.0f + rowH + gap);
    if (showLegacyHaze) {
        height += 26.0f + rowH + gap;
        if (atmosphere.haze.enabled) height += 10.0f * (rowH + gap);
    }
    if (atmosphere.dust.enabled) height += 10.0f * (rowH + gap);
    return height;
}

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere, bool showLegacyHaze);
float StaticSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere, bool showLegacyHaze);
float DynamicLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere, bool showLegacyHaze);
float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, float shadowNoteHeight, const SectorLightAtmosphereSettings& atmosphere, bool showLegacyHaze);

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
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool showLegacyHaze,
        bool& deleteRequested,
        bool& bakeRequested,
        bool& sourceRefreshRequested);

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
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool showLegacyHaze,
        bool& deleteRequested,
        bool& bakeRequested,
        bool& sourceRefreshRequested);

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
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool showLegacyHaze,
        bool& deleteRequested,
        bool& sourceRefreshRequested);

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
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool showLegacyHaze,
        bool& deleteRequested,
        bool& sourceRefreshRequested);

} // namespace game
