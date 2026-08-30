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
    bool advancedTransmission = false;
    const Texture2D* sceneColor = nullptr;
    const Texture2D* sceneDepth = nullptr;
    Vector2 viewportSize = {};
    std::string* renderDebugText = nullptr;
};

class SectorWindowRenderer {
public:
    bool Initialize(std::size_t capacity);
    void Shutdown();
    void Reserve(std::size_t capacity);
    void Draw(const SectorWindowDrawContext& context);
    bool HasVisibleWindows(
            engine::World& world,
            const RuntimePortalVisibilityResult* visibility) const;

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
    int thicknessLoc = -1;
    int advancedTransmissionLoc = -1;
    int sceneColorLoc = -1;
    int sceneDepthLoc = -1;
    int viewportSizeLoc = -1;
    int viewMatrixLoc = -1;
    int projectionMatrixLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentBoxProjectionLoc = -1;
    int environmentCapturePositionLoc = -1;
    int environmentInfluenceCenterLoc = -1;
    int environmentHalfExtentsLoc = -1;
    int environmentYawLoc = -1;
    int environmentMaxLodLoc = -1;
    int environmentIntensityLoc = -1;
    int environmentSpecularScaleLoc = -1;
    std::size_t consideredCount = 0;
    std::size_t drawnCount = 0;
    std::size_t localEnvironmentCount = 0;
    std::size_t globalEnvironmentCount = 0;
    std::size_t missingEnvironmentCount = 0;
};

} // namespace game
