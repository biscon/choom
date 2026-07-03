#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <algorithm>
#include <functional>

namespace game {

inline SectorLightmapBakeSettings NormalizeSectorPreviewObjectProbeSettings(
        SectorLightmapBakeSettings settings)
{
    settings.objectProbeSpacingWorld = std::clamp(settings.objectProbeSpacingWorld, 0.25f, 128.0f);
    settings.objectProbeHeightWorld = std::clamp(settings.objectProbeHeightWorld, 0.0f, 16.0f);
    return settings;
}

inline void ResetSectorPreviewSettingsModalLightingDefaults(
        SectorPreviewSettingsModalState& modalState)
{
    modalState.draftDirectionalLight = DefaultSectorTopologyDirectionalLightSettings();
    modalState.draftLightmapSettings = SectorLightmapBakeSettings{};
    modalState.lightDirectionXInput = engine::UIFloatInputState{};
    modalState.lightDirectionYInput = engine::UIFloatInputState{};
    modalState.lightDirectionZInput = engine::UIFloatInputState{};
    modalState.lightIntensityInput = engine::UIFloatInputState{};
    modalState.objectProbeSpacingInput = engine::UIFloatInputState{};
    modalState.objectProbeHeightInput = engine::UIFloatInputState{};
    modalState.lightColorRedInput = engine::UIIntInputState{};
    modalState.lightColorGreenInput = engine::UIIntInputState{};
    modalState.lightColorBlueInput = engine::UIIntInputState{};
}

inline bool ApplySectorPreviewObjectProbeSettings(
        SectorTopologyMap& map,
        const SectorLightmapBakeSettings& draftSettings)
{
    const SectorLightmapBakeSettings normalizedDraft =
            NormalizeSectorPreviewObjectProbeSettings(draftSettings);
    const SectorLightmapBakeSettings normalizedCurrent =
            NormalizeSectorPreviewObjectProbeSettings(map.lightmapSettings);
    if (normalizedCurrent.objectProbeSpacingWorld == normalizedDraft.objectProbeSpacingWorld
            && normalizedCurrent.objectProbeHeightWorld == normalizedDraft.objectProbeHeightWorld) {
        return false;
    }

    map.lightmapSettings.objectProbeSpacingWorld = normalizedDraft.objectProbeSpacingWorld;
    map.lightmapSettings.objectProbeHeightWorld = normalizedDraft.objectProbeHeightWorld;
    return true;
}

struct SectorEditorPreviewSettingsModalCallbacks {
    std::function<void()> close;
    std::function<void()> apply;
    std::function<void()> openSkyTexturePicker;
};

void DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorPreviewSettingsModalState& modalState,
        bool texturePickerOpen,
        const SectorEditorPreviewSettingsModalCallbacks& callbacks);

} // namespace game
