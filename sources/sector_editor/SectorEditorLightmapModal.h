#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>

namespace game {

struct SectorEditorLightmapBakeModalCallbacks {
    std::function<bool()> isBlocking;
    std::function<void()> requestCancel;
    std::function<void()> closeAcknowledgement;
};

void DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        LightmapBakeAsyncState& lightmapBake,
        const SectorEditorLightmapBakeModalCallbacks& callbacks);

} // namespace game
