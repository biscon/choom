#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {
namespace {

std::uint64_t SaturatingMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left == 0 || right == 0) return 0;
    if (left > std::numeric_limits<std::uint64_t>::max() / right) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return left * right;
}

} // namespace

const char* SectorVolumetricQualityName(
        SectorTopologyFogSettings::VolumetricQuality quality)
{
    switch (quality) {
        case SectorTopologyFogSettings::VolumetricQuality::Off: return "Off";
        case SectorTopologyFogSettings::VolumetricQuality::Low: return "Low";
        case SectorTopologyFogSettings::VolumetricQuality::Medium: return "Medium";
        case SectorTopologyFogSettings::VolumetricQuality::High: return "High";
    }
    return "Unknown";
}

SectorVolumetricQualityContract GetSectorVolumetricQualityContract(
        SectorTopologyFogSettings::VolumetricQuality quality)
{
    switch (quality) {
        case SectorTopologyFogSettings::VolumetricQuality::Low:
            return SectorVolumetricQualityContract{120, 68, 32, 4, false};
        case SectorTopologyFogSettings::VolumetricQuality::Medium:
            return SectorVolumetricQualityContract{160, 90, 48, 6, true};
        case SectorTopologyFogSettings::VolumetricQuality::High:
            return SectorVolumetricQualityContract{240, 135, 64, 8, true};
        case SectorTopologyFogSettings::VolumetricQuality::Off:
            return SectorVolumetricQualityContract{};
    }
    return SectorVolumetricQualityContract{};
}

SectorLegacyAtmosphereQualityContract GetSectorLegacyAtmosphereQualityContract(
        SectorTopologyFogSettings::VolumetricQuality quality)
{
    switch (quality) {
        case SectorTopologyFogSettings::VolumetricQuality::Low:
            return SectorLegacyAtmosphereQualityContract{0.25f, 4, 4, 0.25f, 4, 2};
        case SectorTopologyFogSettings::VolumetricQuality::Medium:
            return SectorLegacyAtmosphereQualityContract{0.5f, 8, 8, 0.5f, 8, 4};
        case SectorTopologyFogSettings::VolumetricQuality::High:
            return SectorLegacyAtmosphereQualityContract{1.0f, 12, 16, 1.0f, 12, 8};
        case SectorTopologyFogSettings::VolumetricQuality::Off:
            return SectorLegacyAtmosphereQualityContract{};
    }
    return SectorLegacyAtmosphereQualityContract{};
}

SectorVolumetricGridSize ComputeSectorVolumetricGridSize(
        SectorTopologyFogSettings::VolumetricQuality quality,
        int sceneWidth,
        int sceneHeight)
{
    const SectorVolumetricQualityContract preset =
            GetSectorVolumetricQualityContract(quality);
    if (preset.depthSlices <= 0 || sceneWidth <= 0 || sceneHeight <= 0) return {};
    const double scale = std::min({
            1.0,
            static_cast<double>(preset.referenceWidth) / sceneWidth,
            static_cast<double>(preset.referenceHeight) / sceneHeight});
    return SectorVolumetricGridSize{
            std::clamp(static_cast<int>(std::lround(sceneWidth * scale)),
                    1, preset.referenceWidth),
            std::clamp(static_cast<int>(std::lround(sceneHeight * scale)),
                    1, preset.referenceHeight),
            preset.depthSlices};
}

SectorVolumetricAtlasLayout ComputeSectorVolumetricAtlasLayout(
        SectorVolumetricGridSize grid)
{
    SectorVolumetricAtlasLayout result;
    result.grid = grid;
    if (grid.x <= 0 || grid.y <= 0 || grid.z <= 0) return result;

    std::uint64_t bestMaximumDimension = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t bestArea = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t bestDifference = std::numeric_limits<std::uint64_t>::max();
    for (int columns = 1; columns <= grid.z; ++columns) {
        const int rows = (grid.z + columns - 1) / columns;
        const std::uint64_t width = static_cast<std::uint64_t>(grid.x) * columns;
        const std::uint64_t height = static_cast<std::uint64_t>(grid.y) * rows;
        const std::uint64_t maximumDimension = std::max(width, height);
        const std::uint64_t area = SaturatingMultiply(width, height);
        const std::uint64_t difference = width > height ? width - height : height - width;
        if (maximumDimension > bestMaximumDimension
                || (maximumDimension == bestMaximumDimension && area > bestArea)
                || (maximumDimension == bestMaximumDimension && area == bestArea
                        && difference >= bestDifference)) {
            continue;
        }
        bestMaximumDimension = maximumDimension;
        bestArea = area;
        bestDifference = difference;
        result.tileColumns = columns;
        result.tileRows = rows;
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
    }
    return result;
}

bool ComputeSectorVolumetricAtlasTexel(
        const SectorVolumetricAtlasLayout& layout,
        int froxelX,
        int froxelY,
        int froxelZ,
        SectorVolumetricAtlasTexel& outTexel)
{
    outTexel = {};
    if (layout.tileColumns <= 0 || layout.tileRows <= 0
            || froxelX < 0 || froxelX >= layout.grid.x
            || froxelY < 0 || froxelY >= layout.grid.y
            || froxelZ < 0 || froxelZ >= layout.grid.z) {
        return false;
    }
    const int tileX = froxelZ % layout.tileColumns;
    const int tileY = froxelZ / layout.tileColumns;
    outTexel = SectorVolumetricAtlasTexel{
            tileX * layout.grid.x + froxelX,
            tileY * layout.grid.y + froxelY};
    return outTexel.x >= 0 && outTexel.x < layout.width
            && outTexel.y >= 0 && outTexel.y < layout.height;
}

SectorVolumetricClusterListLayout ComputeSectorVolumetricClusterListLayout(
        SectorVolumetricGridSize grid,
        int clusterBands)
{
    if (grid.x <= 0 || grid.y <= 0 || clusterBands <= 0) return {};
    const int width = grid.x * SectorVolumetricClusterListTexels;
    const int height = grid.y * clusterBands;
    return SectorVolumetricClusterListLayout{
            width,
            height,
            EstimateSectorAtmosphereTargetBytes(width, height, 4)};
}

std::uint64_t EstimateSectorAtmosphereTargetBytes(
        int width,
        int height,
        int bytesPerPixel)
{
    if (width <= 0 || height <= 0 || bytesPerPixel <= 0) return 0;
    return SaturatingMultiply(
            SaturatingMultiply(static_cast<std::uint64_t>(width),
                    static_cast<std::uint64_t>(height)),
            static_cast<std::uint64_t>(bytesPerPixel));
}

} // namespace game
