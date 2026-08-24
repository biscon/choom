#pragma once

#include "engine/assets/AssetHandles.h"
#include "sector_demo/SectorReflectionProbeTypes.h"

#include <raylib.h>

#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

struct SectorTopologyMap;

struct SectorPbrEnvironment {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    struct LocalProbe {
        SectorCompiledReflectionProbe definition;
        engine::TextureHandle cubemap = engine::NullTextureHandle();
        int mipCount = 1;
    };
    std::vector<LocalProbe> localProbes;
    bool active = false;
    bool usedSky = false;
};

struct SectorPbrEnvironmentSelection {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    Vector3 capturePosition = {};
    Vector3 influenceCenter = {};
    Vector3 halfExtents = {1.0f, 1.0f, 1.0f};
    float yawRadians = 0.0f;
    float intensity = 1.0f;
    float maxLod = 0.0f;
    bool boxProjection = false;
    bool localProbe = false;
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

SectorPbrEnvironmentSelection SelectSectorPbrEnvironment(
        const SectorPbrEnvironment& environment,
        Vector3 receiverPosition,
        int receiverSectorId = -1);

} // namespace game
