#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/renderer/SectorDoorRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorStaticSpecularLighting.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorWindowDrawContext {
    engine::AssetManager* assets = nullptr;
    engine::World* world = nullptr;
    Camera3D camera = {};
    const RuntimePortalVisibilityResult* visibility = nullptr;
    const SectorPbrEnvironment* environment = nullptr;
    bool localReflectionProbesCurrent = true;
    SectorPbrContributionSettings pbr;
    SectorBillboardDynamicLightContext dynamicLights;
    const SectorStaticSpecularLightState* staticSpecularLights = nullptr;
    bool staticSpecularEligible = false;
    SectorFogRenderContext fog;
    std::string* renderDebugText = nullptr;
};

class SectorWindowRenderer {
public:
    bool Initialize(std::size_t capacity);
    void Shutdown();
    void Reserve(std::size_t capacity);
    void Draw(const SectorWindowDrawContext& context);

    bool IsLoaded() const { return materialLoaded && meshLoaded; }
    std::size_t ConsideredCount() const { return consideredCount; }
    std::size_t DrawnCount() const { return drawnCount; }

private:
    struct DrawItem {
        engine::Entity entity = engine::NullEntity();
        int placedObjectId = 0;
        float distanceSquared = 0.0f;
    };

    Shader shader = {};
    Material material = {};
    Mesh cube = {};
    bool materialLoaded = false;
    bool meshLoaded = false;
    std::vector<DrawItem> drawItems;
    SectorDynamicLightShaderLocations dynamicLightLocations;
    SectorStaticSpecularShaderLocations staticSpecularLocations;
    SectorFogShaderLocations fogLocations;
    int cameraPositionLoc = -1;
    int tintLoc = -1;
    int opacityLoc = -1;
    int roughnessLoc = -1;
    int iorLoc = -1;
    int ambientLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentYawLoc = -1;
    int environmentMaxLodLoc = -1;
    int environmentIntensityLoc = -1;
    int environmentSpecularScaleLoc = -1;
    std::size_t consideredCount = 0;
    std::size_t drawnCount = 0;
};

} // namespace game
