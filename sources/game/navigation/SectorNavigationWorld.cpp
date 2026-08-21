#include "game/navigation/SectorNavigationWorld.h"

#include "game/navigation/SectorNavigationBuildInput.h"
#include "game/navigation/SectorNavigationCompression.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourCrowd.h"
#include "DetourStatus.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "Recast.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>

#include <raymath.h>

namespace game {
namespace {

uint32_t NextGeneration(uint32_t generation)
{
    ++generation;
    return generation == 0 ? 1 : generation;
}

struct RecordSlot {
    uint32_t generation = 1;
    int crowdIndex = -1;
    bool crowdWarningReported = false;
    bool occupied = false;
};

struct NavigationTileCacheAllocator final : dtTileCacheAlloc {
    std::vector<uint8_t> memory;
    size_t used = 0;
    size_t peak = 0;

    void Reserve(size_t bytes)
    {
        memory.resize(bytes);
        used = 0;
        peak = 0;
    }

    void reset() override { used = 0; }

    void* alloc(size_t size) override
    {
        const size_t aligned = (used + 15u) & ~size_t(15u);
        if (aligned > memory.size() || size > memory.size() - aligned) return nullptr;
        used = aligned + size;
        peak = std::max(peak, used);
        return memory.data() + aligned;
    }

    void free(void*) override {}
};

struct NavigationTileCacheCompressor final : dtTileCacheCompressor {
    int maxCompressedSize(const int bufferSize) override
    {
        return SectorNavigationMaximumCompressedLayerSize(bufferSize);
    }

    dtStatus compress(
            const unsigned char* buffer,
            const int bufferSize,
            unsigned char* compressed,
            const int maxCompressedSize,
            int* compressedSize) override
    {
        int resultSize = 0;
        if (compressedSize == nullptr
            || !CompressSectorNavigationLayer(buffer, bufferSize, compressed,
                    maxCompressedSize, resultSize)) {
            return DT_FAILURE | DT_INVALID_PARAM;
        }
        *compressedSize = resultSize;
        return DT_SUCCESS;
    }

    dtStatus decompress(
            const unsigned char* compressed,
            const int compressedSize,
            unsigned char* buffer,
            const int maxBufferSize,
            int* bufferSize) override
    {
        int resultSize = 0;
        if (bufferSize == nullptr
            || !DecompressSectorNavigationLayer(compressed, compressedSize, buffer,
                    maxBufferSize, resultSize)) {
            return DT_FAILURE | DT_WRONG_MAGIC;
        }
        *bufferSize = resultSize;
        return DT_SUCCESS;
    }
};

struct NavigationTileCacheMeshProcess final : dtTileCacheMeshProcess {
    std::vector<float> connectionVertices;
    std::vector<float> connectionRadii;
    std::vector<unsigned short> connectionFlags;
    std::vector<unsigned char> connectionAreas;
    std::vector<unsigned char> connectionDirections;
    std::vector<unsigned int> connectionUserIds;

    void Configure(const std::vector<SectorNavigationBuildDoorLink>& links)
    {
        connectionVertices.clear();
        connectionRadii.clear();
        connectionFlags.clear();
        connectionAreas.clear();
        connectionDirections.clear();
        connectionUserIds.clear();
        connectionVertices.reserve(links.size() * 6u);
        connectionRadii.reserve(links.size());
        connectionFlags.reserve(links.size());
        connectionAreas.reserve(links.size());
        connectionDirections.reserve(links.size());
        connectionUserIds.reserve(links.size());
        for (const SectorNavigationBuildDoorLink& link : links) {
            connectionVertices.insert(connectionVertices.end(), {
                    link.frontStage.x, link.frontStage.y, link.frontStage.z,
                    link.backStage.x, link.backStage.y, link.backStage.z});
            connectionRadii.push_back(link.radius);
            connectionFlags.push_back(SectorNavigationPolyFlag_Walk
                    | SectorNavigationPolyFlag_Door
                    | SectorNavigationPolyFlag_DoorRequiresOpening);
            connectionAreas.push_back(static_cast<unsigned char>(SectorNavigationArea::Door));
            connectionDirections.push_back(DT_OFFMESH_CON_BIDIR);
            connectionUserIds.push_back(static_cast<unsigned int>(link.placedObjectId));
        }
    }

    void process(
            dtNavMeshCreateParams* params,
            unsigned char* polyAreas,
            unsigned short* polyFlags) override
    {
        for (int index = 0; index < params->polyCount; ++index) {
            if (polyAreas[index] == DT_TILECACHE_WALKABLE_AREA) {
                polyAreas[index] = static_cast<unsigned char>(SectorNavigationArea::Ground);
            }
            if (IsSectorNavigationGroundArea(polyAreas[index])) {
                polyFlags[index] = SectorNavigationPolyFlag_Walk;
            } else if (polyAreas[index] == static_cast<unsigned char>(SectorNavigationArea::Door)) {
                polyFlags[index] = SectorNavigationPolyFlag_Walk
                        | SectorNavigationPolyFlag_Door;
            } else {
                polyFlags[index] = SectorNavigationPolyFlag_None;
            }
        }
        params->offMeshConVerts = connectionVertices.empty()
                ? nullptr : connectionVertices.data();
        params->offMeshConRad = connectionRadii.empty()
                ? nullptr : connectionRadii.data();
        params->offMeshConFlags = connectionFlags.empty()
                ? nullptr : connectionFlags.data();
        params->offMeshConAreas = connectionAreas.empty()
                ? nullptr : connectionAreas.data();
        params->offMeshConDir = connectionDirections.empty()
                ? nullptr : connectionDirections.data();
        params->offMeshConUserID = connectionUserIds.empty()
                ? nullptr : connectionUserIds.data();
        params->offMeshConCount = static_cast<int>(connectionUserIds.size());
    }
};

struct NavigationTileCoordinate {
    int x = 0;
    int y = 0;
};

struct HeightfieldDeleter {
    void operator()(rcHeightfield* value) const { rcFreeHeightField(value); }
};
struct CompactHeightfieldDeleter {
    void operator()(rcCompactHeightfield* value) const { rcFreeCompactHeightfield(value); }
};
struct LayerSetDeleter {
    void operator()(rcHeightfieldLayerSet* value) const { rcFreeHeightfieldLayerSet(value); }
};

bool TriangleOverlapsBoundsXZ(
        const SectorNavigationBuildTriangle& triangle,
        const float* minimum,
        const float* maximum)
{
    const float triangleMinX = std::min({triangle.a.x, triangle.b.x, triangle.c.x});
    const float triangleMaxX = std::max({triangle.a.x, triangle.b.x, triangle.c.x});
    const float triangleMinZ = std::min({triangle.a.z, triangle.b.z, triangle.c.z});
    const float triangleMaxZ = std::max({triangle.a.z, triangle.b.z, triangle.c.z});
    return triangleMaxX >= minimum[0] && triangleMinX <= maximum[0]
            && triangleMaxZ >= minimum[2] && triangleMinZ <= maximum[2];
}

dtQueryFilter MakeQueryFilter(const SectorNavigationQueryFilterPolicy& policy)
{
    dtQueryFilter filter;
    filter.setIncludeFlags(policy.includedFlags);
    filter.setExcludeFlags(policy.excludedFlags);
    filter.setAreaCost(static_cast<int>(SectorNavigationArea::Ground), policy.groundCost);
    for (uint8_t area = SectorNavigationFirstGroundVariantArea;
            area <= SectorNavigationLastGroundVariantArea; ++area) {
        filter.setAreaCost(static_cast<int>(area), policy.groundCost);
    }
    filter.setAreaCost(static_cast<int>(SectorNavigationArea::Door), policy.doorCost);
    return filter;
}

dtQueryFilter MakeQueryFilter(
        const SectorNavigationQueryFilterPolicy& policy,
        SectorNavigationQueryOptions options)
{
    SectorNavigationQueryFilterPolicy effective = policy;
    if (!options.canOpenDoors) {
        effective.excludedFlags |= SectorNavigationPolyFlag_DoorRequiresOpening;
    }
    return MakeQueryFilter(effective);
}

Vector3 FromFloat3(const float* value)
{
    return {value[0], value[1], value[2]};
}

bool ProjectionWithinExtents(const float* requested, const float* projected, const float* extents)
{
    return std::fabs(projected[0] - requested[0]) <= extents[0] + 0.0001f
            && std::fabs(projected[1] - requested[1]) <= extents[1] + 0.0001f
            && std::fabs(projected[2] - requested[2]) <= extents[2] + 0.0001f;
}

} // namespace

struct SectorNavigationWorld::Impl {
    struct RuntimeDoorLink {
        int placedObjectId = 0;
        dtPolyRef polygon = 0;
        SectorNavigationDoorLinkState state =
                SectorNavigationDoorLinkState::RequiresOpening;
        uint32_t holderCount = 0;
    };
    enum class ObstaclePhase : uint8_t {
        PendingAdd,
        Active,
        PendingRemove,
        FastSuppressed,
        Failed
    };
    struct RuntimeObstacle {
        int placedObjectId = 0;
        SectorStaticModelCollider desired;
        SectorStaticModelCollider committed;
        SectorStaticModelCollider lastObserved;
        dtObstacleRef reference = 0;
        ObstaclePhase phase = ObstaclePhase::PendingAdd;
        float updateSeconds = 0.0f;
        float settleSeconds = 0.0f;
        bool seen = false;
        bool addAfterRemove = false;
        bool deleteAfterRemove = false;
    };
    struct TileRuntimeRevision {
        SectorNavigationTileKey key;
        dtTileRef reference = 0;
        uint64_t revision = 0;
    };
    struct DebugTileChunk {
        SectorNavigationTileKey key;
        SectorNavigationDebugTileBounds bounds;
        std::vector<SectorNavigationDebugTriangle> triangles;
        std::vector<SectorNavigationDebugSegment> edges;
        std::vector<SectorNavigationDebugSegment> stepConnections;
        int polygonCount = 0;
        bool populated = false;
    };
    SectorNavigationSettings settings;
    SectorNavigationCapacitySettings capacities;
    SectorNavigationDynamicObstacleSettings dynamicObstacleSettings;
    SectorNavigationCrowdSettings crowdSettings;
    SectorNavigationQueryFilterPolicy filterPolicy;
    SectorNavigationState state = SectorNavigationState::Uninitialized;
    SectorNavigationBuildStage stage = SectorNavigationBuildStage::None;
    SectorNavigationCounters counters;
    SectorNavigationDynamicObstacleStatistics dynamicObstacleStatistics;
    SectorNavigationCrowdStatistics crowdStatistics;
    SectorNavigationBuildStatistics statistics;
    SectorNavigationDebugCache debugCache;
    std::vector<SectorNavigationDiagnostic> diagnostics;
    std::vector<RecordSlot> agentSlots;
    std::vector<RecordSlot> pathSlots;
    SectorNavigationBuildInput buildInput;
    std::vector<NavigationTileCoordinate> tileCoordinates;
    std::vector<RuntimeDoorLink> doorLinks;
    std::vector<RuntimeObstacle> obstacles;
    std::vector<SectorNavigationTileKey> pendingAffectedTiles;
    std::vector<TileRuntimeRevision> tileRevisions;
    std::vector<int> seenObstacleIdsScratch;
    std::vector<SectorNavigationTileKey> affectedTilesScratch;
    std::vector<dtTileRef> tileReferencesScratch;
    std::vector<RuntimeDoorLink> doorLinksScratch;
    std::vector<SectorNavigationTileKey> changedTilesScratch;
    std::vector<DebugTileChunk> debugTileChunks;
    size_t nextTileCoordinate = 0;
    uint64_t sourceRevision = 0;
    uint64_t buildRevision = 0;
    uint64_t tileRevision = 0;
    uint64_t debugRevision = 0;
    uint64_t sourceHash = 0;
    bool agentGrowthWarned = false;
    bool pathGrowthWarned = false;
    bool obstacleGrowthWarned = false;
    bool diagnosticOverflowWarned = false;
    bool duplicateObstacleWarningReported = false;
    bool tileCacheUpToDate = true;
    bool buildTimingActive = false;
    std::chrono::steady_clock::time_point buildStartedAt;

    dtNavMesh* navMesh = nullptr;
    dtTileCache* tileCache = nullptr;
    dtNavMeshQuery* query = nullptr;
    dtCrowd* crowd = nullptr;
    NavigationTileCacheAllocator tileAllocator;
    NavigationTileCacheCompressor tileCompressor;
    NavigationTileCacheMeshProcess meshProcess;

    ~Impl() { ReleaseNavigation(); }

    void BumpDebugRevision()
    {
        ++debugRevision;
        if (debugRevision == 0) ++debugRevision;
        debugCache.navigationRevision = debugRevision;
        debugCache.tileRevision = tileRevision;
    }

    void FinishBuildTiming()
    {
        if (!buildTimingActive) return;
        const auto elapsed = std::chrono::steady_clock::now() - buildStartedAt;
        statistics.lastBuildMilliseconds =
                std::chrono::duration<float, std::milli>(elapsed).count();
        statistics.peakBuildMilliseconds = std::max(
                statistics.peakBuildMilliseconds,
                statistics.lastBuildMilliseconds);
        buildTimingActive = false;
    }

    void ReleaseNavigation()
    {
        if (crowd != nullptr) dtFreeCrowd(crowd);
        if (query != nullptr) dtFreeNavMeshQuery(query);
        if (tileCache != nullptr) dtFreeTileCache(tileCache);
        if (navMesh != nullptr) dtFreeNavMesh(navMesh);
        crowd = nullptr;
        query = nullptr;
        tileCache = nullptr;
        navMesh = nullptr;
        buildInput = {};
        tileCoordinates.clear();
        doorLinks.clear();
        obstacles.clear();
        pendingAffectedTiles.clear();
        tileRevisions.clear();
        seenObstacleIdsScratch.clear();
        affectedTilesScratch.clear();
        tileReferencesScratch.clear();
        doorLinksScratch.clear();
        changedTilesScratch.clear();
        debugTileChunks.clear();
        meshProcess.Configure({});
        nextTileCoordinate = 0;
        statistics = {};
        debugCache = {};
        debugCache.navigationRevision = debugRevision;
        sourceHash = 0;
        tileRevision = 0;
        dynamicObstacleStatistics = {};
        crowdStatistics = {};
        tileCacheUpToDate = true;
        buildTimingActive = false;
        duplicateObstacleWarningReported = false;
    }

    void Record(
            SectorNavigationDiagnosticSeverity severity,
            SectorNavigationBuildStage diagnosticStage,
            const std::string& message)
    {
        std::string boundedMessage = message;
        if (boundedMessage.size() > SectorNavigationMaximumDiagnosticMessageBytes) {
            constexpr const char suffix[] = "... [truncated]";
            boundedMessage.resize(
                    SectorNavigationMaximumDiagnosticMessageBytes - sizeof(suffix) + 1u);
            boundedMessage += suffix;
            ++counters.truncatedDiagnostics;
        }
        if (diagnostics.size() >= capacities.diagnosticCapacity) {
            ++counters.capacityWarnings;
            ++counters.droppedDiagnostics;
            if (!diagnosticOverflowWarned) {
                diagnosticOverflowWarned = true;
                std::fprintf(stderr,
                        "[Navigation WARNING] Diagnostic capacity exceeded; oldest diagnostic will be discarded.\n");
            }
            if (!diagnostics.empty()) diagnostics.erase(diagnostics.begin());
        }
        diagnostics.push_back({severity, diagnosticStage, std::move(boundedMessage)});
    }

    struct DiagnosticRecastContext final : rcContext {
        Impl& owner;

        explicit DiagnosticRecastContext(Impl& owner)
            : rcContext(true), owner(owner)
        {
        }

        void doLog(rcLogCategory category, const char* message, int length) override
        {
            if (category == RC_LOG_PROGRESS || message == nullptr || length <= 0) return;
            owner.Record(
                    category == RC_LOG_ERROR
                            ? SectorNavigationDiagnosticSeverity::Error
                            : SectorNavigationDiagnosticSeverity::Warning,
                    owner.stage,
                    std::string(message, static_cast<size_t>(length)));
        }
    };

    template <typename Handle>
    Handle Allocate(std::vector<RecordSlot>& slots, bool& growthWarned, const char* label)
    {
        for (uint32_t index = 0; index < slots.size(); ++index) {
            RecordSlot& slot = slots[index];
            if (!slot.occupied) {
                slot.occupied = true;
                slot.crowdIndex = -1;
                slot.crowdWarningReported = false;
                return Handle{index, slot.generation};
            }
        }
        if (slots.size() >= std::numeric_limits<uint32_t>::max()) return Handle{};
        if (slots.size() == slots.capacity() && !growthWarned) {
            growthWarned = true;
            ++counters.capacityWarnings;
            std::fprintf(stderr,
                    "[Navigation WARNING] %s capacity exceeded; runtime allocation may occur.\n",
                    label);
        }
        slots.push_back({});
        slots.back().occupied = true;
        return Handle{static_cast<uint32_t>(slots.size() - 1), slots.back().generation};
    }

    template <typename Handle>
    bool IsValid(const std::vector<RecordSlot>& slots, Handle handle) const
    {
        return !IsNull(handle)
                && handle.index < slots.size()
                && slots[handle.index].occupied
                && slots[handle.index].generation == handle.generation;
    }

    template <typename Handle>
    bool Release(std::vector<RecordSlot>& slots, Handle handle)
    {
        if (IsNull(handle) || handle.index >= slots.size()) return false;
        RecordSlot& slot = slots[handle.index];
        if (!slot.occupied || slot.generation != handle.generation) return false;
        slot.occupied = false;
        slot.crowdIndex = -1;
        slot.crowdWarningReported = false;
        slot.generation = NextGeneration(slot.generation);
        return true;
    }

    void InvalidateRecords()
    {
        for (RecordSlot& slot : agentSlots) {
            if (crowd != nullptr && slot.crowdIndex >= 0) {
                crowd->removeAgent(slot.crowdIndex);
            }
            slot.crowdIndex = -1;
            slot.occupied = false;
            slot.generation = NextGeneration(slot.generation);
        }
        for (RecordSlot& slot : pathSlots) {
            slot.occupied = false;
            slot.generation = NextGeneration(slot.generation);
        }
    }

    static bool ValidDynamicCollider(const SectorStaticModelCollider& collider)
    {
        return collider.placedObjectId > 0
                && collider.resolved
                && !collider.failed
                && std::isfinite(collider.center.x)
                && std::isfinite(collider.center.y)
                && std::isfinite(collider.axisX.x)
                && std::isfinite(collider.axisX.y)
                && std::isfinite(collider.axisZ.x)
                && std::isfinite(collider.axisZ.y)
                && std::isfinite(collider.halfExtents.x)
                && std::isfinite(collider.halfExtents.y)
                && std::isfinite(collider.bottom)
                && std::isfinite(collider.top)
                && collider.halfExtents.x > 0.0001f
                && collider.halfExtents.y > 0.0001f
                && collider.top > collider.bottom + 0.0001f;
    }

    float DynamicYaw(const SectorStaticModelCollider& collider) const
    {
        return std::atan2(collider.axisZ.x, collider.axisX.x);
    }

    float YawDistance(float a, float b) const
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float TwoPi = Pi * 2.0f;
        float delta = std::fmod(b - a, TwoPi);
        if (delta > Pi) delta -= TwoPi;
        if (delta < -Pi) delta += TwoPi;
        return std::fabs(delta);
    }

    bool MeaningfullyDifferent(
            const SectorStaticModelCollider& a,
            const SectorStaticModelCollider& b) const
    {
        const float threshold = dynamicObstacleSettings.positionThresholdWorld;
        const float yawThreshold = dynamicObstacleSettings.yawThresholdDegrees
                * 3.14159265358979323846f / 180.0f;
        const float dx = a.center.x - b.center.x;
        const float dz = a.center.y - b.center.y;
        return std::sqrt(dx * dx + dz * dz) >= threshold
                || std::fabs(a.halfExtents.x - b.halfExtents.x) >= threshold
                || std::fabs(a.halfExtents.y - b.halfExtents.y) >= threshold
                || std::fabs(a.bottom - b.bottom) >= threshold
                || std::fabs(a.top - b.top) >= threshold
                || YawDistance(DynamicYaw(a), DynamicYaw(b)) >= yawThreshold;
    }

    void ObstacleBounds(
            const SectorStaticModelCollider& collider,
            float* center,
            float* halfExtents,
            float* bmin,
            float* bmax) const
    {
        const float ex = collider.halfExtents.x + settings.agentRadius;
        const float ez = collider.halfExtents.y + settings.agentRadius;
        const float bottom = collider.bottom - settings.agentHeight
                - settings.cellHeight;
        const float top = collider.top + settings.cellHeight;
        center[0] = collider.center.x;
        center[1] = (bottom + top) * 0.5f;
        center[2] = collider.center.y;
        halfExtents[0] = ex;
        halfExtents[1] = (top - bottom) * 0.5f;
        halfExtents[2] = ez;
        const float hx = std::fabs(collider.axisX.x) * ex
                + std::fabs(collider.axisZ.x) * ez;
        const float hz = std::fabs(collider.axisX.y) * ex
                + std::fabs(collider.axisZ.y) * ez;
        bmin[0] = collider.center.x - hx;
        bmin[1] = bottom;
        bmin[2] = collider.center.y - hz;
        bmax[0] = collider.center.x + hx;
        bmax[1] = top;
        bmax[2] = collider.center.y + hz;
    }

    bool CollectAffectedTiles(
            const SectorStaticModelCollider& collider,
            std::vector<SectorNavigationTileKey>& out,
            bool diagnoseOversize)
    {
        if (tileCache == nullptr) return false;
        float center[3]{};
        float halfExtents[3]{};
        float bmin[3]{};
        float bmax[3]{};
        ObstacleBounds(collider, center, halfExtents, bmin, bmax);
        std::array<dtCompressedTileRef, DT_MAX_TOUCHED_TILES + 1> refs{};
        int count = 0;
        const dtStatus status = tileCache->queryTiles(
                bmin, bmax, refs.data(), &count, static_cast<int>(refs.size()));
        if (dtStatusFailed(status)) return false;
        if (count > DT_MAX_TOUCHED_TILES) {
            if (diagnoseOversize) {
                ++dynamicObstacleStatistics.failures;
                Record(SectorNavigationDiagnosticSeverity::Warning,
                        SectorNavigationBuildStage::Complete,
                        "Dynamic obstacle "
                                + std::to_string(collider.placedObjectId)
                                + " touches more than the supported eight TileCache layers; it remains collision-only");
            }
            return false;
        }
        for (int index = 0; index < count; ++index) {
            const dtCompressedTile* tile = tileCache->getTileByRef(refs[index]);
            if (tile == nullptr || tile->header == nullptr) continue;
            const SectorNavigationTileKey key{
                    tile->header->tx, tile->header->ty, tile->header->tlayer};
            if (std::find(out.begin(), out.end(), key) == out.end()) {
                out.push_back(key);
            }
        }
        return true;
    }

    void AppendPendingAffectedTiles(const SectorStaticModelCollider& collider)
    {
        affectedTilesScratch.clear();
        if (!CollectAffectedTiles(collider, affectedTilesScratch, false)) return;
        for (const SectorNavigationTileKey key : affectedTilesScratch) {
            if (std::find(pendingAffectedTiles.begin(),
                        pendingAffectedTiles.end(), key)
                    == pendingAffectedTiles.end()) {
                pendingAffectedTiles.push_back(key);
            }
        }
    }

    TileRuntimeRevision* FindTileRevision(SectorNavigationTileKey key)
    {
        const auto found = std::find_if(
                tileRevisions.begin(), tileRevisions.end(),
                [key](const TileRuntimeRevision& tile) {
                    return tile.key == key;
                });
        return found == tileRevisions.end() ? nullptr : &*found;
    }

    void InitializeTileRevisions()
    {
        tileRevisions.clear();
        tileRevisions.reserve(debugCache.tileBounds.size());
        for (const SectorNavigationDebugTileBounds& tile : debugCache.tileBounds) {
            const SectorNavigationTileKey key{tile.tileX, tile.tileY, tile.layer};
            tileRevisions.push_back({key,
                    navMesh != nullptr
                            ? navMesh->getTileRefAt(key.x, key.y, key.layer) : 0,
                    tileRevision});
        }
    }

    SectorNavigationDynamicObstacleState DebugState(
            const RuntimeObstacle& obstacle) const
    {
        switch (obstacle.phase) {
            case ObstaclePhase::PendingAdd:
                return SectorNavigationDynamicObstacleState::Pending;
            case ObstaclePhase::Active:
                return SectorNavigationDynamicObstacleState::Active;
            case ObstaclePhase::PendingRemove:
                return SectorNavigationDynamicObstacleState::Removing;
            case ObstaclePhase::FastSuppressed:
                return SectorNavigationDynamicObstacleState::FastSuppressed;
            case ObstaclePhase::Failed:
                return SectorNavigationDynamicObstacleState::Failed;
        }
        return SectorNavigationDynamicObstacleState::Failed;
    }

    void RefreshDynamicObstacleDebugAndStatistics()
    {
        const uint64_t additions = dynamicObstacleStatistics.additions;
        const uint64_t removals = dynamicObstacleStatistics.removals;
        const uint64_t transforms = dynamicObstacleStatistics.transforms;
        const uint64_t updatedTiles = dynamicObstacleStatistics.updatedTiles;
        const uint64_t failures = dynamicObstacleStatistics.failures;
        const float lastMilliseconds =
                dynamicObstacleStatistics.lastUpdateMilliseconds;
        const float peakMilliseconds =
                dynamicObstacleStatistics.peakUpdateMilliseconds;
        dynamicObstacleStatistics = {};
        dynamicObstacleStatistics.additions = additions;
        dynamicObstacleStatistics.removals = removals;
        dynamicObstacleStatistics.transforms = transforms;
        dynamicObstacleStatistics.updatedTiles = updatedTiles;
        dynamicObstacleStatistics.failures = failures;
        dynamicObstacleStatistics.lastUpdateMilliseconds = lastMilliseconds;
        dynamicObstacleStatistics.peakUpdateMilliseconds = peakMilliseconds;
        debugCache.dynamicObstacles.clear();
        debugCache.dynamicObstacles.reserve(obstacles.size());
        for (const RuntimeObstacle& obstacle : obstacles) {
            switch (obstacle.phase) {
                case ObstaclePhase::PendingAdd:
                    ++dynamicObstacleStatistics.pendingCount;
                    break;
                case ObstaclePhase::Active:
                    ++dynamicObstacleStatistics.activeCount;
                    break;
                case ObstaclePhase::PendingRemove:
                    ++dynamicObstacleStatistics.removingCount;
                    break;
                case ObstaclePhase::FastSuppressed:
                    ++dynamicObstacleStatistics.fastSuppressedCount;
                    break;
                case ObstaclePhase::Failed:
                    ++dynamicObstacleStatistics.failedCount;
                    break;
            }
            if (obstacle.phase != ObstaclePhase::Active
                    && obstacle.phase != ObstaclePhase::FastSuppressed
                    && obstacle.phase != ObstaclePhase::Failed) {
                ++dynamicObstacleStatistics.backlogCount;
            }
            const SectorStaticModelCollider& collider = obstacle.desired;
            SectorNavigationDebugDynamicObstacle debug;
            debug.placedObjectId = collider.placedObjectId;
            debug.center = collider.center;
            debug.axisX = collider.axisX;
            debug.axisZ = collider.axisZ;
            debug.halfExtents = {
                    collider.halfExtents.x + settings.agentRadius,
                    collider.halfExtents.y + settings.agentRadius};
            debug.bottom = collider.bottom - settings.agentHeight
                    - settings.cellHeight;
            debug.top = collider.top + settings.cellHeight;
            debug.state = DebugState(obstacle);
            debugCache.dynamicObstacles.push_back(debug);
        }
    }

    bool BeginBuild(
            const SectorTopologyMap& map,
            const std::vector<SectorStaticModelCollider>& colliders,
            std::string& error)
    {
        const float peakBuildMilliseconds = statistics.peakBuildMilliseconds;
        ReleaseNavigation();
        statistics.peakBuildMilliseconds = peakBuildMilliseconds;
        buildStartedAt = std::chrono::steady_clock::now();
        buildTimingActive = true;
        diagnostics.clear();
        diagnosticOverflowWarned = false;
        stage = SectorNavigationBuildStage::BuildingInput;
        std::vector<std::string> warnings;
        const bool inputBuilt = BuildSectorNavigationBuildInput(
                map, colliders, settings, buildInput, warnings, error);
        sourceHash = buildInput.sourceHash;
        if (!inputBuilt) {
            return false;
        }
        meshProcess.Configure(buildInput.doorLinks);
        for (const std::string& warning : warnings) {
            Record(SectorNavigationDiagnosticSeverity::Warning, stage, warning);
        }
        if (buildInput.triangles.empty()) {
            state = SectorNavigationState::Empty;
            stage = SectorNavigationBuildStage::None;
            ++buildRevision;
            ++counters.completedBuilds;
            FinishBuildTiming();
            BumpDebugRevision();
            return true;
        }

        stage = SectorNavigationBuildStage::CalculatingCapacity;
        const float tileWorldSize = settings.cellSize * settings.tileSizeCells;
        const float width = buildInput.bounds.max.x - buildInput.bounds.min.x;
        const float depth = buildInput.bounds.max.z - buildInput.bounds.min.z;
        const int tileCountX = std::max(1, static_cast<int>(std::ceil(width / tileWorldSize)));
        const int tileCountY = std::max(1, static_cast<int>(std::ceil(depth / tileWorldSize)));
        const int64_t coordinateCount = static_cast<int64_t>(tileCountX) * tileCountY;
        const int64_t tileCapacity = coordinateCount * capacities.maximumLayersPerTileCoordinate;
        if (coordinateCount > std::numeric_limits<int>::max()
            || tileCapacity > capacities.maximumTotalTiles) {
            std::ostringstream message;
            message << "Navigation bounds require " << coordinateCount
                    << " tile coordinates and up to " << tileCapacity
                    << " layers, exceeding the configured "
                    << capacities.maximumTotalTiles << " tile limit";
            error = message.str();
            return false;
        }
        if (capacities.plannedMaximumPolygonsPerTile > (1 << DT_POLY_BITS)) {
            error = "Navigation polygon capacity exceeds the 64-bit Detour polygon reference limit";
            return false;
        }

        tileCoordinates.reserve(static_cast<size_t>(coordinateCount));
        for (int y = 0; y < tileCountY; ++y) {
            for (int x = 0; x < tileCountX; ++x) tileCoordinates.push_back({x, y});
        }
        statistics.tileCoordinateCount = static_cast<int>(coordinateCount);
        statistics.tileLayerCapacity = static_cast<int>(tileCapacity);
        statistics.tileWorldSize = tileWorldSize;
        statistics.worldBounds = buildInput.bounds;
        statistics.tileReferenceBits = DT_TILE_BITS;
        statistics.polygonReferenceBits = DT_POLY_BITS;
        statistics.tileTemporaryBytes = 0;

        navMesh = dtAllocNavMesh();
        tileCache = dtAllocTileCache();
        if (navMesh == nullptr || tileCache == nullptr) {
            error = "Could not allocate Detour navigation objects";
            return false;
        }
        dtNavMeshParams navParams{};
        navParams.orig[0] = buildInput.bounds.min.x;
        navParams.orig[1] = buildInput.bounds.min.y;
        navParams.orig[2] = buildInput.bounds.min.z;
        navParams.tileWidth = tileWorldSize;
        navParams.tileHeight = tileWorldSize;
        navParams.maxTiles = static_cast<int>(tileCapacity);
        navParams.maxPolys = capacities.plannedMaximumPolygonsPerTile;
        if (dtStatusFailed(navMesh->init(&navParams))) {
            error = "Detour navmesh rejected calculated tile/polygon capacities";
            return false;
        }

        tileAllocator.Reserve(capacities.tileCacheTemporaryBytes);
        dtTileCacheParams cacheParams{};
        std::memcpy(cacheParams.orig, navParams.orig, sizeof(cacheParams.orig));
        cacheParams.cs = settings.cellSize;
        cacheParams.ch = settings.cellHeight;
        cacheParams.width = settings.tileSizeCells;
        cacheParams.height = settings.tileSizeCells;
        cacheParams.walkableHeight = settings.agentHeight;
        cacheParams.walkableRadius = settings.agentRadius;
        cacheParams.walkableClimb = settings.agentMaximumClimb;
        cacheParams.maxSimplificationError = settings.maximumSimplificationErrorCells;
        cacheParams.maxTiles = static_cast<int>(tileCapacity);
        cacheParams.maxObstacles = capacities.dynamicObstacleCapacity;
        if (dtStatusFailed(tileCache->init(&cacheParams, &tileAllocator,
                    &tileCompressor, &meshProcess))) {
            error = "Detour TileCache rejected calculated build capacities";
            return false;
        }
        state = SectorNavigationState::Building;
        stage = SectorNavigationBuildStage::RasterizingTiles;
        return true;
    }

    bool BuildTile(NavigationTileCoordinate coordinate, std::string& error)
    {
        rcConfig config{};
        config.cs = settings.cellSize;
        config.ch = settings.cellHeight;
        config.walkableSlopeAngle = settings.agentMaximumSlopeDegrees;
        config.walkableHeight = static_cast<int>(std::ceil(settings.agentHeight / config.ch));
        config.walkableClimb = static_cast<int>(std::floor(settings.agentMaximumClimb / config.ch));
        config.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / config.cs));
        config.maxEdgeLen = static_cast<int>(settings.maximumEdgeLengthWorld / config.cs);
        config.maxSimplificationError = settings.maximumSimplificationErrorCells;
        config.minRegionArea = settings.minimumRegionSizeCells * settings.minimumRegionSizeCells;
        config.mergeRegionArea = settings.mergeRegionSizeCells * settings.mergeRegionSizeCells;
        config.maxVertsPerPoly = settings.maximumVerticesPerPolygon;
        config.tileSize = settings.tileSizeCells;
        config.borderSize = config.walkableRadius + 3;
        config.width = config.tileSize + config.borderSize * 2;
        config.height = config.tileSize + config.borderSize * 2;
        const float tileWorldSize = config.tileSize * config.cs;
        const float borderWorld = config.borderSize * config.cs;
        config.bmin[0] = buildInput.bounds.min.x + coordinate.x * tileWorldSize - borderWorld;
        config.bmin[1] = buildInput.bounds.min.y;
        config.bmin[2] = buildInput.bounds.min.z + coordinate.y * tileWorldSize - borderWorld;
        config.bmax[0] = config.bmin[0] + config.width * config.cs;
        config.bmax[1] = buildInput.bounds.max.y;
        config.bmax[2] = config.bmin[2] + config.height * config.cs;

        int candidates = 0;
        for (const SectorNavigationBuildTriangle& triangle : buildInput.triangles) {
            if (TriangleOverlapsBoundsXZ(triangle, config.bmin, config.bmax)) ++candidates;
        }
        if (candidates > capacities.maximumCandidateTrianglesPerTile) {
            error = "Navigation tile (" + std::to_string(coordinate.x) + ","
                    + std::to_string(coordinate.y) + ") has " + std::to_string(candidates)
                    + " candidate triangles, exceeding the configured per-tile limit";
            return false;
        }
        if (candidates == 0) {
            ++statistics.builtTileCoordinateCount;
            return true;
        }

        DiagnosticRecastContext context(*this);
        std::unique_ptr<rcHeightfield, HeightfieldDeleter> heightfield(rcAllocHeightfield());
        if (!heightfield || !rcCreateHeightfield(&context, *heightfield,
                    config.width, config.height, config.bmin, config.bmax,
                    config.cs, config.ch)) {
            error = "Could not allocate navigation tile heightfield";
            return false;
        }
        for (const SectorNavigationBuildTriangle& triangle : buildInput.triangles) {
            if (!TriangleOverlapsBoundsXZ(triangle, config.bmin, config.bmax)) continue;
            const float a[3]{triangle.a.x, triangle.a.y, triangle.a.z};
            const float b[3]{triangle.b.x, triangle.b.y, triangle.b.z};
            const float c[3]{triangle.c.x, triangle.c.y, triangle.c.z};
            if (!rcRasterizeTriangle(&context, a, b, c, triangle.area,
                        *heightfield, config.walkableClimb)) {
                error = "Navigation triangle rasterization ran out of memory";
                return false;
            }
        }
        rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *heightfield);
        rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *heightfield);
        rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *heightfield);

        std::unique_ptr<rcCompactHeightfield, CompactHeightfieldDeleter> compact(
                rcAllocCompactHeightfield());
        if (!compact || !rcBuildCompactHeightfield(&context, config.walkableHeight,
                    config.walkableClimb, *heightfield, *compact)
            || !rcErodeWalkableArea(&context, config.walkableRadius, *compact)) {
            error = "Could not build or erode navigation compact heightfield";
            return false;
        }
        heightfield.reset();

        std::unique_ptr<rcHeightfieldLayerSet, LayerSetDeleter> layers(
                rcAllocHeightfieldLayerSet());
        if (!layers || !rcBuildHeightfieldLayers(&context, *compact,
                    config.borderSize, config.walkableHeight, *layers)) {
            error = "Could not build navigation heightfield layers";
            return false;
        }
        if (layers->nlayers > capacities.maximumLayersPerTileCoordinate) {
            error = "Navigation tile (" + std::to_string(coordinate.x) + ","
                    + std::to_string(coordinate.y) + ") produced "
                    + std::to_string(layers->nlayers)
                    + " layers, exceeding the configured layer limit";
            return false;
        }

        stage = SectorNavigationBuildStage::BuildingDetourTiles;
        for (int layerIndex = 0; layerIndex < layers->nlayers; ++layerIndex) {
            const rcHeightfieldLayer& source = layers->layers[layerIndex];
            dtTileCacheLayerHeader header{};
            header.magic = DT_TILECACHE_MAGIC;
            header.version = DT_TILECACHE_VERSION;
            header.tx = coordinate.x;
            header.ty = coordinate.y;
            header.tlayer = layerIndex;
            std::memcpy(header.bmin, source.bmin, sizeof(header.bmin));
            std::memcpy(header.bmax, source.bmax, sizeof(header.bmax));
            header.width = static_cast<unsigned char>(source.width);
            header.height = static_cast<unsigned char>(source.height);
            header.minx = static_cast<unsigned char>(source.minx);
            header.maxx = static_cast<unsigned char>(source.maxx);
            header.miny = static_cast<unsigned char>(source.miny);
            header.maxy = static_cast<unsigned char>(source.maxy);
            header.hmin = static_cast<unsigned short>(source.hmin);
            header.hmax = static_cast<unsigned short>(source.hmax);
            unsigned char* data = nullptr;
            int dataSize = 0;
            const dtStatus layerStatus = dtBuildTileCacheLayer(&tileCompressor,
                    &header, source.heights, source.areas, source.cons, &data, &dataSize);
            if (dtStatusFailed(layerStatus) || data == nullptr) {
                error = "Could not compress navigation tile layer";
                return false;
            }
            dtCompressedTileRef tileRef = 0;
            const dtStatus addStatus = tileCache->addTile(
                    data, dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tileRef);
            if (dtStatusFailed(addStatus)) {
                dtFree(data);
                error = "Detour TileCache rejected a compressed navigation layer";
                return false;
            }
            if (dtStatusFailed(tileCache->buildNavMeshTile(tileRef, navMesh))) {
                error = "Detour failed to build a navigation mesh tile from its cached layer";
                return false;
            }
            statistics.compressedLayerBytes += static_cast<size_t>(dataSize);
            ++statistics.builtLayerCount;
        }
        statistics.tileTemporaryBytes = std::max(
                statistics.tileTemporaryBytes, tileAllocator.peak);
        ++statistics.builtTileCoordinateCount;
        stage = SectorNavigationBuildStage::RasterizingTiles;
        return true;
    }

    void ExtractDebugTileChunk(DebugTileChunk& chunk)
    {
        chunk.triangles.clear();
        chunk.edges.clear();
        chunk.stepConnections.clear();
        chunk.polygonCount = 0;
        chunk.populated = false;
        const dtMeshTile* tile = navMesh != nullptr
                ? navMesh->getTileAt(chunk.key.x, chunk.key.y, chunk.key.layer)
                : nullptr;
        if (tile == nullptr || tile->header == nullptr) return;
        chunk.populated = true;
        chunk.polygonCount = tile->header->polyCount;
        chunk.bounds = {
                {{tile->header->bmin[0], tile->header->bmin[1], tile->header->bmin[2]},
                 {tile->header->bmax[0], tile->header->bmax[1], tile->header->bmax[2]}},
                tile->header->x,
                tile->header->y,
                tile->header->layer};
        for (int polyIndex = 0; polyIndex < tile->header->polyCount; ++polyIndex) {
            const dtPoly& poly = tile->polys[polyIndex];
            if (poly.getType() != DT_POLYTYPE_GROUND || poly.vertCount < 3) continue;
            std::array<Vector3, DT_VERTS_PER_POLYGON> vertices{};
            for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                vertices[vertexIndex] = FromFloat3(
                        &tile->verts[poly.verts[vertexIndex] * 3]);
                const int next = (vertexIndex + 1) % poly.vertCount;
                chunk.edges.push_back({vertices[vertexIndex],
                        FromFloat3(&tile->verts[poly.verts[next] * 3])});
            }
            for (int vertexIndex = 2; vertexIndex < poly.vertCount; ++vertexIndex) {
                chunk.triangles.push_back({vertices[0],
                        vertices[vertexIndex - 1], vertices[vertexIndex],
                        poly.getArea()});
            }
            float minimumVertexY = vertices[0].y;
            float maximumVertexY = vertices[0].y;
            Vector3 center{};
            for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                center = Vector3Add(center, vertices[vertexIndex]);
                minimumVertexY = std::min(minimumVertexY, vertices[vertexIndex].y);
                maximumVertexY = std::max(maximumVertexY, vertices[vertexIndex].y);
            }
            center = Vector3Scale(center, 1.0f / static_cast<float>(poly.vertCount));
            if (maximumVertexY - minimumVertexY
                    > settings.cellHeight * 0.5f) {
                Vector3 lowerCenter{};
                Vector3 upperCenter{};
                int lowerCount = 0;
                int upperCount = 0;
                for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                    if (vertices[vertexIndex].y
                            <= minimumVertexY + settings.cellHeight * 0.5f) {
                        lowerCenter = Vector3Add(lowerCenter, vertices[vertexIndex]);
                        ++lowerCount;
                    }
                    if (vertices[vertexIndex].y
                            >= maximumVertexY - settings.cellHeight * 0.5f) {
                        upperCenter = Vector3Add(upperCenter, vertices[vertexIndex]);
                        ++upperCount;
                    }
                }
                if (lowerCount > 0 && upperCount > 0) {
                    chunk.stepConnections.push_back({
                            Vector3Scale(lowerCenter,
                                    1.0f / static_cast<float>(lowerCount)),
                            Vector3Scale(upperCenter,
                                    1.0f / static_cast<float>(upperCount))});
                }
            }
            const dtPolyRef sourceRef = navMesh->getPolyRefBase(tile)
                    | static_cast<dtPolyRef>(polyIndex);
            for (unsigned int linkIndex = poly.firstLink;
                    linkIndex != DT_NULL_LINK;
                    linkIndex = tile->links[linkIndex].next) {
                const dtPolyRef targetRef = tile->links[linkIndex].ref;
                if (targetRef == 0 || sourceRef >= targetRef) continue;
                const dtMeshTile* targetTile = nullptr;
                const dtPoly* targetPoly = nullptr;
                if (dtStatusFailed(navMesh->getTileAndPolyByRef(
                            targetRef, &targetTile, &targetPoly))
                        || targetTile == nullptr || targetPoly == nullptr
                        || targetPoly->getType() != DT_POLYTYPE_GROUND
                        || targetPoly->vertCount < 3) {
                    continue;
                }
                Vector3 targetCenter{};
                for (int targetVertex = 0;
                        targetVertex < targetPoly->vertCount;
                        ++targetVertex) {
                    targetCenter = Vector3Add(targetCenter,
                            FromFloat3(&targetTile->verts[
                                    targetPoly->verts[targetVertex] * 3]));
                }
                targetCenter = Vector3Scale(targetCenter,
                        1.0f / static_cast<float>(targetPoly->vertCount));
                if (std::fabs(targetCenter.y - center.y)
                        > settings.cellHeight * 0.5f) {
                    chunk.stepConnections.push_back({center, targetCenter});
                }
            }
        }
    }

    void InitializeDebugTileChunks()
    {
        debugTileChunks.clear();
        debugTileChunks.reserve(debugCache.tileBounds.size());
        for (const SectorNavigationDebugTileBounds& bounds : debugCache.tileBounds) {
            debugTileChunks.push_back({});
            DebugTileChunk& chunk = debugTileChunks.back();
            chunk.key = {bounds.tileX, bounds.tileY, bounds.layer};
            ExtractDebugTileChunk(chunk);
        }
    }

    void RefreshChangedDebugTileChunks()
    {
        for (const SectorNavigationTileKey key : changedTilesScratch) {
            auto found = std::find_if(
                    debugTileChunks.begin(), debugTileChunks.end(),
                    [key](const DebugTileChunk& chunk) {
                        return chunk.key == key;
                    });
            if (found == debugTileChunks.end()) {
                debugTileChunks.push_back({});
                found = debugTileChunks.end() - 1;
                found->key = key;
            }
            ExtractDebugTileChunk(*found);
        }
        debugCache.walkableTriangles.clear();
        debugCache.polygonEdges.clear();
        debugCache.tileBounds.clear();
        debugCache.stepConnections.clear();
        statistics.navMeshTileCount = 0;
        statistics.navMeshPolygonCount = 0;
        for (const DebugTileChunk& chunk : debugTileChunks) {
            if (!chunk.populated) continue;
            ++statistics.navMeshTileCount;
            statistics.navMeshPolygonCount += chunk.polygonCount;
            debugCache.tileBounds.push_back(chunk.bounds);
            debugCache.walkableTriangles.insert(
                    debugCache.walkableTriangles.end(),
                    chunk.triangles.begin(), chunk.triangles.end());
            debugCache.polygonEdges.insert(
                    debugCache.polygonEdges.end(),
                    chunk.edges.begin(), chunk.edges.end());
            debugCache.stepConnections.insert(
                    debugCache.stepConnections.end(),
                    chunk.stepConnections.begin(), chunk.stepConnections.end());
        }
    }

    void RefreshRuntimeNavigationDerivedData()
    {
        if (navMesh == nullptr) return;
        doorLinksScratch.clear();
        doorLinksScratch.insert(
                doorLinksScratch.end(), doorLinks.begin(), doorLinks.end());
        doorLinks.clear();
        for (SectorNavigationDebugDoorLink& link : debugCache.doorLinks) {
            link.valid = false;
        }
        const dtNavMesh* readableNavMesh = navMesh;
        for (int tileIndex = 0; tileIndex < navMesh->getMaxTiles(); ++tileIndex) {
            const dtMeshTile* tile = readableNavMesh->getTile(tileIndex);
            if (tile == nullptr || tile->header == nullptr) continue;
            for (int connectionIndex = 0;
                    connectionIndex < tile->header->offMeshConCount;
                    ++connectionIndex) {
                const dtOffMeshConnection& connection =
                        tile->offMeshCons[connectionIndex];
                if (connection.userId == 0) continue;
                const int placedObjectId = static_cast<int>(connection.userId);
                const dtPolyRef reference = navMesh->getPolyRefBase(tile)
                        | static_cast<dtPolyRef>(connection.poly);
                const auto previous = std::find_if(
                        doorLinksScratch.begin(), doorLinksScratch.end(),
                        [placedObjectId](const RuntimeDoorLink& link) {
                            return link.placedObjectId == placedObjectId;
                        });
                RuntimeDoorLink runtimeLink;
                runtimeLink.placedObjectId = placedObjectId;
                runtimeLink.polygon = reference;
                if (previous != doorLinksScratch.end()) {
                    runtimeLink.state = previous->state;
                    runtimeLink.holderCount = previous->holderCount;
                }
                doorLinks.push_back(runtimeLink);
                unsigned short flags = SectorNavigationPolyFlag_Walk
                        | SectorNavigationPolyFlag_Door;
                if (runtimeLink.state
                        == SectorNavigationDoorLinkState::RequiresOpening) {
                    flags |= SectorNavigationPolyFlag_DoorRequiresOpening;
                } else if (runtimeLink.state
                        == SectorNavigationDoorLinkState::Disabled) {
                    flags |= SectorNavigationPolyFlag_Disabled;
                }
                navMesh->setPolyFlags(reference, flags);
                const auto debug = std::find_if(
                        debugCache.doorLinks.begin(), debugCache.doorLinks.end(),
                        [placedObjectId](const SectorNavigationDebugDoorLink& link) {
                            return link.placedObjectId == placedObjectId;
                        });
                if (debug != debugCache.doorLinks.end()) {
                    debug->valid = true;
                    debug->state = runtimeLink.state;
                    debug->holderCount = runtimeLink.holderCount;
                }
            }
        }
        RefreshChangedDebugTileChunks();
        BumpDebugRevision();
    }

    bool FinalizeBuild(std::string& error)
    {
        query = dtAllocNavMeshQuery();
        if (query == nullptr
            || dtStatusFailed(query->init(navMesh, capacities.queryNodeCapacity))) {
            error = "Could not initialize bounded Detour navigation queries";
            return false;
        }
        crowd = dtAllocCrowd();
        if (crowd == nullptr
                || !crowd->init(
                        static_cast<int>(capacities.agentCapacity),
                        settings.agentRadius,
                        navMesh)) {
            error = "Could not initialize bounded DetourCrowd storage";
            return false;
        }
        dtQueryFilter* crowdFilter = crowd->getEditableFilter(0);
        if (crowdFilter == nullptr) {
            error = "Could not initialize the DetourCrowd query filter";
            return false;
        }
        crowdFilter->setIncludeFlags(SectorNavigationPolyFlag_Walk);
        crowdFilter->setExcludeFlags(
                SectorNavigationPolyFlag_Door
                | SectorNavigationPolyFlag_Disabled);
        dtObstacleAvoidanceParams avoidance =
                *crowd->getObstacleAvoidanceParams(0);
        avoidance.velBias = 0.5f;
        avoidance.adaptiveDivs = 7;
        avoidance.adaptiveRings = 3;
        avoidance.adaptiveDepth = 3;
        crowd->setObstacleAvoidanceParams(
                static_cast<int>(crowdSettings.avoidanceQuality),
                &avoidance);
        stage = SectorNavigationBuildStage::BuildingDebugCache;
        debugCache = {};
        debugCache.dynamicObstacles.reserve(static_cast<size_t>(
                capacities.dynamicObstacleCapacity));
        debugCache.recentlyUpdatedTiles.reserve(32);
        debugCache.staticObstacles = buildInput.staticObstacles;
        debugCache.doorPlaceholders = buildInput.doorPlaceholders;
        debugCache.doorLinks.reserve(buildInput.doorLinks.size());
        for (const SectorNavigationBuildDoorLink& link : buildInput.doorLinks) {
            debugCache.doorLinks.push_back({
                    link.placedObjectId,
                    link.frontStage,
                    link.backStage,
                    SectorNavigationDoorLinkState::RequiresOpening,
                    0,
                    false});
        }
        const dtNavMesh* readableNavMesh = navMesh;
        for (int tileIndex = 0; tileIndex < navMesh->getMaxTiles(); ++tileIndex) {
            const dtMeshTile* tile = readableNavMesh->getTile(tileIndex);
            if (tile == nullptr || tile->header == nullptr) continue;
            ++statistics.navMeshTileCount;
            statistics.navMeshPolygonCount += tile->header->polyCount;
            for (int connectionIndex = 0;
                    connectionIndex < tile->header->offMeshConCount;
                    ++connectionIndex) {
                const dtOffMeshConnection& connection =
                        tile->offMeshCons[connectionIndex];
                if (connection.userId == 0) continue;
                const dtPolyRef reference = navMesh->getPolyRefBase(tile)
                        | static_cast<dtPolyRef>(connection.poly);
                const int placedObjectId = static_cast<int>(connection.userId);
                const auto duplicate = std::find_if(
                        doorLinks.begin(), doorLinks.end(),
                        [placedObjectId](const RuntimeDoorLink& link) {
                            return link.placedObjectId == placedObjectId;
                        });
                if (duplicate != doorLinks.end()) {
                    Record(SectorNavigationDiagnosticSeverity::Error, stage,
                            "Duplicate navigation door link ID "
                                    + std::to_string(placedObjectId));
                    continue;
                }
                doorLinks.push_back({placedObjectId, reference,
                        SectorNavigationDoorLinkState::RequiresOpening});
                const auto debug = std::find_if(
                        debugCache.doorLinks.begin(), debugCache.doorLinks.end(),
                        [placedObjectId](const SectorNavigationDebugDoorLink& link) {
                            return link.placedObjectId == placedObjectId;
                        });
                if (debug != debugCache.doorLinks.end()) debug->valid = true;
            }
            debugCache.tileBounds.push_back({
                    {{tile->header->bmin[0], tile->header->bmin[1], tile->header->bmin[2]},
                     {tile->header->bmax[0], tile->header->bmax[1], tile->header->bmax[2]}},
                    tile->header->x, tile->header->y, tile->header->layer});
            for (int polyIndex = 0; polyIndex < tile->header->polyCount; ++polyIndex) {
                const dtPoly& poly = tile->polys[polyIndex];
                if (poly.getType() != DT_POLYTYPE_GROUND || poly.vertCount < 3) continue;
                std::array<Vector3, DT_VERTS_PER_POLYGON> vertices{};
                for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                    vertices[vertexIndex] = FromFloat3(
                            &tile->verts[poly.verts[vertexIndex] * 3]);
                    const int next = (vertexIndex + 1) % poly.vertCount;
                    debugCache.polygonEdges.push_back({
                            vertices[vertexIndex],
                            FromFloat3(&tile->verts[poly.verts[next] * 3])});
                }
                for (int vertexIndex = 2; vertexIndex < poly.vertCount; ++vertexIndex) {
                    debugCache.walkableTriangles.push_back({vertices[0],
                            vertices[vertexIndex - 1], vertices[vertexIndex], poly.getArea()});
                }
                Vector3 center{};
                float minimumVertexY = vertices[0].y;
                float maximumVertexY = vertices[0].y;
                for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                    center = Vector3Add(center, vertices[vertexIndex]);
                    minimumVertexY = std::min(minimumVertexY, vertices[vertexIndex].y);
                    maximumVertexY = std::max(maximumVertexY, vertices[vertexIndex].y);
                }
                center = Vector3Scale(center, 1.0f / static_cast<float>(poly.vertCount));
                if (maximumVertexY - minimumVertexY
                        > settings.cellHeight * 0.5f) {
                    Vector3 lowerCenter{};
                    Vector3 upperCenter{};
                    int lowerCount = 0;
                    int upperCount = 0;
                    for (int vertexIndex = 0; vertexIndex < poly.vertCount; ++vertexIndex) {
                        if (vertices[vertexIndex].y
                                <= minimumVertexY + settings.cellHeight * 0.5f) {
                            lowerCenter = Vector3Add(lowerCenter, vertices[vertexIndex]);
                            ++lowerCount;
                        }
                        if (vertices[vertexIndex].y
                                >= maximumVertexY - settings.cellHeight * 0.5f) {
                            upperCenter = Vector3Add(upperCenter, vertices[vertexIndex]);
                            ++upperCount;
                        }
                    }
                    if (lowerCount > 0 && upperCount > 0) {
                        lowerCenter = Vector3Scale(
                                lowerCenter, 1.0f / static_cast<float>(lowerCount));
                        upperCenter = Vector3Scale(
                                upperCenter, 1.0f / static_cast<float>(upperCount));
                        debugCache.stepConnections.push_back({lowerCenter, upperCenter});
                    }
                }
                const dtPolyRef sourceRef = navMesh->getPolyRefBase(tile)
                        | static_cast<dtPolyRef>(polyIndex);
                for (unsigned int linkIndex = poly.firstLink;
                        linkIndex != DT_NULL_LINK;
                        linkIndex = tile->links[linkIndex].next) {
                    const dtPolyRef targetRef = tile->links[linkIndex].ref;
                    if (targetRef == 0 || sourceRef >= targetRef) continue;
                    const dtMeshTile* targetTile = nullptr;
                    const dtPoly* targetPoly = nullptr;
                    if (dtStatusFailed(navMesh->getTileAndPolyByRef(
                                targetRef, &targetTile, &targetPoly))
                            || targetTile == nullptr || targetPoly == nullptr
                            || targetPoly->getType() != DT_POLYTYPE_GROUND
                            || targetPoly->vertCount < 3) {
                        continue;
                    }
                    Vector3 targetCenter{};
                    for (int targetVertex = 0;
                            targetVertex < targetPoly->vertCount;
                            ++targetVertex) {
                        targetCenter = Vector3Add(
                                targetCenter,
                                FromFloat3(&targetTile->verts[
                                        targetPoly->verts[targetVertex] * 3]));
                    }
                    targetCenter = Vector3Scale(
                            targetCenter,
                            1.0f / static_cast<float>(targetPoly->vertCount));
                    if (std::fabs(targetCenter.y - center.y)
                            > settings.cellHeight * 0.5f) {
                        debugCache.stepConnections.push_back({center, targetCenter});
                    }
                }
            }
        }
        for (const SectorNavigationDebugDoorLink& link : debugCache.doorLinks) {
            if (!link.valid) {
                Record(SectorNavigationDiagnosticSeverity::Warning, stage,
                        "Door " + std::to_string(link.placedObjectId)
                                + " has no usable off-mesh connection");
            }
        }
        InitializeDebugTileChunks();
        doorLinksScratch.reserve(doorLinks.size());
        buildInput = {};
        tileCoordinates.clear();
        ++buildRevision;
        ++counters.completedBuilds;
        FinishBuildTiming();
        BumpDebugRevision();
        InitializeTileRevisions();
        RefreshDynamicObstacleDebugAndStatistics();
        state = statistics.navMeshPolygonCount > 0
                ? SectorNavigationState::Ready : SectorNavigationState::Empty;
        stage = SectorNavigationBuildStage::Complete;
        std::ostringstream message;
        message << "Built " << statistics.navMeshPolygonCount << " polygons in "
                << statistics.navMeshTileCount << " layers across "
                << statistics.tileCoordinateCount << " tile coordinates ("
                << FormatSectorNavigationSourceHash(sourceHash) << ")";
        Record(SectorNavigationDiagnosticSeverity::Info, stage, message.str());
        return true;
    }
};

SectorNavigationWorld::SectorNavigationWorld() : impl(std::make_unique<Impl>()) {}
SectorNavigationWorld::~SectorNavigationWorld() = default;
SectorNavigationWorld::SectorNavigationWorld(SectorNavigationWorld&&) noexcept = default;
SectorNavigationWorld& SectorNavigationWorld::operator=(SectorNavigationWorld&&) noexcept = default;

bool SectorNavigationWorld::Initialize(
        SectorNavigationSettings settings,
        SectorNavigationCapacitySettings capacities,
        SectorNavigationDynamicObstacleSettings dynamicObstacleSettings,
        SectorNavigationCrowdSettings crowdSettings)
{
    if (!impl) impl = std::make_unique<Impl>();
    impl->ReleaseNavigation();
    impl->settings = NormalizeSectorNavigationSettings(settings);
    impl->capacities = NormalizeSectorNavigationCapacitySettings(capacities);
    impl->dynamicObstacleSettings =
            NormalizeSectorNavigationDynamicObstacleSettings(
                    dynamicObstacleSettings);
    impl->crowdSettings = NormalizeSectorNavigationCrowdSettings(crowdSettings);
    impl->diagnostics.clear();
    impl->diagnostics.reserve(impl->capacities.diagnosticCapacity);
    impl->agentSlots.clear();
    impl->agentSlots.reserve(impl->capacities.agentCapacity);
    impl->pathSlots.clear();
    impl->pathSlots.reserve(impl->capacities.pathRecordCapacity);
    impl->obstacles.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity));
    impl->pendingAffectedTiles.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity) * DT_MAX_TOUCHED_TILES);
    impl->seenObstacleIdsScratch.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity));
    impl->affectedTilesScratch.reserve(DT_MAX_TOUCHED_TILES + 1u);
    impl->tileReferencesScratch.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity) * DT_MAX_TOUCHED_TILES);
    impl->doorLinksScratch.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity));
    impl->changedTilesScratch.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity) * DT_MAX_TOUCHED_TILES);
    impl->debugCache.dynamicObstacles.reserve(static_cast<size_t>(
            impl->capacities.dynamicObstacleCapacity));
    impl->debugCache.recentlyUpdatedTiles.reserve(32);
    impl->counters = {};
    impl->crowdStatistics = {};
    impl->sourceRevision = 0;
    impl->buildRevision = 0;
    impl->tileRevision = 0;
    impl->debugRevision = 0;
    impl->debugCache.navigationRevision = 0;
    impl->agentGrowthWarned = false;
    impl->pathGrowthWarned = false;
    impl->obstacleGrowthWarned = false;
    impl->diagnosticOverflowWarned = false;
    impl->duplicateObstacleWarningReported = false;
    impl->stage = SectorNavigationBuildStage::None;
    impl->state = SectorNavigationState::Empty;
    return true;
}

void SectorNavigationWorld::Shutdown()
{
    if (!impl) return;
    impl->InvalidateRecords();
    impl->ReleaseNavigation();
    impl->diagnostics.clear();
    impl->agentSlots.clear();
    impl->pathSlots.clear();
    impl->counters = {};
    impl->sourceRevision = 0;
    impl->buildRevision = 0;
    impl->tileRevision = 0;
    impl->debugRevision = 0;
    impl->debugCache.navigationRevision = 0;
    impl->stage = SectorNavigationBuildStage::None;
    impl->state = SectorNavigationState::Uninitialized;
}

void SectorNavigationWorld::ResetForRebuild()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    impl->ReleaseNavigation();
    impl->stage = SectorNavigationBuildStage::None;
    impl->state = SectorNavigationState::Empty;
    ++impl->sourceRevision;
    ++impl->buildRevision;
    impl->BumpDebugRevision();
}

void SectorNavigationWorld::RequestRebuild()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    const float peakBuildMilliseconds = impl->statistics.peakBuildMilliseconds;
    impl->ReleaseNavigation();
    impl->statistics.peakBuildMilliseconds = peakBuildMilliseconds;
    impl->stage = SectorNavigationBuildStage::WaitingForStaticCollision;
    impl->state = SectorNavigationState::Queued;
    ++impl->sourceRevision;
    ++impl->counters.rebuildRequests;
    impl->BumpDebugRevision();
}

void SectorNavigationWorld::MarkStale()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    if (impl->state == SectorNavigationState::Ready) impl->state = SectorNavigationState::Stale;
    ++impl->sourceRevision;
}

void SectorNavigationWorld::SetEmpty()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    impl->ReleaseNavigation();
    impl->stage = SectorNavigationBuildStage::None;
    impl->state = SectorNavigationState::Empty;
    ++impl->buildRevision;
    impl->BumpDebugRevision();
}

void SectorNavigationWorld::Fail(
        SectorNavigationBuildStage stage,
        const std::string& message)
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    impl->FinishBuildTiming();
    const SectorNavigationBuildStatistics failedStatistics = impl->statistics;
    const uint64_t failedSourceHash = impl->sourceHash;
    impl->ReleaseNavigation();
    impl->statistics = failedStatistics;
    impl->sourceHash = failedSourceHash;
    impl->stage = stage;
    impl->state = SectorNavigationState::Failed;
    impl->Record(SectorNavigationDiagnosticSeverity::Error, stage, message);
    ++impl->buildRevision;
    ++impl->counters.failedBuilds;
    impl->BumpDebugRevision();
}

void SectorNavigationWorld::UpdateBuild(
        const SectorTopologyMap& map,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        size_t pendingStaticColliderCount)
{
    if (!impl || (impl->state != SectorNavigationState::Queued
                  && impl->state != SectorNavigationState::Building)) {
        return;
    }
    if (impl->state == SectorNavigationState::Queued) {
        if (pendingStaticColliderCount > 0) {
            impl->stage = SectorNavigationBuildStage::WaitingForStaticCollision;
            return;
        }
        std::string error;
        if (!impl->BeginBuild(map, staticColliders, error)) {
            Fail(impl->stage, error);
            return;
        }
        if (impl->state != SectorNavigationState::Building) return;
    }

    const int budget = impl->capacities.tileBuildBudgetPerUpdate;
    for (int built = 0; built < budget
            && impl->nextTileCoordinate < impl->tileCoordinates.size(); ++built) {
        std::string error;
        if (!impl->BuildTile(impl->tileCoordinates[impl->nextTileCoordinate], error)) {
            Fail(impl->stage, error);
            return;
        }
        ++impl->nextTileCoordinate;
    }
    if (impl->nextTileCoordinate == impl->tileCoordinates.size()) {
        std::string error;
        if (!impl->FinalizeBuild(error)) Fail(impl->stage, error);
    }
}

void SectorNavigationWorld::UpdateDynamicObstacles(
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        float rawDt)
{
    if (!impl || impl->state != SectorNavigationState::Ready
            || impl->tileCache == nullptr || impl->navMesh == nullptr) {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    impl->changedTilesScratch.clear();
    for (Impl::RuntimeObstacle& obstacle : impl->obstacles) {
        obstacle.seen = false;
        obstacle.updateSeconds += dt;
    }

    impl->seenObstacleIdsScratch.clear();
    for (const SectorStaticModelCollider& collider : dynamicColliders) {
        if (!Impl::ValidDynamicCollider(collider)) continue;
        if (std::find(impl->seenObstacleIdsScratch.begin(),
                    impl->seenObstacleIdsScratch.end(), collider.placedObjectId)
                != impl->seenObstacleIdsScratch.end()) {
            if (!impl->duplicateObstacleWarningReported) {
                impl->duplicateObstacleWarningReported = true;
                impl->Record(SectorNavigationDiagnosticSeverity::Warning,
                        SectorNavigationBuildStage::Complete,
                        "Duplicate dynamic obstacle ID "
                                + std::to_string(collider.placedObjectId)
                                + " was ignored; further duplicate warnings are suppressed");
            }
            continue;
        }
        if (impl->seenObstacleIdsScratch.size()
                < static_cast<size_t>(impl->capacities.dynamicObstacleCapacity)) {
            impl->seenObstacleIdsScratch.push_back(collider.placedObjectId);
        }
        auto found = std::find_if(
                impl->obstacles.begin(), impl->obstacles.end(),
                [&collider](const Impl::RuntimeObstacle& obstacle) {
                    return obstacle.placedObjectId == collider.placedObjectId;
                });
        if (found == impl->obstacles.end()) {
            if (impl->obstacles.size()
                    >= static_cast<size_t>(impl->capacities.dynamicObstacleCapacity)) {
                if (!impl->obstacleGrowthWarned) {
                    impl->obstacleGrowthWarned = true;
                    ++impl->counters.capacityWarnings;
                    ++impl->dynamicObstacleStatistics.failures;
                    std::fprintf(stderr,
                            "[Navigation WARNING] Dynamic obstacle capacity exceeded; additional props remain collision-only.\n");
                    impl->Record(SectorNavigationDiagnosticSeverity::Warning,
                            SectorNavigationBuildStage::Complete,
                            "Dynamic obstacle capacity exceeded; additional props remain collision-only");
                }
                continue;
            }
            impl->obstacles.push_back({});
            Impl::RuntimeObstacle& obstacle = impl->obstacles.back();
            obstacle.placedObjectId = collider.placedObjectId;
            obstacle.desired = collider;
            obstacle.lastObserved = collider;
            obstacle.seen = true;
            continue;
        }

        Impl::RuntimeObstacle& obstacle = *found;
        obstacle.seen = true;
        const float dx = collider.center.x - obstacle.lastObserved.center.x;
        const float dz = collider.center.y - obstacle.lastObserved.center.y;
        const float sampleDistance = std::sqrt(dx * dx + dz * dz);
        const float sampleYaw = impl->YawDistance(
                impl->DynamicYaw(obstacle.lastObserved),
                impl->DynamicYaw(collider));
        const float linearSpeed = dt > 0.000001f ? sampleDistance / dt : 0.0f;
        const float angularSpeedDegrees = dt > 0.000001f
                ? sampleYaw * 180.0f / 3.14159265358979323846f / dt
                : 0.0f;
        const bool fast = linearSpeed
                        > impl->dynamicObstacleSettings.fastLinearSpeedWorld
                || angularSpeedDegrees
                        > impl->dynamicObstacleSettings.fastAngularSpeedDegrees;
        const float settleLinearSpeed =
                impl->dynamicObstacleSettings.positionThresholdWorld
                / impl->dynamicObstacleSettings.settleSeconds;
        const float settleAngularSpeed =
                impl->dynamicObstacleSettings.yawThresholdDegrees
                / impl->dynamicObstacleSettings.settleSeconds;
        const bool settled = linearSpeed <= settleLinearSpeed
                && angularSpeedDegrees <= settleAngularSpeed;
        obstacle.desired = collider;
        obstacle.lastObserved = collider;

        if (fast) {
            obstacle.settleSeconds = 0.0f;
            obstacle.addAfterRemove = false;
            if (obstacle.phase == Impl::ObstaclePhase::Active) {
                obstacle.phase = Impl::ObstaclePhase::PendingRemove;
            } else if (obstacle.phase == Impl::ObstaclePhase::PendingAdd
                    && obstacle.reference == 0) {
                obstacle.phase = Impl::ObstaclePhase::FastSuppressed;
            }
            continue;
        }
        obstacle.settleSeconds = settled
                ? obstacle.settleSeconds + dt : 0.0f;
        if (obstacle.phase == Impl::ObstaclePhase::FastSuppressed
                && obstacle.settleSeconds
                        >= impl->dynamicObstacleSettings.settleSeconds) {
            obstacle.phase = Impl::ObstaclePhase::PendingAdd;
            obstacle.updateSeconds = 0.0f;
        } else if (obstacle.phase == Impl::ObstaclePhase::Active
                && impl->MeaningfullyDifferent(
                        obstacle.desired, obstacle.committed)
                && obstacle.updateSeconds
                        >= impl->dynamicObstacleSettings.slowUpdateIntervalSeconds) {
            obstacle.phase = Impl::ObstaclePhase::PendingRemove;
            obstacle.addAfterRemove = true;
            obstacle.updateSeconds = 0.0f;
            ++impl->dynamicObstacleStatistics.transforms;
        }
    }

    for (Impl::RuntimeObstacle& obstacle : impl->obstacles) {
        if (obstacle.seen) continue;
        obstacle.addAfterRemove = false;
        obstacle.deleteAfterRemove = true;
        if (obstacle.reference != 0) {
            obstacle.phase = Impl::ObstaclePhase::PendingRemove;
        }
    }

    int requestBudget = impl->capacities.dynamicObstacleRequestBudgetPerUpdate;
    if (impl->tileCacheUpToDate) {
        for (Impl::RuntimeObstacle& obstacle : impl->obstacles) {
            if (requestBudget <= 0) break;
            if (obstacle.phase == Impl::ObstaclePhase::PendingAdd
                    && obstacle.reference == 0
                    && !obstacle.deleteAfterRemove) {
                impl->affectedTilesScratch.clear();
                if (!impl->CollectAffectedTiles(
                            obstacle.desired,
                            impl->affectedTilesScratch,
                            true)) {
                    obstacle.phase = Impl::ObstaclePhase::Failed;
                    continue;
                }
                float center[3]{};
                float halfExtents[3]{};
                float bmin[3]{};
                float bmax[3]{};
                impl->ObstacleBounds(
                        obstacle.desired, center, halfExtents, bmin, bmax);
                dtObstacleRef reference = 0;
                const dtStatus status = impl->tileCache->addBoxObstacle(
                        center, halfExtents,
                        impl->DynamicYaw(obstacle.desired), &reference);
                if (dtStatusFailed(status)) {
                    if (dtStatusDetail(status, DT_BUFFER_TOO_SMALL)) break;
                    obstacle.phase = Impl::ObstaclePhase::Failed;
                    ++impl->dynamicObstacleStatistics.failures;
                    impl->Record(SectorNavigationDiagnosticSeverity::Warning,
                            SectorNavigationBuildStage::Complete,
                            "Could not add dynamic obstacle "
                                    + std::to_string(obstacle.placedObjectId)
                                    + "; it remains collision-only");
                    continue;
                }
                obstacle.reference = reference;
                obstacle.committed = obstacle.desired;
                for (const SectorNavigationTileKey key :
                        impl->affectedTilesScratch) {
                    if (std::find(impl->pendingAffectedTiles.begin(),
                                impl->pendingAffectedTiles.end(), key)
                            == impl->pendingAffectedTiles.end()) {
                        impl->pendingAffectedTiles.push_back(key);
                    }
                }
                ++impl->dynamicObstacleStatistics.additions;
                --requestBudget;
                impl->tileCacheUpToDate = false;
            } else if (obstacle.phase == Impl::ObstaclePhase::PendingRemove
                    && obstacle.reference != 0) {
                impl->AppendPendingAffectedTiles(obstacle.committed);
                const dtStatus status = impl->tileCache->removeObstacle(
                        obstacle.reference);
                if (dtStatusFailed(status)) {
                    if (dtStatusDetail(status, DT_BUFFER_TOO_SMALL)) break;
                    obstacle.phase = Impl::ObstaclePhase::Failed;
                    obstacle.reference = 0;
                    ++impl->dynamicObstacleStatistics.failures;
                    impl->Record(SectorNavigationDiagnosticSeverity::Warning,
                            SectorNavigationBuildStage::Complete,
                            "Could not remove dynamic obstacle "
                                    + std::to_string(obstacle.placedObjectId));
                    continue;
                }
                ++impl->dynamicObstacleStatistics.removals;
                --requestBudget;
                impl->tileCacheUpToDate = false;
            }
        }
    }

    bool changedAnyTile = false;
    for (int work = 0;
            work < impl->capacities.dynamicObstacleTileBudgetPerUpdate
                    && !impl->tileCacheUpToDate;
            ++work) {
        impl->tileReferencesScratch.clear();
        for (const SectorNavigationTileKey key : impl->pendingAffectedTiles) {
            impl->tileReferencesScratch.push_back(
                    impl->navMesh->getTileRefAt(key.x, key.y, key.layer));
        }
        bool upToDate = false;
        const dtStatus status = impl->tileCache->update(dt, impl->navMesh, &upToDate);
        if (dtStatusFailed(status)) {
            Fail(SectorNavigationBuildStage::BuildingDetourTiles,
                    "Dynamic TileCache update failed; navigation was disabled safely");
            return;
        }
        impl->tileCacheUpToDate = upToDate;
        for (size_t index = 0; index < impl->pendingAffectedTiles.size(); ++index) {
            const SectorNavigationTileKey key = impl->pendingAffectedTiles[index];
            const dtTileRef after = impl->navMesh->getTileRefAt(
                    key.x, key.y, key.layer);
            if (after == impl->tileReferencesScratch[index]) continue;
            changedAnyTile = true;
            if (std::find(impl->changedTilesScratch.begin(),
                        impl->changedTilesScratch.end(), key)
                    == impl->changedTilesScratch.end()) {
                impl->changedTilesScratch.push_back(key);
            }
            ++impl->tileRevision;
            ++impl->dynamicObstacleStatistics.updatedTiles;
            Impl::TileRuntimeRevision* tile = impl->FindTileRevision(key);
            if (tile == nullptr) {
                impl->tileRevisions.push_back({key, after, impl->tileRevision});
            } else {
                tile->reference = after;
                tile->revision = impl->tileRevision;
            }
            impl->debugCache.recentlyUpdatedTiles.push_back({key, impl->tileRevision});
            if (impl->debugCache.recentlyUpdatedTiles.size() > 32) {
                impl->debugCache.recentlyUpdatedTiles.erase(
                        impl->debugCache.recentlyUpdatedTiles.begin());
            }
        }
    }

    if (impl->tileCacheUpToDate) {
        for (Impl::RuntimeObstacle& obstacle : impl->obstacles) {
            if (obstacle.phase == Impl::ObstaclePhase::PendingAdd
                    && obstacle.reference != 0) {
                const dtTileCacheObstacle* detourObstacle =
                        impl->tileCache->getObstacleByRef(obstacle.reference);
                if (detourObstacle != nullptr
                        && detourObstacle->state == DT_OBSTACLE_PROCESSED) {
                    obstacle.phase = Impl::ObstaclePhase::Active;
                    obstacle.updateSeconds = 0.0f;
                }
            } else if (obstacle.phase == Impl::ObstaclePhase::PendingRemove
                    && obstacle.reference != 0
                    && impl->tileCache->getObstacleByRef(obstacle.reference)
                            == nullptr) {
                obstacle.reference = 0;
                if (obstacle.deleteAfterRemove) {
                    continue;
                }
                obstacle.phase = obstacle.addAfterRemove
                        ? Impl::ObstaclePhase::PendingAdd
                        : Impl::ObstaclePhase::FastSuppressed;
                obstacle.addAfterRemove = false;
            }
        }
        impl->pendingAffectedTiles.clear();
    }
    impl->obstacles.erase(
            std::remove_if(
                    impl->obstacles.begin(), impl->obstacles.end(),
                    [](const Impl::RuntimeObstacle& obstacle) {
                        return obstacle.deleteAfterRemove
                                && obstacle.reference == 0;
                    }),
            impl->obstacles.end());
    if (changedAnyTile) impl->RefreshRuntimeNavigationDerivedData();
    impl->RefreshDynamicObstacleDebugAndStatistics();
    impl->debugCache.tileRevision = impl->tileRevision;
    const float elapsedMilliseconds = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    impl->dynamicObstacleStatistics.lastUpdateMilliseconds = elapsedMilliseconds;
    impl->dynamicObstacleStatistics.peakUpdateMilliseconds = std::max(
            impl->dynamicObstacleStatistics.peakUpdateMilliseconds,
            elapsedMilliseconds);
}

bool SectorNavigationWorld::CorridorTouchesChangedTile(
        const SectorNavigationTileKey* tiles,
        size_t tileCount,
        uint64_t capturedTileRevision) const
{
    if (!impl || tiles == nullptr || capturedTileRevision >= impl->tileRevision) {
        return false;
    }
    for (size_t index = 0; index < tileCount; ++index) {
        const auto found = std::find_if(
                impl->tileRevisions.begin(), impl->tileRevisions.end(),
                [key = tiles[index]](const Impl::TileRuntimeRevision& tile) {
                    return tile.key == key;
                });
        if (found != impl->tileRevisions.end()
                && found->revision > capturedTileRevision) {
            return true;
        }
    }
    return false;
}

SectorNavigationNearestPointResult SectorNavigationWorld::FindNearestPoint(Vector3 position) const
{
    SectorNavigationNearestPointResult result;
    result.requestedPosition = position;
    if (!impl || impl->state != SectorNavigationState::Ready || impl->query == nullptr) return result;
    const float point[3]{position.x, position.y, position.z};
    const float extents[3]{0.5f, impl->settings.agentHeight, 0.5f};
    float nearest[3]{};
    dtPolyRef reference = 0;
    const dtQueryFilter filter = MakeQueryFilter(impl->filterPolicy);
    const dtStatus status = impl->query->findNearestPoly(
            point, extents, &filter, &reference, nearest);
    if (dtStatusFailed(status) || reference == 0
        || !ProjectionWithinExtents(point, nearest, extents)) {
        result.status = SectorNavigationQueryStatus::StartNotOnNavmesh;
        return result;
    }
    result.nearestPosition = FromFloat3(nearest);
    result.status = SectorNavigationQueryStatus::Success;
    return result;
}

SectorNavigationPathResult SectorNavigationWorld::FindPath(
        Vector3 start,
        Vector3 destination,
        SectorNavigationQueryOptions options)
{
    SectorNavigationPathResult result;
    result.requestedStart = start;
    result.requestedDestination = destination;
    if (!impl || impl->state != SectorNavigationState::Ready || impl->query == nullptr) return result;
    const float extents[3]{0.5f, impl->settings.agentHeight, 0.5f};
    const float startPoint[3]{start.x, start.y, start.z};
    const float destinationPoint[3]{destination.x, destination.y, destination.z};
    float projectedStart[3]{};
    float projectedDestination[3]{};
    dtPolyRef startReference = 0;
    dtPolyRef destinationReference = 0;
    const dtQueryFilter filter = MakeQueryFilter(impl->filterPolicy, options);
    if (dtStatusFailed(impl->query->findNearestPoly(startPoint, extents, &filter,
                &startReference, projectedStart)) || startReference == 0
        || !ProjectionWithinExtents(startPoint, projectedStart, extents)) {
        result.status = SectorNavigationQueryStatus::StartNotOnNavmesh;
        ++impl->counters.failedQueries;
        return result;
    }
    result.projectedStart = FromFloat3(projectedStart);
    if (dtStatusFailed(impl->query->findNearestPoly(destinationPoint, extents, &filter,
                &destinationReference, projectedDestination)) || destinationReference == 0
        || !ProjectionWithinExtents(destinationPoint, projectedDestination, extents)) {
        result.status = SectorNavigationQueryStatus::DestinationNotOnNavmesh;
        ++impl->counters.failedQueries;
        return result;
    }
    result.projectedDestination = FromFloat3(projectedDestination);

    std::array<dtPolyRef, SectorNavigationMaximumPathPolygons> corridor{};
    int corridorCount = 0;
    const int corridorCapacity = std::min(impl->capacities.maximumPathPolygons,
            static_cast<int>(corridor.size()));
    const dtStatus pathStatus = impl->query->findPath(
            startReference, destinationReference, projectedStart, projectedDestination,
            &filter, corridor.data(), &corridorCount, corridorCapacity);
    if (dtStatusFailed(pathStatus) || corridorCount <= 0) {
        result.status = dtStatusDetail(pathStatus, DT_OUT_OF_NODES)
                ? SectorNavigationQueryStatus::CapacityExceeded
                : SectorNavigationQueryStatus::NoPath;
        ++impl->counters.failedQueries;
        return result;
    }
    result.corridorPolygonCount = static_cast<size_t>(corridorCount);
    result.tileRevision = impl->tileRevision;
    for (int corridorIndex = 0; corridorIndex < corridorCount; ++corridorIndex) {
        const dtMeshTile* corridorTile = nullptr;
        const dtPoly* corridorPoly = nullptr;
        if (dtStatusFailed(impl->navMesh->getTileAndPolyByRef(
                    corridor[corridorIndex], &corridorTile, &corridorPoly))
                || corridorTile == nullptr || corridorTile->header == nullptr) {
            continue;
        }
        const SectorNavigationTileKey key{
                corridorTile->header->x,
                corridorTile->header->y,
                corridorTile->header->layer};
        const auto begin = result.corridorTiles.begin();
        const auto end = begin
                + static_cast<std::ptrdiff_t>(result.corridorTileCount);
        if (std::find(begin, end, key) != end) continue;
        if (result.corridorTileCount >= result.corridorTiles.size()) {
            result.status = SectorNavigationQueryStatus::CapacityExceeded;
            ++impl->counters.failedQueries;
            return result;
        }
        result.corridorTiles[result.corridorTileCount++] = key;
    }
    const bool capacityExceeded = dtStatusDetail(pathStatus, DT_BUFFER_TOO_SMALL)
            || dtStatusDetail(pathStatus, DT_OUT_OF_NODES);
    const bool partial = dtStatusDetail(pathStatus, DT_PARTIAL_RESULT)
            || corridor[corridorCount - 1] != destinationReference;
    if (partial && !capacityExceeded && corridorCount == 1
        && startReference != destinationReference) {
        result.status = SectorNavigationQueryStatus::NoPath;
        ++impl->counters.failedQueries;
        return result;
    }
    float straightDestination[3]{projectedDestination[0], projectedDestination[1],
            projectedDestination[2]};
    if (partial) {
        impl->query->closestPointOnPoly(
                corridor[corridorCount - 1], projectedDestination, straightDestination, nullptr);
    }
    std::array<float, SectorNavigationMaximumStraightPathCorners * 3> straightPoints{};
    std::array<unsigned char, SectorNavigationMaximumStraightPathCorners> straightFlags{};
    std::array<dtPolyRef, SectorNavigationMaximumStraightPathCorners> straightRefs{};
    int straightCount = 0;
    const int straightCapacity = std::min(impl->capacities.maximumStraightPathCorners,
            static_cast<int>(result.corners.size()));
    const dtStatus straightStatus = impl->query->findStraightPath(
            projectedStart, straightDestination, corridor.data(), corridorCount,
            straightPoints.data(), straightFlags.data(), straightRefs.data(), &straightCount,
            straightCapacity, DT_STRAIGHTPATH_AREA_CROSSINGS);
    if (dtStatusFailed(straightStatus)) {
        result.status = SectorNavigationQueryStatus::InternalError;
        ++impl->counters.failedQueries;
        return result;
    }
    result.cornerCount = static_cast<size_t>(straightCount);
    for (int index = 0; index < straightCount; ++index) {
        result.corners[index] = FromFloat3(&straightPoints[index * 3]);
        result.cornerFlags[index] = straightFlags[index];
        if ((straightFlags[index] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) == 0
                || straightRefs[index] == 0) {
            continue;
        }
        const dtOffMeshConnection* connection =
                impl->navMesh->getOffMeshConnectionByRef(straightRefs[index]);
        if (connection == nullptr || connection->userId == 0) {
            result.status = SectorNavigationQueryStatus::InternalError;
            ++impl->counters.failedQueries;
            return result;
        }
        const int doorId = static_cast<int>(connection->userId);
        result.cornerDoorIds[index] = doorId;
        dtPolyRef previousReference = 0;
        for (int corridorIndex = 0; corridorIndex < corridorCount; ++corridorIndex) {
            if (corridor[corridorIndex] == straightRefs[index]) {
                if (corridorIndex > 0) previousReference = corridor[corridorIndex - 1];
                break;
            }
        }
        float connectionStart[3]{};
        float connectionEnd[3]{};
        if (previousReference == 0
                || dtStatusFailed(impl->navMesh->getOffMeshConnectionPolyEndPoints(
                        previousReference,
                        straightRefs[index],
                        connectionStart,
                        connectionEnd))) {
            result.status = SectorNavigationQueryStatus::InternalError;
            ++impl->counters.failedQueries;
            return result;
        }
        result.corners[index] = FromFloat3(connectionStart);
        result.cornerDoorLandings[index] = FromFloat3(connectionEnd);
        const auto debug = std::find_if(
                impl->debugCache.doorLinks.begin(),
                impl->debugCache.doorLinks.end(),
                [doorId](const SectorNavigationDebugDoorLink& link) {
                    return link.placedObjectId == doorId;
                });
        if (debug != impl->debugCache.doorLinks.end()) {
            const float frontDistance = Vector3DistanceSqr(
                    result.corners[index], debug->frontStage);
            const float backDistance = Vector3DistanceSqr(
                    result.corners[index], debug->backStage);
            result.cornerDoorDirections[index] = frontDistance <= backDistance
                    ? SectorNavigationDoorDirection::FrontToBack
                    : SectorNavigationDoorDirection::BackToFront;
        }
    }
    if (capacityExceeded || dtStatusDetail(straightStatus, DT_BUFFER_TOO_SMALL)) {
        result.status = SectorNavigationQueryStatus::CapacityExceeded;
        ++impl->counters.failedQueries;
    } else if (partial) {
        result.status = SectorNavigationQueryStatus::Partial;
        ++impl->counters.partialQueries;
    } else {
        result.status = SectorNavigationQueryStatus::Success;
        ++impl->counters.successfulQueries;
    }
    return result;
}

bool SectorNavigationWorld::SetDoorLinkRuntimeState(
        int placedObjectId,
        SectorNavigationDoorLinkState state,
        uint32_t holderCount)
{
    if (!impl || impl->navMesh == nullptr || placedObjectId <= 0) return false;
    const auto found = std::find_if(
            impl->doorLinks.begin(), impl->doorLinks.end(),
            [placedObjectId](const Impl::RuntimeDoorLink& link) {
                return link.placedObjectId == placedObjectId;
            });
    if (found == impl->doorLinks.end()) return false;
    unsigned short flags = SectorNavigationPolyFlag_Walk
            | SectorNavigationPolyFlag_Door;
    if (state == SectorNavigationDoorLinkState::RequiresOpening) {
        flags |= SectorNavigationPolyFlag_DoorRequiresOpening;
    } else if (state == SectorNavigationDoorLinkState::Disabled) {
        flags |= SectorNavigationPolyFlag_Disabled;
    }
    if (dtStatusFailed(impl->navMesh->setPolyFlags(found->polygon, flags))) {
        return false;
    }
    const bool changed = found->state != state || found->holderCount != holderCount;
    found->state = state;
    found->holderCount = holderCount;
    const auto debug = std::find_if(
            impl->debugCache.doorLinks.begin(),
            impl->debugCache.doorLinks.end(),
            [placedObjectId](const SectorNavigationDebugDoorLink& link) {
                return link.placedObjectId == placedObjectId;
            });
    if (debug != impl->debugCache.doorLinks.end()) {
        debug->state = state;
        debug->holderCount = holderCount;
    }
    if (changed) impl->BumpDebugRevision();
    return true;
}

bool SectorNavigationWorld::GetDoorLinkRuntimeState(
        int placedObjectId,
        SectorNavigationDoorLinkState& outState) const
{
    if (!impl) return false;
    const auto found = std::find_if(
            impl->doorLinks.begin(), impl->doorLinks.end(),
            [placedObjectId](const Impl::RuntimeDoorLink& link) {
                return link.placedObjectId == placedObjectId;
            });
    if (found == impl->doorLinks.end()) return false;
    outState = found->state;
    return true;
}

SectorNavigationAgentHandle SectorNavigationWorld::AllocateAgentRecord()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return {};
    return impl->Allocate<SectorNavigationAgentHandle>(
            impl->agentSlots, impl->agentGrowthWarned, "agent record");
}

SectorNavigationPathHandle SectorNavigationWorld::AllocatePathRecord()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return {};
    return impl->Allocate<SectorNavigationPathHandle>(
            impl->pathSlots, impl->pathGrowthWarned, "path record");
}

bool SectorNavigationWorld::IsAgentRecordValid(
        SectorNavigationAgentHandle handle) const
{
    return impl && impl->IsValid(impl->agentSlots, handle);
}

bool SectorNavigationWorld::IsPathRecordValid(
        SectorNavigationPathHandle handle) const
{
    return impl && impl->IsValid(impl->pathSlots, handle);
}

bool SectorNavigationWorld::ReleaseAgentRecord(SectorNavigationAgentHandle handle)
{
    if (!impl || !impl->IsValid(impl->agentSlots, handle)) return false;
    RecordSlot& slot = impl->agentSlots[handle.index];
    if (impl->crowd != nullptr && slot.crowdIndex >= 0) {
        impl->crowd->removeAgent(slot.crowdIndex);
    }
    return impl->Release(impl->agentSlots, handle);
}

bool SectorNavigationWorld::ReleasePathRecord(SectorNavigationPathHandle handle)
{
    return impl && impl->Release(impl->pathSlots, handle);
}

bool SectorNavigationWorld::SynchronizeCrowdAgent(
        SectorNavigationAgentHandle handle,
        Vector3 physicalPosition,
        Vector2 actualVelocity,
        float maximumSpeed,
        bool participate)
{
    if (!impl || impl->state != SectorNavigationState::Ready
            || impl->crowd == nullptr
            || !impl->IsValid(impl->agentSlots, handle)) {
        return false;
    }
    RecordSlot& slot = impl->agentSlots[handle.index];
    if (!participate) {
        if (slot.crowdIndex >= 0) impl->crowd->removeAgent(slot.crowdIndex);
        slot.crowdIndex = -1;
        return true;
    }
    if (!std::isfinite(physicalPosition.x)
            || !std::isfinite(physicalPosition.y)
            || !std::isfinite(physicalPosition.z)
            || !std::isfinite(actualVelocity.x)
            || !std::isfinite(actualVelocity.y)) {
        return false;
    }
    maximumSpeed = std::clamp(
            std::isfinite(maximumSpeed) ? maximumSpeed : 0.0f,
            0.0f, 1000.0f);
    bool reattach = slot.crowdIndex < 0;
    if (!reattach) {
        const dtCrowdAgent* agent = impl->crowd->getAgent(slot.crowdIndex);
        if (agent == nullptr || !agent->active) {
            reattach = true;
        } else {
            const float dx = agent->npos[0] - physicalPosition.x;
            const float dz = agent->npos[2] - physicalPosition.z;
            const float threshold = impl->settings.agentRadius
                    * impl->crowdSettings.reconciliationDistanceRadiusScale;
            if (dx * dx + dz * dz > threshold * threshold) {
                impl->crowd->removeAgent(slot.crowdIndex);
                slot.crowdIndex = -1;
                reattach = true;
                ++impl->crowdStatistics.reconciliations;
            }
        }
    }
    dtCrowdAgentParams parameters{};
    parameters.radius = impl->settings.agentRadius;
    parameters.height = impl->settings.agentHeight;
    parameters.maxAcceleration = impl->crowdSettings.maximumAcceleration;
    parameters.maxSpeed = maximumSpeed;
    parameters.collisionQueryRange = impl->settings.agentRadius
            * impl->crowdSettings.collisionQueryRangeRadiusScale;
    parameters.pathOptimizationRange = impl->settings.agentRadius
            * impl->crowdSettings.pathOptimizationRangeRadiusScale;
    parameters.separationWeight = impl->crowdSettings.separationWeight;
    parameters.updateFlags = DT_CROWD_OBSTACLE_AVOIDANCE
            | DT_CROWD_SEPARATION;
    parameters.obstacleAvoidanceType = static_cast<unsigned char>(
            impl->crowdSettings.avoidanceQuality);
    parameters.queryFilterType = 0;
    if (reattach) {
        const float position[3]{
                physicalPosition.x, physicalPosition.y, physicalPosition.z};
        slot.crowdIndex = impl->crowd->addAgent(position, &parameters);
        const dtCrowdAgent* attached = slot.crowdIndex >= 0
                ? impl->crowd->getAgent(slot.crowdIndex) : nullptr;
        if (slot.crowdIndex < 0 || attached == nullptr
                || attached->state == DT_CROWDAGENT_STATE_INVALID) {
            if (slot.crowdIndex >= 0) {
                impl->crowd->removeAgent(slot.crowdIndex);
                slot.crowdIndex = -1;
            }
            if (!slot.crowdWarningReported) {
                slot.crowdWarningReported = true;
                ++impl->crowdStatistics.attachmentFailures;
                ++impl->crowdStatistics.capacityWarnings;
                ++impl->counters.capacityWarnings;
                impl->Record(
                        SectorNavigationDiagnosticSeverity::Warning,
                        SectorNavigationBuildStage::Complete,
                        "Crowd agent could not attach within fixed capacity or navigation projection; collision-only steering fallback remains active");
            }
            return false;
        }
        slot.crowdWarningReported = false;
    } else {
        impl->crowd->updateAgentParameters(slot.crowdIndex, &parameters);
    }
    dtCrowdAgent* agent = impl->crowd->getEditableAgent(slot.crowdIndex);
    if (agent != nullptr) {
        agent->vel[0] = actualVelocity.x;
        agent->vel[1] = 0.0f;
        agent->vel[2] = actualVelocity.y;
    }
    return true;
}

bool SectorNavigationWorld::SetCrowdAgentDesiredVelocity(
        SectorNavigationAgentHandle handle,
        Vector2 desiredVelocity)
{
    if (!impl || impl->crowd == nullptr
            || !impl->IsValid(impl->agentSlots, handle)) {
        return false;
    }
    const RecordSlot& slot = impl->agentSlots[handle.index];
    if (slot.crowdIndex < 0
            || !std::isfinite(desiredVelocity.x)
            || !std::isfinite(desiredVelocity.y)) {
        return false;
    }
    const float velocity[3]{desiredVelocity.x, 0.0f, desiredVelocity.y};
    return impl->crowd->requestMoveVelocity(slot.crowdIndex, velocity);
}

void SectorNavigationWorld::UpdateCrowd(float rawDt)
{
    if (!impl || impl->state != SectorNavigationState::Ready
            || impl->crowd == nullptr) {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    const float simulated = std::min(
            dt,
            impl->crowdSettings.maximumStepSeconds
                    * static_cast<float>(impl->crowdSettings.maximumSubsteps));
    const int substeps = simulated > 0.0f
            ? std::clamp(
                    static_cast<int>(std::ceil(
                            simulated / impl->crowdSettings.maximumStepSeconds)),
                    1,
                    impl->crowdSettings.maximumSubsteps)
            : 0;
    const float step = substeps > 0
            ? simulated / static_cast<float>(substeps) : 0.0f;
    for (int index = 0; index < substeps; ++index) {
        impl->crowd->update(step, nullptr);
    }
    impl->crowdStatistics.activeAgentCount = 0;
    for (const RecordSlot& slot : impl->agentSlots) {
        if (!slot.occupied || slot.crowdIndex < 0) continue;
        const dtCrowdAgent* agent = impl->crowd->getAgent(slot.crowdIndex);
        if (agent != nullptr && agent->active) {
            ++impl->crowdStatistics.activeAgentCount;
        }
    }
    impl->crowdStatistics.lastVelocitySampleCount =
            impl->crowd->getVelocitySampleCount();
    const float elapsed = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - started).count();
    impl->crowdStatistics.lastUpdateMilliseconds = elapsed;
    impl->crowdStatistics.peakUpdateMilliseconds = std::max(
            impl->crowdStatistics.peakUpdateMilliseconds, elapsed);
}

SectorNavigationCrowdAgentState SectorNavigationWorld::GetCrowdAgentState(
        SectorNavigationAgentHandle handle) const
{
    SectorNavigationCrowdAgentState result;
    if (!impl || impl->crowd == nullptr
            || !impl->IsValid(impl->agentSlots, handle)) {
        return result;
    }
    const RecordSlot& slot = impl->agentSlots[handle.index];
    if (slot.crowdIndex < 0) return result;
    const dtCrowdAgent* agent = impl->crowd->getAgent(slot.crowdIndex);
    if (agent == nullptr || !agent->active) return result;
    result.attached = true;
    result.steeredVelocity = {agent->vel[0], agent->vel[2]};
    result.neighborCount = agent->nneis;
    result.nearestNeighborDistance = agent->nneis > 0
            ? agent->neis[0].dist : 0.0f;
    return result;
}

SectorNavigationState SectorNavigationWorld::State() const
{
    return impl ? impl->state : SectorNavigationState::Uninitialized;
}

SectorNavigationBuildStage SectorNavigationWorld::BuildStage() const
{
    return impl ? impl->stage : SectorNavigationBuildStage::None;
}

const SectorNavigationSettings& SectorNavigationWorld::Settings() const
{
    static const SectorNavigationSettings defaults;
    return impl ? impl->settings : defaults;
}

const SectorNavigationCapacitySettings& SectorNavigationWorld::Capacities() const
{
    static const SectorNavigationCapacitySettings defaults;
    return impl ? impl->capacities : defaults;
}

const SectorNavigationDynamicObstacleSettings&
SectorNavigationWorld::DynamicObstacleSettings() const
{
    static const SectorNavigationDynamicObstacleSettings defaults;
    return impl ? impl->dynamicObstacleSettings : defaults;
}

const SectorNavigationDynamicObstacleStatistics&
SectorNavigationWorld::DynamicObstacleStatistics() const
{
    static const SectorNavigationDynamicObstacleStatistics empty;
    return impl ? impl->dynamicObstacleStatistics : empty;
}

const SectorNavigationCrowdSettings& SectorNavigationWorld::CrowdSettings() const
{
    static const SectorNavigationCrowdSettings defaults;
    return impl ? impl->crowdSettings : defaults;
}

const SectorNavigationCrowdStatistics& SectorNavigationWorld::CrowdStatistics() const
{
    static const SectorNavigationCrowdStatistics empty;
    return impl ? impl->crowdStatistics : empty;
}

const SectorNavigationCounters& SectorNavigationWorld::Counters() const
{
    static const SectorNavigationCounters empty;
    return impl ? impl->counters : empty;
}

const std::vector<SectorNavigationDiagnostic>& SectorNavigationWorld::Diagnostics() const
{
    static const std::vector<SectorNavigationDiagnostic> empty;
    return impl ? impl->diagnostics : empty;
}

uint64_t SectorNavigationWorld::SourceRevision() const { return impl ? impl->sourceRevision : 0; }
uint64_t SectorNavigationWorld::BuildRevision() const { return impl ? impl->buildRevision : 0; }
uint64_t SectorNavigationWorld::TileRevision() const { return impl ? impl->tileRevision : 0; }
uint64_t SectorNavigationWorld::SourceHash() const { return impl ? impl->sourceHash : 0; }

const SectorNavigationBuildStatistics& SectorNavigationWorld::BuildStatistics() const
{
    static const SectorNavigationBuildStatistics empty;
    return impl ? impl->statistics : empty;
}

const SectorNavigationDebugCache& SectorNavigationWorld::DebugCache() const
{
    static const SectorNavigationDebugCache empty;
    return impl ? impl->debugCache : empty;
}

} // namespace game
