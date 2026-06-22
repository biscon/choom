#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorGeneratedGeometry.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

using game::SectorTopologyCoordPoint;
using game::SectorTopologyCreatePolygonOptions;
using game::SectorTopologyInsertPolygonOptions;
using game::SectorTopologyLineDef;
using game::SectorTopologyLoop;
using game::SectorTopologyLoopSet;
using game::SectorTopologyMap;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyDeleteSectorResult;
using game::SectorTopologySplitLineResult;

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
    if (before.vertices.size() == after.vertices.size()) {
        for (size_t i = 0; i < before.vertices.size(); ++i) {
            Check(before.vertices[i].id == after.vertices[i].id
                          && before.vertices[i].x == after.vertices[i].x
                          && before.vertices[i].y == after.vertices[i].y,
                  description);
        }
    }
    if (before.lineDefs.size() == after.lineDefs.size()) {
        for (size_t i = 0; i < before.lineDefs.size(); ++i) {
            Check(before.lineDefs[i].id == after.lineDefs[i].id
                          && before.lineDefs[i].startVertexId == after.lineDefs[i].startVertexId
                          && before.lineDefs[i].endVertexId == after.lineDefs[i].endVertexId
                          && before.lineDefs[i].frontSideDefId == after.lineDefs[i].frontSideDefId
                          && before.lineDefs[i].backSideDefId == after.lineDefs[i].backSideDefId,
                  description);
        }
    }
    if (before.sideDefs.size() == after.sideDefs.size()) {
        for (size_t i = 0; i < before.sideDefs.size(); ++i) {
            Check(before.sideDefs[i].id == after.sideDefs[i].id
                          && before.sideDefs[i].lineDefId == after.sideDefs[i].lineDefId
                          && before.sideDefs[i].side == after.sideDefs[i].side
                          && before.sideDefs[i].sectorId == after.sideDefs[i].sectorId
                          && before.sideDefs[i].wall.textureId == after.sideDefs[i].wall.textureId
                          && before.sideDefs[i].lower.textureId == after.sideDefs[i].lower.textureId
                          && before.sideDefs[i].upper.textureId == after.sideDefs[i].upper.textureId
                          && before.sideDefs[i].wall.uv.scale.x == after.sideDefs[i].wall.uv.scale.x
                          && before.sideDefs[i].wall.uv.scale.y == after.sideDefs[i].wall.uv.scale.y
                          && before.sideDefs[i].wall.uv.offset.x == after.sideDefs[i].wall.uv.offset.x
                          && before.sideDefs[i].wall.uv.offset.y == after.sideDefs[i].wall.uv.offset.y
                          && before.sideDefs[i].lower.uv.scale.x == after.sideDefs[i].lower.uv.scale.x
                          && before.sideDefs[i].lower.uv.scale.y == after.sideDefs[i].lower.uv.scale.y
                          && before.sideDefs[i].lower.uv.offset.x == after.sideDefs[i].lower.uv.offset.x
                          && before.sideDefs[i].lower.uv.offset.y == after.sideDefs[i].lower.uv.offset.y
                          && before.sideDefs[i].upper.uv.scale.x == after.sideDefs[i].upper.uv.scale.x
                          && before.sideDefs[i].upper.uv.scale.y == after.sideDefs[i].upper.uv.scale.y
                          && before.sideDefs[i].upper.uv.offset.x == after.sideDefs[i].upper.uv.offset.x
                          && before.sideDefs[i].upper.uv.offset.y == after.sideDefs[i].upper.uv.offset.y,
                  description);
        }
    }
    if (before.sectors.size() == after.sectors.size()) {
        for (size_t i = 0; i < before.sectors.size(); ++i) {
            Check(before.sectors[i].id == after.sectors[i].id
                          && before.sectors[i].name == after.sectors[i].name,
                  description);
        }
    }
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

int CountLinesReferencingVertex(const SectorTopologyMap& map, int vertexId)
{
    int count = 0;
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == vertexId || lineDef.endVertexId == vertexId) {
            ++count;
        }
    }
    return count;
}

bool LoopContainsVertex(const SectorTopologyLoop& loop, int vertexId)
{
    return std::find(loop.vertexIds.begin(), loop.vertexIds.end(), vertexId) != loop.vertexIds.end();
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

const SectorTopologyLineDef* FindLineById(const SectorTopologyMap& map, int lineDefId)
{
    return game::FindSectorTopologyLineDef(map, lineDefId);
}

bool SameUv(game::SectorTopologyUvSettings a, game::SectorTopologyUvSettings b)
{
    return a.scale.x == b.scale.x
           && a.scale.y == b.scale.y
           && a.offset.x == b.offset.x
           && a.offset.y == b.offset.y;
}

bool SameWallPart(
        const game::SectorTopologyWallPartSettings& a,
        const game::SectorTopologyWallPartSettings& b)
{
    return a.textureId == b.textureId && SameUv(a.uv, b.uv);
}

bool SameSideDefSettings(const SectorTopologySideDef& a, const SectorTopologySideDef& b)
{
    return a.side == b.side
           && a.sectorId == b.sectorId
           && SameWallPart(a.wall, b.wall)
           && SameWallPart(a.lower, b.lower)
           && SameWallPart(a.upper, b.upper);
}

void SetDistinctSideSettings(SectorTopologySideDef& sideDef, const char* prefix)
{
    sideDef.wall.textureId = std::string{prefix} + "_wall";
    sideDef.wall.uv.scale = {2.0f, 3.0f};
    sideDef.wall.uv.offset = {4.0f, 5.0f};
    sideDef.lower.textureId = std::string{prefix} + "_lower";
    sideDef.lower.uv.scale = {6.0f, 7.0f};
    sideDef.lower.uv.offset = {8.0f, 9.0f};
    sideDef.upper.textureId = std::string{prefix} + "_upper";
    sideDef.upper.uv.scale = {10.0f, 11.0f};
    sideDef.upper.uv.offset = {12.0f, 13.0f};
}

int CountSideDefsForSector(const SectorTopologyMap& map, int sectorId)
{
    int count = 0;
    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        if (sideDef.sectorId == sectorId) {
            ++count;
        }
    }
    return count;
}

int CountOneSidedLinesForSector(const SectorTopologyMap& map, int sectorId)
{
    int count = 0;
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const SectorTopologySideDef* front = FindSideDef(map, lineDef.frontSideDefId);
        const SectorTopologySideDef* back = FindSideDef(map, lineDef.backSideDefId);
        if (front != nullptr && front->sectorId == sectorId && back == nullptr) {
            ++count;
        } else if (back != nullptr && back->sectorId == sectorId && front == nullptr) {
            ++count;
        }
    }
    return count;
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

void TestTopologyGeometryHelpers()
{
    Check(game::SectorTopologyPointOnSegment({32, 0}, {0, 0}, {64, 0}),
          "geometry helper accepts horizontal point");
    Check(game::SectorTopologyPointOnSegment({0, 32}, {0, 0}, {0, 64}),
          "geometry helper accepts vertical point");
    Check(game::SectorTopologyPointOnSegment({32, 32}, {0, 0}, {64, 64}),
          "geometry helper accepts diagonal point");
    Check(game::SectorTopologyPointStrictlyInsideSegment({32, 32}, {0, 0}, {64, 64}),
          "geometry helper accepts strict diagonal interior point");
    Check(!game::SectorTopologyPointStrictlyInsideSegment({0, 0}, {0, 0}, {64, 64}),
          "geometry helper strict inside rejects start endpoint");
    Check(!game::SectorTopologyPointStrictlyInsideSegment({64, 64}, {0, 0}, {64, 64}),
          "geometry helper strict inside rejects end endpoint");
    Check(!game::SectorTopologyPointOnSegment({96, 0}, {0, 0}, {64, 0}),
          "geometry helper rejects off-segment collinear point");
    Check(!game::SectorTopologyPointOnSegment({32, 16}, {0, 0}, {64, 0}),
          "geometry helper rejects non-collinear point");
}

void TestSplitOneSidedWallAtPoint()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId),
          "split at point one-sided square creation succeeds");
    const SectorTopologyLineDef originalLine = map.lineDefs.front();
    SectorTopologySideDef* originalSide = FindSideDef(map, originalLine.frontSideDefId);
    Check(originalSide != nullptr, "split at point one-sided finds original side");
    if (originalSide == nullptr) {
        return;
    }
    SetDistinctSideSettings(*originalSide, "point_outer");
    const SectorTopologySideDef expectedSide = *originalSide;
    const size_t vertexCount = map.vertices.size();
    const size_t lineCount = map.lineDefs.size();
    const size_t sideCount = map.sideDefs.size();

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDefAtPoint(map, originalLine.id, {16, 0}, &split, &error),
          "split at point one-sided wall succeeds");
    Check(error.empty(), "successful one-sided point split clears error");
    Check(split.newVertexId == split.midpointVertexId, "point split reports new vertex in midpoint-compatible field");
    Check(map.vertices.size() == vertexCount + 1, "point split one-sided adds vertex");
    Check(map.lineDefs.size() == lineCount + 1, "point split one-sided replaces one line with two");
    Check(map.sideDefs.size() == sideCount + 1, "point split one-sided replaces one sidedef with two");
    const game::SectorTopologyVertex* splitVertex = game::FindSectorTopologyVertex(map, split.newVertexId);
    Check(splitVertex != nullptr && splitVertex->x == 16 && splitVertex->y == 0,
          "point split one-sided creates vertex at requested point");
    Check(FindLineById(map, originalLine.id) == nullptr, "point split one-sided removes original line");
    Check(FindSideDef(map, expectedSide.id) == nullptr, "point split one-sided removes original sidedef");

    const SectorTopologyLineDef* first = FindLineById(map, split.firstLineDefId);
    const SectorTopologyLineDef* second = FindLineById(map, split.secondLineDefId);
    Check(first != nullptr && second != nullptr, "point split one-sided replacement lines exist");
    if (first != nullptr && second != nullptr) {
        Check(first->startVertexId == originalLine.startVertexId
                      && first->endVertexId == split.newVertexId,
              "point split one-sided first line is A to split point");
        Check(second->startVertexId == split.newVertexId
                      && second->endVertexId == originalLine.endVertexId,
              "point split one-sided second line is split point to B");
    }

    const SectorTopologySideDef* firstSide = FindSideDef(map, split.firstFrontSideDefId);
    const SectorTopologySideDef* secondSide = FindSideDef(map, split.secondFrontSideDefId);
    Check(firstSide != nullptr && secondSide != nullptr, "point split one-sided duplicated sidedefs exist");
    if (firstSide != nullptr && secondSide != nullptr) {
        Check(SameSideDefSettings(*firstSide, expectedSide),
              "point split one-sided first side preserves settings");
        Check(SameSideDefSettings(*secondSide, expectedSide),
              "point split one-sided second side preserves settings");
    }

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "point split one-sided validates and extracts");
    Check(LoopContainsVertex(loops.outer, split.newVertexId),
          "point split one-sided sector loop includes split vertex");
}

void TestSplitTwoSidedSharedLineAtPoint()
{
    SectorTopologyMap map;
    int leftId = -1;
    int rightId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &leftId),
          "point split shared first adjacent square succeeds");
    Check(Create(map, {{64, 0}, {128, 0}, {128, 64}, {64, 64}}, &rightId),
          "point split shared second adjacent square succeeds");
    const SectorTopologyLineDef* shared = FindLine(map, 2, 3);
    Check(shared != nullptr, "point split shared finds shared linedef");
    if (shared == nullptr) {
        return;
    }
    SectorTopologySideDef* front = FindSideDef(map, shared->frontSideDefId);
    SectorTopologySideDef* back = FindSideDef(map, shared->backSideDefId);
    Check(front != nullptr && back != nullptr, "point split shared finds front and back");
    if (front == nullptr || back == nullptr) {
        return;
    }
    SetDistinctSideSettings(*front, "point_front");
    SetDistinctSideSettings(*back, "point_back");
    const SectorTopologySideDef expectedFront = *front;
    const SectorTopologySideDef expectedBack = *back;
    const int sharedLineId = shared->id;

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDefAtPoint(map, sharedLineId, {64, 16}, &split, &error),
          "point split shared two-sided line succeeds");
    Check(error.empty(), "successful shared point split clears error");

    const SectorTopologySideDef* firstFront = FindSideDef(map, split.firstFrontSideDefId);
    const SectorTopologySideDef* secondFront = FindSideDef(map, split.secondFrontSideDefId);
    const SectorTopologySideDef* firstBack = FindSideDef(map, split.firstBackSideDefId);
    const SectorTopologySideDef* secondBack = FindSideDef(map, split.secondBackSideDefId);
    Check(firstFront != nullptr && secondFront != nullptr && firstBack != nullptr && secondBack != nullptr,
          "point split shared duplicated sidedefs exist");
    if (firstFront != nullptr && secondFront != nullptr && firstBack != nullptr && secondBack != nullptr) {
        Check(SameSideDefSettings(*firstFront, expectedFront)
                      && SameSideDefSettings(*secondFront, expectedFront),
              "point split shared preserves front settings on both halves");
        Check(SameSideDefSettings(*firstBack, expectedBack)
                      && SameSideDefSettings(*secondBack, expectedBack),
              "point split shared preserves back settings on both halves");
        Check(firstFront->sectorId == leftId
                      && secondFront->sectorId == leftId
                      && firstBack->sectorId == rightId
                      && secondBack->sectorId == rightId,
              "point split shared preserves front/back sector ownership");
    }
    Check(FindLineById(map, sharedLineId) == nullptr, "point split shared removes original line");

    SectorTopologyLoopSet leftLoops;
    SectorTopologyLoopSet rightLoops;
    Check(ValidateAndExtract(map, leftId, leftLoops), "point split shared left sector extracts");
    Check(ValidateAndExtract(map, rightId, rightLoops), "point split shared right sector extracts");
    Check(LoopContainsVertex(leftLoops.outer, split.newVertexId)
                  && LoopContainsVertex(rightLoops.outer, split.newVertexId),
          "point split shared vertex belongs to both sector loops");
}

void TestSplitDiagonalLineAtPoint()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 64}, {0, 64}}, &sectorId),
          "diagonal point split triangle creation succeeds");
    const SectorTopologyLineDef originalLine = map.lineDefs.front();

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDefAtPoint(map, originalLine.id, {32, 32}, &split, &error),
          "diagonal point split succeeds");
    const game::SectorTopologyVertex* splitVertex = game::FindSectorTopologyVertex(map, split.newVertexId);
    Check(splitVertex != nullptr && splitVertex->x == 32 && splitVertex->y == 32,
          "diagonal point split creates exact integer vertex");

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "diagonal point split validates and extracts");
}

void TestRejectInvalidSplitAtPoint()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "invalid point split square creation succeeds");
    const int lineId = map.lineDefs.front().id;

    {
        SectorTopologyMap candidate = map;
        const SectorTopologyMap before = candidate;
        std::string error;
        Check(!game::SplitSectorTopologyLineDefAtPoint(candidate, 999999, {16, 0}, nullptr, &error),
              "point split rejects missing line ID");
        Check(!error.empty(), "missing line point split reports error");
        CheckUnchanged(before, candidate, "missing line point split leaves map unchanged");
    }
    {
        SectorTopologyMap candidate = map;
        const SectorTopologyMap before = candidate;
        std::string error;
        Check(!game::SplitSectorTopologyLineDefAtPoint(candidate, lineId, {16, 8}, nullptr, &error),
              "point split rejects point not on segment");
        Check(!error.empty(), "off-line point split reports error");
        CheckUnchanged(before, candidate, "off-line point split leaves map unchanged");
    }
    {
        SectorTopologyMap candidate = map;
        const SectorTopologyMap before = candidate;
        std::string error;
        Check(!game::SplitSectorTopologyLineDefAtPoint(candidate, lineId, {0, 0}, nullptr, &error),
              "point split rejects endpoint");
        Check(!error.empty(), "endpoint point split reports error");
        CheckUnchanged(before, candidate, "endpoint point split leaves map unchanged");
    }
    {
        SectorTopologyMap candidate = map;
        candidate.vertices.push_back(game::SectorTopologyVertex{999, 32, 0});
        const SectorTopologyMap before = candidate;
        std::string error;
        Check(!game::SplitSectorTopologyLineDefAtPoint(candidate, lineId, {32, 0}, nullptr, &error),
              "point split rejects existing vertex coordinate");
        Check(!error.empty(), "existing vertex point split reports error");
        CheckUnchanged(before, candidate, "existing vertex point split leaves map unchanged");
    }
    {
        SectorTopologyMap candidate = map;
        Check(!candidate.sideDefs.empty(), "transaction failure setup has sideDefs");
        if (candidate.sideDefs.empty()) {
            return;
        }
        candidate.sideDefs.back().sectorId = 999999;
        const SectorTopologyMap before = candidate;
        std::string error;
        Check(!game::SplitSectorTopologyLineDefAtPoint(candidate, lineId, {16, 0}, nullptr, &error),
              "point split transactional validation failure rejects");
        Check(!error.empty(), "transactional point split failure reports error");
        CheckUnchanged(before, candidate, "transactional point split failure leaves map unchanged");
    }
}

void TestSplitAtPointGeneratedGeometryBuilds()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "point split geometry square creation succeeds");

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDefAtPoint(map, map.lineDefs.front().id, {16, 0}, &split, &error),
          "point split geometry line succeeds");

    game::SectorGeneratedGeometry geometry;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error),
          "generated geometry builds after point split");
}

void TestSplitOneSidedWall()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId),
          "split one-sided square creation succeeds");
    const SectorTopologyLineDef originalLine = map.lineDefs.front();
    SectorTopologySideDef* originalSide = FindSideDef(map, originalLine.frontSideDefId);
    Check(originalSide != nullptr, "split one-sided finds original side");
    if (originalSide == nullptr) {
        return;
    }
    SetDistinctSideSettings(*originalSide, "outer");
    const SectorTopologySideDef expectedSide = *originalSide;
    const size_t vertexCount = map.vertices.size();
    const size_t lineCount = map.lineDefs.size();
    const size_t sideCount = map.sideDefs.size();

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDef(map, originalLine.id, &split, &error),
          "split one-sided wall succeeds");
    Check(error.empty(), "successful one-sided split clears error");
    Check(map.vertices.size() == vertexCount + 1, "split one-sided adds midpoint vertex");
    Check(map.lineDefs.size() == lineCount + 1, "split one-sided replaces one line with two");
    Check(map.sideDefs.size() == sideCount + 1, "split one-sided replaces one sidedef with two");
    Check(game::FindSectorTopologyVertex(map, split.midpointVertexId) != nullptr,
          "split one-sided midpoint exists");
    Check(FindLineById(map, originalLine.id) == nullptr, "split one-sided removes original line");
    Check(FindSideDef(map, expectedSide.id) == nullptr, "split one-sided removes original sidedef");

    const SectorTopologyLineDef* first = FindLineById(map, split.firstLineDefId);
    const SectorTopologyLineDef* second = FindLineById(map, split.secondLineDefId);
    Check(first != nullptr && second != nullptr, "split one-sided replacement lines exist");
    if (first != nullptr && second != nullptr) {
        Check(first->startVertexId == originalLine.startVertexId
                      && first->endVertexId == split.midpointVertexId,
              "split one-sided first line is A to midpoint");
        Check(second->startVertexId == split.midpointVertexId
                      && second->endVertexId == originalLine.endVertexId,
              "split one-sided second line is midpoint to B");
        Check(first->frontSideDefId == split.firstFrontSideDefId
                      && first->backSideDefId == -1
                      && second->frontSideDefId == split.secondFrontSideDefId
                      && second->backSideDefId == -1,
              "split one-sided replacements remain one-sided front lines");
    }

    const SectorTopologySideDef* firstSide = FindSideDef(map, split.firstFrontSideDefId);
    const SectorTopologySideDef* secondSide = FindSideDef(map, split.secondFrontSideDefId);
    Check(firstSide != nullptr && secondSide != nullptr, "split one-sided duplicated sidedefs exist");
    if (firstSide != nullptr && secondSide != nullptr) {
        Check(SameSideDefSettings(*firstSide, expectedSide),
              "split one-sided first side preserves settings");
        Check(SameSideDefSettings(*secondSide, expectedSide),
              "split one-sided second side preserves settings");
        Check(firstSide->side == SectorTopologySideKind::Front
                      && secondSide->side == SectorTopologySideKind::Front,
              "split one-sided preserves front side kind");
    }

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "split one-sided validates and extracts");
    Check(LoopContainsVertex(loops.outer, split.midpointVertexId),
          "split one-sided sector loop includes midpoint");
}

void TestDeleteStandaloneSector()
{
    SectorTopologyMap map;
    map.texturesById["floor"] = game::SectorTextureDefinition{"floor", "floor.png"};
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId),
          "delete standalone square creation succeeds");

    SectorTopologyDeleteSectorResult result;
    std::string error;
    Check(game::DeleteSectorTopologySector(map, sectorId, &result, &error),
          "delete standalone sector succeeds");
    Check(error.empty(), "successful standalone delete clears error");
    Check(result.deletedSectorId == sectorId, "standalone delete reports deleted sector");
    Check(result.removedSideDefCount == 4, "standalone delete removes four sidedefs");
    Check(result.removedLineDefCount == 4, "standalone delete removes four linedefs");
    Check(result.removedVertexCount == 4, "standalone delete removes four vertices");
    Check(map.sectors.empty(), "standalone delete leaves no sectors");
    Check(map.sideDefs.empty(), "standalone delete leaves no sidedefs");
    Check(map.lineDefs.empty(), "standalone delete leaves no linedefs");
    Check(map.vertices.empty(), "standalone delete leaves no vertices");
    Check(map.texturesById.find("floor") != map.texturesById.end(),
          "standalone delete preserves textures");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(map)),
          "empty map validates after standalone delete");
}

void TestDeleteAdjacentSector()
{
    SectorTopologyMap map;
    int leftId = -1;
    int rightId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &leftId),
          "delete adjacent first square succeeds");
    Check(Create(map, {{64, 0}, {128, 0}, {128, 64}, {64, 64}}, &rightId),
          "delete adjacent second square succeeds");

    const SectorTopologyLineDef* shared = FindLine(map, 2, 3);
    Check(shared != nullptr, "delete adjacent finds shared line");
    if (shared == nullptr) {
        return;
    }
    SectorTopologySideDef* survivingSide = FindSideDef(map, shared->backSideDefId);
    Check(survivingSide != nullptr && survivingSide->sectorId == rightId,
          "delete adjacent finds surviving shared side");
    if (survivingSide == nullptr) {
        return;
    }
    SetDistinctSideSettings(*survivingSide, "survivor");
    const SectorTopologySideDef expectedSurvivor = *survivingSide;
    const int sharedLineId = shared->id;

    SectorTopologyDeleteSectorResult result;
    std::string error;
    Check(game::DeleteSectorTopologySector(map, leftId, &result, &error),
          "delete adjacent sector succeeds");
    Check(error.empty(), "successful adjacent delete clears error");
    Check(map.sectors.size() == 1
                  && game::FindSectorTopologySector(map, rightId) != nullptr
                  && game::FindSectorTopologySector(map, leftId) == nullptr,
          "delete adjacent leaves only surviving sector");
    Check(result.removedSideDefCount == 4, "delete adjacent removes deleted sector sidedefs");
    Check(result.removedLineDefCount == 3, "delete adjacent removes only deleted outer linedefs");
    Check(result.removedVertexCount == 2, "delete adjacent removes unreferenced deleted outer vertices");

    const SectorTopologyLineDef* remainingShared = FindLineById(map, sharedLineId);
    Check(remainingShared != nullptr, "delete adjacent keeps shared linedef");
    if (remainingShared != nullptr) {
        Check(remainingShared->frontSideDefId == -1
                      && remainingShared->backSideDefId == expectedSurvivor.id,
              "delete adjacent shared line becomes one-sided for survivor");
    }
    const SectorTopologySideDef* actualSurvivor = FindSideDef(map, expectedSurvivor.id);
    Check(actualSurvivor != nullptr && SameSideDefSettings(*actualSurvivor, expectedSurvivor),
          "delete adjacent preserves surviving sidedef settings");
    Check(CountSideDefsForSector(map, leftId) == 0, "delete adjacent removes deleted sector sidedefs");

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, rightId, loops), "delete adjacent survivor validates and extracts");
}

void TestDeleteInsertedChildSector()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {160, 0}, {160, 160}, {0, 160}}, &parentId),
          "delete child parent creation succeeds");
    Check(Insert(map, parentId, {{48, 48}, {112, 48}, {112, 112}, {48, 112}}, &childId),
          "delete child insert succeeds");

    std::vector<int> boundaryLineIds;
    for (const SectorTopologyLineDef* lineDef : InsertedBoundaryLines(map, parentId, childId)) {
        boundaryLineIds.push_back(lineDef->id);
    }
    Check(boundaryLineIds.size() == 4, "delete child finds inserted boundary");

    SectorTopologyDeleteSectorResult result;
    std::string error;
    Check(game::DeleteSectorTopologySector(map, childId, &result, &error),
          "delete inserted child succeeds");
    Check(error.empty(), "successful child delete clears error");
    Check(game::FindSectorTopologySector(map, parentId) != nullptr
                  && game::FindSectorTopologySector(map, childId) == nullptr,
          "delete child leaves parent and removes child");
    Check(CountSideDefsForSector(map, childId) == 0, "delete child removes child sidedefs");
    for (int lineId : boundaryLineIds) {
        const SectorTopologyLineDef* lineDef = FindLineById(map, lineId);
        Check(lineDef != nullptr, "delete child keeps parent former-hole boundary line");
        if (lineDef != nullptr) {
            const SectorTopologySideDef* front = FindSideDef(map, lineDef->frontSideDefId);
            const SectorTopologySideDef* back = FindSideDef(map, lineDef->backSideDefId);
            Check(front == nullptr && back != nullptr && back->sectorId == parentId,
                  "delete child former-hole boundary is parent one-sided back side");
        }
    }

    SectorTopologyLoopSet parentLoops;
    Check(ValidateAndExtract(map, parentId, parentLoops), "delete child parent validates and extracts");
    Check(parentLoops.holes.size() == 1, "delete child leaves parent former hole boundary");
    Check(CountOneSidedLinesForSector(map, parentId) == 8,
          "delete child leaves parent outer and former-hole boundaries one-sided");
}

void TestDeleteParentWithChildSector()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {160, 0}, {160, 160}, {0, 160}}, &parentId),
          "delete parent parent creation succeeds");
    Check(Insert(map, parentId, {{48, 48}, {112, 48}, {112, 112}, {48, 112}}, &childId),
          "delete parent child insert succeeds");

    SectorTopologyDeleteSectorResult result;
    std::string error;
    Check(game::DeleteSectorTopologySector(map, parentId, &result, &error),
          "delete parent with child succeeds");
    Check(error.empty(), "successful parent delete clears error");
    Check(game::FindSectorTopologySector(map, parentId) == nullptr
                  && game::FindSectorTopologySector(map, childId) != nullptr,
          "delete parent removes parent and leaves child");
    Check(CountSideDefsForSector(map, parentId) == 0, "delete parent removes parent sidedefs");
    Check(CountOneSidedLinesForSector(map, childId) == 4,
          "delete parent leaves child boundary one-sided");
    Check(map.vertices.size() == 4 && map.lineDefs.size() == 4 && map.sideDefs.size() == 4,
          "delete parent removes orphaned parent outer topology");

    SectorTopologyLoopSet childLoops;
    Check(ValidateAndExtract(map, childId, childLoops), "delete parent child validates and extracts");
    Check(childLoops.holes.empty(), "delete parent child has no holes");
}

void TestDeleteFailureIsTransactional()
{
    SectorTopologyMap map;
    int deletedId = -1;
    int brokenId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &deletedId),
          "delete transaction first sector creation succeeds");
    Check(Create(map, {{96, 0}, {160, 0}, {160, 64}, {96, 64}}, &brokenId),
          "delete transaction second sector creation succeeds");
    Check(!map.sideDefs.empty(), "delete transaction has sidedefs to corrupt");
    if (map.sideDefs.empty()) {
        return;
    }
    map.sideDefs.back().sectorId = 999999;
    const SectorTopologyMap before = map;

    SectorTopologyDeleteSectorResult result;
    std::string error;
    Check(!game::DeleteSectorTopologySector(map, deletedId, &result, &error),
          "delete transaction fails when candidate validation fails");
    Check(!error.empty(), "failed delete reports validation error");
    Check(result.deletedSectorId == -1
                  && result.removedSideDefCount == 0
                  && result.removedLineDefCount == 0
                  && result.removedVertexCount == 0,
          "failed delete leaves result default");
    CheckUnchanged(before, map, "failed delete leaves map unchanged");
}

void TestSplitTwoSidedSharedLine()
{
    SectorTopologyMap map;
    int leftId = -1;
    int rightId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &leftId),
          "split shared first adjacent square succeeds");
    Check(Create(map, {{64, 0}, {128, 0}, {128, 64}, {64, 64}}, &rightId),
          "split shared second adjacent square succeeds");
    const SectorTopologyLineDef* shared = FindLine(map, 2, 3);
    Check(shared != nullptr, "split shared finds shared linedef");
    if (shared == nullptr) {
        return;
    }
    SectorTopologySideDef* front = FindSideDef(map, shared->frontSideDefId);
    SectorTopologySideDef* back = FindSideDef(map, shared->backSideDefId);
    Check(front != nullptr && back != nullptr, "split shared finds front and back");
    if (front == nullptr || back == nullptr) {
        return;
    }
    SetDistinctSideSettings(*front, "front");
    SetDistinctSideSettings(*back, "back");
    const SectorTopologySideDef expectedFront = *front;
    const SectorTopologySideDef expectedBack = *back;
    const int sharedLineId = shared->id;

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDef(map, sharedLineId, &split, &error),
          "split shared two-sided line succeeds");
    Check(error.empty(), "successful shared split clears error");

    const SectorTopologyLineDef* first = FindLineById(map, split.firstLineDefId);
    const SectorTopologyLineDef* second = FindLineById(map, split.secondLineDefId);
    Check(first != nullptr && second != nullptr, "split shared replacement lines exist");
    if (first != nullptr && second != nullptr) {
        Check(first->frontSideDefId == split.firstFrontSideDefId
                      && first->backSideDefId == split.firstBackSideDefId
                      && second->frontSideDefId == split.secondFrontSideDefId
                      && second->backSideDefId == split.secondBackSideDefId,
              "split shared replacement lines are two-sided");
    }

    const SectorTopologySideDef* firstFront = FindSideDef(map, split.firstFrontSideDefId);
    const SectorTopologySideDef* secondFront = FindSideDef(map, split.secondFrontSideDefId);
    const SectorTopologySideDef* firstBack = FindSideDef(map, split.firstBackSideDefId);
    const SectorTopologySideDef* secondBack = FindSideDef(map, split.secondBackSideDefId);
    Check(firstFront != nullptr && secondFront != nullptr && firstBack != nullptr && secondBack != nullptr,
          "split shared duplicated sidedefs exist");
    if (firstFront != nullptr && secondFront != nullptr && firstBack != nullptr && secondBack != nullptr) {
        Check(SameSideDefSettings(*firstFront, expectedFront)
                      && SameSideDefSettings(*secondFront, expectedFront),
              "split shared preserves front settings on both halves");
        Check(SameSideDefSettings(*firstBack, expectedBack)
                      && SameSideDefSettings(*secondBack, expectedBack),
              "split shared preserves back settings on both halves");
        Check(firstFront->side == SectorTopologySideKind::Front
                      && secondFront->side == SectorTopologySideKind::Front
                      && firstBack->side == SectorTopologySideKind::Back
                      && secondBack->side == SectorTopologySideKind::Back,
              "split shared preserves original side kinds");
    }
    Check(FindLineById(map, sharedLineId) == nullptr, "split shared removes original line");
    Check(FindSideDef(map, expectedFront.id) == nullptr, "split shared removes original front sidedef");
    Check(FindSideDef(map, expectedBack.id) == nullptr, "split shared removes original back sidedef");

    SectorTopologyLoopSet leftLoops;
    SectorTopologyLoopSet rightLoops;
    Check(ValidateAndExtract(map, leftId, leftLoops), "split shared left sector extracts");
    Check(ValidateAndExtract(map, rightId, rightLoops), "split shared right sector extracts");
    Check(LoopContainsVertex(leftLoops.outer, split.midpointVertexId)
                  && LoopContainsVertex(rightLoops.outer, split.midpointVertexId),
          "split shared midpoint belongs to both sector loops");
}

void TestSplitInsertedPlatformBoundary()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {160, 0}, {160, 160}, {0, 160}}, &parentId),
          "split inserted parent creation succeeds");
    Check(Insert(map, parentId, {{48, 48}, {112, 48}, {112, 112}, {48, 112}}, &childId),
          "split inserted child succeeds");
    const std::vector<const SectorTopologyLineDef*> boundary =
            InsertedBoundaryLines(map, parentId, childId);
    Check(!boundary.empty(), "split inserted finds boundary line");
    if (boundary.empty()) {
        return;
    }
    const int lineId = boundary.front()->id;

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDef(map, lineId, &split, &error),
          "split inserted boundary succeeds");
    Check(error.empty(), "successful inserted split clears error");

    const SectorTopologySideDef* firstFront = FindSideDef(map, split.firstFrontSideDefId);
    const SectorTopologySideDef* firstBack = FindSideDef(map, split.firstBackSideDefId);
    const SectorTopologySideDef* secondFront = FindSideDef(map, split.secondFrontSideDefId);
    const SectorTopologySideDef* secondBack = FindSideDef(map, split.secondBackSideDefId);
    Check(firstFront != nullptr && firstBack != nullptr && secondFront != nullptr && secondBack != nullptr,
          "split inserted duplicated sidedefs exist");
    if (firstFront != nullptr && firstBack != nullptr && secondFront != nullptr && secondBack != nullptr) {
        Check(firstFront->sectorId == childId
                      && secondFront->sectorId == childId
                      && firstFront->side == SectorTopologySideKind::Front
                      && secondFront->side == SectorTopologySideKind::Front,
              "split inserted child remains front side");
        Check(firstBack->sectorId == parentId
                      && secondBack->sectorId == parentId
                      && firstBack->side == SectorTopologySideKind::Back
                      && secondBack->side == SectorTopologySideKind::Back,
              "split inserted parent remains back side");
    }

    SectorTopologyLoopSet parentLoops;
    SectorTopologyLoopSet childLoops;
    Check(ValidateAndExtract(map, parentId, parentLoops), "split inserted parent extracts");
    Check(ValidateAndExtract(map, childId, childLoops), "split inserted child extracts");
    Check(parentLoops.holes.size() == 1, "split inserted parent still has one hole");
    Check(childLoops.holes.empty(), "split inserted child still has one outer loop only");
    Check(LoopContainsVertex(parentLoops.holes.front(), split.midpointVertexId),
          "split inserted parent hole includes midpoint");
    Check(LoopContainsVertex(childLoops.outer, split.midpointVertexId),
          "split inserted child outer includes midpoint");
}

void TestRejectNonRepresentableSplitMidpoint()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {1, 0}, {1, 16}, {0, 16}}),
          "fractional midpoint square creation succeeds");
    const SectorTopologyMap before = map;

    std::string error;
    Check(!game::SplitSectorTopologyLineDef(map, map.lineDefs.front().id, nullptr, &error),
          "fractional midpoint split fails");
    Check(error.find("integer coordinate grid") != std::string::npos,
          "fractional midpoint split reports coordinate grid error");
    CheckUnchanged(before, map, "fractional midpoint split leaves map unchanged");
}

void TestSplitGeneratedGeometryBuilds()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "split geometry square creation succeeds");

    SectorTopologySplitLineResult split;
    std::string error;
    Check(game::SplitSectorTopologyLineDef(map, map.lineDefs.front().id, &split, &error),
          "split geometry line succeeds");

    game::SectorGeneratedGeometry geometry;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error),
          "generated geometry builds after split");
}

void TestMoveOuterVertex()
{
    SectorTopologyMap map;
    int sectorId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &sectorId),
          "move outer vertex square creation succeeds");
    const size_t vertexCount = map.vertices.size();
    const int movedVertexId = 3;
    const int connectedBefore = CountLinesReferencingVertex(map, movedVertexId);

    std::string error;
    Check(game::MoveSectorTopologyVertex(map, movedVertexId, {80, 64}, &error),
          "move outer vertex succeeds");
    Check(error.empty(), "successful outer vertex move clears error");
    Check(map.vertices.size() == vertexCount, "move outer vertex creates no vertex");
    const game::SectorTopologyVertex* moved = game::FindSectorTopologyVertex(map, movedVertexId);
    Check(moved != nullptr && moved->x == 80 && moved->y == 64,
          "moved outer vertex keeps stable ID with new coordinate");
    Check(CountLinesReferencingVertex(map, movedVertexId) == connectedBefore,
          "moved outer vertex connected linedefs keep stable vertex ID");

    SectorTopologyLoopSet loops;
    Check(ValidateAndExtract(map, sectorId, loops), "moved outer vertex validates and extracts");
}

void TestMoveSharedAdjacentVertex()
{
    SectorTopologyMap map;
    int leftId = -1;
    int rightId = -1;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}, &leftId),
          "move shared first adjacent square succeeds");
    Check(Create(map, {{64, 0}, {128, 0}, {128, 64}, {64, 64}}, &rightId),
          "move shared second adjacent square succeeds");
    const size_t vertexCount = map.vertices.size();
    const int movedVertexId = 3;
    const int connectedBefore = CountLinesReferencingVertex(map, movedVertexId);

    std::string error;
    Check(game::MoveSectorTopologyVertex(map, movedVertexId, {64, 80}, &error),
          "move shared adjacent vertex succeeds");
    Check(map.vertices.size() == vertexCount, "move shared adjacent vertex creates no vertex");
    Check(CountLinesReferencingVertex(map, movedVertexId) == connectedBefore,
          "move shared adjacent vertex preserves connected linedef references");

    SectorTopologyLoopSet leftLoops;
    SectorTopologyLoopSet rightLoops;
    Check(ValidateAndExtract(map, leftId, leftLoops), "moved shared left sector extracts");
    Check(ValidateAndExtract(map, rightId, rightLoops), "moved shared right sector extracts");
    Check(LoopContainsVertex(leftLoops.outer, movedVertexId),
          "moved shared vertex remains in left sector loop");
    Check(LoopContainsVertex(rightLoops.outer, movedVertexId),
          "moved shared vertex remains in right sector loop");
}

void TestMoveInsertedPlatformVertex()
{
    SectorTopologyMap map;
    int parentId = -1;
    int childId = -1;
    Check(Create(map, {{0, 0}, {160, 0}, {160, 160}, {0, 160}}, &parentId),
          "move platform parent creation succeeds");
    Check(Insert(map, parentId, {{48, 48}, {112, 48}, {112, 112}, {48, 112}}, &childId),
          "move platform insert succeeds");

    const std::vector<const SectorTopologyLineDef*> boundary =
            InsertedBoundaryLines(map, parentId, childId);
    Check(boundary.size() == 4, "move platform finds child parent boundary");
    if (boundary.empty()) {
        return;
    }
    const int movedVertexId = boundary.front()->startVertexId;
    const size_t vertexCount = map.vertices.size();
    const int connectedBefore = CountLinesReferencingVertex(map, movedVertexId);

    std::string error;
    Check(game::MoveSectorTopologyVertex(map, movedVertexId, {40, 48}, &error),
          "move inserted platform vertex succeeds");
    Check(map.vertices.size() == vertexCount, "move platform vertex creates no vertex");
    Check(CountLinesReferencingVertex(map, movedVertexId) == connectedBefore,
          "move platform vertex preserves connected linedef references");

    SectorTopologyLoopSet parentLoops;
    SectorTopologyLoopSet childLoops;
    Check(ValidateAndExtract(map, parentId, parentLoops), "moved platform parent extracts");
    Check(ValidateAndExtract(map, childId, childLoops), "moved platform child extracts");
    Check(parentLoops.holes.size() == 1, "moved platform parent still has one hole");
    Check(LoopContainsVertex(parentLoops.holes.front(), movedVertexId),
          "moved platform vertex remains in parent hole loop");
    Check(LoopContainsVertex(childLoops.outer, movedVertexId),
          "moved platform vertex remains in child outer loop");
}

void TestRejectMoveOntoExistingVertex()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "move onto existing square creation succeeds");
    const SectorTopologyMap before = map;

    std::string error;
    Check(!game::MoveSectorTopologyVertex(map, 3, {64, 0}, &error),
          "move onto existing vertex fails");
    Check(error.find("existing vertex") != std::string::npos,
          "move onto existing vertex reports merge limitation");
    CheckUnchanged(before, map, "move onto existing vertex leaves map unchanged");
}

void TestRejectInvalidMovedLoop()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "invalid moved loop square creation succeeds");
    const SectorTopologyMap before = map;

    std::string error;
    Check(!game::MoveSectorTopologyVertex(map, 3, {32, 0}, &error),
          "move creating invalid loop fails");
    Check(!error.empty(), "move creating invalid loop reports error");
    CheckUnchanged(before, map, "move creating invalid loop leaves map unchanged");
}

void TestRejectMovedCrossing()
{
    SectorTopologyMap map;
    Check(Create(map, {{0, 0}, {64, 0}, {64, 64}, {0, 64}}),
          "crossing move first square creation succeeds");
    Check(Create(map, {{96, 0}, {160, 0}, {160, 64}, {96, 64}}),
          "crossing move second square creation succeeds");
    const SectorTopologyMap before = map;

    std::string error;
    Check(!game::MoveSectorTopologyVertex(map, 3, {128, 32}, &error),
          "move creating crossing linedef fails");
    Check(!error.empty(), "move creating crossing linedef reports error");
    CheckUnchanged(before, map, "move creating crossing linedef leaves map unchanged");
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
    TestTopologyGeometryHelpers();
    TestSplitOneSidedWallAtPoint();
    TestSplitTwoSidedSharedLineAtPoint();
    TestSplitDiagonalLineAtPoint();
    TestRejectInvalidSplitAtPoint();
    TestSplitAtPointGeneratedGeometryBuilds();
    TestDeleteStandaloneSector();
    TestDeleteAdjacentSector();
    TestDeleteInsertedChildSector();
    TestDeleteParentWithChildSector();
    TestDeleteFailureIsTransactional();
    TestSplitOneSidedWall();
    TestSplitTwoSidedSharedLine();
    TestSplitInsertedPlatformBoundary();
    TestRejectNonRepresentableSplitMidpoint();
    TestSplitGeneratedGeometryBuilds();
    TestMoveOuterVertex();
    TestMoveSharedAdjacentVertex();
    TestMoveInsertedPlatformVertex();
    TestRejectMoveOntoExistingVertex();
    TestRejectInvalidMovedLoop();
    TestRejectMovedCrossing();
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
