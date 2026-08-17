#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <cstdint>

namespace game {

inline constexpr int SectorVolumetricMaximumViewLights = 254;
inline constexpr int SectorVolumetricMaximumViewVolumes = 254;
inline constexpr int SectorVolumetricMaximumClusterLights = 16;
inline constexpr int SectorVolumetricMaximumClusterVolumes = 16;
inline constexpr int SectorVolumetricLightRecordTexels = 4;
inline constexpr int SectorVolumetricClusterListTexels = 4;
inline constexpr int SectorVolumetricFirstReservedIndex = 254;
inline constexpr int SectorVolumetricListTerminator = 255;
inline constexpr float SectorVolumetricPrototypeMaximumDistanceWorld = 32.0f;
inline constexpr float SectorVolumetricPrototypeAnisotropy = 0.20f;
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

struct SectorLegacyAtmosphereQualityContract {
    float localFogTargetScale = 0.0f;
    int localFogMarchSteps = 0;
    int maximumLocalFogVolumes = 0;
    float hazeTargetScale = 0.0f;
    int hazeMarchSteps = 0;
    int maximumHazeVolumes = 0;
};

const char* SectorVolumetricQualityName(
        SectorTopologyFogSettings::LocalVolumeQuality quality);
SectorVolumetricQualityContract GetSectorVolumetricQualityContract(
        SectorTopologyFogSettings::LocalVolumeQuality quality);
SectorLegacyAtmosphereQualityContract GetSectorLegacyAtmosphereQualityContract(
        SectorTopologyFogSettings::LocalVolumeQuality quality);
SectorVolumetricGridSize ComputeSectorVolumetricGridSize(
        SectorTopologyFogSettings::LocalVolumeQuality quality,
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
SectorVolumetricClusterListLayout ComputeSectorVolumetricClusterListLayout(
        SectorVolumetricGridSize grid,
        int clusterBands);
std::uint64_t EstimateSectorAtmosphereTargetBytes(
        int width,
        int height,
        int bytesPerPixel);

} // namespace game
