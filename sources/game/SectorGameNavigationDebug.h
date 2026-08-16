#pragma once

#include "game/navigation/SectorNavigationDebugDraw.h"

namespace engine {
class AssetManager;
struct UIConfig;
struct FontHandle;
}

namespace game {

class SectorNavigationWorld;
struct NpcNavigationRuntime;
struct SectorScriptHost;

struct SectorGameNavigationDebugState {
    SectorNavigationDebugDrawSettings drawSettings;
    bool visible = false;
};

void DrawSectorGameNavigationDebugPanel(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        const SectorScriptHost& scriptHost);

} // namespace game
