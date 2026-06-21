#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorGeneratedGeometry.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

using game::SectorTopologyCoordPoint;
using game::SectorTopologyCreatePolygonOptions;
using game::SectorTopologyInsertPolygonOptions;
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

bool Insert(
        SectorTopologyMap& map,
        int parentSectorId,
        const std::vector<SectorTopologyCoordPoint>& points,
        int* outSectorId = nullptr,
        std::string* outError = nullptr)
{
    return game::InsertSectorTopologyPolygon(
            map,
            parentSectorId,
            points,
            SectorTopologyInsertPolygonOptions{},
            outSectorId,
            outError);
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

const game::SectorTopologySideDef* FindSideDef(const SectorTopologyMap& map, int sideDefId)
{
    return game::FindSectorTopologySideDef(map, sideDefId);
}

game::SectorTopologySideDef* FindSideDef(SectorTopologyMap& map, int sideDefId)
{
    return game::FindSectorTopologySideDef(map, sideDefId);
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

std::vector<const SectorTopologyLineDef*> InsertedBoundaryLines(
        const SectorTopologyMap& map,
        int parentSectorId,
        int childSectorId)
{
    std::vector<const SectorTopologyLineDef*> lines;
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const game::SectorTopologySideDef* front = FindSideDef(map, lineDef.frontSideDefId);
        const game::SectorTopologySideDef* back = FindSideDef(map, lineDef.backSideDefId);
        if (front != nullptr
                && back != nullptr
                && front->sectorId == childSectorId
                && back->sectorId == parentSectorId
                && front->side == SectorTopologySideKind::Front
                && back->side == SectorTopologySideKind::Back) {
            lines.push_back(&lineDef);
        }
    }
    return lines;
}

const game::SectorGeneratedSurface* FindSurface(
        const game::SectorGeneratedGeometry& geometry,
        game::SectorGeneratedSurfaceKind kind,
        int sectorId,
        int lineDefId = -2)
{
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind == kind
                && surface.ref.topologySectorId == sectorId
                && (lineDefId == -2 || surface.ref.topologyLineDefId == lineDefId)) {
            return &surface;
        }
    }
    return nullptr;
}

double TriangleAreaXZ(const game::SectorGeneratedSurface& surface)
{
    double area = 0.0;
    for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
        const Vector3 a = surface.vertices[i].position;
        const Vector3 b = surface.vertices[i + 1].position;
        const Vector3 c = surface.vertices[i + 2].position;
        area += std::fabs(
                static_cast<double>(b.x - a.x) * static_cast<double>(c.z - a.z)
                - static_cast<double>(b.z - a.z) * static_cast<double>(c.x - a.x)) * 0.5;
    }
    return area;
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

void TestInsertSquareInsideRoom()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    std::string error;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "insert parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId, &error),
          "insert square inside room succeeds");
    Check(error.empty(), "successful insert clears error");

    SectorTopologyLoopSet parentLoops;
    SectorTopologyLoopSet childLoops;
    Check(ValidateAndExtract(map, parentId, parentLoops), "insert parent extracts");
    Check(ValidateAndExtract(map, childId, childLoops), "insert child extracts");
    Check(parentLoops.outer.signedAreaTwice > 0 && parentLoops.holes.size() == 1,
          "parent has one outer and one hole");
    Check(childLoops.outer.signedAreaTwice > 0 && childLoops.holes.empty(),
          "child has one outer and no holes");

    const std::vector<const SectorTopologyLineDef*> lines =
            InsertedBoundaryLines(map, parentId, childId);
    Check(lines.size() == 4, "inserted boundary has four two-sided linedefs");
    for (const SectorTopologyLineDef* lineDef : lines) {
        Check(lineDef->frontSideDefId >= 0 && lineDef->backSideDefId >= 0,
              "inserted linedef has front and back sidedefs");
        const game::SectorTopologySideDef* front = FindSideDef(map, lineDef->frontSideDefId);
        const game::SectorTopologySideDef* back = FindSideDef(map, lineDef->backSideDefId);
        Check(front != nullptr && front->sectorId == childId,
              "inserted front sidedef belongs to child");
        Check(back != nullptr && back->sectorId == parentId,
              "inserted back sidedef belongs to parent");
    }
}

void TestInsertChildCopiesParentFields()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "copy parent creation succeeds");
    game::SectorTopologySector* parent = game::FindSectorTopologySector(map, parentId);
    Check(parent != nullptr, "copy test finds parent");
    if (parent != nullptr) {
        parent->floorZ = -8.0f;
        parent->ceilingZ = 48.0f;
        parent->floorTextureId = "parent_floor";
        parent->ceilingTextureId = "parent_ceiling";
        parent->floorUv.scale = {2.0f, 3.0f};
        parent->floorUv.offset = {4.0f, 5.0f};
        parent->ceilingUv.scale = {6.0f, 7.0f};
        parent->ceilingUv.offset = {8.0f, 9.0f};
        parent->ambientColor = Color{10, 20, 30, 255};
        parent->ambientIntensity = 0.42f;
        parent->defaultWall.textureId = "parent_wall";
        parent->defaultWall.uv.scale = {1.5f, 2.5f};
        parent->defaultLower.textureId = "parent_lower";
        parent->defaultLower.uv.offset = {3.5f, 4.5f};
        parent->defaultUpper.textureId = "parent_upper";
        parent->defaultUpper.uv.scale = {5.5f, 6.5f};
    }

    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId),
          "copy insert succeeds");
    const game::SectorTopologySector* copiedParent = game::FindSectorTopologySector(map, parentId);
    const game::SectorTopologySector* child = game::FindSectorTopologySector(map, childId);
    Check(copiedParent != nullptr && child != nullptr, "copy test finds sectors after insert");
    if (copiedParent != nullptr && child != nullptr) {
        Check(child->floorZ == copiedParent->floorZ, "child copies floor");
        Check(child->ceilingZ == copiedParent->ceilingZ, "child copies ceiling");
        Check(child->floorTextureId == copiedParent->floorTextureId, "child copies floor texture");
        Check(child->ceilingTextureId == copiedParent->ceilingTextureId, "child copies ceiling texture");
        Check(child->floorUv.scale.x == copiedParent->floorUv.scale.x
                      && child->floorUv.offset.y == copiedParent->floorUv.offset.y,
              "child copies floor UV");
        Check(child->ceilingUv.scale.y == copiedParent->ceilingUv.scale.y
                      && child->ceilingUv.offset.x == copiedParent->ceilingUv.offset.x,
              "child copies ceiling UV");
        Check(child->ambientColor.r == copiedParent->ambientColor.r
                      && child->ambientIntensity == copiedParent->ambientIntensity,
              "child copies ambient fields");
        Check(child->defaultWall.textureId == copiedParent->defaultWall.textureId,
              "child copies default wall");
        Check(child->defaultLower.textureId == copiedParent->defaultLower.textureId,
              "child copies default lower");
        Check(child->defaultUpper.textureId == copiedParent->defaultUpper.textureId,
              "child copies default upper");
    }

    for (const SectorTopologyLineDef* lineDef : InsertedBoundaryLines(map, parentId, childId)) {
        const game::SectorTopologySideDef* front = FindSideDef(map, lineDef->frontSideDefId);
        const game::SectorTopologySideDef* back = FindSideDef(map, lineDef->backSideDefId);
        Check(front != nullptr && child != nullptr
                      && front->wall.textureId == child->defaultWall.textureId
                      && front->lower.textureId == child->defaultLower.textureId
                      && front->upper.textureId == child->defaultUpper.textureId,
              "child front sidedef copies child defaults");
        Check(back != nullptr && copiedParent != nullptr
                      && back->wall.textureId == copiedParent->defaultWall.textureId
                      && back->lower.textureId == copiedParent->defaultLower.textureId
                      && back->upper.textureId == copiedParent->defaultUpper.textureId,
              "parent back sidedef copies parent defaults");
    }
}

void TestInsertSidesAreIndependent()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "independent parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId),
          "independent insert succeeds");
    const std::vector<const SectorTopologyLineDef*> lines =
            InsertedBoundaryLines(map, parentId, childId);
    Check(!lines.empty(), "independent test finds inserted line");
    if (lines.empty()) {
        return;
    }

    const int frontId = lines.front()->frontSideDefId;
    const int backId = lines.front()->backSideDefId;
    game::SectorTopologySideDef* front = FindSideDef(map, frontId);
    game::SectorTopologySideDef* back = FindSideDef(map, backId);
    Check(front != nullptr && back != nullptr, "independent test finds sidedefs");
    if (front == nullptr || back == nullptr) {
        return;
    }

    front->wall.textureId = "child_changed_wall";
    front->wall.uv.scale = {3.0f, 4.0f};
    Check(back->wall.textureId != "child_changed_wall"
                  && back->wall.uv.scale.x != 3.0f,
          "parent back sidedef remains independent after child edit");

    back->wall.textureId = "parent_changed_wall";
    back->wall.uv.offset = {5.0f, 6.0f};
    Check(front->wall.textureId != "parent_changed_wall"
                  && front->wall.uv.offset.x != 5.0f,
          "child front sidedef remains independent after parent edit");
}

void TestInsertGeneratedGeometryHole()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "geometry parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId),
          "geometry insert succeeds");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error),
          "inserted geometry builds");
    const game::SectorGeneratedSurface* parentFloor =
            FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, parentId);
    const game::SectorGeneratedSurface* childFloor =
            FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, childId);
    Check(parentFloor != nullptr, "parent floor exists");
    Check(childFloor != nullptr, "child floor exists");
    const double outerWorldSide = game::SectorCoordToWorldDistance(128);
    const double childWorldSide = game::SectorCoordToWorldDistance(64);
    const double expectedParentArea = outerWorldSide * outerWorldSide - childWorldSide * childWorldSide;
    Check(parentFloor != nullptr
                  && std::fabs(TriangleAreaXZ(*parentFloor) - expectedParentArea) < 0.000001,
          "parent floor triangulation excludes child area");
}

void TestInsertRaisedChildPlatform()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "platform parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId),
          "platform insert succeeds");
    game::SectorTopologySector* child = game::FindSectorTopologySector(map, childId);
    Check(child != nullptr, "platform test finds child");
    if (child != nullptr) {
        child->floorZ += 8.0f;
    }

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error),
          "raised child geometry builds");
    int lowerWalls = 0;
    for (const SectorTopologyLineDef* lineDef : InsertedBoundaryLines(map, parentId, childId)) {
        if (FindSurface(geometry, game::SectorGeneratedSurfaceKind::LowerWall, parentId, lineDef->id) != nullptr) {
            ++lowerWalls;
        }
        Check(FindSurface(geometry, game::SectorGeneratedSurfaceKind::LowerWall, childId, lineDef->id) == nullptr,
              "raised child does not emit duplicate child lower wall");
    }
    Check(lowerWalls == 4, "raised child emits parent-side lower walls around boundary");
}

void TestNestedInsert()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    int grandchildId = -1;
    Check(Create(map, {{0, 0}, {192, 0}, {192, 192}, {0, 192}}, &parentId),
          "nested parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {160, 32}, {160, 160}, {32, 160}}, &childId),
          "nested child insert succeeds");
    Check(Insert(map, childId, {{64, 64}, {128, 64}, {128, 128}, {64, 128}}, &grandchildId),
          "nested grandchild insert succeeds");

    SectorTopologyLoopSet parentLoops;
    SectorTopologyLoopSet childLoops;
    SectorTopologyLoopSet grandchildLoops;
    Check(ValidateAndExtract(map, parentId, parentLoops), "nested parent extracts");
    Check(ValidateAndExtract(map, childId, childLoops), "nested child extracts");
    Check(ValidateAndExtract(map, grandchildId, grandchildLoops), "nested grandchild extracts");
    Check(parentLoops.holes.size() == 1, "nested parent has one hole");
    Check(childLoops.holes.size() == 1, "nested child has one hole");
    Check(grandchildLoops.holes.empty(), "nested grandchild has no holes");
}

void ExpectInsertRejected(
        const SectorTopologyMap& input,
        int parentSectorId,
        const std::vector<SectorTopologyCoordPoint>& points,
        const char* description)
{
    SectorTopologyMap map = input;
    const SectorTopologyMap before = map;
    std::string error;
    Check(!Insert(map, parentSectorId, points, nullptr, &error), description);
    Check(!error.empty(), "rejected insert reports an error");
    CheckUnchanged(before, map, "rejected insert leaves map unchanged");
}

void TestInvalidInsertsRejectTransactionally()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {128, 0}, {128, 128}, {0, 128}}, &parentId),
          "invalid insert parent creation succeeds");
    Check(Insert(map, parentId, {{32, 32}, {96, 32}, {96, 96}, {32, 96}}, &childId),
          "invalid insert existing hole creation succeeds");

    ExpectInsertRejected(
            map,
            parentId,
            {{144, 32}, {176, 32}, {176, 64}, {144, 64}},
            "outside parent insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{0, 16}, {24, 16}, {24, 40}, {0, 40}},
            "touching parent boundary insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{112, 32}, {144, 32}, {144, 64}, {112, 64}},
            "crossing parent boundary insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{48, 48}, {80, 48}, {80, 80}, {48, 80}},
            "inside existing parent hole insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{24, 24}, {104, 24}, {104, 104}, {24, 104}},
            "enclosing existing parent hole insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{96, 48}, {112, 48}, {112, 80}, {96, 80}},
            "touching existing topology insert fails");
    ExpectInsertRejected(
            map,
            parentId,
            {{48, 32}, {80, 32}, {80, 48}, {48, 48}},
            "partial overlap with existing topology insert fails");
    ExpectInsertRejected(
            map,
            childId,
            {{40, 40}, {88, 40}, {88, 40}, {40, 88}},
            "duplicate insert points fail");
    ExpectInsertRejected(
            map,
            childId,
            {{40, 40}, {64, 64}, {88, 88}},
            "zero-area insert fails");
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

void TestChangingSectorDefaultsDoesNotRewriteExistingSideDefs()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId),
          "default rewrite test sector creation succeeds");
    Check(!map.sideDefs.empty(), "default rewrite test has sidedefs");

    std::vector<std::string> wallTextures;
    std::vector<std::string> lowerTextures;
    std::vector<std::string> upperTextures;
    wallTextures.reserve(map.sideDefs.size());
    lowerTextures.reserve(map.sideDefs.size());
    upperTextures.reserve(map.sideDefs.size());
    for (const game::SectorTopologySideDef& sideDef : map.sideDefs) {
        wallTextures.push_back(sideDef.wall.textureId);
        lowerTextures.push_back(sideDef.lower.textureId);
        upperTextures.push_back(sideDef.upper.textureId);
    }

    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, sectorId);
    Check(sector != nullptr, "default rewrite test finds sector");
    if (sector != nullptr) {
        sector->defaultWall.textureId = "future_wall";
        sector->defaultLower.textureId = "future_lower";
        sector->defaultUpper.textureId = "future_upper";
    }

    for (size_t i = 0; i < map.sideDefs.size(); ++i) {
        Check(map.sideDefs[i].wall.textureId == wallTextures[i],
              "changing default wall leaves existing sidedef wall unchanged");
        Check(map.sideDefs[i].lower.textureId == lowerTextures[i],
              "changing default lower leaves existing sidedef lower unchanged");
        Check(map.sideDefs[i].upper.textureId == upperTextures[i],
              "changing default upper leaves existing sidedef upper unchanged");
    }
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
    TestInsertSquareInsideRoom();
    TestInsertChildCopiesParentFields();
    TestInsertSidesAreIndependent();
    TestInsertGeneratedGeometryHole();
    TestInsertRaisedChildPlatform();
    TestNestedInsert();
    TestInvalidInsertsRejectTransactionally();
    TestAdjacentSharesFullEdge();
    TestChangingSectorDefaultsDoesNotRewriteExistingSideDefs();
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
