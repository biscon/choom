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

bool ComputeSectorVolumetricFroxel(
        const SectorVolumetricAtlasLayout& layout,
        int atlasX,
        int atlasY,
        int& outFroxelX,
        int& outFroxelY,
        int& outFroxelZ)
{
    outFroxelX = 0;
    outFroxelY = 0;
    outFroxelZ = 0;
    if (layout.grid.x <= 0 || layout.grid.y <= 0
            || layout.tileColumns <= 0 || layout.tileRows <= 0
            || atlasX < 0 || atlasX >= layout.width
            || atlasY < 0 || atlasY >= layout.height) {
        return false;
    }
    const int tileX = atlasX / layout.grid.x;
    const int tileY = atlasY / layout.grid.y;
    const int slice = tileY * layout.tileColumns + tileX;
    if (slice < 0 || slice >= layout.grid.z) return false;
    outFroxelX = atlasX % layout.grid.x;
    outFroxelY = atlasY % layout.grid.y;
    outFroxelZ = slice;
    return true;
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

SectorVolumetricResourceLayout ComputeSectorVolumetricResourceLayout(
        SectorTopologyFogSettings::VolumetricQuality quality,
        int sceneWidth,
        int sceneHeight)
{
    SectorVolumetricResourceLayout result;
    result.quality = quality;
    result.grid = ComputeSectorVolumetricGridSize(
            quality, sceneWidth, sceneHeight);
    result.atlas = ComputeSectorVolumetricAtlasLayout(result.grid);
    const SectorVolumetricQualityContract contract =
            GetSectorVolumetricQualityContract(quality);
    result.clusters = ComputeSectorVolumetricClusterListLayout(
            result.grid, contract.clusterBands);
    result.integratedWidth = result.grid.x;
    result.integratedHeight = result.grid.y;
    return result;
}

bool SectorVolumetricResourceLayoutFitsTextureLimit(
        const SectorVolumetricResourceLayout& layout,
        int maximumTextureSize)
{
    if (layout.quality == SectorTopologyFogSettings::VolumetricQuality::Off) {
        return false;
    }
    const auto fits = [maximumTextureSize](int width, int height) {
        return maximumTextureSize > 0 && width > 0 && height > 0
                && width <= maximumTextureSize && height <= maximumTextureSize;
    };
    return fits(layout.atlas.width, layout.atlas.height)
            && fits(layout.clusters.width, layout.clusters.height)
            && fits(layout.integratedWidth, layout.integratedHeight)
            && fits(layout.lightDataWidth, layout.lightDataHeight)
            && fits(layout.volumeDataWidth, layout.volumeDataHeight);
}

SectorVolumetricResourceLayout ResolveSectorVolumetricResourceLayout(
        SectorTopologyFogSettings::VolumetricQuality requestedQuality,
        int sceneWidth,
        int sceneHeight,
        int maximumTextureSize)
{
    int quality = static_cast<int>(requestedQuality);
    for (; quality >= static_cast<int>(
                    SectorTopologyFogSettings::VolumetricQuality::Low);
         --quality) {
        const auto candidate = ComputeSectorVolumetricResourceLayout(
                static_cast<SectorTopologyFogSettings::VolumetricQuality>(quality),
                sceneWidth,
                sceneHeight);
        if (SectorVolumetricResourceLayoutFitsTextureLimit(
                    candidate, maximumTextureSize)) {
            return candidate;
        }
    }
    return ComputeSectorVolumetricResourceLayout(
            SectorTopologyFogSettings::VolumetricQuality::Off,
            sceneWidth,
            sceneHeight);
}

bool ComputeSectorVolumetricDepthSliceLayout(
        float startDistance,
        float endDistance,
        int sliceCount,
        int clusterBandCount,
        SectorVolumetricDepthSliceLayout& outLayout)
{
    outLayout = {};
    if (!std::isfinite(startDistance) || !std::isfinite(endDistance)
            || startDistance <= 0.0f || endDistance <= startDistance
            || sliceCount <= 0 || sliceCount > 64
            || clusterBandCount <= 0
            || sliceCount != clusterBandCount * 8) {
        return false;
    }
    outLayout.sliceCount = sliceCount;
    outLayout.clusterBandCount = clusterBandCount;
    outLayout.endpoints[0] = startDistance;
    const double logStart = std::log(static_cast<double>(startDistance));
    const double logEnd = std::log(static_cast<double>(endDistance));
    for (int index = 1; index < sliceCount; ++index) {
        const double t = static_cast<double>(index)
                / static_cast<double>(sliceCount);
        const double value = std::exp(logStart + (logEnd - logStart) * t);
        if (!std::isfinite(value)) {
            outLayout = {};
            return false;
        }
        outLayout.endpoints[static_cast<std::size_t>(index)] =
                std::clamp(static_cast<float>(value), startDistance, endDistance);
    }
    outLayout.endpoints[static_cast<std::size_t>(sliceCount)] = endDistance;
    return true;
}

int FindSectorVolumetricDepthSlice(
        const SectorVolumetricDepthSliceLayout& layout,
        float viewDepth)
{
    if (layout.sliceCount <= 0 || !std::isfinite(viewDepth)
            || viewDepth < layout.endpoints[0]
            || viewDepth > layout.endpoints[
                    static_cast<std::size_t>(layout.sliceCount)]) {
        return -1;
    }
    if (viewDepth == layout.endpoints[
                static_cast<std::size_t>(layout.sliceCount)]) {
        return layout.sliceCount - 1;
    }
    const auto begin = layout.endpoints.begin();
    const auto end = begin + layout.sliceCount + 1;
    const auto upper = std::upper_bound(begin, end, viewDepth);
    return std::clamp(static_cast<int>(upper - begin) - 1,
            0, layout.sliceCount - 1);
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
