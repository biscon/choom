#include "sector_demo/SectorLightmapReport.h"

#include "sector_demo/SectorLightmap.h"

#include <raylib.h>

#include <cstdint>
#include <sstream>

namespace game {

std::string FormatSectorLightmapBakeReport(const SectorLightmapBakeResult& result)
{
    const double atlasPixels = static_cast<double>(result.width) * static_cast<double>(result.height);
    const double validAtlasOccupancy = atlasPixels > 0.0
            ? (static_cast<double>(result.validChartTexels) / atlasPixels) * 100.0
            : 0.0;
    const double chartRectangleOccupancy = atlasPixels > 0.0
            ? (static_cast<double>(result.allocatedChartRectanglePixels) / atlasPixels) * 100.0
            : 0.0;
    const double chartPayloadEfficiency = result.allocatedChartRectanglePixels > 0
            ? (static_cast<double>(result.validChartTexels) / static_cast<double>(result.allocatedChartRectanglePixels)) * 100.0
            : 0.0;
    const auto averageTriangleTestsPerRay = [](const SectorLightmapRaycastStats& stats) {
        return stats.raysCast > 0
                ? static_cast<double>(stats.triangleTests) / static_cast<double>(stats.raysCast)
                : 0.0;
    };
    const uint64_t totalRays = result.directHardShadowStats.raysCast
            + result.softShadowSourceStats.raysCast
            + result.ambientOcclusionStats.raysCast
            + result.indirectBounceStats.raysCast;
    const uint64_t totalTriangleTests = result.directHardShadowStats.triangleTests
            + result.softShadowSourceStats.triangleTests
            + result.ambientOcclusionStats.triangleTests
            + result.indirectBounceStats.triangleTests;
    const uint64_t totalLogicalSelfHitsIgnored = result.directHardShadowStats.logicalSelfHitsIgnored
            + result.softShadowSourceStats.logicalSelfHitsIgnored
            + result.ambientOcclusionStats.logicalSelfHitsIgnored
            + result.indirectBounceStats.logicalSelfHitsIgnored;
    const double totalAverageTriangleTestsPerRay = totalRays > 0
            ? static_cast<double>(totalTriangleTests) / static_cast<double>(totalRays)
            : 0.0;

    std::ostringstream report;
    report << "Lightmap bake report\n";
    report << TextFormat("  Atlas: %d x %d\n", result.width, result.height);
    report << TextFormat("  Atlas pixels: %llu\n", static_cast<unsigned long long>(static_cast<uint64_t>(result.width) * static_cast<uint64_t>(result.height)));
    report << TextFormat("  Valid chart texels: %d\n", result.validChartTexels);
    report << TextFormat("  Valid atlas occupancy: %.2f%%\n", validAtlasOccupancy);
    report << TextFormat("  Allocated chart rectangle pixels: %d\n", result.allocatedChartRectanglePixels);
    report << TextFormat("  Chart rectangle occupancy: %.2f%%\n", chartRectangleOccupancy);
    report << TextFormat("  Chart payload efficiency: %.2f%%\n", chartPayloadEfficiency);
    report << TextFormat("  Static geometry triangles: %d\n", result.staticGeometryTriangles);
    report << TextFormat("  BVH nodes: %d\n", result.bvhNodes);
    report << TextFormat("  BVH leaves: %d\n", result.bvhLeaves);
    report << TextFormat("  BVH leaf triangle limit: %d\n", result.bvhLeafTriangleLimit);
    report << TextFormat("  Average triangles per leaf: %.2f\n", result.bvhAverageTrianglesPerLeaf);
    report << TextFormat("  Max triangles in leaf: %d\n", result.bvhMaxTrianglesInLeaf);
    report << TextFormat(
            "  Static lights: %d (%d point, %d spot)\n\n",
            result.staticLightCount,
            result.staticLightCount - result.staticSpotLightCount,
            result.staticSpotLightCount);
    report << TextFormat("  Object light probes: %d\n", result.objectProbes.count);
    report << TextFormat("  Object probe placement diagnostics: %d\n", result.objectProbePlacementDiagnostics);
    if (!result.objectProbes.path.empty()) {
        report << TextFormat("  Object probe sidecar: %s\n", result.objectProbes.path.c_str());
    }
    report << "\n";
    auto appendRayStats = [&](const char* label, const SectorLightmapRaycastStats& stats) {
        report << TextFormat("  %s: %llu\n", label, static_cast<unsigned long long>(stats.raysCast));
        report << TextFormat("    AABB tests: %llu\n", static_cast<unsigned long long>(stats.aabbTests));
        report << TextFormat("    AABB hits: %llu\n", static_cast<unsigned long long>(stats.aabbHits));
        report << TextFormat("    Triangle tests: %llu\n", static_cast<unsigned long long>(stats.triangleTests));
        report << TextFormat("    Triangle hits: %llu\n", static_cast<unsigned long long>(stats.triangleHits));
        report << TextFormat("    Logical source-surface self hits ignored: %llu\n", static_cast<unsigned long long>(stats.logicalSelfHitsIgnored));
        report << TextFormat("    Average triangle tests/ray: %.2f\n", averageTriangleTestsPerRay(stats));
    };
    appendRayStats("Direct hard-shadow rays", result.directHardShadowStats);
    report << "\n";
    appendRayStats("Soft-shadow source rays", result.softShadowSourceStats);
    report << "\n";
    appendRayStats("AO rays", result.ambientOcclusionStats);
    report << "\n";
    appendRayStats("Indirect bounce rays", result.indirectBounceStats);
    report << "\n";
    report << TextFormat("  Total rays: %llu\n", static_cast<unsigned long long>(totalRays));
    report << TextFormat("  Total triangle tests: %llu\n", static_cast<unsigned long long>(totalTriangleTests));
    report << TextFormat("  Total logical source-surface self hits ignored: %llu\n", static_cast<unsigned long long>(totalLogicalSelfHitsIgnored));
    report << TextFormat("  Average triangle tests/ray: %.2f\n\n", totalAverageTriangleTestsPerRay);
    report << TextFormat("  Layout: %.2fs\n", result.layoutSeconds);
    report << TextFormat("  BVH build: %.2fs\n", result.bvhBuildSeconds);
    report << TextFormat("  Direct lighting: %.2fs\n", result.directLightingSeconds);
    report << TextFormat("  AO: %.2fs\n", result.ambientOcclusionSeconds);
    report << TextFormat("  Indirect bounce: %.2fs\n", result.indirectBounceSeconds);
    report << TextFormat("  Object probe bake: %.2fs\n", result.objectProbeBakeSeconds);
    report << TextFormat("  Object probe sidecar write: %.2fs\n", result.objectProbeSidecarWriteSeconds);
    report << TextFormat("  Gutter dilation/export: %.2fs\n", result.gutterExportSeconds);
    report << TextFormat("  Total bake: %.2fs", result.totalBakeSeconds);
    return report.str();
}

void PrintSectorLightmapBakeReport(const SectorLightmapBakeResult& result)
{
    const std::string report = FormatSectorLightmapBakeReport(result);
    std::istringstream stream(report);
    std::string line;
    while (std::getline(stream, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
}

} // namespace game
