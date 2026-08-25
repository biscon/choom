#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ecs/Entity.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine {
class World;
}

namespace game {

struct SectorDynamicModelShadowCaster {
    engine::Entity entity = engine::NullEntity();
    int placedObjectId = 0;
    engine::ModelHandle model = engine::NullModelHandle();
    Matrix transform = {};
    uint64_t contentFingerprint = 0;
    bool animated = true;
};

struct SectorDynamicModelShadowCasterCollection {
    std::vector<SectorDynamicModelShadowCaster> casters;
    uint64_t fingerprint = 0;
    uint64_t revision = 0;
    bool fingerprintInitialized = false;
    bool capacityWarningPrinted = false;
};

void ReserveSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection,
        size_t capacity);

void UpdateSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection,
        engine::World* runtimeObjectWorld);

void ClearSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection);

} // namespace game
