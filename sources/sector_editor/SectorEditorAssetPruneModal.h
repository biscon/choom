#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"

namespace game {

enum class SectorEditorAssetPruneModalResult {
    None,
    Cancelled,
    Confirmed
};

inline void OpenSectorEditorAssetPruneModal(
        SectorEditorAssetPruneModalState& state)
{
    state = SectorEditorAssetPruneModalState{};
    state.open = true;
}

inline void CloseSectorEditorAssetPruneModal(
        SectorEditorAssetPruneModalState& state)
{
    state = SectorEditorAssetPruneModalState{};
}

SectorEditorAssetPruneModalResult DrawSectorEditorAssetPruneModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorAssetPruneModalState& state);

} // namespace game
