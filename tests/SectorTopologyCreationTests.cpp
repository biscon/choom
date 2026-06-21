#include "sector_demo/SectorTopologyCreation.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using game::SectorTopologyCoordPoint;
using game::SectorTopologyCreatePolygonOptions;
using game::SectorTopologyLineDef;
using game::SectorTopologyLoopSet;
using game::SectorTopologyMap;
using game::SectorTopologySideKind;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

SectorTopologyCreatePolygonOptions Options()
{
    SectorTopologyCreatePolygonOptions options;
    options.floorTextureId = "floor";
    options.ceilingTextureId = "ceiling";
    options.defaultWall.textureId = "wall";
    options.defaultLower.textureId = "step_wall";
    options.defaultUpper.textureId = "upper_wall";
    return options;
}

bool Create(
        SectorTopologyMap& map,
        const std::vector<SectorTopologyCoordPoint>& points,
        int* outSectorId = nullptr,
        std::string* outError = nullptr)
{
    return game::CreateSectorTopologyPolygon(map, points, Options(), outSectorId, outError);
}

bool ValidateAndExtract(
        const SectorTopologyMap& map,
        int sectorId,
        SectorTopologyLoopSet& loops)
{
    const auto issues = game::ValidateSectorTopologyMap(map);
    if (game::HasSectorTopologyValidationErrors(issues)) {
        if (!issues.empty()) {
            std::cerr << game::FormatSectorTopologyValidationIssue(issues.front()) << '\n';
        }
        return false;
    }
    return game::ExtractSectorTopologyLoops(map, sectorId, loops);
}

void CheckUnchanged(
        const SectorTopologyMap& before,
        const SectorTopologyMap& after,
        const char* description)
{
    Check(before.vertices.size() == after.vertices.size(), description);
    Check(before.lineDefs.size() == after.lineDefs.size(), description);
    Check(before.sideDefs.size() == after.sideDefs.size(), description);
    Check(before.sectors.size() == after.sectors.size(), description);
}

const SectorTopologyLineDef* FindLine(
        const SectorTopologyMap& map,
        int startVertexId,
        int endVertexId)
{
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == startVertexId && lineDef.endVertexId == endVertexId) {
            return &lineDef;
        }
    }
    return nullptr;
}

void TestCreateSquare()
{
    SectorTopologyMap map;
    int sectorId = -1;
    std::string error;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId, &error),
          "square creation succeeds");
    Check(error.empty(), "successful square creation clears error");
    Check(map.vertices.size() == 4, "square has 4 vertices");
    Check(map.lineDefs.size() == 4, "square has 4 linedefs");
    Check(map.sideDefs.size() == 4, "square has 4 sidedefs");
    Check(map.sectors.size() == 1, "square has 1 sector");

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "square validates and extracts");
    Check(loops.outer.signedAreaTwice > 0 && loops.holes.empty(),
          "square extracts one CCW outer loop");
}

void TestAdjacentSharesFullEdge()
{
    SectorTopologyMap map;
    int firstSectorId = -1;
    int secondSectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &firstSectorId),
          "first adjacent square succeeds");
    Check(Create(map, {{64, 0}, {128, 0}, {128, 64}, {64, 64}}, &secondSectorId),
          "second adjacent square succeeds");

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, firstSectorId, loops), "first adjacent sector extracts");
    Check(ValidateAndExtract(map, secondSectorId, loops), "second adjacent sector extracts");
    Check(map.vertices.size() == 6, "adjacent sectors reuse shared endpoint vertices");
    Check(map.lineDefs.size() == 7, "adjacent sectors share one linedef");
    Check(map.sideDefs.size() == 8, "adjacent sectors have 8 sidedefs");
    Check(map.sectors.size() == 2, "adjacent sectors have 2 sectors");

    const SectorTopologyLineDef* shared = FindLine(map, 2, 3);
    Check(shared != nullptr, "shared edge keeps original linedef");
    Check(shared != nullptr
                  && game::IsValidSectorTopologyId(shared->frontSideDefId)
                  && game::IsValidSectorTopologyId(shared->backSideDefId),
          "shared linedef has both front and back sidedefs");
}

void TestRejectOccupiedSlot()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "base square for occupied-slot test succeeds");
    const SectorTopologyMap before = map;
    std::string error;
    Check(!Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, nullptr, &error),
          "duplicate square fails");
    Check(error.find("already occupied") != std::string::npos,
          "duplicate square reports occupied slot");
    CheckUnchanged(before, map, "duplicate square leaves map unchanged");
}

void TestRejectPartialOverlap()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "base square for overlap test succeeds");
    const SectorTopologyMap before = map;
    std::string error;
    Check(!Create(map, {{32, -32}, {96, -32}, {96, 0}, {32, 0}}, nullptr, &error),
          "partial overlap fails");
    Check(!error.empty(), "partial overlap reports an error");
    CheckUnchanged(before, map, "partial overlap leaves map unchanged");
}

void TestRejectCrossing()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "base square for crossing test succeeds");
    const SectorTopologyMap before = map;
    std::string error;
    Check(!Create(map, {{80, -16}, {96, 16}, {-16, 16}, {-16, -16}}, nullptr, &error),
          "crossing polygon fails");
    Check(!error.empty(), "crossing polygon reports an error");
    CheckUnchanged(before, map, "crossing polygon leaves map unchanged");
}

void TestClockwiseNormalizes()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {0, 64}, {64, 64}, {64, 0}}, &sectorId),
          "clockwise square creation succeeds");
    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "clockwise input validates and extracts");
    Check(loops.outer.signedAreaTwice > 0, "clockwise input normalizes to CCW outer loop");
}

void TestInvalidPoints()
{
    SectorTopologyMap map;
    std::string error;
    Check(!Create(map, {{0, 0}, {64, 0}}, nullptr, &error),
          "fewer than 3 points fails");
    Check(map.vertices.empty() && map.lineDefs.empty() && map.sideDefs.empty() && map.sectors.empty(),
          "short polygon leaves map empty");

    Check(!Create(map, {{0, 0}, {64, 0}, {64, 0}, {0, 64}}, nullptr, &error),
          "duplicate consecutive point fails");
    Check(map.vertices.empty() && map.lineDefs.empty() && map.sideDefs.empty() && map.sectors.empty(),
          "duplicate consecutive point leaves map empty");

    Check(!Create(map, {{0, 0}, {64, 0}, {0, 64}, {64, 0}}, nullptr, &error),
          "repeated interior point fails");
    Check(map.vertices.empty() && map.lineDefs.empty() && map.sideDefs.empty() && map.sectors.empty(),
          "repeated interior point leaves map empty");

    Check(!Create(map, {{0, 0}, {32, 32}, {64, 64}}, nullptr, &error),
          "zero-area polygon fails");
    Check(map.vertices.empty() && map.lineDefs.empty() && map.sideDefs.empty() && map.sectors.empty(),
          "zero-area polygon leaves map empty");
}

} // namespace

int main()
{
    TestCreateSquare();
    TestAdjacentSharesFullEdge();
    TestRejectOccupiedSlot();
    TestRejectPartialOverlap();
    TestRejectCrossing();
    TestClockwiseNormalizes();
    TestInvalidPoints();

    if (failures != 0) {
        std::cerr << failures << " topology creation test(s) failed\n";
        return 1;
    }
    std::cout << "Sector topology creation tests passed\n";
    return 0;
}
