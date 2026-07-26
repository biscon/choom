#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorDecalTintModalCallbacks {
    std::function<void()> close;
    std::function<bool(TopologySurfaceEditTarget)> isTargetValid;
    std::function<const SectorTopologyDecalLayer*(TopologySurfaceEditTarget)> decalForTarget;
    std::function<bool(TopologySurfaceEditTarget, Vector3)> applyTint;
};

struct SectorEditorDecalTintModalContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    DecalTintModalState& modalState;
    std::string& statusText;
    const SectorEditorDecalTintModalCallbacks& callbacks;
};

void DrawSectorEditorDecalTintModal(SectorEditorDecalTintModalContext& context);

} // namespace game
