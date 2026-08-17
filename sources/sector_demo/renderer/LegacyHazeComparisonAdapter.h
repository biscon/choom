#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <array>
#include <cstddef>
#include <vector>

namespace game {

enum class SectorVolumetricComparisonShape {
    Ellipsoid,
    Sphere,
    Cone
};

struct SectorVolumetricComparisonVolume {
    SectorVolumetricComparisonShape shape =
            SectorVolumetricComparisonShape::Ellipsoid;
    int stableId = -1;
    Vector3 centerWorld = {};
    Vector3 directionWorld = {0.0f, -1.0f, 0.0f};
    Vector3 radiiWorld = {};
    float extentWorld = 0.0f;
    float coneRadiusWorld = 0.0f;
    Vector3 tint = {};
    float density = 0.0f;
    float maximumOpacity = 0.0f;
    float edgeSoftness = 0.0f;
    float noiseAmount = 0.0f;
    float noiseScaleWorld = 1.0f;
    float flowDirectionRadians = 0.0f;
    float flowSpeedWorld = 0.0f;
    std::array<Vector3, 8> staticLighting{};
    int staticLightingSampleCount = 0;
};

class LegacyHazeComparisonAdapter {
public:
    static constexpr std::size_t MaximumVolumes = 8;

    int Build(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            SectorTopologyFogSettings::LocalVolumeQuality quality,
            const Camera3D& camera,
            float aspectRatio,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds,
            std::array<SectorVolumetricComparisonVolume, MaximumVolumes>& outVolumes);
    void Reset();
    void ClearFrameCounts() { eligibleCount = 0; activeCount = 0; }

    int EligibleCount() const { return eligibleCount; }
    int ActiveCount() const { return activeCount; }

private:
    struct ProbeCacheEntry {
        bool valid = false;
        SectorLightAtmosphereSourceKind kind =
                SectorLightAtmosphereSourceKind::StaticPoint;
        int lightId = 0;
        int ownerSectorId = 0;
        Vector3 origin = {};
        Vector3 direction = {};
        float extent = 0.0f;
        float coneRadius = 0.0f;
        SectorLightHazeStaticLightingSamples lighting;
    };

    void RefreshProbeIdentity(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes);
    const SectorLightHazeStaticLightingSamples& LightingFor(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            const SectorLightAtmosphereVolume& volume);

    std::array<ProbeCacheEntry, MaximumVolumes> probeCache{};
    const SectorBakedObjectLightProbe* cachedProbeData = nullptr;
    std::size_t cachedProbeCount = 0;
    std::size_t cachedProbeHash = 0;
    std::size_t cachedMapProbeHash = 0;
    int eligibleCount = 0;
    int activeCount = 0;
};

} // namespace game
