#pragma once

#include "sector_demo/SectorTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorLightmapChart {
    int surfaceIndex = -1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int usableX = 0;
    int usableY = 0;
    int usableWidth = 0;
    int usableHeight = 0;
    std::vector<Vector2> vertexUvs;
};

struct SectorLightmapLayout {
    int atlasWidth = 2048;
    int atlasHeight = 2048;
    int gutter = 2;
    float texelsPerWorldUnit = 8.0f;
    std::vector<SectorLightmapChart> charts;
};

struct SectorLightmapBakeResult {
    int width = 0;
    int height = 0;
    std::string sourceHash;
};

enum class SectorLightmapStatus {
    None,
    Valid,
    Stale
};

constexpr int SectorLightmapAtlasWidth = 2048;
constexpr int SectorLightmapAtlasHeight = 2048;
constexpr int SectorLightmapGutterTexels = 2;
constexpr float SectorLightmapTexelsPerWorldUnit = 8.0f;
constexpr int kSectorLightmapBakeVersion = 2;
constexpr int kDirectSoftShadowSampleCount = 8;
constexpr int kAmbientOcclusionSampleCount = 12;

bool BuildSectorLightmapLayout(
        const SectorMap& map,
        SectorLightmapLayout& outLayout,
        std::string& outError);

bool BakeSectorLightmap(
        const SectorMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        SectorLightmapBakeResult& outResult,
        std::string& outError);

std::string ComputeSectorLightmapSourceHash(const SectorMap& map);
SectorLightmapStatus GetSectorLightmapStatus(const SectorMap& map);
const char* SectorLightmapStatusText(SectorLightmapStatus status);
std::string ResolveSectorAssetPath(const std::string& path);
std::string MakeSectorLightmapPathForMapPath(const std::string& mapPath);
std::string MakeSectorAssetRelativePath(const std::string& path);

} // namespace game
