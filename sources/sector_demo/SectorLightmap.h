#pragma once

#include "sector_demo/SectorTypes.h"

#include <raylib.h>

#include <cstdint>
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

struct SectorLightmapRaycastStats {
    uint64_t raysCast = 0;
    uint64_t aabbTests = 0;
    uint64_t aabbHits = 0;
    uint64_t triangleTests = 0;
    uint64_t triangleHits = 0;
};

struct SectorLightmapBakeResult {
    int width = 0;
    int height = 0;
    std::string sourceHash;
    int validChartTexels = 0;
    int allocatedChartRectanglePixels = 0;
    int staticGeometryTriangles = 0;
    int bvhNodes = 0;
    int bvhLeaves = 0;
    int bvhLeafTriangleLimit = 0;
    double bvhAverageTrianglesPerLeaf = 0.0;
    int bvhMaxTrianglesInLeaf = 0;
    int staticLightCount = 0;
    long long directShadowRays = 0;
    long long softShadowSourceRays = 0;
    long long ambientOcclusionRays = 0;
    long long indirectBounceRays = 0;
    SectorLightmapRaycastStats directHardShadowStats;
    SectorLightmapRaycastStats softShadowSourceStats;
    SectorLightmapRaycastStats ambientOcclusionStats;
    SectorLightmapRaycastStats indirectBounceStats;
    double layoutSeconds = 0.0;
    double bvhBuildSeconds = 0.0;
    double directLightingSeconds = 0.0;
    double ambientOcclusionSeconds = 0.0;
    double indirectBounceSeconds = 0.0;
    double gutterExportSeconds = 0.0;
    double totalBakeSeconds = 0.0;
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
constexpr int kSectorLightmapBakeVersion = 3;
constexpr int kDirectSoftShadowSampleCount = 8;
constexpr int kAmbientOcclusionSampleCount = 12;
constexpr int kIndirectBounceSampleCount = 8;
constexpr float kNeutralBounceAlbedo = 0.55f;

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

void PrintSectorLightmapBakeReport(const SectorLightmapBakeResult& result);
std::string ComputeSectorLightmapSourceHash(const SectorMap& map);
SectorLightmapStatus GetSectorLightmapStatus(const SectorMap& map);
const char* SectorLightmapStatusText(SectorLightmapStatus status);
std::string ResolveSectorAssetPath(const std::string& path);
std::string MakeSectorLightmapPathForMapPath(const std::string& mapPath);
std::string MakeSectorAssetRelativePath(const std::string& path);

} // namespace game
