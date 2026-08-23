#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

namespace game {

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere);
float StaticSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere);
float DynamicLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere);
float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, float shadowNoteHeight, const SectorLightAtmosphereSettings& atmosphere);
float RectLightInspectorContentHeight(float rowH, float gap, bool hasIdError,
        bool dynamic, const SectorLightAtmosphereSettings& atmosphere);

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
        bool& deleteRequested,
        bool& convertRequested,
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
        bool& deleteRequested,
        bool& convertRequested,
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
        bool& deleteRequested,
        bool& convertRequested,
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
        bool& deleteRequested,
        bool& convertRequested,
        bool& sourceRefreshRequested);

bool DrawSelectedStaticRectLightInspector(
        engine::UIContext&, const engine::UIConfig&, engine::Input&, engine::AssetManager&,
        engine::FontHandle, engine::UIScrollAreaResult, float, float, float,
        SectorTopologyStaticRectLight&, SectorEditorUiState&, InspectorIdUiState&,
        SectorEditorLightEditingService&, bool&, bool&, bool&, bool&);
bool DrawSelectedDynamicRectLightInspector(
        engine::UIContext&, const engine::UIConfig&, engine::Input&, engine::AssetManager&,
        engine::FontHandle, engine::UIScrollAreaResult, float, float, float,
        SectorTopologyDynamicRectLight&, SectorEditorUiState&, InspectorIdUiState&,
        SectorEditorLightEditingService&, bool&, bool&, bool&);

} // namespace game
