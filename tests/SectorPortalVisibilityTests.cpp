#include "sector_demo/SectorPortalVisibility.h"

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
    TestMalformedReferencesFailCleanly();

    if (failures != 0) {
        std::cerr << failures << " SectorPortalVisibility test(s) failed\n";
        return 1;
    }

    return 0;
}
