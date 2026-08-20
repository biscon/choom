#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorLightmapAsyncTypes.h"
#include "sector_editor/SectorEditorModalTypes.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorLightmapBakeModalCallbacks {
    std::function<void()> requestCancel;
    std::function<void()> closeAcknowledgement;
};

enum class SectorEditorLightmapBakeSetupModalResult {
    None,
    Cancelled,
    BakeRequested
};

inline void OpenSectorEditorLightmapBakeSetupModal(
        SectorLightmapBakeSetupModalState& state,
        SectorLightmapBakeQualityPreset currentQuality)
{
    state = SectorLightmapBakeSetupModalState{};
    state.open = true;
    state.selectedQuality =
            NormalizeSectorLightmapBakeQualityPreset(currentQuality);
}

inline void CloseSectorEditorLightmapBakeSetupModal(
        SectorLightmapBakeSetupModalState& state)
{
    state = SectorLightmapBakeSetupModalState{};
}

SectorEditorLightmapBakeSetupModalResult DrawLightmapBakeSetupModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorLightmapBakeSetupModalState& state);

void DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const SectorEditorLightmapBakeModalView& view,
        const SectorEditorLightmapBakeModalCallbacks& callbacks);

} // namespace game
