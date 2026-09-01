#include "game/navigation/SectorNavigationBuildInput.h"
#include "game/navigation/SectorNavigationCompression.h"
#include "game/navigation/SectorNavigationTypes.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) <= 0.00001f;
}

void TestDependencyAndCoordinates()
{
    Check(sizeof(dtPolyRef) == sizeof(uint64_t),
          "Detour polygon references use the selected 64-bit layout");
    dtNavMesh* mesh = dtAllocNavMesh();
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    Check(mesh != nullptr && query != nullptr,
          "project-owned test links the complete Recast Navigation target");
    dtFreeNavMeshQuery(query);
    dtFreeNavMesh(mesh);

    const Vector3 world{12.5f, -3.25f, 9.75f};
    const game::SectorNavigationPosition navigation =
            game::SectorWorldToNavigationPosition(world);
    const Vector3 roundTrip = game::SectorNavigationToWorldPosition(navigation);
    Check(Near(roundTrip.x, world.x)
                  && Near(roundTrip.y, world.y)
                  && Near(roundTrip.z, world.z),
          "navigation coordinate conversion preserves engine axes");
    Check(Near(game::SectorNavigationAuthoredHeightToWorld(8.0f), 1.0f),
          "authored sector height converts to world units exactly once");
}

void TestSettingsAndStatusContracts()
{
    game::SectorNavigationSettings settings;
    settings.agentRadius = -1.0f;
    settings.maximumVerticesPerPolygon = 20;
    const game::SectorNavigationSettings normalized =
            game::NormalizeSectorNavigationSettings(settings);
    Check(normalized.agentRadius > 0.0f
                  && normalized.maximumVerticesPerPolygon == 6,
          "navigation settings normalize invalid values");
    Check(std::string(game::SectorNavigationQueryStatusName(
                  game::SectorNavigationQueryStatus::CapacityExceeded))
                    == "capacity exceeded",
          "project status formatting does not expose raw Detour status");
    game::SectorNavigationDynamicObstacleSettings dynamicSettings;
    dynamicSettings.positionThresholdWorld = -1.0f;
    dynamicSettings.fastLinearSpeedWorld = INFINITY;
    const game::SectorNavigationDynamicObstacleSettings normalizedDynamic =
            game::NormalizeSectorNavigationDynamicObstacleSettings(
                    dynamicSettings);
    Check(normalizedDynamic.positionThresholdWorld > 0.0f
                  && std::isfinite(normalizedDynamic.fastLinearSpeedWorld),
          "dynamic obstacle motion settings normalize invalid thresholds");
    game::SectorNavigationCrowdSettings crowdSettings;
    crowdSettings.maximumAcceleration = INFINITY;
    crowdSettings.maximumSubsteps = 0;
    const game::SectorNavigationCrowdSettings normalizedCrowd =
            game::NormalizeSectorNavigationCrowdSettings(crowdSettings);
    Check(std::isfinite(normalizedCrowd.maximumAcceleration)
                  && normalizedCrowd.maximumSubsteps >= 1
                  && normalizedCrowd.avoidanceQuality
                          == game::SectorNavigationAvoidanceQuality::High,
          "Crowd settings normalize to bounded high-quality avoidance");

    game::SectorTopologyMap map;
    Check(Near(game::BuildSectorNavigationSettingsForMap(map).agentMaximumClimb,
                  0.25f),
          "legacy map navigation keeps the default 0.25m climb");
    map.previewSettings.stepHeight = 0.4f;
    const game::SectorNavigationSettings mapSettings =
            game::BuildSectorNavigationSettingsForMap(map);
    Check(Near(mapSettings.agentMaximumClimb, 0.4f)
                  && Near(mapSettings.agentRadius, 0.25f)
                  && Near(mapSettings.agentHeight, 1.6f),
          "map step height configures climb without changing the fixed NPC radius or height");
}

void TestLifecycleAndHandles()
{
    game::SectorNavigationWorld world;
    Check(world.State() == game::SectorNavigationState::Uninitialized,
          "navigation begins uninitialized");

    game::SectorNavigationCapacitySettings capacities;
    capacities.agentCapacity = 1;
    capacities.pathRecordCapacity = 1;
    Check(world.Initialize({}, capacities), "navigation initializes");
    Check(world.State() == game::SectorNavigationState::Empty,
          "initialized navigation is empty before a build");

    const game::SectorNavigationAgentHandle first = world.AllocateAgentRecord();
    const game::SectorNavigationAgentHandle grown = world.AllocateAgentRecord();
    Check(!game::IsNull(first) && !game::IsNull(grown),
          "agent record capacity growth remains a development fallback");
    Check(world.Counters().capacityWarnings == 1,
          "agent record fallback reports one capacity warning");
    Check(world.ReleaseAgentRecord(first), "live agent handle releases");
    const game::SectorNavigationAgentHandle replacement =
            world.AllocateAgentRecord();
    Check(replacement.index == first.index
                  && replacement.generation != first.generation,
          "reused agent slot receives a new generation");
    Check(!world.ReleaseAgentRecord(first), "stale agent handle is rejected");

    world.RequestRebuild();
    Check(world.State() == game::SectorNavigationState::Queued,
          "rebuild request enters queued state");
    Check(!world.IsAgentRecordValid(replacement),
          "navigation rebuild invalidates external agent handles");
    world.Fail(game::SectorNavigationBuildStage::RasterizingTiles, "fixture failure");
    Check(world.State() == game::SectorNavigationState::Failed
                  && !world.Diagnostics().empty(),
          "failed lifecycle retains a bounded diagnostic");
    world.ResetForRebuild();
    Check(world.State() == game::SectorNavigationState::Empty,
          "rebuild reset returns to empty");
    world.Shutdown();
    Check(world.State() == game::SectorNavigationState::Uninitialized,
          "shutdown returns to uninitialized");
}

game::SectorTopologyMap MakeSquareMap(game::SectorCoord size = 2048)
{
    game::SectorTopologyMap map;
    game::SectorTopologySector sector;
    sector.id = 1;
    sector.floorZ = 0.0f;
    sector.ceilingZ = 24.0f;
    map.sectors.push_back(sector);
    const std::vector<std::pair<game::SectorCoord, game::SectorCoord>> points{
            {0, 0}, {size, 0}, {size, size}, {0, size}};
    for (size_t index = 0; index < points.size(); ++index) {
        map.vertices.push_back({static_cast<int>(index + 1),
                points[index].first, points[index].second});
    }
    for (size_t index = 0; index < points.size(); ++index) {
        const int id = static_cast<int>(index + 1);
        map.lineDefs.push_back({id, id, static_cast<int>((index + 1) % 4 + 1), id, -1});
        game::SectorTopologySideDef side;
        side.id = id;
        side.lineDefId = id;
        side.side = game::SectorTopologySideKind::Front;
        side.sectorId = 1;
        map.sideDefs.push_back(side);
    }
    return map;
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    map.sideDefs.push_back(sideDef);
}

game::SectorTopologyMap MakeAdjacentMap(
        float leftFloor = 0.0f,
        float leftCeiling = 24.0f,
        float rightFloor = 0.0f,
        float rightCeiling = 24.0f)
{
    game::SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 1024, 0}, {3, 1024, 1024}, {4, 0, 1024},
            {5, 2048, 0}, {6, 2048, 1024}};
    map.lineDefs = {
            {1, 1, 2, 1, -1}, {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1}, {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1}, {6, 5, 6, 6, -1}, {7, 6, 3, 7, -1}};
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, game::SectorTopologySideKind::Back, 20);
    game::SectorTopologySector left;
    left.id = 10;
    left.floorZ = leftFloor;
    left.ceilingZ = leftCeiling;
    game::SectorTopologySector right;
    right.id = 20;
    right.floorZ = rightFloor;
    right.ceilingZ = rightCeiling;
    map.sectors = {left, right};
    return map;
}

game::SectorTopologyMap MakeSectorStairMap(
        int stepCount,
        float authoredRisePerStep)
{
    game::SectorTopologyMap map;
    constexpr game::SectorCoord TreadDepth = 128; // 1.0 world unit.
    constexpr game::SectorCoord StairWidth = 512; // 4.0 world units.
    for (int boundary = 0; boundary <= stepCount; ++boundary) {
        const game::SectorCoord x = boundary * TreadDepth;
        map.vertices.push_back({boundary * 2 + 1, x, 0});
        map.vertices.push_back({boundary * 2 + 2, x, StairWidth});
    }

    int nextLineId = 1;
    int nextSideId = 1;
    const auto addLine = [&](int startVertexId, int endVertexId,
                             int frontSectorId, int backSectorId = 0) {
        const int lineId = nextLineId++;
        const int frontSideId = nextSideId++;
        const int backSideId = backSectorId != 0 ? nextSideId++ : -1;
        map.lineDefs.push_back({lineId, startVertexId, endVertexId,
                frontSideId, backSideId});
        AddSide(map, frontSideId, lineId,
                game::SectorTopologySideKind::Front, frontSectorId);
        if (backSectorId != 0) {
            AddSide(map, backSideId, lineId,
                    game::SectorTopologySideKind::Back, backSectorId);
        }
    };

    for (int step = 0; step < stepCount; ++step) {
        const int sectorId = step + 1;
        game::SectorTopologySector sector;
        sector.id = sectorId;
        sector.floorZ = authoredRisePerStep * static_cast<float>(step);
        sector.ceilingZ = sector.floorZ + 24.0f;
        map.sectors.push_back(sector);

        const int leftBottom = step * 2 + 1;
        const int leftTop = step * 2 + 2;
        const int rightBottom = (step + 1) * 2 + 1;
        const int rightTop = (step + 1) * 2 + 2;
        addLine(leftBottom, rightBottom, sectorId);
        addLine(rightTop, leftTop, sectorId);
        if (step == 0) addLine(leftTop, leftBottom, sectorId);
        if (step == stepCount - 1) {
            addLine(rightBottom, rightTop, sectorId);
        } else {
            addLine(rightBottom, rightTop, sectorId, sectorId + 1);
        }
    }
    return map;
}

game::SectorTopologyMap MakeDoorThresholdStepMap()
{
    game::SectorTopologyMap map;
    constexpr game::SectorCoord DoorX = 1024; // 8.0 world units.
    constexpr game::SectorCoord ThresholdX = 1056; // 0.25m threshold depth.
    constexpr game::SectorCoord EndX = 2048;
    constexpr game::SectorCoord Width = 160; // 1.25m door width.
    map.vertices = {
            {1, 0, 0}, {2, DoorX, 0}, {3, DoorX, Width}, {4, 0, Width},
            {5, ThresholdX, 0}, {6, ThresholdX, Width},
            {7, EndX, 0}, {8, EndX, Width}};

    int nextLineId = 1;
    int nextSideId = 1;
    int doorLineId = 0;
    int doorFrontSideId = 0;
    int doorBackSideId = 0;
    const auto addLine = [&](int startVertexId, int endVertexId,
                             int frontSectorId, int backSectorId = 0) {
        const int lineId = nextLineId++;
        const int frontSideId = nextSideId++;
        const int backSideId = backSectorId != 0 ? nextSideId++ : -1;
        map.lineDefs.push_back({lineId, startVertexId, endVertexId,
                frontSideId, backSideId});
        AddSide(map, frontSideId, lineId,
                game::SectorTopologySideKind::Front, frontSectorId);
        if (backSectorId != 0) {
            AddSide(map, backSideId, lineId,
                    game::SectorTopologySideKind::Back, backSectorId);
        }
        return std::array<int, 3>{lineId, frontSideId, backSideId};
    };

    addLine(1, 2, 10);
    const std::array<int, 3> doorPortal = addLine(2, 3, 10, 20);
    doorLineId = doorPortal[0];
    doorFrontSideId = doorPortal[1];
    doorBackSideId = doorPortal[2];
    addLine(3, 4, 10);
    addLine(4, 1, 10);
    addLine(2, 5, 20);
    addLine(5, 6, 20, 30);
    addLine(6, 3, 20);
    addLine(5, 7, 30);
    addLine(7, 8, 30);
    addLine(8, 6, 30);

    game::SectorTopologySector room;
    room.id = 10;
    game::SectorTopologySector threshold;
    threshold.id = 20;
    game::SectorTopologySector raisedRoom;
    raisedRoom.id = 30;
    raisedRoom.floorZ = 3.0f; // 0.375 world units.
    raisedRoom.ceilingZ = 27.0f;
    map.sectors = {room, threshold, raisedRoom};

    game::SectorPlacedRuntimeObject object;
    object.id = 77;
    object.kind = "door";
    object.door.anchor.lineDefId = doorLineId;
    object.door.anchor.frontSectorId = 10;
    object.door.anchor.backSectorId = 20;
    object.door.anchor.frontSideDefId = doorFrontSideId;
    object.door.anchor.backSideDefId = doorBackSideId;
    object.door.width = 1.25f;
    object.door.motion = game::SectorDoorMotionType::Swing;
    map.runtimeObjects.push_back(object);
    return map;
}

void AddIndependentLoop(
        game::SectorTopologyMap& map,
        int sectorId,
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& points)
{
    std::vector<int> vertexIds;
    for (const auto& point : points) {
        const int vertexId = game::AllocateSectorTopologyVertexId(map);
        map.vertices.push_back({vertexId, point.first, point.second});
        vertexIds.push_back(vertexId);
    }
    for (size_t index = 0; index < vertexIds.size(); ++index) {
        const int lineId = game::AllocateSectorTopologyLineDefId(map);
        const int sideId = game::AllocateSectorTopologySideDefId(map);
        map.lineDefs.push_back({lineId, vertexIds[index],
                vertexIds[(index + 1) % vertexIds.size()], sideId, -1});
        AddSide(map, sideId, lineId, game::SectorTopologySideKind::Front, sectorId);
    }
}

void FinishBuild(game::SectorNavigationWorld& world, const game::SectorTopologyMap& map)
{
    const std::vector<game::SectorStaticModelCollider> colliders;
    for (int iteration = 0; iteration < 1000
            && (world.State() == game::SectorNavigationState::Queued
                || world.State() == game::SectorNavigationState::Building); ++iteration) {
        world.UpdateBuild(map, colliders, 0);
    }
}

void TestHardenedLifecycleAndDiagnostics()
{
    game::SectorNavigationCapacitySettings capacities;
    capacities.diagnosticCapacity = 2;
    capacities.tileBuildBudgetPerUpdate = 1;
    game::SectorNavigationWorld world;
    Check(world.Initialize({}, capacities),
          "hardened lifecycle fixture initializes");

    const std::string oversized(
            game::SectorNavigationMaximumDiagnosticMessageBytes + 80u, 'x');
    world.Fail(game::SectorNavigationBuildStage::BuildingInput, oversized);
    world.Fail(game::SectorNavigationBuildStage::RasterizingTiles, "second");
    world.Fail(game::SectorNavigationBuildStage::BuildingDebugCache, "third");
    Check(world.Diagnostics().size() == capacities.diagnosticCapacity
                  && world.Diagnostics().front().message == "second"
                  && world.Counters().truncatedDiagnostics == 1
                  && world.Counters().droppedDiagnostics == 1,
          "diagnostics truncate messages and retain only the configured newest entries");

    const game::SectorTopologyMap map = MakeSquareMap(4096);
    world.RequestRebuild();
    const uint64_t clearedDebugRevision =
            world.DebugCache().navigationRevision;
    Check(world.State() == game::SectorNavigationState::Queued
                  && world.DebugCache().walkableTriangles.empty(),
          "a rebuild request immediately releases stale derived navigation data");
    world.UpdateBuild(map, {}, 0);
    Check(world.State() == game::SectorNavigationState::Building,
          "a low-budget build exposes an interruptible building state");
    world.RequestRebuild();
    Check(world.State() == game::SectorNavigationState::Queued
                  && world.DebugCache().navigationRevision > clearedDebugRevision,
          "requesting rebuild during a partial build cancels it and advances debug revision");
    FinishBuild(world, map);
    Check(world.State() == game::SectorNavigationState::Ready
                  && world.Counters().rebuildRequests == 2
                  && world.Counters().completedBuilds == 1
                  && world.BuildStatistics().lastBuildMilliseconds >= 0.0f,
          "cancelled build recovers and reports deterministic lifecycle counters and timing");

    const uint64_t readyDebugRevision =
            world.DebugCache().navigationRevision;
    world.MarkStale();
    Check(world.State() == game::SectorNavigationState::Stale
                  && !world.DebugCache().walkableTriangles.empty(),
          "stale state remains diagnosable until an explicit rebuild is requested");
    world.RequestRebuild();
    Check(world.DebugCache().walkableTriangles.empty()
                  && world.DebugCache().navigationRevision > readyDebugRevision,
          "queued rebuild never exposes the previous mesh as current");
    FinishBuild(world, map);
    world.Shutdown();
    Check(world.State() == game::SectorNavigationState::Uninitialized
                  && world.Counters().rebuildRequests == 0
                  && world.SourceRevision() == 0
                  && world.BuildRevision() == 0
                  && world.DebugCache().navigationRevision == 0,
          "shutdown clears per-level counters and all public revisions");
}

void TestCrowdCapacityAndAgentReuse()
{
    game::SectorNavigationCapacitySettings capacities;
    capacities.agentCapacity = 1;
    game::SectorNavigationWorld world;
    world.Initialize({}, capacities);
    world.RequestRebuild();
    FinishBuild(world, MakeSquareMap());
    Check(world.State() == game::SectorNavigationState::Ready,
          "Crowd capacity fixture builds navigation");
    const game::SectorNavigationAgentHandle first = world.AllocateAgentRecord();
    const game::SectorNavigationAgentHandle overflow = world.AllocateAgentRecord();
    Check(world.SynchronizeCrowdAgent(
                  first, {2.0f, 0.0f, 2.0f}, {}, 2.0f)
                  && !world.SynchronizeCrowdAgent(
                          overflow, {4.0f, 0.0f, 2.0f}, {}, 2.0f),
          "fixed Crowd capacity diagnoses an excess agent safely");
    Check(world.CrowdStatistics().attachmentFailures == 1
                  && world.CrowdStatistics().capacityWarnings == 1,
          "Crowd capacity failure is exposed through bounded diagnostics");
    Check(world.SetCrowdAgentDesiredVelocity(first, {1.0f, 0.0f}),
          "attached Crowd agent accepts a desired velocity");
    world.UpdateCrowd(0.1f);
    Check(world.GetCrowdAgentState(first).attached
                  && world.GetCrowdAgentState(first).steeredVelocity.x > 0.0f,
          "Crowd produces acceleration-limited steering for an attached agent");
    Check(world.ReleaseAgentRecord(first)
                  && world.SynchronizeCrowdAgent(
                          overflow, {4.0f, 0.0f, 2.0f}, {}, 2.0f),
          "removing an agent makes its fixed Crowd slot reusable");
    world.Shutdown();
}

void FinishDynamicObstacleUpdates(
        game::SectorNavigationWorld& world,
        const std::vector<game::SectorStaticModelCollider>& colliders,
        int maximumUpdates = 128,
        float dt = 0.016f)
{
    for (int update = 0; update < maximumUpdates; ++update) {
        world.UpdateDynamicObstacles(colliders, dt);
        const auto& stats = world.DynamicObstacleStatistics();
        if (stats.backlogCount == 0
                && stats.removingCount == 0
                && stats.pendingCount == 0) {
            break;
        }
    }
}

void TestLayerCompression()
{
    const std::vector<uint8_t> source{
            1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 6, 6, 6, 7, 8, 9};
    std::vector<uint8_t> compressed(
            static_cast<size_t>(game::SectorNavigationMaximumCompressedLayerSize(
                    static_cast<int>(source.size()))));
    int compressedSize = 0;
    Check(game::CompressSectorNavigationLayer(
                  source.data(), static_cast<int>(source.size()), compressed.data(),
                  static_cast<int>(compressed.size()), compressedSize),
          "TileCache layer compressor accepts mixed runs and literals");
    std::vector<uint8_t> decoded(source.size());
    int decodedSize = 0;
    Check(game::DecompressSectorNavigationLayer(
                  compressed.data(), compressedSize, decoded.data(),
                  static_cast<int>(decoded.size()), decodedSize)
                  && decodedSize == static_cast<int>(source.size())
                  && decoded == source,
          "TileCache layer compression round-trips exactly");
    compressed[compressedSize - 1] ^= 0x40u;
    Check(!game::DecompressSectorNavigationLayer(
                  compressed.data(), compressedSize, decoded.data(),
                  static_cast<int>(decoded.size()), decodedSize),
          "TileCache layer decompression rejects corrupt data");
}

void TestStaticBuildAndQueries()
{
    const game::SectorTopologyMap map = MakeSquareMap();
    game::SectorNavigationWorld world;
    Check(world.Initialize(), "static navigation world initializes");
    world.RequestRebuild();
    FinishBuild(world, map);
    if (world.State() != game::SectorNavigationState::Ready
        || world.BuildStatistics().navMeshPolygonCount == 0) {
        std::cerr << "nav state=" << game::SectorNavigationStateName(world.State())
                  << " stage=" << game::SectorNavigationBuildStageName(world.BuildStage()) << '\n';
        for (const auto& diagnostic : world.Diagnostics()) {
            std::cerr << "nav diagnostic: " << diagnostic.message << '\n';
        }
    }
    Check(world.State() == game::SectorNavigationState::Ready,
          "rectangular topology builds a ready tiled navmesh");
    Check(world.BuildStatistics().navMeshPolygonCount > 0
                  && world.BuildStatistics().tileCoordinateCount > 1,
          "static navigation reports polygons and multi-tile capacity");
    Check(!world.DebugCache().walkableTriangles.empty()
                  && !world.DebugCache().polygonEdges.empty()
                  && !world.DebugCache().tileBounds.empty(),
          "static build finalization creates a project-owned debug cache");
    const uint64_t cachedRevision = world.DebugCache().navigationRevision;
    const size_t cachedTriangleCount = world.DebugCache().walkableTriangles.size();
    Check(world.DebugCache().navigationRevision == cachedRevision
                  && world.DebugCache().walkableTriangles.size() == cachedTriangleCount
                  && world.BuildRevision() > 0,
          "steady debug-cache reads do not rebuild or extract Detour data");

    const game::SectorNavigationNearestPointResult nearest =
            world.FindNearestPoint({2.0f, 0.0f, 2.0f});
    if (nearest.status != game::SectorNavigationQueryStatus::Success) {
        std::cerr << "nav stats coordinates=" << world.BuildStatistics().tileCoordinateCount
                  << " polys=" << world.BuildStatistics().navMeshPolygonCount
                  << " tiles=" << world.BuildStatistics().navMeshTileCount
                  << " nearest=" << game::SectorNavigationQueryStatusName(nearest.status) << '\n';
    }
    Check(nearest.status == game::SectorNavigationQueryStatus::Success,
          "nearest-point query projects a position inside the sector");
    const game::SectorNavigationPathResult path = world.FindPath(
            {1.0f, 0.0f, 1.0f}, {7.0f, 0.0f, 7.0f});
    Check(path.status == game::SectorNavigationQueryStatus::Success
                  && path.cornerCount >= 2,
          "bounded path query crosses static tile boundaries");
    const game::SectorNavigationPathResult invalidStart = world.FindPath(
            {-20.0f, 0.0f, -20.0f}, {2.0f, 0.0f, 2.0f});
    Check(invalidStart.status == game::SectorNavigationQueryStatus::StartNotOnNavmesh,
          "path query diagnoses an invalid start projection");
    Check(world.FindPath({2.0f, 0.0f, 2.0f}, {40.0f, 0.0f, 40.0f}).status
                  == game::SectorNavigationQueryStatus::DestinationNotOnNavmesh,
          "path query diagnoses an invalid destination projection");

    game::SectorNavigationWorld emptyWorld;
    emptyWorld.Initialize();
    emptyWorld.RequestRebuild();
    const game::SectorTopologyMap emptyMap;
    FinishBuild(emptyWorld, emptyMap);
    Check(emptyWorld.State() == game::SectorNavigationState::Empty,
          "empty topology produces an explicit Empty navigation state");
}

void TestBuildInputAndSourceHash()
{
    game::SectorTopologyMap map = MakeSquareMap(64);
    std::vector<game::SectorStaticModelCollider> colliders;
    const uint64_t base = game::ComputeSectorNavigationSourceHash(map, colliders, {});
    map.previewSettings.mouseSensitivity += 0.25f;
    const uint64_t visual = game::ComputeSectorNavigationSourceHash(map, colliders, {});
    Check(base == visual, "navigation source hash ignores visual preview settings");
    map.previewSettings.npcToNpcCollisionEnabled = false;
    Check(game::ComputeSectorNavigationSourceHash(map, colliders, {}) == base,
          "navigation source hash ignores NPC-to-NPC collision policy");
    map.lineDefs.front().flags.blocksPlayer = true;
    const uint64_t blocking = game::ComputeSectorNavigationSourceHash(map, colliders, {});
    Check(blocking != visual, "navigation source hash includes player-blocking topology flags");
    map.lineDefs.front().flags.blocksPlayer = false;
    map.sectors.front().floorZ = 1.0f;
    Check(game::ComputeSectorNavigationSourceHash(map, colliders, {}) != base,
          "navigation source hash includes authored floor height");
    map.sectors.front().floorZ = 0.0f;
    map.sectors.front().ceilingSky = true;
    Check(game::ComputeSectorNavigationSourceHash(map, colliders, {}) != base,
          "navigation source hash includes sky ceilings because they change geometry");
    game::SectorNavigationSettings changedSettings;
    changedSettings.agentRadius = 0.35f;
    Check(game::ComputeSectorNavigationSourceHash(map, colliders, changedSettings)
                  != game::ComputeSectorNavigationSourceHash(map, colliders, {}),
          "navigation source hash includes normalized profile/build settings");
    map.previewSettings.stepHeight = 0.4f;
    Check(game::ComputeSectorNavigationSourceHash(
                  map, colliders, game::BuildSectorNavigationSettingsForMap(map))
                  != game::ComputeSectorNavigationSourceHash(map, colliders, {}),
          "effective map step height changes the navigation source hash through climb settings");

    game::SectorTopologyMap doorMap = MakeAdjacentMap();
    game::SectorPlacedRuntimeObject door;
    door.id = 77;
    door.kind = "door";
    door.door.anchor.lineDefId = 2;
    door.door.anchor.frontSectorId = 10;
    door.door.anchor.backSectorId = 20;
    door.door.anchor.frontSideDefId = 2;
    door.door.anchor.backSideDefId = 8;
    doorMap.runtimeObjects.push_back(door);
    const uint64_t doorBase = game::ComputeSectorNavigationSourceHash(
            doorMap, colliders, {});
    doorMap.runtimeObjects.front().door.modelAssetId = "visual-only.glb";
    doorMap.runtimeObjects.front().door.materialId = "visual-only";
    doorMap.runtimeObjects.front().door.openSoundId = "open";
    doorMap.runtimeObjects.front().door.speed += 2.0f;
    doorMap.runtimeObjects.front().door.openAngleDegrees += 15.0f;
    Check(game::ComputeSectorNavigationSourceHash(doorMap, colliders, {})
                  == doorBase,
          "navigation source hash excludes door presentation, audio, and timing fields");
    doorMap.runtimeObjects.front().door.thickness += 0.25f;
    Check(game::ComputeSectorNavigationSourceHash(doorMap, colliders, {})
                  != doorBase,
          "navigation source hash includes door geometry that changes link staging");

    game::SectorTopologyMap windowMap = MakeAdjacentMap();
    game::SectorPlacedRuntimeObject window;
    window.id = 78;
    window.kind = "window";
    window.window.anchor.lineDefId = 2;
    window.window.anchor.frontSectorId = 10;
    window.window.anchor.backSectorId = 20;
    window.window.anchor.frontSideDefId = 2;
    window.window.anchor.backSideDefId = 8;
    window.window.width = 1.0f;
    windowMap.runtimeObjects.push_back(window);
    game::SectorStaticModelCollider windowCollider;
    windowCollider.placedObjectId = 78;
    windowCollider.center = {4.0f, 2.0f};
    windowCollider.halfExtents = {0.5f, 0.02f};
    windowCollider.bottom = 0.0f;
    windowCollider.top = 2.0f;
    windowCollider.resolved = true;
    const std::vector<game::SectorStaticModelCollider> windowColliders{
            windowCollider};
    const uint64_t windowBase = game::ComputeSectorNavigationSourceHash(
            windowMap, windowColliders, {});
    windowMap.runtimeObjects.front().window.tint = Color{50, 140, 220, 255};
    windowMap.runtimeObjects.front().window.opacity = 0.6f;
    windowMap.runtimeObjects.front().window.roughness = 0.4f;
    windowMap.runtimeObjects.front().window.indexOfRefraction = 1.7f;
    Check(game::ComputeSectorNavigationSourceHash(
                  windowMap, windowColliders, {}) == windowBase,
          "navigation source hash excludes window appearance fields");
    windowMap.runtimeObjects.front().window.horizontalOffsetWorld += 0.25f;
    Check(game::ComputeSectorNavigationSourceHash(
                  windowMap, windowColliders, {}) != windowBase,
          "navigation source hash includes collision-enabled window geometry");
    windowMap.runtimeObjects.front().window.collision = false;
    Check(game::ComputeSectorNavigationSourceHash(
                  windowMap, windowColliders, {}) != windowBase,
          "navigation source hash includes whether a window is physically solid");

    game::SectorNavigationBuildInput input;
    std::vector<std::string> warnings;
    std::string error;
    Check(game::BuildSectorNavigationBuildInput(
                  map, colliders, {}, input, warnings, error)
                  && !input.triangles.empty(),
          "CPU navigation input builds without generated render meshes");
    map.lineDefs.front().startVertexId = 9999;
    Check(!game::BuildSectorNavigationBuildInput(
                  map, colliders, {}, input, warnings, error)
                  && error.find("validation") != std::string::npos,
          "malformed navigation input fails deterministically");
}

void TestRotatedStructuralStairNavigation()
{
    game::SectorTopologyMap map = MakeSquareMap(2048);
    game::SectorAuthoringStructuralPrimitive stairs =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Stairs);
    stairs.id = 801;
    stairs.x = 1024;
    stairs.z = 1024;
    stairs.stairs.width = 128;
    stairs.stairs.run = 256;
    stairs.stairs.rise = 8.0f;
    std::vector<game::SectorStructuralDiagnostic> diagnostics;
    Check(game::CompileSectorStructuralPrimitives(
                  {stairs}, map, map.compiledStructuralPrimitives, diagnostics),
          "upright structural stair navigation fixture compiles");

    game::SectorNavigationBuildInput input;
    std::vector<std::string> warnings;
    std::string error;
    Check(game::BuildSectorNavigationBuildInput(
                  map, {}, {}, input, warnings, error),
          "upright structural stair navigation input builds");
    const auto walkableForStairs = [&]() {
        return std::count_if(input.triangles.begin(), input.triangles.end(),
                [&](const game::SectorNavigationBuildTriangle& triangle) {
                    return triangle.sourceId == stairs.id && triangle.area != 0;
                });
    };
    Check(walkableForStairs() == 2,
          "upright stairs contribute their smoothed walkable proxy");

    stairs.pitchDegrees = 90.0f;
    diagnostics.clear();
    Check(game::CompileSectorStructuralPrimitives(
                  {stairs}, map, map.compiledStructuralPrimitives, diagnostics),
          "wall-oriented structural stair navigation fixture compiles");
    input = game::SectorNavigationBuildInput{};
    warnings.clear();
    error.clear();
    Check(game::BuildSectorNavigationBuildInput(
                  map, {}, {}, input, warnings, error),
          "wall-oriented structural stair navigation input builds");
    Check(walkableForStairs() == 0,
          "wall-oriented stairs do not retain an upright walkable proxy");
}

void TestTopologyWalkabilityFixtures()
{
    game::SectorTopologyMap passable = MakeAdjacentMap(0.0f, 24.0f, 2.0f, 24.0f);
    game::SectorNavigationWorld passableWorld;
    passableWorld.Initialize();
    passableWorld.RequestRebuild();
    FinishBuild(passableWorld, passable);
    Check(passableWorld.FindPath({4.0f, 0.0f, 4.0f}, {12.0f, 0.25f, 4.0f}).status
                  == game::SectorNavigationQueryStatus::Success,
          "portal at maximum climb remains connected");

    game::SectorTopologyMap blocked = passable;
    blocked.lineDefs[1].flags.blocksPlayer = true;
    game::SectorNavigationWorld blockedWorld;
    blockedWorld.Initialize();
    blockedWorld.RequestRebuild();
    FinishBuild(blockedWorld, blocked);
    const auto blockedPath = blockedWorld.FindPath(
            {4.0f, 0.0f, 4.0f}, {12.0f, 0.25f, 4.0f});
    Check(blockedPath.status == game::SectorNavigationQueryStatus::Partial,
          "Blocks Player boundary returns a diagnosed partial route on the start side");

    game::SectorTopologyMap overClimb = MakeAdjacentMap(0.0f, 24.0f, 3.0f, 24.0f);
    game::SectorNavigationWorld climbWorld;
    climbWorld.Initialize();
    climbWorld.RequestRebuild();
    FinishBuild(climbWorld, overClimb);
    const auto overClimbPath = climbWorld.FindPath(
            {4.0f, 0.0f, 4.0f}, {12.0f, 0.375f, 4.0f});
    Check(overClimbPath.status != game::SectorNavigationQueryStatus::Success,
          "over-climb portal boundary remains disconnected");

    constexpr int StairStepCount = 6;
    constexpr float AuthoredRisePerStep = 1.6f; // 0.20 world units.
    game::SectorTopologyMap stairs = MakeSectorStairMap(
            StairStepCount, AuthoredRisePerStep);
    game::SectorNavigationWorld stairWorld;
    stairWorld.Initialize();
    stairWorld.RequestRebuild();
    FinishBuild(stairWorld, stairs);
    const float stairTop = game::SectorNavigationAuthoredHeightToWorld(
            AuthoredRisePerStep * static_cast<float>(StairStepCount - 1));
    const auto ascending = stairWorld.FindPath(
            {0.5f, 0.0f, 2.0f}, {5.5f, stairTop, 2.0f});
    const auto descending = stairWorld.FindPath(
            {5.5f, stairTop, 2.0f}, {0.5f, 0.0f, 2.0f});
    Check(ascending.status == game::SectorNavigationQueryStatus::Success
                  && descending.status == game::SectorNavigationQueryStatus::Success,
          "successive sector-geometry stair treads connect in both directions");
    const auto preservesEveryStairCrossing = [](const auto& path, bool forward) {
        if (path.cornerCount < StairStepCount + 1
                || path.corridorPolygonCount < StairStepCount) {
            return false;
        }
        for (int boundary = 1; boundary < StairStepCount; ++boundary) {
            const float expectedX = static_cast<float>(boundary);
            bool found = false;
            for (size_t corner = 1; corner + 1 < path.cornerCount; ++corner) {
                if (std::fabs(path.corners[corner].x - expectedX) <= 0.1f) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        for (size_t corner = 1; corner < path.cornerCount; ++corner) {
            if (forward
                    ? path.corners[corner].x + 0.0001f
                            < path.corners[corner - 1].x
                    : path.corners[corner].x - 0.0001f
                            > path.corners[corner - 1].x) {
                return false;
            }
        }
        return true;
    };
    const bool preservesStairCrossings =
            preservesEveryStairCrossing(ascending, true)
            && preservesEveryStairCrossing(descending, false);
    if (!preservesStairCrossings) {
        std::cerr << "stair corners ascending=" << ascending.cornerCount
                  << " polys=" << ascending.corridorPolygonCount << ':';
        for (size_t index = 0; index < ascending.cornerCount; ++index) {
            std::cerr << ' ' << ascending.corners[index].x;
        }
        std::cerr << " descending=" << descending.cornerCount
                  << " polys=" << descending.corridorPolygonCount << ':';
        for (size_t index = 0; index < descending.cornerCount; ++index) {
            std::cerr << ' ' << descending.corners[index].x;
        }
        std::cerr << '\n';
    }
    Check(preservesStairCrossings,
          "straight paths preserve every physical stair transition in order");
    Check(!stairWorld.DebugCache().stepConnections.empty(),
          "navigation debug cache exposes Detour adjacency between stair levels");

    constexpr float TallAuthoredRisePerStep = 3.0f; // 0.375 world units.
    game::SectorTopologyMap mapConfiguredStairs = MakeSectorStairMap(
            StairStepCount, TallAuthoredRisePerStep);
    mapConfiguredStairs.previewSettings.stepHeight = 0.4f;
    game::SectorNavigationWorld mapConfiguredStairWorld;
    mapConfiguredStairWorld.Initialize(
            game::BuildSectorNavigationSettingsForMap(mapConfiguredStairs));
    mapConfiguredStairWorld.RequestRebuild();
    FinishBuild(mapConfiguredStairWorld, mapConfiguredStairs);
    const float tallStairTop = game::SectorNavigationAuthoredHeightToWorld(
            TallAuthoredRisePerStep * static_cast<float>(StairStepCount - 1));
    const game::SectorNavigationPathResult tallAscending =
            mapConfiguredStairWorld.FindPath(
                    {0.5f, 0.0f, 2.0f}, {5.5f, tallStairTop, 2.0f});
    const game::SectorNavigationPathResult tallDescending =
            mapConfiguredStairWorld.FindPath(
                    {5.5f, tallStairTop, 2.0f}, {0.5f, 0.0f, 2.0f});
    if (tallAscending.status != game::SectorNavigationQueryStatus::Success
        || tallDescending.status != game::SectorNavigationQueryStatus::Success) {
        std::cerr << "configured stairs ascending="
                  << game::SectorNavigationQueryStatusName(tallAscending.status)
                  << " descending="
                  << game::SectorNavigationQueryStatusName(tallDescending.status)
                  << " step links="
                  << mapConfiguredStairWorld.DebugCache().stepConnections.size()
                  << '\n';
    }
    Check(tallAscending.status == game::SectorNavigationQueryStatus::Success
                  && tallDescending.status == game::SectorNavigationQueryStatus::Success
                  && preservesEveryStairCrossing(tallAscending, true)
                  && preservesEveryStairCrossing(tallDescending, false)
                  && !mapConfiguredStairWorld.DebugCache().stepConnections.empty(),
          "map-configured 0.40m climb connects 0.375m stair treads in both directions");

    game::SectorNavigationWorld defaultTallStairWorld;
    defaultTallStairWorld.Initialize();
    defaultTallStairWorld.RequestRebuild();
    FinishBuild(defaultTallStairWorld, mapConfiguredStairs);
    Check(defaultTallStairWorld.FindPath(
                  {0.5f, 0.0f, 2.0f}, {5.5f, tallStairTop, 2.0f}).status
                    != game::SectorNavigationQueryStatus::Success,
          "the same 0.375m stairs remain disconnected under the legacy 0.25m profile");

    game::SectorTopologyMap invalidStairs = MakeSectorStairMap(
            StairStepCount, AuthoredRisePerStep);
    invalidStairs.sectors[3].floorZ += 1.0f;
    invalidStairs.sectors[3].ceilingZ += 1.0f;
    game::SectorNavigationWorld invalidStairWorld;
    invalidStairWorld.Initialize();
    invalidStairWorld.RequestRebuild();
    FinishBuild(invalidStairWorld, invalidStairs);
    Check(invalidStairWorld.FindPath(
                  {0.5f, 0.0f, 2.0f}, {5.5f, stairTop, 2.0f}).status
                    != game::SectorNavigationQueryStatus::Success,
          "a stair rise above maximum climb disconnects the staircase");

    game::SectorTopologyMap doored = MakeAdjacentMap();
    game::SectorPlacedRuntimeObject doorObject;
    doorObject.id = 77;
    doorObject.kind = "door";
    doorObject.door.anchor.lineDefId = 2;
    doorObject.door.anchor.frontSectorId = 10;
    doorObject.door.anchor.backSectorId = 20;
    doorObject.door.anchor.frontSideDefId = 2;
    doorObject.door.anchor.backSideDefId = 8;
    doored.runtimeObjects.push_back(doorObject);
    game::SectorNavigationWorld doorWorld;
    doorWorld.Initialize();
    doorWorld.RequestRebuild();
    FinishBuild(doorWorld, doored);
    const auto capableDoorPath = doorWorld.FindPath(
            {4.0f, 0.0f, 4.0f}, {12.0f, 0.0f, 4.0f}, {true});
    const auto incapableClosedDoorPath = doorWorld.FindPath(
            {4.0f, 0.0f, 4.0f}, {12.0f, 0.0f, 4.0f}, {false});
    Check(doorWorld.DebugCache().doorPlaceholders.size() == 1
                  && doorWorld.DebugCache().doorLinks.size() == 1
                  && doorWorld.DebugCache().doorLinks[0].valid
                  && capableDoorPath.status
                          == game::SectorNavigationQueryStatus::Success
                  && incapableClosedDoorPath.status
                          != game::SectorNavigationQueryStatus::Success,
          "closed door is cut from ground navigation and crossed by a typed capable-only link");
    bool pathCarriesDoorMetadata = false;
    for (size_t corner = 0; corner < capableDoorPath.cornerCount; ++corner) {
        pathCarriesDoorMetadata = pathCarriesDoorMetadata
                || capableDoorPath.cornerDoorIds[corner] == 77;
    }
    Check(pathCarriesDoorMetadata,
          "door path corner carries the stable placed door ID");
    Check(doorWorld.SetDoorLinkRuntimeState(
                  77, game::SectorNavigationDoorLinkState::Clear, 0)
                  && doorWorld.FindPath(
                          {4.0f, 0.0f, 4.0f},
                          {12.0f, 0.0f, 4.0f},
                          {false}).status
                          == game::SectorNavigationQueryStatus::Success,
          "NPC without door-opening capability may traverse an already-clear door");
    Check(doorWorld.SetDoorLinkRuntimeState(
                  77, game::SectorNavigationDoorLinkState::Disabled, 0)
                  && doorWorld.FindPath(
                          {4.0f, 0.0f, 4.0f},
                          {12.0f, 0.0f, 4.0f},
                          {true}).status
                          != game::SectorNavigationQueryStatus::Success,
          "disabled door link yields a diagnosed unavailable route");

    game::SectorTopologyMap thresholdStep = MakeDoorThresholdStepMap();
    game::SectorNavigationBuildInput thresholdInput;
    std::vector<std::string> thresholdWarnings;
    std::string thresholdError;
    Check(game::BuildSectorNavigationBuildInput(
                  thresholdStep, {}, {}, thresholdInput,
                  thresholdWarnings, thresholdError)
                  && thresholdInput.doorLinks.empty()
                  && std::any_of(
                          thresholdWarnings.begin(), thresholdWarnings.end(),
                          [](const std::string& warning) {
                              return warning.find("Door 77") != std::string::npos
                                      && warning.find("disconnected sector")
                                              != std::string::npos;
                          }),
          "door staging on an over-climb island is omitted with a stable door diagnostic");
    thresholdStep.previewSettings.stepHeight = 0.4f;
    game::SectorNavigationWorld thresholdStepWorld;
    thresholdStepWorld.Initialize(
            game::BuildSectorNavigationSettingsForMap(thresholdStep));
    thresholdStepWorld.RequestRebuild();
    FinishBuild(thresholdStepWorld, thresholdStep);
    const game::SectorNavigationPathResult thresholdStepPath =
            thresholdStepWorld.FindPath(
                    {4.0f, 0.0f, 0.625f},
                    {12.0f, 0.375f, 0.625f});
    bool thresholdPathCarriesDoor = false;
    for (size_t corner = 0; corner < thresholdStepPath.cornerCount; ++corner) {
        thresholdPathCarriesDoor = thresholdPathCarriesDoor
                || thresholdStepPath.cornerDoorIds[corner] == 77;
    }
    Check(thresholdStepPath.status == game::SectorNavigationQueryStatus::Success
                  && thresholdPathCarriesDoor,
          "map-configured climb connects a swing-door landing through the raised side region");

    game::SectorTopologyMap lowClearance = MakeAdjacentMap(0.0f, 24.0f, 0.0f, 12.0f);
    game::SectorNavigationWorld clearanceWorld;
    clearanceWorld.Initialize();
    clearanceWorld.RequestRebuild();
    FinishBuild(clearanceWorld, lowClearance);
    const auto clearanceResult = clearanceWorld.FindPath(
            {4.0f, 0.0f, 4.0f}, {12.0f, 0.0f, 4.0f});
    Check(clearanceResult.status == game::SectorNavigationQueryStatus::NoPath
                  || clearanceResult.status
                          == game::SectorNavigationQueryStatus::DestinationNotOnNavmesh,
          "inadequate ceiling/portal clearance is not traversable");

    game::SectorTopologyMap concave;
    game::SectorTopologySector concaveSector;
    concaveSector.id = 1;
    concave.sectors.push_back(concaveSector);
    AddIndependentLoop(concave, 1,
            {{0, 0}, {2048, 0}, {2048, 512}, {1024, 512},
             {1024, 1536}, {2048, 1536}, {2048, 2048}, {0, 2048}});
    game::SectorNavigationWorld concaveWorld;
    concaveWorld.Initialize();
    concaveWorld.RequestRebuild();
    FinishBuild(concaveWorld, concave);
    Check(concaveWorld.State() == game::SectorNavigationState::Ready,
          "concave sector floor builds through CPU earcut input");

    game::SectorTopologyMap isolated;
    game::SectorTopologySector isolatedA;
    isolatedA.id = 1;
    game::SectorTopologySector isolatedB;
    isolatedB.id = 2;
    isolated.sectors = {isolatedA, isolatedB};
    AddIndependentLoop(isolated, 1, {{0, 0}, {512, 0}, {512, 512}, {0, 512}});
    AddIndependentLoop(isolated, 2,
            {{1280, 0}, {1792, 0}, {1792, 512}, {1280, 512}});
    game::SectorNavigationWorld isolatedWorld;
    isolatedWorld.Initialize();
    isolatedWorld.RequestRebuild();
    FinishBuild(isolatedWorld, isolated);
    Check(isolatedWorld.FindPath({2.0f, 0.0f, 2.0f}, {12.0f, 0.0f, 2.0f}).status
                  == game::SectorNavigationQueryStatus::NoPath,
          "fully disconnected single-polygon islands report no path");

    game::SectorTopologyMap coincident;
    game::SectorTopologySector seamA;
    seamA.id = 1;
    game::SectorTopologySector seamB;
    seamB.id = 2;
    coincident.sectors = {seamA, seamB};
    AddIndependentLoop(coincident, 1, {{0, 0}, {512, 0}, {512, 512}, {0, 512}});
    AddIndependentLoop(coincident, 2,
            {{512, 0}, {1024, 0}, {1024, 512}, {512, 512}});
    game::SectorNavigationWorld coincidentWorld;
    coincidentWorld.Initialize();
    coincidentWorld.RequestRebuild();
    FinishBuild(coincidentWorld, coincident);
    Check(coincidentWorld.State() == game::SectorNavigationState::Failed,
          "coincident non-portal seams are rejected instead of accidentally connected");

    game::SectorTopologyMap holed = MakeSquareMap(2048);
    AddIndependentLoop(holed, 1,
            {{768, 768}, {768, 1280}, {1280, 1280}, {1280, 768}});
    game::SectorNavigationWorld holedWorld;
    holedWorld.Initialize();
    holedWorld.RequestRebuild();
    FinishBuild(holedWorld, holed);
    const auto holePoint = holedWorld.FindNearestPoint({8.0f, 0.0f, 8.0f});
    if (holePoint.status != game::SectorNavigationQueryStatus::StartNotOnNavmesh) {
        std::cerr << "hole state=" << game::SectorNavigationStateName(holedWorld.State())
                  << " nearest=" << game::SectorNavigationQueryStatusName(holePoint.status)
                  << " point=" << holePoint.nearestPosition.x << ","
                  << holePoint.nearestPosition.y << "," << holePoint.nearestPosition.z << '\n';
    }
    Check(holePoint.status == game::SectorNavigationQueryStatus::StartNotOnNavmesh,
          "holed sector does not create walkable navigation inside its hole");

    const auto makePassage = [](game::SectorCoord lower, game::SectorCoord upper) {
        game::SectorTopologyMap passage;
        game::SectorTopologySector sector;
        sector.id = 1;
        passage.sectors.push_back(sector);
        AddIndependentLoop(passage, 1,
                {{0, 0}, {512, 0}, {512, lower}, {1024, lower},
                 {1024, 0}, {1536, 0}, {1536, 512}, {1024, 512},
                 {1024, upper}, {512, upper}, {512, 512}, {0, 512}});
        return passage;
    };
    game::SectorNavigationWorld widePassage;
    widePassage.Initialize();
    widePassage.RequestRebuild();
    FinishBuild(widePassage, makePassage(192, 320));
    Check(widePassage.FindPath({2.0f, 0.0f, 2.0f}, {10.0f, 0.0f, 2.0f}).status
                  == game::SectorNavigationQueryStatus::Success,
          "passage wider than the agent erosion diameter remains connected");
    game::SectorNavigationWorld narrowPassage;
    narrowPassage.Initialize();
    narrowPassage.RequestRebuild();
    FinishBuild(narrowPassage, makePassage(240, 272));
    Check(narrowPassage.FindPath({2.0f, 0.0f, 2.0f}, {10.0f, 0.0f, 2.0f}).status
                  != game::SectorNavigationQueryStatus::Success,
          "passage narrower than the agent erosion diameter is disconnected");
}

void TestStaticObstacleAndCapacityFixtures()
{
    game::SectorTopologyMap map = MakeSquareMap(2048);
    game::SectorPlacedRuntimeObject object;
    object.id = 41;
    object.kind = "static_model";
    object.staticModel.collision = true;
    object.staticModel.geometryFingerprint = "fixture-obb-v1";
    map.runtimeObjects.push_back(object);
    game::SectorStaticModelCollider collider;
    collider.placedObjectId = 41;
    collider.center = {8.0f, 8.0f};
    const float diagonal = std::sqrt(0.5f);
    collider.axisX = {diagonal, diagonal};
    collider.axisZ = {-diagonal, diagonal};
    collider.halfExtents = {2.0f, 0.5f};
    collider.bottom = 0.0f;
    collider.top = 1.0f;
    collider.resolved = true;
    const std::vector<game::SectorStaticModelCollider> colliders{collider};

    game::SectorNavigationWorld world;
    world.Initialize();
    world.RequestRebuild();
    world.UpdateBuild(map, colliders, 1);
    Check(world.State() == game::SectorNavigationState::Queued
                  && world.BuildStage()
                          == game::SectorNavigationBuildStage::WaitingForStaticCollision,
          "queued build waits for static collision assets to reach a terminal state");
    for (int iteration = 0; iteration < 1000
            && (world.State() == game::SectorNavigationState::Queued
                || world.State() == game::SectorNavigationState::Building); ++iteration) {
        world.UpdateBuild(map, colliders, 0);
    }
    Check(world.State() == game::SectorNavigationState::Ready
                  && world.DebugCache().staticObstacles.size() == 1,
          "rotated collision-enabled static OBB contributes geometry and debug bounds");
    Check(world.SourceHash() != game::ComputeSectorNavigationSourceHash(map, {}, {}),
          "resolved static OBB geometry participates in the navigation source hash");

    game::SectorStaticModelCollider failedCollider = collider;
    failedCollider.resolved = false;
    failedCollider.failed = true;
    game::SectorNavigationWorld assetTransition;
    assetTransition.Initialize();
    assetTransition.RequestRebuild();
    for (int iteration = 0; iteration < 1000
            && (assetTransition.State() == game::SectorNavigationState::Queued
                || assetTransition.State() == game::SectorNavigationState::Building);
            ++iteration) {
        assetTransition.UpdateBuild(map, {failedCollider}, 0);
    }
    Check(assetTransition.State() == game::SectorNavigationState::Ready
                  && assetTransition.DebugCache().staticObstacles.empty()
                  && std::any_of(
                          assetTransition.Diagnostics().begin(),
                          assetTransition.Diagnostics().end(),
                          [](const game::SectorNavigationDiagnostic& diagnostic) {
                              return diagnostic.message.find("omitted")
                                      != std::string::npos;
                          }),
          "terminal failed static collision is omitted with a diagnostic instead of blocking load");
    assetTransition.RequestRebuild();
    for (int iteration = 0; iteration < 1000
            && (assetTransition.State() == game::SectorNavigationState::Queued
                || assetTransition.State() == game::SectorNavigationState::Building);
            ++iteration) {
        assetTransition.UpdateBuild(map, colliders, 0);
    }
    Check(assetTransition.State() == game::SectorNavigationState::Ready
                  && assetTransition.DebugCache().staticObstacles.size() == 1,
          "a later successful collision asset rebuild installs the static obstacle");

    game::SectorNavigationCapacitySettings limited;
    limited.maximumTotalTiles = 32;
    game::SectorNavigationWorld oversized;
    oversized.Initialize({}, limited);
    oversized.RequestRebuild();
    oversized.UpdateBuild(map, colliders, 0);
    Check(oversized.State() == game::SectorNavigationState::Failed
                  && !oversized.Diagnostics().empty()
                  && oversized.Diagnostics().back().message.find("tile limit")
                          != std::string::npos,
          "large bounds fail with actionable tile headroom diagnostics before rasterization");

    game::SectorNavigationCapacitySettings queryLimited;
    queryLimited.maximumPathPolygons = 1;
    game::SectorNavigationWorld limitedQuery;
    limitedQuery.Initialize({}, queryLimited);
    limitedQuery.RequestRebuild();
    FinishBuild(limitedQuery, MakeSquareMap(2048));
    Check(limitedQuery.FindPath({1.0f, 0.0f, 1.0f}, {15.0f, 0.0f, 15.0f}).status
                  == game::SectorNavigationQueryStatus::CapacityExceeded,
          "bounded corridor query reports capacity exhaustion");
}

game::SectorStaticModelCollider MakeDynamicObstacle(
        int id,
        Vector2 center,
        Vector2 halfExtents,
        float yaw = 0.0f,
        float bottom = 0.0f,
        float top = 2.0f)
{
    game::SectorStaticModelCollider collider;
    collider.placedObjectId = id;
    collider.center = center;
    collider.axisX = {std::cos(yaw), -std::sin(yaw)};
    collider.axisZ = {std::sin(yaw), std::cos(yaw)};
    collider.halfExtents = halfExtents;
    collider.bottom = bottom;
    collider.top = top;
    collider.resolved = true;
    return collider;
}

void TestDynamicObstacleLifecycleAndTileRevisions()
{
    const game::SectorTopologyMap map = MakeSquareMap(16384);
    game::SectorNavigationWorld world;
    world.Initialize();
    world.RequestRebuild();
    FinishBuild(world, map);
    Check(world.State() == game::SectorNavigationState::Ready,
          "dynamic-obstacle fixture builds its static TileCache navmesh");
    const uint64_t sourceHash = world.SourceHash();
    const uint64_t buildRevision = world.BuildRevision();
    const game::SectorNavigationPathResult affectedPath = world.FindPath(
            {2.0f, 0.0f, 8.0f}, {14.0f, 0.0f, 8.0f});
    const game::SectorNavigationPathResult unaffectedPath = world.FindPath(
            {80.0f, 0.0f, 80.0f}, {100.0f, 0.0f, 80.0f});
    if (affectedPath.status != game::SectorNavigationQueryStatus::Success
            || unaffectedPath.status
                    != game::SectorNavigationQueryStatus::Success) {
        std::cerr << "dynamic fixture paths affected="
                  << game::SectorNavigationQueryStatusName(affectedPath.status)
                  << " distant="
                  << game::SectorNavigationQueryStatusName(unaffectedPath.status)
                  << '\n';
    }
    Check(affectedPath.status == game::SectorNavigationQueryStatus::Success
                  && unaffectedPath.status
                          == game::SectorNavigationQueryStatus::Success,
          "dynamic-obstacle fixture captures affected and distant corridors");

    game::SectorStaticModelCollider obstacle = MakeDynamicObstacle(
            701, {8.0f, 8.0f}, {0.75f, 3.0f}, 0.35f, 0.5f, 2.0f);
    FinishDynamicObstacleUpdates(world, {obstacle});
    const auto& activeStats = world.DynamicObstacleStatistics();
    Check(activeStats.activeCount == 1
                  && activeStats.updatedTiles > 0
                  && world.DebugCache().dynamicObstacles.size() == 1
                  && world.DebugCache().dynamicObstacles[0].state
                          == game::SectorNavigationDynamicObstacleState::Active,
          "resolved dynamic OBB becomes an active TileCache obstacle with debug state");
    Check(Near(world.DebugCache().dynamicObstacles[0].halfExtents.x,
                       obstacle.halfExtents.x + world.Settings().agentRadius)
                  && Near(world.DebugCache().dynamicObstacles[0].bottom,
                          obstacle.bottom - world.Settings().agentHeight
                                  - world.Settings().cellHeight),
          "dynamic obstacle uses the conservative agent-radius and vertical-overlap band");
    Check(world.SourceHash() == sourceHash
                  && world.BuildRevision() == buildRevision
                  && world.TileRevision() > 0,
          "dynamic obstacle advances only runtime tile revisions and leaves the static source stable");
    Check(world.CorridorTouchesChangedTile(
                  affectedPath.corridorTiles.data(),
                  affectedPath.corridorTileCount,
                  affectedPath.tileRevision)
                  && !world.CorridorTouchesChangedTile(
                          unaffectedPath.corridorTiles.data(),
                          unaffectedPath.corridorTileCount,
                          unaffectedPath.tileRevision),
          "only corridors touching rebuilt tiles are invalidated");

    const uint64_t revisionBeforeRotation = world.TileRevision();
    obstacle.axisX = {std::cos(0.55f), -std::sin(0.55f)};
    obstacle.axisZ = {std::sin(0.55f), std::cos(0.55f)};
    world.UpdateDynamicObstacles({obstacle}, 0.25f);
    FinishDynamicObstacleUpdates(world, {obstacle});
    Check(world.DynamicObstacleStatistics().activeCount == 1
                  && world.TileRevision() > revisionBeforeRotation
                  && world.DynamicObstacleStatistics().transforms > 0,
          "a meaningful slow rotation coalesces into remove/add tile work");

    obstacle.center.x += 1.0f;
    world.UpdateDynamicObstacles({obstacle}, 0.016f);
    for (int update = 0; update < 12; ++update) {
        world.UpdateDynamicObstacles({obstacle}, 0.016f);
    }
    Check(world.DynamicObstacleStatistics().fastSuppressedCount == 1
                  || world.DynamicObstacleStatistics().pendingCount == 1,
          "a fast transform suppresses its navmesh cut instead of rebuilding every frame");
    for (int update = 0; update < 128
            && world.DynamicObstacleStatistics().activeCount != 1;
            ++update) {
        world.UpdateDynamicObstacles({obstacle}, 0.05f);
    }
    if (world.DynamicObstacleStatistics().activeCount != 1) {
        const auto& stats = world.DynamicObstacleStatistics();
        std::cerr << "settled obstacle active=" << stats.activeCount
                  << " pending=" << stats.pendingCount
                  << " removing=" << stats.removingCount
                  << " fast=" << stats.fastSuppressedCount
                  << " failed=" << stats.failedCount
                  << " backlog=" << stats.backlogCount << '\n';
    }
    Check(world.DynamicObstacleStatistics().activeCount == 1,
          "a fast-suppressed obstacle returns after the balanced settle interval");

    FinishDynamicObstacleUpdates(world, {});
    Check(world.DynamicObstacleStatistics().activeCount == 0
                  && world.DebugCache().dynamicObstacles.empty(),
          "removing or disabling a dynamic collider removes its TileCache obstacle");
    Check(world.FindPath({2.0f, 0.0f, 8.0f}, {14.0f, 0.0f, 8.0f}).status
                  == game::SectorNavigationQueryStatus::Success,
          "path queries recover after a dynamic obstacle is removed");

    game::SectorNavigationCapacitySettings capacity;
    capacity.dynamicObstacleCapacity = 1;
    capacity.dynamicObstacleRequestBudgetPerUpdate = 1;
    capacity.dynamicObstacleTileBudgetPerUpdate = 1;
    game::SectorNavigationWorld limited;
    limited.Initialize({}, capacity);
    limited.RequestRebuild();
    FinishBuild(limited, map);
    const game::SectorStaticModelCollider second = MakeDynamicObstacle(
            702, {20.0f, 20.0f}, {0.5f, 0.5f});
    limited.UpdateDynamicObstacles({obstacle, second}, 0.016f);
    Check(limited.Counters().capacityWarnings == 1
                  && !limited.Diagnostics().empty(),
          "dynamic obstacle capacity overflow warns and leaves excess props collision-only");
    limited.Shutdown();
    Check(limited.State() == game::SectorNavigationState::Uninitialized
                  && limited.DebugCache().dynamicObstacles.empty(),
          "dynamic obstacle records and debug state clear during teardown");

    game::SectorNavigationWorld oversized;
    oversized.Initialize();
    oversized.RequestRebuild();
    FinishBuild(oversized, map);
    const game::SectorStaticModelCollider huge = MakeDynamicObstacle(
            703, {64.0f, 64.0f}, {40.0f, 40.0f});
    oversized.UpdateDynamicObstacles({huge}, 0.016f);
    if (oversized.DynamicObstacleStatistics().failedCount != 1) {
        const auto& stats = oversized.DynamicObstacleStatistics();
        std::cerr << "oversized obstacle active=" << stats.activeCount
                  << " pending=" << stats.pendingCount
                  << " failed=" << stats.failedCount
                  << " backlog=" << stats.backlogCount << '\n';
    }
    Check(oversized.DynamicObstacleStatistics().failedCount == 1,
          "an obstacle exceeding Detour's eight touched-layer limit fails safely");

    game::SectorTopologyMap doorMap = MakeAdjacentMap();
    game::SectorPlacedRuntimeObject doorObject;
    doorObject.id = 77;
    doorObject.kind = "door";
    doorObject.door.anchor.lineDefId = 2;
    doorObject.door.anchor.frontSectorId = 10;
    doorObject.door.anchor.backSectorId = 20;
    doorObject.door.anchor.frontSideDefId = 2;
    doorObject.door.anchor.backSideDefId = 8;
    doorMap.runtimeObjects.push_back(doorObject);
    game::SectorNavigationWorld doorWorld;
    doorWorld.Initialize();
    doorWorld.RequestRebuild();
    FinishBuild(doorWorld, doorMap);
    const game::SectorStaticModelCollider nearDoor = MakeDynamicObstacle(
            704, {6.0f, 4.0f}, {0.5f, 0.5f});
    FinishDynamicObstacleUpdates(doorWorld, {nearDoor});
    Check(doorWorld.SetDoorLinkRuntimeState(
                  77, game::SectorNavigationDoorLinkState::Clear, 1),
          "door off-mesh references are rebound after an affected TileCache tile is replaced");
}

} // namespace

int main()
{
    TestDependencyAndCoordinates();
    TestSettingsAndStatusContracts();
    TestLifecycleAndHandles();
    TestHardenedLifecycleAndDiagnostics();
    TestCrowdCapacityAndAgentReuse();
    TestLayerCompression();
    TestStaticBuildAndQueries();
    TestBuildInputAndSourceHash();
    TestRotatedStructuralStairNavigation();
    TestTopologyWalkabilityFixtures();
    TestStaticObstacleAndCapacityFixtures();
    TestDynamicObstacleLifecycleAndTileRevisions();
    if (failures != 0) {
        std::cerr << failures << " navigation test(s) failed\n";
        return 1;
    }
    std::cout << "Sector navigation tests passed\n";
    return 0;
}
