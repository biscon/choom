#include "sector_demo/SectorTopologyMap.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using game::SectorCoord;
using game::SectorTopologyLineDef;
using game::SectorTopologyLoopSet;
using game::SectorTopologyMap;
using game::SectorTopologySector;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyValidationIssue;
using game::SectorTopologyVertex;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool ContainsMessage(
        const std::vector<SectorTopologyValidationIssue>& issues,
        const std::string& text)
{
    for (const auto& issue : issues) {
        if (issue.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
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
                -1
        });
        SectorTopologySideDef sideDef;
        sideDef.id = sideId;
        sideDef.lineDefId = lineId;
        sideDef.side = SectorTopologySideKind::Front;
        sideDef.sectorId = sectorId;
        map.sideDefs.push_back(std::move(sideDef));
    }
}

SectorTopologyMap MakeSquare()
{
    SectorTopologyMap map;
    SectorTopologySector sector;
    sector.id = 1;
    map.sectors.push_back(sector);
    AddSectorLoop(map, 1, {{0, 0}, {64, 0}, {64, 64}, {0, 64}});
    return map;
}

void TestValidSquare()
{
    const SectorTopologyMap map = MakeSquare();
    const auto issues = game::ValidateSectorTopologyMap(map);
    Check(!game::HasSectorTopologyValidationErrors(issues), "valid square validates");

    SectorTopologyLoopSet loops;
    Check(game::ExtractSectorTopologyLoops(map, 1, loops), "valid square extracts");
    Check(loops.outer.vertexIds == std::vector<int>({1, 2, 3, 4}),
          "outer loop ordering is deterministic");
    Check(loops.outer.signedAreaTwice > 0 && loops.holes.empty(),
          "square is one CCW outer loop");
}

void TestValidHole()
{
    SectorTopologyMap map = MakeSquare();
    AddSectorLoop(map, 1, {{16, 16}, {16, 48}, {48, 48}, {48, 16}});

    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> issues;
    Check(game::ExtractSectorTopologyLoops(map, 1, loops, &issues),
          "square with clockwise hole extracts");
    Check(loops.holes.size() == 1 && loops.holes.front().signedAreaTwice < 0,
          "clockwise loop is classified as a hole");
}

void TestBackSideDirection()
{
    SectorTopologyMap map = MakeSquare();
    for (size_t i = 0; i < map.lineDefs.size(); ++i) {
        SectorTopologyLineDef& line = map.lineDefs[i];
        std::swap(line.startVertexId, line.endVertexId);
        line.backSideDefId = line.frontSideDefId;
        line.frontSideDefId = -1;
        map.sideDefs[i].side = SectorTopologySideKind::Back;
    }

    SectorTopologyLoopSet loops;
    Check(game::ExtractSectorTopologyLoops(map, 1, loops),
          "back sidedefs use end-to-start direction");
    Check(loops.outer.signedAreaTwice > 0,
          "back-sided square preserves CCW sector winding");
}

void TestMalformedReferencesAndIds()
{
    SectorTopologyMap map = MakeSquare();
    map.vertices.push_back(map.vertices.front());
    map.vertices.back().id = 1;
    map.sectors.push_back(SectorTopologySector{});
    map.sectors.back().id = 0;
    map.lineDefs.front().startVertexId = 999;
    const auto issues = game::ValidateSectorTopologyMap(map);
    Check(ContainsMessage(issues, "duplicate ID"), "duplicate vertex ID is diagnosed");
    Check(ContainsMessage(issues, "ID must be positive"), "non-positive ID is diagnosed");
    Check(ContainsMessage(issues, "start vertex 999"), "dangling endpoint is diagnosed safely");
}

void TestSideSlotConsistency()
{
    SectorTopologyMap mismatched = MakeSquare();
    mismatched.sideDefs.front().side = SectorTopologySideKind::Back;
    auto issues = game::ValidateSectorTopologyMap(mismatched);
    Check(ContainsMessage(issues, "inconsistent front sidedef slot"),
          "line-to-side slot mismatch is diagnosed");
    Check(ContainsMessage(issues, "matching slot"),
          "side-to-line slot mismatch is diagnosed");

    SectorTopologyMap multiple = MakeSquare();
    SectorTopologySideDef extra;
    extra.id = 5;
    extra.lineDefId = 1;
    extra.sectorId = 1;
    multiple.sideDefs.push_back(extra);
    issues = game::ValidateSectorTopologyMap(multiple);
    Check(ContainsMessage(issues, "multiple sidedefs"),
          "multiple sidedefs claiming one slot are diagnosed");
}

void TestDuplicatePhysicalLine()
{
    SectorTopologyMap map = MakeSquare();
    SectorTopologyLineDef duplicate = map.lineDefs.front();
    duplicate.id = 5;
    duplicate.startVertexId = map.lineDefs.front().endVertexId;
    duplicate.endVertexId = map.lineDefs.front().startVertexId;
    duplicate.frontSideDefId = 5;
    map.lineDefs.push_back(duplicate);
    SectorTopologySideDef side;
    side.id = 5;
    side.lineDefId = 5;
    side.sectorId = 1;
    map.sideDefs.push_back(side);
    const auto issues = game::ValidateSectorTopologyMap(map);
    Check(ContainsMessage(issues, "physical linedef"), "reversed duplicate linedef is diagnosed");
}

void TestOpenAndBranchingBoundaries()
{
    SectorTopologyMap open = MakeSquare();
    open.lineDefs.pop_back();
    open.sideDefs.pop_back();
    auto issues = game::ValidateSectorTopologyMap(open);
    Check(ContainsMessage(issues, "open boundary"), "open sector chain is diagnosed");

    SectorTopologyMap branching = MakeSquare();
    branching.vertices.push_back(SectorTopologyVertex{5, 96, 0});
    branching.vertices.push_back(SectorTopologyVertex{6, 64, -32});
    for (const SectorTopologyLineDef line : {
            SectorTopologyLineDef{5, 1, 5, 5, -1},
            SectorTopologyLineDef{6, 5, 6, 6, -1},
            SectorTopologyLineDef{7, 6, 1, 7, -1}}) {
        branching.lineDefs.push_back(line);
        SectorTopologySideDef branchSide;
        branchSide.id = line.frontSideDefId;
        branchSide.lineDefId = line.id;
        branchSide.sectorId = 1;
        branching.sideDefs.push_back(branchSide);
    }
    issues = game::ValidateSectorTopologyMap(branching);
    Check(ContainsMessage(issues, "branching"), "branching boundary is diagnosed");
}

SectorTopologyMap MakeLooseLines(
        const std::vector<std::pair<SectorCoord, SectorCoord>>& points,
        const std::vector<std::pair<int, int>>& endpointIndices)
{
    SectorTopologyMap map;
    for (size_t i = 0; i < points.size(); ++i) {
        map.vertices.push_back(SectorTopologyVertex{
                static_cast<int>(i + 1), points[i].first, points[i].second});
    }
    for (size_t i = 0; i < endpointIndices.size(); ++i) {
        map.lineDefs.push_back(SectorTopologyLineDef{
                static_cast<int>(i + 1),
                endpointIndices[i].first,
                endpointIndices[i].second,
                -1,
                -1
        });
    }
    return map;
}

void TestIntersections()
{
    auto crossing = MakeLooseLines(
            {{0, 0}, {64, 64}, {0, 64}, {64, 0}}, {{1, 2}, {3, 4}});
    auto issues = game::ValidateSectorTopologyMap(crossing);
    Check(ContainsMessage(issues, "invalid intersection"), "crossing linedefs are diagnosed");

    auto overlap = MakeLooseLines(
            {{0, 0}, {64, 0}, {32, 0}, {96, 0}}, {{1, 2}, {3, 4}});
    issues = game::ValidateSectorTopologyMap(overlap);
    Check(ContainsMessage(issues, "partially overlapping"), "partial overlap is diagnosed");

    auto unsplitTouch = MakeLooseLines(
            {{0, 0}, {64, 0}, {32, -32}, {32, 0}}, {{1, 2}, {3, 4}});
    issues = game::ValidateSectorTopologyMap(unsplitTouch);
    Check(ContainsMessage(issues, "invalid intersection"), "unsplit interior touch is diagnosed");
}

void TestWindingAndContainmentFailures()
{
    SectorTopologyMap multiple = MakeSquare();
    AddSectorLoop(multiple, 1, {{96, 0}, {128, 0}, {128, 32}, {96, 32}});
    auto issues = game::ValidateSectorTopologyMap(multiple);
    Check(ContainsMessage(issues, "multiple CCW"), "multiple outer loops are diagnosed");

    SectorTopologyMap outsideHole = MakeSquare();
    AddSectorLoop(outsideHole, 1, {{96, 0}, {96, 32}, {128, 32}, {128, 0}});
    issues = game::ValidateSectorTopologyMap(outsideHole);
    Check(ContainsMessage(issues, "hole outside"), "outside hole is diagnosed");

    SectorTopologyMap onlyHole;
    SectorTopologySector sector;
    sector.id = 1;
    onlyHole.sectors.push_back(sector);
    AddSectorLoop(onlyHole, 1, {{0, 0}, {0, 64}, {64, 64}, {64, 0}});
    issues = game::ValidateSectorTopologyMap(onlyHole);
    Check(ContainsMessage(issues, "only clockwise"), "hole-only sector is diagnosed");
}

void TestLoopGeometryFailures()
{
    SectorTopologyMap selfIntersecting;
    SectorTopologySector sector;
    sector.id = 1;
    selfIntersecting.sectors.push_back(sector);
    AddSectorLoop(selfIntersecting, 1,
                  {{0, 0}, {64, 64}, {0, 64}, {64, 0}, {64, -32}});
    auto issues = game::ValidateSectorTopologyMap(selfIntersecting);
    Check(ContainsMessage(issues, "self-intersecting"),
          "nonzero-area self-intersecting loop is diagnosed");

    SectorTopologyMap zeroArea;
    zeroArea.sectors.push_back(sector);
    AddSectorLoop(zeroArea, 1, {{0, 0}, {64, 64}, {0, 64}, {64, 0}});
    issues = game::ValidateSectorTopologyMap(zeroArea);
    Check(ContainsMessage(issues, "zero-area"), "zero-area loop is diagnosed");

    SectorTopologyMap intersectingHoles = MakeSquare();
    AddSectorLoop(intersectingHoles, 1, {{8, 8}, {8, 40}, {40, 40}, {40, 8}});
    AddSectorLoop(intersectingHoles, 1, {{24, 24}, {24, 56}, {56, 56}, {56, 24}});
    issues = game::ValidateSectorTopologyMap(intersectingHoles);
    Check(ContainsMessage(issues, "intersect or touch"),
          "intersecting holes are diagnosed");

    SectorTopologyMap nestedHoles = MakeSquare();
    AddSectorLoop(nestedHoles, 1, {{8, 8}, {8, 56}, {56, 56}, {56, 8}});
    AddSectorLoop(nestedHoles, 1, {{16, 16}, {16, 32}, {32, 32}, {32, 16}});
    issues = game::ValidateSectorTopologyMap(nestedHoles);
    Check(ContainsMessage(issues, "nested holes"), "nested holes are diagnosed");
}

} // namespace

int main()
{
    TestValidSquare();
    TestValidHole();
    TestBackSideDirection();
    TestMalformedReferencesAndIds();
    TestSideSlotConsistency();
    TestDuplicatePhysicalLine();
    TestOpenAndBranchingBoundaries();
    TestIntersections();
    TestWindingAndContainmentFailures();
    TestLoopGeometryFailures();

    if (failures != 0) {
        std::cerr << failures << " topology validation test(s) failed\n";
        return 1;
    }
    std::cout << "Sector topology validation tests passed\n";
    return 0;
}
