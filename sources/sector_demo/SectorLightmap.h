#pragma once

#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
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

struct SectorLightmapAlphaOccluderTriangle {
    Vector3 worldPosition0 = {};
    Vector3 worldPosition1 = {};
    Vector3 worldPosition2 = {};
    Vector3 normal = {};
    Vector2 uv0 = {};
    Vector2 uv1 = {};
    Vector2 uv2 = {};
    std::string textureId;
    float alphaCutoff = 0.5f;
    SectorGeneratedSurfaceRef surfaceRef;
    int sourceSurfaceIndex = -1;
    int triangleIndex = -1;
};

struct SectorLightmapAlphaSample {
    bool valid = false;
    bool opaque = true;
    unsigned char alpha = 255;
    int width = 0;
    int height = 0;
};

class SectorLightmapAlphaMaskCache {
public:
    SectorLightmapAlphaSample Sample(
            const SectorTopologyMap& map,
            const std::string& textureId,
            Vector2 uv,
            float alphaCutoff);

    size_t CachedTextureCount() const;
    int LoadAttemptCount(const SectorTopologyMap& map, const std::string& textureId) const;

private:
    struct AlphaMask {
        bool valid = false;
        int width = 0;
        int height = 0;
        std::vector<unsigned char> alpha;
    };

    const AlphaMask& LoadOrGet(const SectorTopologyMap& map, const std::string& textureId);
    static std::string CacheKey(const SectorTopologyMap& map, const std::string& textureId);

    std::unordered_map<std::string, AlphaMask> masksByKey;
    std::unordered_map<std::string, int> loadAttemptsByKey;
};

struct SectorLightmapRaycastStats {
    uint64_t raysCast = 0;
    uint64_t aabbTests = 0;
    uint64_t aabbHits = 0;
    uint64_t triangleTests = 0;
    uint64_t triangleHits = 0;
    uint64_t logicalSelfHitsIgnored = 0;
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
    int staticSpotLightCount = 0;
    long long directShadowRays = 0;
    long long softShadowSourceRays = 0;
    long long ambientOcclusionRays = 0;
    long long indirectBounceRays = 0;
    SectorLightmapRaycastStats directHardShadowStats;
    SectorLightmapRaycastStats softShadowSourceStats;
    SectorLightmapRaycastStats ambientOcclusionStats;
    SectorLightmapRaycastStats indirectBounceStats;
    SectorBakedObjectLightProbeMetadata objectProbes;
    int objectProbePlacementDiagnostics = 0;
    double layoutSeconds = 0.0;
    double bvhBuildSeconds = 0.0;
    double directLightingSeconds = 0.0;
    double ambientOcclusionSeconds = 0.0;
    double indirectBounceSeconds = 0.0;
    double objectProbeBakeSeconds = 0.0;
    double objectProbeSidecarWriteSeconds = 0.0;
    double gutterExportSeconds = 0.0;
    double totalBakeSeconds = 0.0;
};

enum class SectorLightmapBakePhase {
    Idle,
    Preparing,
    BuildingLayout,
    BuildingBvh,
    DirectLighting,
    AmbientOcclusion,
    IndirectBounce,
    DilatingAndEncoding,
    InstallingResult,
    Completed,
    Cancelled,
    Failed
};

struct SectorLightmapBakeCallbacks {
    std::function<void(SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork)> onProgress;
    std::function<bool()> isCancellationRequested;
};

struct SectorBakedObjectLightProbePlacementDiagnostic {
    int sectorId = -1;
    std::string message;
};

struct SectorTopologyLightmapBakeInput {
    SectorTopologyMap mapSnapshot;
    std::string expectedSourceHash;
    std::string finalOutputPath;
    std::string temporaryOutputPath;
    uint64_t editorMapRevision = 0;
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
// Version 10: baked output includes object lighting probe sidecar data.
constexpr int kSectorLightmapBakeVersion = 10;
constexpr int kSectorBakedObjectLightProbeSidecarVersion = 1;
constexpr const char* kSectorBakedObjectLightProbeSidecarFormat = "ambientCubeF32LE";
constexpr float kObjectProbeAdjacentPortalBlendDistanceWorld = 1.0f;
constexpr int kObjectProbeMaxAdjacentBlendSectors = 8;
constexpr int kDirectSoftShadowSampleCount = 8;
constexpr int kAmbientOcclusionSampleCount = 12;
constexpr int kIndirectBounceSampleCount = 8;
constexpr float kNeutralBounceAlbedo = 0.55f;

bool BuildSectorLightmapLayout(
        const SectorTopologyMap& map,
        SectorLightmapLayout& outLayout,
        std::string& outError);

std::vector<SectorLightmapAlphaOccluderTriangle> CollectSectorLightmapAlphaOccluders(
        const SectorGeneratedGeometry& geometry);

bool BuildSectorBakedObjectLightProbePlacements(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbePlacementSettings& settings,
        std::vector<SectorBakedObjectLightProbe>& outProbes,
        std::vector<SectorBakedObjectLightProbePlacementDiagnostic>* outDiagnostics,
        std::string& outError);
bool BakeSectorBakedObjectLightProbeAmbientCubes(
        const SectorTopologyMap& map,
        std::vector<SectorBakedObjectLightProbe>& probes,
        std::string& outError);

bool IsSectorLightmapStaticRayOccludedForTests(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout& layout,
        Ray ray,
        float maxDistance);

bool BakeSectorLightmap(
        const SectorTopologyMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        SectorLightmapBakeResult& outResult,
        std::string& outError);

bool BakeSectorLightmap(
        const SectorTopologyMap& map,
        const SectorLightmapLayout& layout,
        const char* outputPath,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError);

bool BakeSectorLightmap(
        const SectorTopologyLightmapBakeInput& input,
        const SectorLightmapBakeCallbacks& callbacks,
        SectorLightmapBakeResult& outResult,
        std::string& outError);

std::string FormatSectorLightmapBakeReport(const SectorLightmapBakeResult& result);
void PrintSectorLightmapBakeReport(const SectorLightmapBakeResult& result);
std::string ComputeSectorLightmapSourceHash(const SectorTopologyMap& map);
SectorLightmapStatus GetSectorLightmapStatus(const SectorTopologyMap& map);
SectorLightmapStatus GetSectorBakedObjectLightProbeStatus(const SectorTopologyMap& map);
const char* SectorLightmapStatusText(SectorLightmapStatus status);
bool IsSameLogicalSectorLightmapSurface(
        const SectorGeneratedSurfaceRef& a,
        const SectorGeneratedSurfaceRef& b);
bool WriteSectorBakedObjectLightProbeSidecar(
        const std::string& path,
        const std::vector<SectorBakedObjectLightProbe>& probes,
        float probeSpacingWorld,
        float probeHeightWorld,
        std::string& outError);
bool ReadSectorBakedObjectLightProbeSidecar(
        const std::string& path,
        const SectorBakedObjectLightProbeMetadata* expectedMetadata,
        std::vector<SectorBakedObjectLightProbe>& outProbes,
        SectorBakedObjectLightProbeMetadata& outMetadata,
        std::string& outError);
bool LoadSectorBakedObjectLightProbeRuntimeData(
        const SectorTopologyMap& map,
        SectorBakedObjectLightProbeRuntimeData& outData,
        std::string& outError);
BakedObjectLightingSample SampleBakedObjectLighting(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        const SectorTopologyMap* mapForFallback);
std::string ResolveSectorAssetPath(const std::string& path);
std::string MakeSectorLightmapPathForMapPath(const std::string& mapPath);
std::string MakeSectorObjectProbeSidecarPathForLightmapPath(const std::string& lightmapPath);
std::string MakeSectorAssetRelativePath(const std::string& path);

} // namespace game
