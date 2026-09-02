#pragma once

#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

struct SectorLiquidDrawContext {
    engine::AssetManager* assets = nullptr;
    Camera3D camera = {};
    const RuntimePortalVisibilityResult* visibility = nullptr;
    const SectorPbrEnvironment* environment = nullptr;
    bool localReflectionProbesCurrent = true;
    SectorPbrContributionSettings pbr;
    SectorTopologyDirectionalLightSettings directionalLight;
    SectorFogRenderContext fog;
    bool advancedTransmission = false;
    const Texture2D* sceneColor = nullptr;
    const Texture2D* sceneDepth = nullptr;
    Vector2 viewportSize = {};
    float runtimeSeconds = 0.0f;
    std::string* renderDebugText = nullptr;
};

class SectorLiquidRenderer {
public:
    bool Initialize(std::size_t capacity);
    bool Rebuild(
            const SectorTopologyMap& map,
            const SectorGeneratedGeometry& geometry,
            std::string& error);
    void Shutdown();
    void Reserve(std::size_t capacity);
    bool HasVisibleLiquids(
            const RuntimePortalVisibilityResult* visibility,
            Vector3 cameraPosition) const;
    void Draw(const SectorLiquidDrawContext& context);

    bool IsLoaded() const { return materialLoaded; }
    std::size_t DrawnCount() const { return drawnCount; }
    std::size_t SurfaceCount() const { return surfaces.size(); }

private:
    struct Surface {
        int sectorId = -1;
        Mesh mesh = {};
        SectorLiquidSettings settings;
        Vector3 center = {};
        float surfaceY = 0.0f;
    };

    struct DrawItem {
        std::size_t surfaceIndex = 0;
        float distanceSquared = 0.0f;
    };

    Shader shader = {};
    Material material = {};
    bool materialLoaded = false;
    std::vector<Surface> surfaces;
    std::vector<DrawItem> drawItems;
    SectorFogShaderLocations fogLocations;
    int cameraPositionLoc = -1;
    int runtimeSecondsLoc = -1;
    int shallowColorLoc = -1;
    int deepColorLoc = -1;
    int foamColorLoc = -1;
    int liquidParams0Loc = -1;
    int liquidParams1Loc = -1;
    int flowParamsLoc = -1;
    int advancedTransmissionLoc = -1;
    int viewportSizeLoc = -1;
    int inverseViewMatrixLoc = -1;
    int inverseProjectionMatrixLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentBoxProjectionLoc = -1;
    int environmentCapturePositionLoc = -1;
    int environmentInfluenceCenterLoc = -1;
    int environmentHalfExtentsLoc = -1;
    int environmentYawLoc = -1;
    int environmentMaxLodLoc = -1;
    int environmentIntensityLoc = -1;
    int environmentSpecularScaleLoc = -1;
    int directionalLightEnabledLoc = -1;
    int directionalLightDirectionLoc = -1;
    int directionalLightColorLoc = -1;
    int directionalLightIntensityLoc = -1;
    std::size_t drawnCount = 0;
};

} // namespace game
