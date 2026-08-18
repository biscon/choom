#pragma once

#include "sector_demo/SectorDynamicModelShadowCasters.h"

#include <raylib.h>
#include <raymath.h>

#include <cstddef>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorCollisionWorld;
struct RuntimePortalVisibilityResult;
struct SectorDynamicSpotLightShadowRenderContext;

struct SectorDynamicModelShadowDrawContext {
    engine::AssetManager* assets = nullptr;
    engine::World* world = nullptr;
    const SectorCollisionWorld* collisionWorld = nullptr;
    const RuntimePortalVisibilityResult* visibility = nullptr;
};

class SectorDynamicModelShadowRenderer {
public:
    bool Load();
    void Shutdown();
    void ReserveShadowCasterCapacity(std::size_t capacity);
    void PrepareShadowRenderContext(
            SectorDynamicSpotLightShadowRenderContext& context,
            engine::World* runtimeObjectWorld);
    void ClearPreparedShadowCasters();
    void Draw(const SectorDynamicModelShadowDrawContext& context);

    bool IsLoaded() const { return loaded; }
    std::size_t DynamicCasterCount() const {
        return shadowCasterCollection.casters.size();
    }

private:
    void DrawContactShadows(const SectorDynamicModelShadowDrawContext& context);

    SectorDynamicModelShadowCasterCollection shadowCasterCollection;
    Material contactMaterial = {};
    Mesh contactMesh = {};
    bool loaded = false;
    int contactOpacityLoc = -1;
};

} // namespace game
