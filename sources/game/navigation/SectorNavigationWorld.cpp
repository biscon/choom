#include "game/navigation/SectorNavigationWorld.h"

#include "game/navigation/SectorNavigationBuildInput.h"
#include "game/navigation/SectorNavigationCompression.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "Recast.h"

#include <algorithm>
#include <array>
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
            if (polyAreas[index] == static_cast<unsigned char>(SectorNavigationArea::Ground)) {
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
    };
    SectorNavigationSettings settings;
    SectorNavigationCapacitySettings capacities;
    SectorNavigationQueryFilterPolicy filterPolicy;
    SectorNavigationState state = SectorNavigationState::Uninitialized;
    SectorNavigationBuildStage stage = SectorNavigationBuildStage::None;
    SectorNavigationCounters counters;
    SectorNavigationBuildStatistics statistics;
    SectorNavigationDebugCache debugCache;
    std::vector<SectorNavigationDiagnostic> diagnostics;
    std::vector<RecordSlot> agentSlots;
    std::vector<RecordSlot> pathSlots;
    SectorNavigationBuildInput buildInput;
    std::vector<NavigationTileCoordinate> tileCoordinates;
    std::vector<RuntimeDoorLink> doorLinks;
    size_t nextTileCoordinate = 0;
    uint64_t sourceRevision = 0;
    uint64_t buildRevision = 0;
    uint64_t sourceHash = 0;
    bool agentGrowthWarned = false;
    bool pathGrowthWarned = false;

    dtNavMesh* navMesh = nullptr;
    dtTileCache* tileCache = nullptr;
    dtNavMeshQuery* query = nullptr;
    NavigationTileCacheAllocator tileAllocator;
    NavigationTileCacheCompressor tileCompressor;
    NavigationTileCacheMeshProcess meshProcess;

    ~Impl() { ReleaseNavigation(); }

    void ReleaseNavigation()
    {
        if (query != nullptr) dtFreeNavMeshQuery(query);
        if (tileCache != nullptr) dtFreeTileCache(tileCache);
        if (navMesh != nullptr) dtFreeNavMesh(navMesh);
        query = nullptr;
        tileCache = nullptr;
        navMesh = nullptr;
        buildInput = {};
        tileCoordinates.clear();
        doorLinks.clear();
        meshProcess.Configure({});
        nextTileCoordinate = 0;
        statistics = {};
        debugCache = {};
        sourceHash = 0;
    }

    void Record(
            SectorNavigationDiagnosticSeverity severity,
            SectorNavigationBuildStage diagnosticStage,
            const std::string& message)
    {
        if (diagnostics.size() == diagnostics.capacity()) {
            ++counters.capacityWarnings;
            std::fprintf(stderr,
                    "[Navigation WARNING] Diagnostic capacity exceeded; oldest diagnostic will be discarded.\n");
            if (!diagnostics.empty()) diagnostics.erase(diagnostics.begin());
        }
        diagnostics.push_back({severity, diagnosticStage, message});
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
        slot.generation = NextGeneration(slot.generation);
        return true;
    }

    void InvalidateRecords()
    {
        for (RecordSlot& slot : agentSlots) {
            slot.occupied = false;
            slot.generation = NextGeneration(slot.generation);
        }
        for (RecordSlot& slot : pathSlots) {
            slot.occupied = false;
            slot.generation = NextGeneration(slot.generation);
        }
    }

    bool BeginBuild(
            const SectorTopologyMap& map,
            const std::vector<SectorStaticModelCollider>& colliders,
            std::string& error)
    {
        ReleaseNavigation();
        diagnostics.clear();
        stage = SectorNavigationBuildStage::BuildingInput;
        std::vector<std::string> warnings;
        if (!BuildSectorNavigationBuildInput(
                    map, colliders, settings, buildInput, warnings, error)) {
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
        sourceHash = buildInput.sourceHash;
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

    bool FinalizeBuild(std::string& error)
    {
        query = dtAllocNavMeshQuery();
        if (query == nullptr
            || dtStatusFailed(query->init(navMesh, capacities.queryNodeCapacity))) {
            error = "Could not initialize bounded Detour navigation queries";
            return false;
        }
        stage = SectorNavigationBuildStage::BuildingDebugCache;
        debugCache = {};
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
        buildInput = {};
        tileCoordinates.clear();
        tileCoordinates.shrink_to_fit();
        ++buildRevision;
        debugCache.navigationRevision = buildRevision;
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
        SectorNavigationCapacitySettings capacities)
{
    if (!impl) impl = std::make_unique<Impl>();
    impl->ReleaseNavigation();
    impl->settings = NormalizeSectorNavigationSettings(settings);
    impl->capacities = NormalizeSectorNavigationCapacitySettings(capacities);
    impl->diagnostics.clear();
    impl->diagnostics.reserve(impl->capacities.diagnosticCapacity);
    impl->agentSlots.clear();
    impl->agentSlots.reserve(impl->capacities.agentCapacity);
    impl->pathSlots.clear();
    impl->pathSlots.reserve(impl->capacities.pathRecordCapacity);
    impl->counters = {};
    impl->sourceRevision = 0;
    impl->buildRevision = 0;
    impl->agentGrowthWarned = false;
    impl->pathGrowthWarned = false;
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
    impl->stage = SectorNavigationBuildStage::None;
    impl->state = SectorNavigationState::Uninitialized;
    ++impl->buildRevision;
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
}

void SectorNavigationWorld::RequestRebuild()
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    impl->stage = SectorNavigationBuildStage::WaitingForStaticCollision;
    impl->state = SectorNavigationState::Queued;
    ++impl->sourceRevision;
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
}

void SectorNavigationWorld::Fail(
        SectorNavigationBuildStage stage,
        const std::string& message)
{
    if (!impl || impl->state == SectorNavigationState::Uninitialized) return;
    impl->InvalidateRecords();
    impl->ReleaseNavigation();
    impl->stage = stage;
    impl->state = SectorNavigationState::Failed;
    impl->Record(SectorNavigationDiagnosticSeverity::Error, stage, message);
    ++impl->buildRevision;
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
            straightCapacity);
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
    found->state = state;
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
    return impl && impl->Release(impl->agentSlots, handle);
}

bool SectorNavigationWorld::ReleasePathRecord(SectorNavigationPathHandle handle)
{
    return impl && impl->Release(impl->pathSlots, handle);
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
