#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace game {

struct SectorVolumetricProjectedClusterBounds {
    float gridMinimumX = 0.0f;
    float gridMaximumX = 0.0f;
    float gridMinimumY = 0.0f;
    float gridMaximumY = 0.0f;
    float minimumDepth = 0.0f;
    float maximumDepth = 0.0f;
    int minimumX = 0;
    int maximumX = -1;
    int minimumY = 0;
    int maximumY = -1;
    int minimumBand = 0;
    int maximumBand = -1;
};

struct SectorVolumetricClusterCamera {
    Camera3D camera = {};
    float aspectRatio = 1.0f;
    SectorVolumetricGridSize grid;
    SectorVolumetricDepthSliceLayout depth;
};

struct SectorVolumetricGpuLightRecord {
    SectorLightAtmosphereSourceKind kind =
            SectorLightAtmosphereSourceKind::StaticPoint;
    int stableId = 0;
    Vector3 positionWorld = {};
    Vector3 directionWorld = {0.0f, -1.0f, 0.0f};
    Vector3 linearColor = {};
    float rangeWorld = 0.0f;
    float effectiveIntensity = 0.0f;
    float scatteringIntensity = 0.0f;
    float innerConeCos = -1.0f;
    float outerConeCos = -1.0f;
    int shadowSlot = -1;
    SectorVolumetricProjectedClusterBounds projected;
    float viewImportance = 0.0f;
};

struct SectorVolumetricGpuVolumeRecord {
    int stableId = -1;
    int topologySectorId = -1;
    Vector3 centerWorld = {};
    Vector3 radiiWorld = {};
    Vector3 linearTint = {};
    float density = 0.0f;
    float maximumOpacity = 0.0f;
    float edgeSoftness = 0.0f;
    float noiseAmount = 0.0f;
    float noiseScaleWorld = 1.0f;
    float flowDirectionRadians = 0.0f;
    float flowSpeedWorld = 0.0f;
    SectorVolumetricProjectedClusterBounds projected;
    float viewDistanceSquared = 0.0f;
};

struct SectorVolumetricClusterBuildDiagnostics {
    int eligibleLightCount = 0;
    int retainedLightCount = 0;
    int lightViewOverflowCount = 0;
    std::uint64_t lightClusterOverflowCount = 0;
    int eligibleVolumeCount = 0;
    int retainedVolumeCount = 0;
    int volumeViewOverflowCount = 0;
    std::uint64_t volumeClusterOverflowCount = 0;
};

bool ProjectSectorVolumetricSphereToClusters(
        const SectorVolumetricClusterCamera& camera,
        Vector3 centerWorld,
        float radiusWorld,
        SectorVolumetricProjectedClusterBounds& outBounds);

class SectorVolumetricClusterBuilder {
public:
    bool Configure(
            SectorVolumetricGridSize grid,
            int clusterBandCount);
    bool Build(
            const SectorTopologyMap& map,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const SectorPreviewDynamicPointLightSource* runtimePointLight,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds,
            const Camera3D& camera,
            float aspectRatio,
            const SectorVolumetricDepthSliceLayout& depth,
            float runtimeSeconds,
            bool dynamicLightingEnabled);
    void ResetFrame();
    void Clear();

    const std::array<SectorVolumetricGpuLightRecord,
            SectorVolumetricMaximumViewLights>& Lights() const { return lights; }
    const std::array<SectorVolumetricGpuVolumeRecord,
            SectorVolumetricMaximumViewVolumes>& Volumes() const { return volumes; }
    const std::vector<std::uint8_t>& LightClusterIndices() const {
        return lightClusterIndices;
    }
    const std::vector<std::uint8_t>& VolumeClusterIndices() const {
        return volumeClusterIndices;
    }
    const SectorVolumetricClusterBuildDiagnostics& Diagnostics() const {
        return diagnostics;
    }
    std::size_t ClusterCount() const { return clusterCount; }
    std::size_t LightIndexCapacity() const { return lightClusterIndices.capacity(); }
    std::size_t VolumeIndexCapacity() const { return volumeClusterIndices.capacity(); }

private:
    bool InsertLightView(const SectorVolumetricGpuLightRecord& record);
    bool InsertVolumeView(const SectorVolumetricGpuVolumeRecord& record);
    void BuildClusterCenters(const SectorVolumetricClusterCamera& camera);
    void BuildLightLists(const SectorVolumetricClusterCamera& camera);
    void BuildVolumeLists(const SectorVolumetricClusterCamera& camera);

    SectorVolumetricGridSize configuredGrid;
    int configuredBands = 0;
    std::size_t clusterCount = 0;
    std::array<SectorVolumetricGpuLightRecord,
            SectorVolumetricMaximumViewLights> lights{};
    std::array<SectorVolumetricGpuVolumeRecord,
            SectorVolumetricMaximumViewVolumes> volumes{};
    std::vector<std::uint8_t> lightClusterIndices;
    std::vector<std::uint8_t> volumeClusterIndices;
    std::vector<float> lightClusterImportance;
    std::vector<float> volumeClusterDistance;
    std::vector<Vector3> clusterCenters;
    SectorVolumetricClusterBuildDiagnostics diagnostics;
};

} // namespace game
