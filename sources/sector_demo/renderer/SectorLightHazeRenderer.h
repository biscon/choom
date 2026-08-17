#pragma once

#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace game {

class SectorLightHazeRenderer {
public:
    bool Apply(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& sceneScratch,
            const SectorTopologyMap& map,
            SectorTopologyFogSettings::VolumetricQuality quality,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds);
    void Shutdown();

    int EligibleCount() const { return eligibleCount; }
    int ActiveCount() const { return activeCount; }
    const engine::RenderTarget& AccumulationTarget() const { return hazeTarget; }
    const std::string& AccumulationDiagnostic() const { return accumulationDiagnostic; }

private:
    struct ShaderLocations {
        int sceneDepth = -1;
        int volumeCount = -1;
        int marchSteps = -1;
        int cameraPosition = -1;
        int cameraForward = -1;
        int cameraRight = -1;
        int cameraUp = -1;
        int tanHalfFov = -1;
        int aspectRatio = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int runtimeSeconds = -1;
        int pathMinimumWorld = -1;
        int pathThicknessMultiplier = -1;
        int pathSaturationPower = -1;
        int centers = -1;
        int directions = -1;
        int boundsCenters = -1;
        int boundsRadii = -1;
        int shapes = -1;
        int extents = -1;
        int coneRadii = -1;
        int colors = -1;
        int paramsA = -1;
        int paramsB = -1;
        int staticLighting = -1;
        int fogEnabled = -1;
        int fogColor = -1;
        int fogStartDistance = -1;
        int fogDensity = -1;
        int fogMaxOpacity = -1;
        int fogReferenceHeight = -1;
        int fogHeightFalloff = -1;
        SectorDynamicLightShaderLocations dynamicLights;
        SectorDynamicSpotLightShadowShaderLocations shadows;
        int shadowMap0 = -1;
        int shadowMap1 = -1;
    };

    struct CompositeLocations {
        int sceneColor = -1;
        int sceneDepth = -1;
        int hazeTexture = -1;
        int hazeTexelSize = -1;
        int bilateralUpsample = -1;
    };

    struct ProbeCacheEntry {
        bool valid = false;
        SectorLightAtmosphereSourceKind kind = SectorLightAtmosphereSourceKind::StaticPoint;
        int lightId = 0;
        int ownerSectorId = 0;
        Vector3 origin = {};
        Vector3 direction = {};
        float extent = 0.0f;
        float coneRadius = 0.0f;
        SectorLightHazeStaticLightingSamples lighting;
    };

    bool EnsureShaders();
    bool EnsureTargets(int width, int height, float scale);
    void ReleaseTargets();
    void RefreshProbeIdentity(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes);
    const SectorLightHazeStaticLightingSamples& LightingFor(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            const SectorLightAtmosphereVolume& volume);

    Shader shader = {};
    Shader compositeShader = {};
    ShaderLocations locations;
    CompositeLocations compositeLocations;
    engine::RenderTarget hazeTarget;
    std::string accumulationDiagnostic = "not allocated";
    std::array<ProbeCacheEntry, 8> probeCache{};
    const SectorBakedObjectLightProbe* cachedProbeData = nullptr;
    std::size_t cachedProbeCount = 0;
    std::size_t cachedProbeHash = 0;
    std::size_t cachedMapProbeHash = 0;
    int width = 0;
    int height = 0;
    float scale = 0.0f;
    int failedWidth = 0;
    int failedHeight = 0;
    float failedScale = 0.0f;
    int eligibleCount = 0;
    int activeCount = 0;
    bool warnedUnavailable = false;
    bool shaderFailed = false;
};

} // namespace game
