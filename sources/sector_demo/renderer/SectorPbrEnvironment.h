#pragma once

#include "engine/assets/AssetHandles.h"

namespace engine {
class AssetManager;
}

namespace game {

struct SectorTopologyMap;

struct SectorPbrEnvironment {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    bool usedSky = false;
};

bool BuildSectorPbrEnvironment(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        const SectorTopologyMap& map,
        SectorPbrEnvironment& outEnvironment);

} // namespace game
