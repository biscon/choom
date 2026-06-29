#include "sector_demo/SectorPortalVisibility.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using game::SectorCoord;
using game::SectorTopologyLineDef;
using game::SectorTopologyMap;
using game::SectorTopologySector;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyVertex;

int failures = 0;
constexpr float Pi = 3.14159265358979323846f;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

bool Contains(const std::vector<int>& values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

Vector2 ForwardFromPreviewYaw(float yawRadians)
{
    return Vector2{std::cos(yawRadians), std::sin(yawRadians)};
}

float Degrees(float degrees)
{
    return degrees * Pi / 180.0f;
}

void AddSide(
        SectorTopologyMap& map,
        int sideId,
        int lineId,
        SectorTopologySideKind side,
        int sectorId)
{
    SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    map.sideDefs.push_back(sideDef);
}

SectorTopologySector Sector(int id, float floorZ = 0.0f, float ceilingZ = 24.0f)
{
    SectorTopologySector sector;
    sector.id = id;
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    return sector;
}

void AddSectorLoop(
        SectorTopologyMap& map,
        int sectorId,
        const std::vector<std::pair<SectorCoord, SectorCoord>>& points)
{
    std::vector<int> vertexIds;
    for (const auto& point : points) {
        const int vertexId = game::AllocateSectorTopologyVertexId(map);
        map.vertices.push_back(SectorTopologyVertex{vertexId, point.first, point.second});
        vertexIds.push_back(vertexId);
    }

    for (size_t i = 0; i < vertexIds.size(); ++i) {
        const int lineId = game::AllocateSectorTopologyLineDefId(map);
        const int sideId = game::AllocateSectorTopologySideDefId(map);
        map.lineDefs.push_back(SectorTopologyLineDef{
                lineId,
                vertexIds[i],
                vertexIds[(i + 1) % vertexIds.size()],
                sideId,
                -1});
        AddSide(map, sideId, lineId, SectorTopologySideKind::Front, sectorId);
    }
}

SectorTopologyMap MakeSquareOnly()
{
    SectorTopologyMap map;
    map.sectors.push_back(Sector(10));
    AddSectorLoop(map, 10, {{0, 0}, {64, 0}, {64, 64}, {0, 64}});
    return map;
}

SectorTopologyMap MakeAdjacent()
{
    SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 128, 0}, {6, 128, 64}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(20, 4.0f, 20.0f));
    map.sectors.push_back(Sector(10, 1.0f, 16.0f));
    return map;
}

SectorTopologyMap MakeDisconnected()
{
    SectorTopologyMap map;
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    AddSectorLoop(map, 10, {{0, 0}, {64, 0}, {64, 64}, {0, 64}});
    AddSectorLoop(map, 20, {{128, 0}, {192, 0}, {192, 64}, {128, 64}});
    return map;
}

SectorTopologyMap MakeSidePortal()
{
    SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 64, 128}, {6, 0, 128}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, -1},
            {3, 3, 4, 3, 8},
            {4, 4, 1, 4, -1},
            {5, 3, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 4, 7, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 3, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    return map;
}

SectorTopologyMap MakeTurnFromMiddle()
{
    SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 128, 0}, {6, 128, 64}, {7, 128, 128}, {8, 64, 128}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 12},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1},
            {8, 6, 7, 8, -1},
            {9, 7, 8, 9, -1},
            {10, 8, 3, 10, 20}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 8, SectorTopologySideKind::Front, 30);
    AddSide(map, 9, 9, SectorTopologySideKind::Front, 30);
    AddSide(map, 10, 10, SectorTopologySideKind::Front, 30);
    AddSide(map, 12, 2, SectorTopologySideKind::Back, 20);
    AddSide(map, 20, 10, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    map.sectors.push_back(Sector(30));
    return map;
}

SectorTopologyMap MakeWideCrossingPortal()
{
    SectorTopologyMap map;
    map.vertices = {
            {1, 0, -128}, {2, 64, -128}, {3, 64, -64}, {4, 64, 64}, {5, 64, 128}, {6, 0, 128},
            {7, 128, -64}, {8, 128, 64}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, -1},
            {3, 3, 4, 3, 9},
            {4, 4, 5, 4, -1},
            {5, 5, 6, 5, -1},
            {6, 6, 1, 6, -1},
            {7, 3, 7, 7, -1},
            {8, 7, 8, 8, -1},
            {9, 8, 4, 10, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 10);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 10);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 8, SectorTopologySideKind::Front, 20);
    AddSide(map, 9, 3, SectorTopologySideKind::Back, 20);
    AddSide(map, 10, 9, SectorTopologySideKind::Front, 20);
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    return map;
}

SectorTopologyMap MakeClippedChain(bool alignedThirdPortal)
{
    SectorTopologyMap map;
    if (alignedThirdPortal) {
        map.vertices = {
                {1, 0, 0}, {2, 64, 0}, {3, 64, 28}, {4, 64, 36}, {5, 64, 64}, {6, 0, 64},
                {7, 128, 0}, {8, 128, 28}, {9, 128, 36}, {10, 128, 64},
                {11, 192, 0}, {12, 192, 64}};
        map.lineDefs = {
                {1, 1, 2, 1, -1}, {2, 2, 3, 2, -1}, {3, 3, 4, 3, 18}, {4, 4, 5, 4, -1}, {5, 5, 6, 5, -1}, {6, 6, 1, 6, -1},
                {7, 2, 7, 7, -1}, {8, 7, 8, 8, -1}, {9, 8, 9, 9, 19}, {10, 9, 10, 10, -1}, {11, 10, 5, 11, -1},
                {12, 7, 11, 12, -1}, {13, 11, 12, 13, -1}, {14, 12, 10, 14, -1}};
        AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
        AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
        AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
        AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
        AddSide(map, 5, 5, SectorTopologySideKind::Front, 10);
        AddSide(map, 6, 6, SectorTopologySideKind::Front, 10);
        AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
        AddSide(map, 8, 8, SectorTopologySideKind::Front, 20);
        AddSide(map, 9, 9, SectorTopologySideKind::Front, 20);
        AddSide(map, 10, 10, SectorTopologySideKind::Front, 20);
        AddSide(map, 11, 11, SectorTopologySideKind::Front, 20);
        AddSide(map, 12, 12, SectorTopologySideKind::Front, 30);
        AddSide(map, 13, 13, SectorTopologySideKind::Front, 30);
        AddSide(map, 14, 14, SectorTopologySideKind::Front, 30);
        AddSide(map, 18, 3, SectorTopologySideKind::Back, 20);
        AddSide(map, 19, 9, SectorTopologySideKind::Back, 30);
    } else {
        map.vertices = {
                {1, 0, 0}, {2, 64, 0}, {3, 64, 28}, {4, 64, 36}, {5, 64, 64}, {6, 0, 64},
                {7, 128, 0}, {8, 128, 64}, {9, 104, 64}, {10, 96, 64}, {11, 96, 128}, {12, 104, 128}};
        map.lineDefs = {
                {1, 1, 2, 1, -1}, {2, 2, 3, 2, -1}, {3, 3, 4, 3, 18}, {4, 4, 5, 4, -1}, {5, 5, 6, 5, -1}, {6, 6, 1, 6, -1},
                {7, 2, 7, 7, -1}, {8, 7, 8, 8, -1}, {9, 8, 9, 9, -1}, {10, 9, 10, 10, 19}, {11, 10, 5, 11, -1},
                {12, 10, 11, 12, -1}, {13, 11, 12, 13, -1}, {14, 12, 9, 14, -1}};
        AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
        AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
        AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
        AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
        AddSide(map, 5, 5, SectorTopologySideKind::Front, 10);
        AddSide(map, 6, 6, SectorTopologySideKind::Front, 10);
        AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
        AddSide(map, 8, 8, SectorTopologySideKind::Front, 20);
        AddSide(map, 9, 9, SectorTopologySideKind::Front, 20);
        AddSide(map, 10, 10, SectorTopologySideKind::Front, 20);
        AddSide(map, 11, 11, SectorTopologySideKind::Front, 20);
        AddSide(map, 12, 12, SectorTopologySideKind::Front, 30);
        AddSide(map, 13, 13, SectorTopologySideKind::Front, 30);
        AddSide(map, 14, 14, SectorTopologySideKind::Front, 30);
        AddSide(map, 18, 3, SectorTopologySideKind::Back, 20);
        AddSide(map, 19, 10, SectorTopologySideKind::Back, 30);
    }
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    map.sectors.push_back(Sector(30));
    return map;
}

SectorTopologyMap MakeCycle()
{
    SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 128, 0}, {4, 192, 0},
            {5, 192, 64}, {6, 128, 64}, {7, 64, 64}, {8, 0, 64}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 7, 2, 12},
            {3, 7, 8, 3, -1},
            {4, 8, 1, 4, -1},
            {5, 2, 3, 5, -1},
            {6, 3, 6, 6, 16},
            {7, 6, 7, 7, -1},
            {8, 3, 4, 8, -1},
            {9, 4, 5, 9, -1},
            {10, 5, 6, 10, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 8, SectorTopologySideKind::Front, 30);
    AddSide(map, 9, 9, SectorTopologySideKind::Front, 30);
    AddSide(map, 10, 10, SectorTopologySideKind::Front, 30);
    AddSide(map, 12, 2, SectorTopologySideKind::Back, 20);
    AddSide(map, 16, 6, SectorTopologySideKind::Back, 30);
    map.lineDefs.push_back(SectorTopologyLineDef{11, 6, 7, 11, 17});
    AddSide(map, 11, 11, SectorTopologySideKind::Front, 30);
    AddSide(map, 17, 11, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    map.sectors.push_back(Sector(30));
    return map;
}

void TestOneSectorNoPortals()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeSquareOnly(), graph, &error),
          "one-sector visibility graph builds");
    Check(error.empty(), "one-sector build has no error");
    Check(graph.sectors.size() == 1, "one-sector graph has one node");
    Check(graph.portals.empty(), "one-sector graph has no portals");

    const game::RuntimePortalVisibilityResult result =
            game::TraverseRuntimeSectorVisibility(graph, 10);
    Check(result.validStartSector, "one-sector traversal has valid start");
    Check(!result.fallbackDrawAll, "one-sector traversal does not fallback");
    Check(result.visibleSectorIds.size() == 1 && result.visibleSectorIds[0] == 10,
          "one-sector traversal sees start sector");
}

void TestAdjacentCreatesDirectedEdges()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "adjacent visibility graph builds");
    Check(graph.sectors.size() == 2, "adjacent graph has two sector nodes");
    Check(graph.portals.size() == 2, "two-sided linedef creates two directed portals");
    Check(graph.portals[0].fromSectorId == 10 && graph.portals[0].toSectorId == 20,
          "front side portal points from front sector to back sector");
    Check(graph.portals[1].fromSectorId == 20 && graph.portals[1].toSectorId == 10,
          "back side portal points from back sector to front sector");
    Check(graph.portals[0].lineDefId == 2 && graph.portals[1].lineDefId == 2,
          "portals retain linedef id");
    Check(Near(graph.portals[0].openBottom, game::SectorAuthoringToWorldDistance(4.0f))
                  && Near(graph.portals[0].openTop, game::SectorAuthoringToWorldDistance(16.0f)),
          "portal open interval is stored in world units");
}

void TestOneSidedWallCreatesNoPortal()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeSquareOnly(), graph, &error),
          "one-sided wall graph builds");
    Check(graph.portals.empty(), "one-sided walls create no portal edges");
}

void TestClosedPortalDoesNotTraverse()
{
    SectorTopologyMap map = MakeAdjacent();
    game::FindSectorTopologySector(map, 20)->floorZ = 20.0f;
    game::FindSectorTopologySector(map, 20)->ceilingZ = 28.0f;

    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "height-closed visibility graph builds");
    Check(graph.portals.size() == 2 && !graph.portals[0].open && !graph.portals[1].open,
          "height-closed portal edges are marked closed");

    const game::RuntimePortalVisibilityResult result =
            game::TraverseRuntimeSectorVisibility(graph, 10);
    Check(result.validStartSector, "height-closed traversal has valid start");
    Check(result.visibleSectorIds.size() == 1 && result.visibleSectorIds[0] == 10,
          "height-closed portal is not traversed");
}

void TestConnectedAndDisconnectedTraversal()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "connected traversal graph builds");
    game::RuntimePortalVisibilityResult result =
            game::TraverseRuntimeSectorVisibility(graph, 10);
    Check(result.visibleSectorIds.size() == 2
                  && Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20),
          "connected open sectors become visible");
    Check(result.traversedPortalLineDefIds.size() == 1
                  && result.traversedPortalLineDefIds[0] == 2,
          "traversed portal linedef ids are deterministic and unique");

    Check(game::BuildRuntimeSectorVisibilityGraph(MakeDisconnected(), graph, &error),
          "disconnected traversal graph builds");
    result = game::TraverseRuntimeSectorVisibility(graph, 10);
    Check(result.visibleSectorIds.size() == 1 && result.visibleSectorIds[0] == 10,
          "disconnected sectors are not visible");
}

void TestCycleTerminates()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeCycle(), graph, &error),
          "cyclic visibility graph builds");
    const game::RuntimePortalVisibilityResult result =
            game::TraverseRuntimeSectorVisibility(graph, 10);
    Check(result.validStartSector, "cyclic traversal has valid start");
    Check(!result.fallbackDrawAll, "cyclic traversal does not hit fallback");
    Check(result.visibleSectorIds.size() == 3
                  && Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20)
                  && Contains(result.visibleSectorIds, 30),
          "cyclic graph traversal visits each connected sector once");
}

void TestInvalidStartFallback()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeSquareOnly(), graph, &error),
          "invalid-start graph builds");
    const game::RuntimePortalVisibilityResult result =
            game::TraverseRuntimeSectorVisibility(graph, 999);
    Check(!result.validStartSector, "invalid start is reported");
    Check(result.fallbackDrawAll, "invalid start requests draw-all fallback");
    Check(result.visibleSectorIds.empty(), "invalid start has no visible sector list");
    Check(result.status.find("invalid start") != std::string::npos,
          "invalid start has clear status");
}

void TestViewYawFacingPortalIncludesNeighbor()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "view-facing graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(70.0f),
                    10);
    Check(result.validStartSector, "view-facing traversal has valid start");
    Check(!result.fallbackDrawAll, "view-facing traversal does not fallback");
    Check(Contains(result.visibleSectorIds, 10) && Contains(result.visibleSectorIds, 20),
          "preview yaw 0 faces positive-X portal and includes neighbor");
}

void TestViewYawFacingAwayExcludesNeighbor()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "view-away graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(Pi),
                    Degrees(70.0f),
                    10);
    Check(result.validStartSector, "view-away traversal has valid start");
    Check(Contains(result.visibleSectorIds, 10), "view-away traversal includes start sector");
    Check(!Contains(result.visibleSectorIds, 20),
          "preview yaw pi faces away from positive-X portal and excludes neighbor");
}

void TestViewSidePortalOutsideFovExcluded()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeSidePortal(), graph, &error),
          "side-portal graph builds");

    const Vector2 camera = game::SectorCoordToWorldPosition2(32, 32);
    const game::RuntimePortalVisibilityResult forwardResult =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(50.0f),
                    10);
    Check(Contains(forwardResult.visibleSectorIds, 10), "side-portal forward includes start");
    Check(!Contains(forwardResult.visibleSectorIds, 20),
          "side portal is outside narrow forward FOV");

    const game::RuntimePortalVisibilityResult turnedResult =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(Pi * 0.5f),
                    Degrees(70.0f),
                    10);
    Check(Contains(turnedResult.visibleSectorIds, 20),
          "side portal becomes visible after turning toward positive Z");
}

void TestViewPortalBarelyIntersectsFovEdge()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "FOV-edge graph builds");

    const Vector2 camera = game::SectorCoordToWorldPosition2(32, 32);
    const game::RuntimePortalVisibilityResult edgeResult =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(Degrees(-80.0f)),
                    Degrees(70.0f),
                    10);
    Check(!edgeResult.fallbackDrawAll, "FOV-edge traversal does not fallback");
    Check(Contains(edgeResult.visibleSectorIds, 10), "FOV-edge traversal includes start");
    Check(Contains(edgeResult.visibleSectorIds, 20),
          "portal barely intersecting FOV edge includes neighbor");

    const game::RuntimePortalVisibilityResult outsideResult =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(Degrees(-90.0f)),
                    Degrees(70.0f),
                    10);
    Check(Contains(outsideResult.visibleSectorIds, 10), "outside-edge traversal includes start");
    Check(!Contains(outsideResult.visibleSectorIds, 20),
          "portal clearly outside FOV plus margin may be excluded");
}

void TestViewPortalSegmentCrossesFovWedge()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeWideCrossingPortal(), graph, &error),
          "crossing portal graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(0, 0),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(30.0f),
                    10);
    Check(!result.fallbackDrawAll, "crossing portal traversal does not fallback");
    Check(Contains(result.visibleSectorIds, 10), "crossing portal includes start");
    Check(Contains(result.visibleSectorIds, 20),
          "portal segment crossing the FOV wedge includes neighbor");
}

void TestViewNearCameraPortalVisible()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "near-camera portal graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    Vector2{0.49f, 0.25f},
                    ForwardFromPreviewYaw(Pi),
                    Degrees(60.0f),
                    10);
    Check(result.validStartSector, "near-camera portal has valid preferred start");
    Check(!result.fallbackDrawAll, "near-camera portal traversal does not fallback");
    Check(Contains(result.visibleSectorIds, 20),
          "near-camera portal is visible even when view angle is numerically fragile");
}

void TestViewRecursivePortalClipping()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeClippedChain(false), graph, &error),
          "clipped chain graph builds");

    const Vector2 camera = game::SectorCoordToWorldPosition2(32, 32);
    game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(90.0f),
                    10);
    Check(Contains(result.visibleSectorIds, 10), "clipped chain includes A");
    Check(Contains(result.visibleSectorIds, 20), "clipped chain includes B");
    Check(!Contains(result.visibleSectorIds, 30),
          "recursive clipping excludes C outside the A/B angular window");

    Check(game::BuildRuntimeSectorVisibilityGraph(MakeClippedChain(true), graph, &error),
          "aligned clipped chain graph builds");
    result = game::ComputeRuntimeSectorVisibilityFromView(
            graph,
            nullptr,
            camera,
            ForwardFromPreviewYaw(0.0f),
            Degrees(90.0f),
            10);
    Check(Contains(result.visibleSectorIds, 30),
          "recursive clipping includes C when the deeper portal is inside the clipped window");
}

void TestViewRecursiveSliverRemainsVisible()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeClippedChain(true), graph, &error),
          "recursive sliver graph builds");

    const Vector2 camera = game::SectorCoordToWorldPosition2(32, 32);
    game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    camera,
                    ForwardFromPreviewYaw(Degrees(-24.0f)),
                    Degrees(52.0f),
                    10);
    Check(!result.fallbackDrawAll, "recursive sliver traversal does not fallback");
    Check(Contains(result.visibleSectorIds, 10), "recursive sliver includes A");
    Check(Contains(result.visibleSectorIds, 20), "recursive sliver includes B");
    Check(Contains(result.visibleSectorIds, 30),
          "recursive clipped sliver remains visible through B/C portal");
}

void TestViewCycleTerminates()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeCycle(), graph, &error),
          "view cycle graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(100.0f),
                    10);
    Check(result.validStartSector, "view cycle has valid start");
    Check(!result.fallbackDrawAll, "view cycle does not hit fallback");
    Check(result.visibleSectorIds.size() <= 3, "view cycle has no duplicate explosion");
    Check(std::is_sorted(result.visibleSectorIds.begin(), result.visibleSectorIds.end()),
          "view cycle visible sectors are deterministic");
}

void TestViewTraversalCapFallbackDrawsAll()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeCycle(), graph, &error),
          "cap fallback graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(100.0f),
                    10,
                    1);
    Check(result.fallbackDrawAll, "cap fallback requests draw-all");
    Check(result.status == "portal traversal cap hit", "cap fallback reports exact status");
    Check(result.visibleSectorIds.size() == 3
                  && Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20)
                  && Contains(result.visibleSectorIds, 30),
          "cap fallback exposes all sectors as visible");
}

void TestViewInvalidStartFallbackDrawsAll()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeAdjacent(), graph, &error),
          "view invalid fallback graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(512, 512),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(70.0f),
                    999);
    Check(!result.validStartSector, "view invalid fallback reports invalid start");
    Check(result.fallbackDrawAll, "view invalid fallback requests draw-all");
    Check(result.visibleSectorIds.size() == 2
                  && Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20),
          "view invalid fallback exposes all sectors as visible");
}

void TestViewClosedPortalDoesNotTraverse()
{
    SectorTopologyMap map = MakeAdjacent();
    game::FindSectorTopologySector(map, 20)->floorZ = 20.0f;
    game::FindSectorTopologySector(map, 20)->ceilingZ = 28.0f;

    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "view height-closed graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(90.0f),
                    10);
    Check(Contains(result.visibleSectorIds, 10), "view height-closed includes start");
    Check(!Contains(result.visibleSectorIds, 20),
          "view height-closed portal is not traversed even inside FOV");
}

void TestViewOneSidedVoidBoundaryDoesNotTraverse()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeSquareOnly(), graph, &error),
          "view void-boundary graph builds");
    Check(graph.portals.empty(), "view void-boundary has no portal edges");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    nullptr,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(90.0f),
                    10);
    Check(result.visibleSectorIds.size() == 1 && result.visibleSectorIds[0] == 10,
          "view void-boundary cannot enter a non-sector face");
}

void TestViewMultiStartUnionAndSortedMetadata()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeTurnFromMiddle(), graph, &error),
          "multi-start graph builds");

    const game::RuntimePortalVisibilityResult single =
            game::ComputeRuntimeSectorVisibilityFromViewSeeds(
                    graph,
                    game::SectorCoordToWorldPosition2(96, 32),
                    ForwardFromPreviewYaw(Pi * 0.5f),
                    Degrees(60.0f),
                    {10},
                    10);
    Check(Contains(single.visibleSectorIds, 10), "single preferred seed includes A");
    Check(!Contains(single.visibleSectorIds, 30),
          "single preferred seed misses north sector through side portal");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromViewSeeds(
                    graph,
                    game::SectorCoordToWorldPosition2(96, 32),
                    ForwardFromPreviewYaw(Pi * 0.5f),
                    Degrees(60.0f),
                    {20, 10, 20},
                    10);
    Check(result.validStartSector, "multi-start has valid start");
    Check(!result.fallbackDrawAll, "multi-start union does not fallback");
    Check(result.startSectorId == 10, "preferred valid seed remains primary start sector");
    Check(result.startSectorIds.size() == 2
                  && result.startSectorIds[0] == 10
                  && result.startSectorIds[1] == 20,
          "multi-start seed ids are deduplicated and sorted");
    Check(Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20)
                  && Contains(result.visibleSectorIds, 30),
          "multi-start union includes sectors visible from both full-FOV seeds");
    Check(std::is_sorted(result.visibleSectorIds.begin(), result.visibleSectorIds.end()),
          "multi-start visible sectors are sorted");
    Check(std::is_sorted(
                  result.traversedPortalLineDefIds.begin(),
                  result.traversedPortalLineDefIds.end()),
          "multi-start traversed portal linedefs are sorted");

    const std::string debugText = game::FormatRuntimePortalVisibilityDebugText(result);
    Check(debugText.find("start sectors: 10,20") != std::string::npos,
          "multi-start debug text reports multiple starts");
}

void TestViewMultiStartFallbackPropagates()
{
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(MakeCycle(), graph, &error),
          "multi-start fallback graph builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromViewSeeds(
                    graph,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(100.0f),
                    {20, 10},
                    10,
                    1);
    Check(result.fallbackDrawAll, "multi-start cap fallback requests draw-all");
    Check(result.visibleSectorIds.size() == 3
                  && Contains(result.visibleSectorIds, 10)
                  && Contains(result.visibleSectorIds, 20)
                  && Contains(result.visibleSectorIds, 30),
          "multi-start fallback exposes all sectors as visible");
    Check(result.startSectorIds.size() == 2
                  && result.startSectorIds[0] == 10
                  && result.startSectorIds[1] == 20,
          "multi-start fallback keeps sorted seed metadata");
}

void TestViewCenterSeedDiffersFromPreferredSeed()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "center-seed graph builds");

    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map, &error), "center-seed collision world builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    &world,
                    game::SectorCoordToWorldPosition2(32, 32),
                    ForwardFromPreviewYaw(0.0f),
                    Degrees(60.0f),
                    20);
    Check(result.startSectorId == 20, "preferred gameplay sector remains primary");
    Check(result.startSectorIds.size() == 2
                  && result.startSectorIds[0] == 10
                  && result.startSectorIds[1] == 20,
          "camera center sector is added as a full-FOV seed");
    Check(Contains(result.visibleSectorIds, 10),
          "center sector full-FOV seed reveals sector missed from preferred seed alone");
}

void TestViewFootprintSamplingAndRadiusCap()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "footprint seed graph builds");

    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map, &error), "footprint seed collision world builds");
    Check(Near(game::ClampRuntimeVisibilitySeedRadiusWorld(4.0f), 0.5f),
          "visibility seed radius is capped in world units");

    const game::RuntimePortalVisibilityResult noRadius =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    &world,
                    Vector2{0.45f, 0.25f},
                    ForwardFromPreviewYaw(Pi),
                    Degrees(45.0f),
                    0,
                    0,
                    0.0f);
    Check(noRadius.startSectorIds.size() == 1 && noRadius.startSectorIds[0] == 10,
          "zero visibility seed radius does not add duplicate cardinal samples");

    const game::RuntimePortalVisibilityResult capped =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    &world,
                    Vector2{0.45f, 0.25f},
                    ForwardFromPreviewYaw(Pi),
                    Degrees(45.0f),
                    0,
                    0,
                    4.0f);
    Check(capped.startSectorIds.size() == 2
                  && capped.startSectorIds[0] == 10
                  && capped.startSectorIds[1] == 20,
          "capped footprint sample reaches adjacent open sector");
}

void TestViewVerticalValidationIsTolerant()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "vertical seed graph builds");

    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map, &error), "vertical seed collision world builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    &world,
                    Vector2{0.45f, 0.25f},
                    ForwardFromPreviewYaw(Pi),
                    Degrees(45.0f),
                    10,
                    0,
                    4.0f,
                    1.30f,
                    true);
    Check(Contains(result.startSectorIds, 20),
          "vertical seed validation remains tolerant for ledge-overstep eye height");
}

void TestViewSolidBoundaryDoesNotCreateSeed()
{
    const SectorTopologyMap map = MakeSquareOnly();
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "solid boundary seed graph builds");

    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map, &error), "solid boundary collision world builds");

    const game::RuntimePortalVisibilityResult result =
            game::ComputeRuntimeSectorVisibilityFromView(
                    graph,
                    &world,
                    Vector2{0.45f, 0.25f},
                    ForwardFromPreviewYaw(Pi),
                    Degrees(45.0f),
                    0,
                    0,
                    4.0f);
    Check(result.startSectorIds.size() == 1 && result.startSectorIds[0] == 10,
          "footprint sampling does not seed through a one-sided solid boundary");
}

void TestPointLookupVisibilityDebug()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "point lookup graph builds");

    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map, &error), "point lookup collision world builds");

    const game::RuntimePortalVisibilityResult leftResult =
            game::ComputeRuntimeSectorVisibilityFromPoint(
                    graph,
                    &world,
                    game::SectorCoordToWorldPosition2(32, 32));
    Check(leftResult.validStartSector && leftResult.startSectorId == 10,
          "point lookup finds expected start sector");
    Check(leftResult.visibleSectorIds.size() == 2
                  && Contains(leftResult.visibleSectorIds, 10)
                  && Contains(leftResult.visibleSectorIds, 20),
          "point lookup visibility remains valid across connected sectors");

    const game::RuntimePortalVisibilityResult preferredResult =
            game::ComputeRuntimeSectorVisibilityFromPoint(
                    graph,
                    &world,
                    game::SectorCoordToWorldPosition2(32, 32),
                    20);
    Check(preferredResult.validStartSector && preferredResult.startSectorId == 20,
          "preferred current sector is used when valid");

    const std::string debugText = game::FormatRuntimePortalVisibilityDebugText(leftResult);
    Check(debugText.find("start sector: 10") != std::string::npos
                  && debugText.find("visible sectors: 10,20") != std::string::npos,
          "visibility debug text reports deterministic visible sector ids");
    Check(debugText.find("visible count: 2 / 2") != std::string::npos
                  && debugText.find("mode: connected portal traversal") != std::string::npos
                  && debugText.find("fallback: none") != std::string::npos,
          "visibility debug text reports count, mode, and fallback");

    const game::RuntimePortalVisibilityResult outsideResult =
            game::ComputeRuntimeSectorVisibilityFromPoint(
                    graph,
                    &world,
                    game::SectorCoordToWorldPosition2(512, 512));
    Check(!outsideResult.validStartSector && outsideResult.fallbackDrawAll,
          "outside-map lookup reports fallback draw-all");
    Check(outsideResult.status.find("outside sectors") != std::string::npos,
          "outside-map lookup reports fallback reason");
}

void TestMalformedReferencesFailCleanly()
{
    SectorTopologyMap map = MakeAdjacent();
    map.lineDefs[1].backSideDefId = 999;

    game::RuntimeSectorVisibilityGraph graph;
    std::string error;
    Check(!game::BuildRuntimeSectorVisibilityGraph(map, graph, &error),
          "malformed sidedef reference fails graph build");
    Check(!error.empty(), "malformed graph build reports error text");
    Check(graph.sectors.empty() && graph.portals.empty(),
          "malformed graph build clears partial result");
}

} // namespace

int main()
{
    TestOneSectorNoPortals();
    TestAdjacentCreatesDirectedEdges();
    TestOneSidedWallCreatesNoPortal();
    TestClosedPortalDoesNotTraverse();
    TestConnectedAndDisconnectedTraversal();
    TestCycleTerminates();
    TestInvalidStartFallback();
    TestViewYawFacingPortalIncludesNeighbor();
    TestViewYawFacingAwayExcludesNeighbor();
    TestViewSidePortalOutsideFovExcluded();
    TestViewPortalBarelyIntersectsFovEdge();
    TestViewPortalSegmentCrossesFovWedge();
    TestViewNearCameraPortalVisible();
    TestViewRecursivePortalClipping();
    TestViewRecursiveSliverRemainsVisible();
    TestViewCycleTerminates();
    TestViewTraversalCapFallbackDrawsAll();
    TestViewInvalidStartFallbackDrawsAll();
    TestViewClosedPortalDoesNotTraverse();
    TestViewOneSidedVoidBoundaryDoesNotTraverse();
    TestViewMultiStartUnionAndSortedMetadata();
    TestViewMultiStartFallbackPropagates();
    TestViewCenterSeedDiffersFromPreferredSeed();
    TestViewFootprintSamplingAndRadiusCap();
    TestViewVerticalValidationIsTolerant();
    TestViewSolidBoundaryDoesNotCreateSeed();
    TestPointLookupVisibilityDebug();
    TestMalformedReferencesFailCleanly();

    if (failures != 0) {
        std::cerr << failures << " SectorPortalVisibility test(s) failed\n";
        return 1;
    }

    return 0;
}
