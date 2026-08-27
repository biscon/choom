#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorMeshRenderer;
struct NpcAiRuntime;
struct NpcNavigationRuntime;

void DrawNpcAiDebugWorld(
        const engine::World& world,
        const NpcNavigationRuntime& navigation,
        const NpcAiRuntime& aiRuntime,
        const SectorMeshRenderer& renderer);

void DrawNpcAiDebugLabels(
        const engine::World& world,
        const NpcNavigationRuntime& navigation,
        const SectorMeshRenderer& renderer,
        float agentHeight,
        Vector3 playerFeetPosition,
        bool aiFrozen,
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle playableViewport);

} // namespace game
