#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <array>
#include <cstdint>

namespace game {

inline constexpr int SectorVolumetricMaximumViewLights = 254;
inline constexpr int SectorVolumetricMaximumViewVolumes = 254;
inline constexpr int SectorVolumetricMaximumClusterLights = 16;
inline constexpr int SectorVolumetricMaximumClusterVolumes = 16;
inline constexpr int SectorVolumetricLightRecordTexels = 4;
inline constexpr int SectorVolumetricVolumeRecordTexels = 5;
inline constexpr int SectorVolumetricClusterListTexels = 4;
inline constexpr int SectorVolumetricFirstReservedIndex = 254;
inline constexpr int SectorVolumetricListTerminator = 255;
static_assert(SectorVolumetricMaximumViewLights
        == SectorVolumetricFirstReservedIndex);
static_assert(SectorVolumetricMaximumViewVolumes
        == SectorVolumetricFirstReservedIndex);
static_assert(SectorVolumetricMaximumClusterLights
        == SectorVolumetricClusterListTexels * 4);
static_assert(SectorVolumetricMaximumClusterVolumes
        == SectorVolumetricClusterListTexels * 4);

struct SectorVolumetricGridSize {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct SectorVolumetricQualityContract {
    int referenceWidth = 0;
    int referenceHeight = 0;
    int depthSlices = 0;
    int clusterBands = 0;
    bool temporalResolve = false;
};

struct SectorVolumetricTemporalPolicy {
    bool enabled = false;
    int jitterPeriod = 1;
    float baseCurrentFrameWeight = 1.0f;
    float responsiveCurrentFrameWeight = 1.0f;
};

enum class SectorVolumetricDebugView {
    Composite,
    Froxels,
    HistoryWeight
};

const char* SectorVolumetricDebugViewName(SectorVolumetricDebugView view);

struct SectorVolumetricAtlasLayout {
    SectorVolumetricGridSize grid;
    int tileColumns = 0;
    int tileRows = 0;
    int width = 0;
    int height = 0;
};

struct SectorVolumetricAtlasTexel {
    int x = 0;
    int y = 0;
};

struct SectorVolumetricClusterListLayout {
    int width = 0;
    int height = 0;
    std::uint64_t estimatedBytes = 0;
};

struct SectorVolumetricDepthSliceLayout {
    int sliceCount = 0;
    int clusterBandCount = 0;
    std::array<float, 65> endpoints{};
};

struct SectorVolumetricResourceLayout {
    SectorTopologyFogSettings::VolumetricQuality quality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    SectorVolumetricGridSize grid;
    SectorVolumetricAtlasLayout atlas;
    SectorVolumetricClusterListLayout clusters;
    int integratedWidth = 0;
    int integratedHeight = 0;
    int lightDataWidth = SectorVolumetricLightRecordTexels;
    int lightDataHeight = SectorVolumetricMaximumViewLights;
    int volumeDataWidth = SectorVolumetricVolumeRecordTexels;
    int volumeDataHeight = SectorVolumetricMaximumViewVolumes;
};

struct SectorLegacyAtmosphereQualityContract {
    float localFogTargetScale = 0.0f;
    int localFogMarchSteps = 0;
    int maximumLocalFogVolumes = 0;
    float hazeTargetScale = 0.0f;
    int hazeMarchSteps = 0;
    int maximumHazeVolumes = 0;
};

const char* SectorVolumetricQualityName(
        SectorTopologyFogSettings::VolumetricQuality quality);
SectorVolumetricQualityContract GetSectorVolumetricQualityContract(
        SectorTopologyFogSettings::VolumetricQuality quality);
SectorVolumetricTemporalPolicy GetSectorVolumetricTemporalPolicy(
        SectorTopologyFogSettings::VolumetricQuality quality);
SectorLegacyAtmosphereQualityContract GetSectorLegacyAtmosphereQualityContract(
        SectorTopologyFogSettings::VolumetricQuality quality);
SectorVolumetricGridSize ComputeSectorVolumetricGridSize(
        SectorTopologyFogSettings::VolumetricQuality quality,
        int sceneWidth,
        int sceneHeight);
SectorVolumetricAtlasLayout ComputeSectorVolumetricAtlasLayout(
        SectorVolumetricGridSize grid);
bool ComputeSectorVolumetricAtlasTexel(
        const SectorVolumetricAtlasLayout& layout,
        int froxelX,
        int froxelY,
        int froxelZ,
        SectorVolumetricAtlasTexel& outTexel);
bool ComputeSectorVolumetricFroxel(
        const SectorVolumetricAtlasLayout& layout,
        int atlasX,
        int atlasY,
        int& outFroxelX,
        int& outFroxelY,
        int& outFroxelZ);
SectorVolumetricClusterListLayout ComputeSectorVolumetricClusterListLayout(
        SectorVolumetricGridSize grid,
        int clusterBands);
SectorVolumetricResourceLayout ComputeSectorVolumetricResourceLayout(
        SectorTopologyFogSettings::VolumetricQuality quality,
        int sceneWidth,
        int sceneHeight);
bool SectorVolumetricResourceLayoutFitsTextureLimit(
        const SectorVolumetricResourceLayout& layout,
        int maximumTextureSize);
SectorVolumetricResourceLayout ResolveSectorVolumetricResourceLayout(
        SectorTopologyFogSettings::VolumetricQuality requestedQuality,
        int sceneWidth,
        int sceneHeight,
        int maximumTextureSize);
bool ComputeSectorVolumetricDepthSliceLayout(
        float startDistance,
        float endDistance,
        int sliceCount,
        int clusterBandCount,
        SectorVolumetricDepthSliceLayout& outLayout);
int FindSectorVolumetricDepthSlice(
        const SectorVolumetricDepthSliceLayout& layout,
        float viewDepth);
std::uint64_t EstimateSectorAtmosphereTargetBytes(
        int width,
        int height,
        int bytesPerPixel);

} // namespace game
