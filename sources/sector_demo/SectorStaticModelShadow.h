#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine {
class World;
}

namespace game {

struct SectorStaticModelShadowCaster {
    int placedObjectId = 0;
    engine::ModelHandle model = engine::NullModelHandle();
    Matrix transform = {};
};

struct SectorStaticModelShadowCasterCollection {
    std::vector<SectorStaticModelShadowCaster> casters;
    uint64_t fingerprint = 0;
    uint64_t revision = 0;
    bool fingerprintInitialized = false;
    bool capacityWarningPrinted = false;
};

void ReserveSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection,
        size_t capacity);

void UpdateSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection,
        engine::World* runtimeObjectWorld);

void ClearSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection);

} // namespace game
