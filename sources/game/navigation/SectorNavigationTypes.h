#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorTopologyMap;

enum class SectorNavigationState {
    Uninitialized,
    Empty,
    Queued,
    Building,
    Ready,
    Stale,
    Failed
};

enum class SectorNavigationQueryStatus {
    Success,
    Partial,
    StartNotOnNavmesh,
    DestinationNotOnNavmesh,
    NoPath,
    CapacityExceeded,
    NavigationUnavailable,
    InvalidAgent,
    Cancelled,
    Stalled,
    TargetRemoved,
    InternalError
};

enum class SectorNavigationDiagnosticSeverity {
    Info,
    Warning,
    Error
};

enum class SectorNavigationBuildStage {
    None,
    WaitingForStaticCollision,
    BuildingInput,
    CalculatingCapacity,
    RasterizingTiles,
    BuildingDetourTiles,
    BuildingDebugCache,
    Complete
};

enum class SectorNavigationArea : uint8_t {
    Null = 0,
    Ground = 1,
    Door = 2
};

constexpr uint8_t SectorNavigationFirstGroundVariantArea = 3;
constexpr uint8_t SectorNavigationLastGroundVariantArea = 62;

constexpr bool IsSectorNavigationGroundArea(uint8_t area)
{
    return area == static_cast<uint8_t>(SectorNavigationArea::Ground)
            || (area >= SectorNavigationFirstGroundVariantArea
                && area <= SectorNavigationLastGroundVariantArea);
}

enum SectorNavigationPolyFlag : uint16_t {
    SectorNavigationPolyFlag_None = 0,
    SectorNavigationPolyFlag_Walk = 1u << 0u,
    SectorNavigationPolyFlag_Door = 1u << 1u,
    SectorNavigationPolyFlag_DoorRequiresOpening = 1u << 2u,
    SectorNavigationPolyFlag_Disabled = 1u << 15u
};

enum class SectorNavigationDoorLinkState : uint8_t {
    RequiresOpening,
    Clear,
    Disabled
};

enum class SectorNavigationDoorDirection : uint8_t {
    None,
    FrontToBack,
    BackToFront
};

enum class SectorNavigationDynamicObstacleState : uint8_t {
    Pending,
    Active,
    Removing,
    FastSuppressed,
    Failed
};

struct SectorNavigationQueryOptions {
    bool canOpenDoors = true;
};

struct SectorNavigationAgentHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

struct SectorNavigationPathHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

bool operator==(SectorNavigationAgentHandle lhs, SectorNavigationAgentHandle rhs);
bool operator==(SectorNavigationPathHandle lhs, SectorNavigationPathHandle rhs);
bool IsNull(SectorNavigationAgentHandle handle);
bool IsNull(SectorNavigationPathHandle handle);

constexpr size_t SectorNavigationMaximumPathPolygons = 256;
constexpr size_t SectorNavigationMaximumStraightPathCorners =
        SectorNavigationMaximumPathPolygons + 1;
constexpr size_t SectorNavigationMaximumCorridorTiles = 256;
constexpr size_t SectorNavigationMaximumDiagnosticMessageBytes = 512;

struct SectorNavigationTileKey {
    int x = 0;
    int y = 0;
    int layer = 0;
};

bool operator==(SectorNavigationTileKey lhs, SectorNavigationTileKey rhs);

struct SectorNavigationSettings {
    float agentRadius = 0.25f;
    float agentHeight = 1.6f;
    float agentMaximumClimb = 0.25f;
    float agentMaximumSlopeDegrees = 45.0f;
    float cellSize = 0.125f;
    float cellHeight = 0.05f;
    int tileSizeCells = 64;
    float boundsPaddingWorld = 1.0f;
    int minimumRegionSizeCells = 8;
    int mergeRegionSizeCells = 20;
    float maximumEdgeLengthWorld = 12.0f;
    float maximumSimplificationErrorCells = 1.3f;
    int maximumVerticesPerPolygon = 6;
};

struct SectorNavigationCapacitySettings {
    size_t agentCapacity = 128;
    size_t pathRecordCapacity = 128;
    size_t diagnosticCapacity = 64;
    int queryNodeCapacity = 4096;
    int maximumPathPolygons = static_cast<int>(SectorNavigationMaximumPathPolygons);
    int maximumStraightPathCorners =
            static_cast<int>(SectorNavigationMaximumStraightPathCorners);
    int tileBuildBudgetPerUpdate = 2;
    int maximumLayersPerTileCoordinate = 32;
    int maximumTotalTiles = 65536;
    int plannedMaximumPolygonsPerTile = 16384;
    int maximumCandidateTrianglesPerTile = 65536;
    size_t tileCacheTemporaryBytes = 4u * 1024u * 1024u;
    int dynamicObstacleCapacity = 256;
    int dynamicObstacleRequestBudgetPerUpdate = 8;
    int dynamicObstacleTileBudgetPerUpdate = 2;
};

struct SectorNavigationDynamicObstacleSettings {
    float positionThresholdWorld = 0.0625f;
    float yawThresholdDegrees = 5.0f;
    float slowUpdateIntervalSeconds = 0.25f;
    float settleSeconds = 0.25f;
    float fastLinearSpeedWorld = 2.0f;
    float fastAngularSpeedDegrees = 180.0f;
};

enum class SectorNavigationAvoidanceQuality : uint8_t {
    High = 3
};

struct SectorNavigationCrowdSettings {
    float maximumAcceleration = 8.0f;
    float collisionQueryRangeRadiusScale = 12.0f;
    float pathOptimizationRangeRadiusScale = 30.0f;
    float separationWeight = 2.0f;
    float reconciliationDistanceRadiusScale = 0.25f;
    float maximumStepSeconds = 1.0f / 30.0f;
    int maximumSubsteps = 8;
    SectorNavigationAvoidanceQuality avoidanceQuality =
            SectorNavigationAvoidanceQuality::High;
};

struct SectorNavigationCrowdAgentState {
    Vector2 steeredVelocity = {};
    int neighborCount = 0;
    float nearestNeighborDistance = 0.0f;
    bool attached = false;
};

struct SectorNavigationCrowdStatistics {
    size_t activeAgentCount = 0;
    uint64_t attachmentFailures = 0;
    uint64_t reconciliations = 0;
    uint64_t capacityWarnings = 0;
    int lastVelocitySampleCount = 0;
    float lastUpdateMilliseconds = 0.0f;
    float peakUpdateMilliseconds = 0.0f;
};

struct SectorNavigationQueryFilterPolicy {
    uint16_t includedFlags = SectorNavigationPolyFlag_Walk
            | SectorNavigationPolyFlag_Door;
    uint16_t excludedFlags = SectorNavigationPolyFlag_Disabled;
    float groundCost = 1.0f;
    float doorCost = 1.0f;
};

struct SectorNavigationDiagnostic {
    SectorNavigationDiagnosticSeverity severity =
            SectorNavigationDiagnosticSeverity::Info;
    SectorNavigationBuildStage stage = SectorNavigationBuildStage::None;
    std::string message;
};

struct SectorNavigationNearestPointResult {
    SectorNavigationQueryStatus status =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector3 requestedPosition = {};
    Vector3 nearestPosition = {};
};

struct SectorNavigationPathResult {
    SectorNavigationQueryStatus status =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector3 requestedStart = {};
    Vector3 requestedDestination = {};
    Vector3 projectedStart = {};
    Vector3 projectedDestination = {};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> corners{};
    std::array<uint8_t, SectorNavigationMaximumStraightPathCorners> cornerFlags{};
    std::array<int, SectorNavigationMaximumStraightPathCorners> cornerDoorIds{};
    std::array<SectorNavigationDoorDirection,
            SectorNavigationMaximumStraightPathCorners> cornerDoorDirections{};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> cornerDoorLandings{};
    size_t cornerCount = 0;
    size_t corridorPolygonCount = 0;
    std::array<SectorNavigationTileKey,
            SectorNavigationMaximumCorridorTiles> corridorTiles{};
    size_t corridorTileCount = 0;
    uint64_t tileRevision = 0;
};

struct SectorNavigationCounters {
    uint64_t successfulQueries = 0;
    uint64_t partialQueries = 0;
    uint64_t failedQueries = 0;
    uint64_t capacityWarnings = 0;
    uint64_t rebuildRequests = 0;
    uint64_t completedBuilds = 0;
    uint64_t failedBuilds = 0;
    uint64_t truncatedDiagnostics = 0;
    uint64_t droppedDiagnostics = 0;
};

struct SectorNavigationDynamicObstacleStatistics {
    size_t activeCount = 0;
    size_t pendingCount = 0;
    size_t removingCount = 0;
    size_t fastSuppressedCount = 0;
    size_t failedCount = 0;
    size_t backlogCount = 0;
    uint64_t additions = 0;
    uint64_t removals = 0;
    uint64_t transforms = 0;
    uint64_t updatedTiles = 0;
    uint64_t failures = 0;
    float lastUpdateMilliseconds = 0.0f;
    float peakUpdateMilliseconds = 0.0f;
};

struct SectorNavigationBuildStatistics {
    int tileCoordinateCount = 0;
    int tileLayerCapacity = 0;
    int builtTileCoordinateCount = 0;
    int builtLayerCount = 0;
    int navMeshTileCount = 0;
    int navMeshPolygonCount = 0;
    int tileReferenceBits = 0;
    int polygonReferenceBits = 0;
    size_t compressedLayerBytes = 0;
    size_t tileTemporaryBytes = 0;
    float lastBuildMilliseconds = 0.0f;
    float peakBuildMilliseconds = 0.0f;
    float tileWorldSize = 0.0f;
    BoundingBox worldBounds = {};
};

struct SectorNavigationDebugTriangle {
    Vector3 a = {};
    Vector3 b = {};
    Vector3 c = {};
    uint8_t area = 0;
};

struct SectorNavigationDebugSegment {
    Vector3 a = {};
    Vector3 b = {};
};

struct SectorNavigationDebugTileBounds {
    BoundingBox bounds = {};
    int tileX = 0;
    int tileY = 0;
    int layer = 0;
};

struct SectorNavigationDebugObstacle {
    int placedObjectId = 0;
    Vector2 center = {};
    Vector2 axisX = {1.0f, 0.0f};
    Vector2 axisZ = {0.0f, 1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
};

struct SectorNavigationDebugDynamicObstacle : SectorNavigationDebugObstacle {
    SectorNavigationDynamicObstacleState state =
            SectorNavigationDynamicObstacleState::Pending;
};

struct SectorNavigationDebugUpdatedTile {
    SectorNavigationTileKey key;
    uint64_t revision = 0;
};

struct SectorNavigationDebugDoorPlaceholder {
    int placedObjectId = 0;
    int lineDefId = 0;
    Vector3 a = {};
    Vector3 b = {};
    float bottom = 0.0f;
    float top = 0.0f;
};

struct SectorNavigationDebugDoorLink {
    int placedObjectId = 0;
    Vector3 frontStage = {};
    Vector3 backStage = {};
    SectorNavigationDoorLinkState state =
            SectorNavigationDoorLinkState::RequiresOpening;
    uint32_t holderCount = 0;
    bool valid = false;
};

struct SectorNavigationDebugCache {
    std::vector<SectorNavigationDebugTriangle> walkableTriangles;
    std::vector<SectorNavigationDebugSegment> polygonEdges;
    std::vector<SectorNavigationDebugTileBounds> tileBounds;
    std::vector<SectorNavigationDebugObstacle> staticObstacles;
    std::vector<SectorNavigationDebugDynamicObstacle> dynamicObstacles;
    std::vector<SectorNavigationDebugUpdatedTile> recentlyUpdatedTiles;
    std::vector<SectorNavigationDebugDoorPlaceholder> doorPlaceholders;
    std::vector<SectorNavigationDebugDoorLink> doorLinks;
    std::vector<SectorNavigationDebugSegment> stepConnections;
    uint64_t navigationRevision = 0;
    uint64_t tileRevision = 0;
};

struct SectorNavigationPosition {
    std::array<float, 3> value{};
};

SectorNavigationSettings NormalizeSectorNavigationSettings(
        SectorNavigationSettings settings);
SectorNavigationSettings BuildSectorNavigationSettingsForMap(
        const SectorTopologyMap& map);
SectorNavigationCapacitySettings NormalizeSectorNavigationCapacitySettings(
        SectorNavigationCapacitySettings settings);
SectorNavigationDynamicObstacleSettings NormalizeSectorNavigationDynamicObstacleSettings(
        SectorNavigationDynamicObstacleSettings settings);
SectorNavigationCrowdSettings NormalizeSectorNavigationCrowdSettings(
        SectorNavigationCrowdSettings settings);

SectorNavigationPosition SectorWorldToNavigationPosition(Vector3 position);
Vector3 SectorNavigationToWorldPosition(SectorNavigationPosition position);
float SectorNavigationAuthoredHeightToWorld(float authoredHeight);

const char* SectorNavigationStateName(SectorNavigationState state);
const char* SectorNavigationQueryStatusName(SectorNavigationQueryStatus status);
const char* SectorNavigationBuildStageName(SectorNavigationBuildStage stage);
const char* SectorNavigationDoorLinkStateName(SectorNavigationDoorLinkState state);
const char* SectorNavigationDoorDirectionName(SectorNavigationDoorDirection direction);
const char* SectorNavigationDynamicObstacleStateName(
        SectorNavigationDynamicObstacleState state);
const char* SectorNavigationAvoidanceQualityName(
        SectorNavigationAvoidanceQuality quality);

} // namespace game
