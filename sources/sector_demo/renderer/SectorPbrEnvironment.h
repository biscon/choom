#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

namespace engine {
class AssetManager;
}

namespace game {

struct SectorTopologyMap;

struct SectorPbrEnvironment {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    bool active = false;
    bool usedSky = false;
};

inline bool IsSectorPbrEnvironmentActive(
        const SectorPbrEnvironment& environment,
        const TextureCubemap* cubemap)
{
    return environment.active
            && !engine::IsNull(environment.cubemap)
            && cubemap != nullptr
            && cubemap->id != 0;
}

bool BuildSectorPbrEnvironment(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        const SectorTopologyMap& map,
        SectorPbrEnvironment& outEnvironment);

} // namespace game
