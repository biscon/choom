#include "sector_demo/SectorAuthoringGraph.h"

#include <algorithm>
#include <iostream>
#include <string>
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

bool HasIssueFor(
        const std::vector<game::SectorAuthoringValidationIssue>& issues,
        game::SectorAuthoringObjectKind objectKind,
        int objectId)
{
    for (const game::SectorAuthoringValidationIssue& issue : issues) {
        if (issue.objectKind == objectKind && issue.objectId == objectId) {
            return true;
        }
    }
    return false;
}

bool HasIssueContaining(
        const std::vector<game::SectorAuthoringValidationIssue>& issues,
        game::SectorAuthoringObjectKind objectKind,
        int objectId,
        const std::string& messageText)
{
    for (const game::SectorAuthoringValidationIssue& issue : issues) {
        if (issue.objectKind == objectKind && issue.objectId == objectId
                && issue.message.find(messageText) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool HasPlanarDiagnostic(
        const std::vector<game::SectorAuthoringPlanarDiagnostic>& diagnostics,
        game::SectorAuthoringPlanarDiagnosticKind kind)
{
    for (const game::SectorAuthoringPlanarDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

void AddAuthoringVertexWithId(game::SectorAuthoringGraph& graph, int id, game::SectorCoord x, game::SectorCoord y)
{
    graph.vertices.push_back(game::SectorAuthoringVertex{id, x, y});
}

void AddAuthoringLineWithId(game::SectorAuthoringGraph& graph, int id, int startVertexId, int endVertexId)
{
    game::SectorAuthoringLine line;
    line.id = id;
    line.startVertexId = startVertexId;
    line.endVertexId = endVertexId;
    graph.lines.push_back(line);
}

game::SectorAuthoringGraph MakeGraphFromLines(
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& endpoints)
{
    game::SectorAuthoringGraph graph;
    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        AddAuthoringVertexWithId(
                graph,
                static_cast<int>(index) + 1,
                endpoints[index].first,
                endpoints[index].second);
    }
    for (std::size_t index = 0; index + 1 < endpoints.size(); index += 2) {
        AddAuthoringLineWithId(
                graph,
                10 + static_cast<int>(index / 2),
                static_cast<int>(index) + 1,
                static_cast<int>(index) + 2);
    }
    return graph;
}

bool PointEqualsInteger(
        const game::SectorAuthoringPlanarPoint& point,
        game::SectorCoord x,
        game::SectorCoord y)
{
    return game::SectorAuthoringPlanarRationalsEqual(
                   point.x,
                   game::SectorAuthoringPlanarRational{static_cast<int64_t>(x), 1})
            && game::SectorAuthoringPlanarRationalsEqual(
                   point.y,
                   game::SectorAuthoringPlanarRational{static_cast<int64_t>(y), 1});
}

const game::SectorAuthoringPlanarVertex* FindPlanarVertexAt(
        const game::SectorAuthoringPlanarizationResult& result,
        game::SectorCoord x,
        game::SectorCoord y)
{
    for (const game::SectorAuthoringPlanarVertex& vertex : result.vertices) {
        if (PointEqualsInteger(vertex.point, x, y)) {
            return &vertex;
        }
    }
    return nullptr;
}

int CountPlanarEdgesForLine(const game::SectorAuthoringPlanarizationResult& result, int lineId)
{
    int count = 0;
    for (const game::SectorAuthoringPlanarEdge& edge : result.edges) {
        if (edge.sourceLineId == lineId) {
            ++count;
        }
    }
    return count;
}

game::SectorTopologyWallPartSettings WallPart(
        const std::string& textureId,
        float scaleX,
        float scaleY,
        float offsetX,
        float offsetY)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    part.uv.scale = Vector2{scaleX, scaleY};
    part.uv.offset = Vector2{offsetX, offsetY};
    return part;
}

game::SectorTopologySideDef SideDef(
        int id,
        int lineDefId,
        game::SectorTopologySideKind side,
        int sectorId,
        const std::string& prefix)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = id;
    sideDef.lineDefId = lineDefId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = WallPart(prefix + "_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    sideDef.lower = WallPart(prefix + "_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    sideDef.upper = WallPart(prefix + "_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    sideDef.middle = WallPart(prefix + "_middle", 4.0f, 5.0f, 6.0f, 7.0f);
    sideDef.middle.decal.textureId = prefix + "_middle_decal";
    sideDef.middle.decal.opacity = 0.5f;
    return sideDef;
}

game::SectorTopologySector Sector(int id, const std::string& name)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.name = name;
    sector.floorZ = -2.0f;
    sector.ceilingZ = 40.0f;
    sector.floorTextureId = name + "_floor";
    sector.ceilingTextureId = name + "_ceiling";
    sector.ceilingSky = true;
    sector.floorUv.scale = Vector2{2.0f, 3.0f};
    sector.floorUv.offset = Vector2{4.0f, 5.0f};
    sector.ceilingUv.scale = Vector2{6.0f, 7.0f};
    sector.ceilingUv.offset = Vector2{8.0f, 9.0f};
    sector.floorDecal.textureId = name + "_floor_decal";
    sector.ceilingDecal.textureId = name + "_ceiling_decal";
    sector.ambientColor = Color{8, 16, 32, 255};
    sector.ambientIntensity = 0.35f;
    sector.defaultWall = WallPart(name + "_default_wall", 1.0f, 1.5f, 2.0f, 2.5f);
    sector.defaultLower = WallPart(name + "_default_lower", 2.0f, 2.5f, 3.0f, 3.5f);
    sector.defaultUpper = WallPart(name + "_default_upper", 3.0f, 3.5f, 4.0f, 4.5f);
    return sector;
}

game::SectorTopologyLineDef LineDef(
        int id,
        int startVertexId,
        int endVertexId,
        int frontSideDefId,
        int backSideDefId,
        bool blocksPlayer = false)
{
    game::SectorTopologyLineDef lineDef;
    lineDef.id = id;
    lineDef.startVertexId = startVertexId;
    lineDef.endVertexId = endVertexId;
    lineDef.frontSideDefId = frontSideDefId;
    lineDef.backSideDefId = backSideDefId;
    lineDef.flags.blocksPlayer = blocksPlayer;
    return lineDef;
}

game::SectorTopologyMap MakeSingleSectorSquareMap()
{
    game::SectorTopologyMap map;
    map.vertices.push_back(game::SectorTopologyVertex{1, 0, 0});
    map.vertices.push_back(game::SectorTopologyVertex{2, 64, 0});
    map.vertices.push_back(game::SectorTopologyVertex{3, 64, 64});
    map.vertices.push_back(game::SectorTopologyVertex{4, 0, 64});

    map.lineDefs.push_back(LineDef(10, 1, 2, 100, -1, true));
    map.lineDefs.push_back(LineDef(11, 2, 3, 101, -1));
    map.lineDefs.push_back(LineDef(12, 3, 4, 102, -1));
    map.lineDefs.push_back(LineDef(13, 4, 1, 103, -1));

    map.sideDefs.push_back(SideDef(100, 10, game::SectorTopologySideKind::Front, 200, "south"));
    map.sideDefs.push_back(SideDef(101, 11, game::SectorTopologySideKind::Front, 200, "east"));
    map.sideDefs.push_back(SideDef(102, 12, game::SectorTopologySideKind::Front, 200, "north"));
    map.sideDefs.push_back(SideDef(103, 13, game::SectorTopologySideKind::Front, 200, "west"));
    map.sectors.push_back(Sector(200, "room"));
    return map;
}

game::SectorTopologyMap MakeAdjacentSectorMap()
{
    game::SectorTopologyMap map;
    map.vertices.push_back(game::SectorTopologyVertex{1, 0, 0});
    map.vertices.push_back(game::SectorTopologyVertex{2, 64, 0});
    map.vertices.push_back(game::SectorTopologyVertex{3, 128, 0});
    map.vertices.push_back(game::SectorTopologyVertex{4, 128, 64});
    map.vertices.push_back(game::SectorTopologyVertex{5, 64, 64});
    map.vertices.push_back(game::SectorTopologyVertex{6, 0, 64});

    map.lineDefs.push_back(LineDef(10, 1, 2, 100, -1));
    map.lineDefs.push_back(LineDef(11, 2, 5, 101, 104, true));
    map.lineDefs.push_back(LineDef(12, 5, 6, 102, -1));
    map.lineDefs.push_back(LineDef(13, 6, 1, 103, -1));
    map.lineDefs.push_back(LineDef(14, 2, 3, 105, -1));
    map.lineDefs.push_back(LineDef(15, 3, 4, 106, -1));
    map.lineDefs.push_back(LineDef(16, 4, 5, 107, -1));

    map.sideDefs.push_back(SideDef(100, 10, game::SectorTopologySideKind::Front, 200, "left_south"));
    map.sideDefs.push_back(SideDef(101, 11, game::SectorTopologySideKind::Front, 200, "portal_left"));
    map.sideDefs.push_back(SideDef(102, 12, game::SectorTopologySideKind::Front, 200, "left_north"));
    map.sideDefs.push_back(SideDef(103, 13, game::SectorTopologySideKind::Front, 200, "left_west"));
    map.sideDefs.push_back(SideDef(104, 11, game::SectorTopologySideKind::Back, 201, "portal_right"));
    map.sideDefs.push_back(SideDef(105, 14, game::SectorTopologySideKind::Front, 201, "right_south"));
    map.sideDefs.push_back(SideDef(106, 15, game::SectorTopologySideKind::Front, 201, "right_east"));
    map.sideDefs.push_back(SideDef(107, 16, game::SectorTopologySideKind::Front, 201, "right_north"));
    map.sectors.push_back(Sector(200, "left"));
    map.sectors.push_back(Sector(201, "right"));
    return map;
}

bool SameUv(game::SectorTopologyUvSettings a, game::SectorTopologyUvSettings b)
{
    return a.scale.x == b.scale.x
           && a.scale.y == b.scale.y
           && a.offset.x == b.offset.x
           && a.offset.y == b.offset.y;
}

void CheckWallPartCopied(
        const game::SectorTopologyWallPartSettings& imported,
        const game::SectorTopologyWallPartSettings& original,
        const char* description)
{
    Check(imported.textureId == original.textureId, description);
    Check(SameUv(imported.uv, original.uv), description);
    Check(imported.decal.textureId == original.decal.textureId, description);
    Check(imported.decal.opacity == original.decal.opacity, description);
}

void CheckSectorPropertiesCopied(
        const game::SectorAuthoringFaceAnchor& anchor,
        const game::SectorTopologySector& sector,
        const char* description)
{
    Check(anchor.id == sector.id, description);
    Check(anchor.name == sector.name, description);
    Check(anchor.floorZ == sector.floorZ, description);
    Check(anchor.ceilingZ == sector.ceilingZ, description);
    Check(anchor.floorTextureId == sector.floorTextureId, description);
    Check(anchor.ceilingTextureId == sector.ceilingTextureId, description);
    Check(anchor.ceilingSky == sector.ceilingSky, description);
    Check(SameUv(anchor.floorUv, sector.floorUv), description);
    Check(SameUv(anchor.ceilingUv, sector.ceilingUv), description);
    Check(anchor.floorDecal.textureId == sector.floorDecal.textureId, description);
    Check(anchor.ceilingDecal.textureId == sector.ceilingDecal.textureId, description);
    Check(anchor.ambientColor.r == sector.ambientColor.r
                  && anchor.ambientColor.g == sector.ambientColor.g
                  && anchor.ambientColor.b == sector.ambientColor.b
                  && anchor.ambientColor.a == sector.ambientColor.a,
          description);
    Check(anchor.ambientIntensity == sector.ambientIntensity, description);
    CheckWallPartCopied(anchor.defaultWall, sector.defaultWall, description);
    CheckWallPartCopied(anchor.defaultLower, sector.defaultLower, description);
    CheckWallPartCopied(anchor.defaultUpper, sector.defaultUpper, description);
}

void TestEmptyGraph()
{
    game::SectorAuthoringGraph graph;

    Check(graph.vertices.empty(), "empty graph has no vertices");
    Check(graph.lines.empty(), "empty graph has no lines");
    Check(graph.lineSides.empty(), "empty graph has no line sides");
    Check(graph.faceAnchors.empty(), "empty graph has no face anchors");
    Check(game::AllocateSectorAuthoringVertexId(graph) == 1, "empty graph next vertex ID is positive");
    Check(game::AllocateSectorAuthoringLineId(graph) == 1, "empty graph next line ID is positive");
    Check(game::AllocateSectorAuthoringFaceAnchorId(graph) == 1, "empty graph next face anchor ID is positive");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "empty graph validates");
}

void TestStablePositiveIdAllocation()
{
    game::SectorAuthoringGraph graph;
    graph.vertices.push_back(game::SectorAuthoringVertex{7, 0, 0});
    graph.vertices.push_back(game::SectorAuthoringVertex{3, 16, 0});
    graph.lines.push_back(game::SectorAuthoringLine{11, 7, 3});
    graph.faceAnchors.push_back(game::SectorAuthoringFaceAnchor{5});

    Check(game::AllocateSectorAuthoringVertexId(graph) == 8, "vertex ID allocation uses max existing ID");
    Check(game::AllocateSectorAuthoringLineId(graph) == 12, "line ID allocation uses max existing ID");
    Check(game::AllocateSectorAuthoringFaceAnchorId(graph) == 6, "face anchor ID allocation uses max existing ID");
}

void TestAddVerticesAndLines()
{
    game::SectorAuthoringGraph graph;

    int firstVertexId = -1;
    int secondVertexId = -1;
    int lineId = -1;

    Check(game::AddSectorAuthoringVertex(graph, 0, 0, &firstVertexId), "add first authoring vertex");
    Check(game::AddSectorAuthoringVertex(graph, 32, 16, &secondVertexId), "add second authoring vertex");
    Check(firstVertexId == 1, "first vertex ID is stable positive");
    Check(secondVertexId == 2, "second vertex ID is stable positive");
    Check(game::AddSectorAuthoringLine(graph, firstVertexId, secondVertexId, &lineId), "add authoring line");
    Check(lineId == 1, "first line ID is stable positive");

    const game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, lineId);
    Check(line != nullptr, "find added line");
    if (line != nullptr) {
        Check(line->startVertexId == firstVertexId, "line stores start vertex");
        Check(line->endVertexId == secondVertexId, "line stores end vertex");
        Check(!line->flags.blocksPlayer, "line flags default to current topology default");
    }
}

void TestInvalidEndpointDiagnosticsAndRejection()
{
    game::SectorAuthoringGraph graph;
    int firstVertexId = -1;
    Check(game::AddSectorAuthoringVertex(graph, 0, 0, &firstVertexId), "add vertex for invalid endpoint test");

    Check(!game::AddSectorAuthoringLine(graph, firstVertexId, 999, nullptr), "reject missing end vertex");
    Check(!game::AddSectorAuthoringLine(graph, firstVertexId, firstVertexId, nullptr), "reject identical endpoints");

    game::SectorAuthoringLine invalidLine;
    invalidLine.id = 42;
    invalidLine.startVertexId = firstVertexId;
    invalidLine.endVertexId = 999;
    graph.lines.push_back(invalidLine);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);
    Check(game::HasSectorAuthoringValidationErrors(issues), "invalid references produce validation errors");
    Check(HasIssueFor(issues, game::SectorAuthoringObjectKind::Line, 42), "invalid line issue includes line ID");
}

void TestValidateAuthoringGraphRejectsDuplicateVertexIds()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 1, 16, 0);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate vertex IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Vertex,
                  1,
                  "Duplicate"),
          "duplicate vertex ID issue includes vertex ID");
}

void TestValidateAuthoringGraphRejectsDuplicateLineIds()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 16, 0);
    AddAuthoringVertexWithId(graph, 3, 16, 16);
    AddAuthoringLineWithId(graph, 7, 1, 2);
    AddAuthoringLineWithId(graph, 7, 2, 3);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate line IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Line,
                  7,
                  "Duplicate"),
          "duplicate line ID issue includes line ID");
}

void TestValidateAuthoringGraphRejectsDuplicateFaceAnchorIds()
{
    game::SectorAuthoringGraph graph;
    game::SectorAuthoringFaceAnchor firstAnchor;
    firstAnchor.id = 3;
    game::SectorAuthoringFaceAnchor secondAnchor;
    secondAnchor.id = 3;
    graph.faceAnchors.push_back(firstAnchor);
    graph.faceAnchors.push_back(secondAnchor);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate face anchor IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::FaceAnchor,
                  3,
                  "Duplicate"),
          "duplicate face anchor ID issue includes anchor ID");
}

void TestValidateAuthoringGraphRejectsDuplicateSideIdentity()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 16, 0);
    AddAuthoringLineWithId(graph, 7, 1, 2);

    game::SectorAuthoringLineSide firstSide;
    firstSide.id = game::SectorAuthoringSideId{7, game::SectorTopologySideKind::Front};
    game::SectorAuthoringLineSide secondSide;
    secondSide.id = game::SectorAuthoringSideId{7, game::SectorTopologySideKind::Front};
    graph.lineSides.push_back(firstSide);
    graph.lineSides.push_back(secondSide);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate side identities produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Side,
                  7,
                  "Duplicate"),
          "duplicate side identity issue includes line ID");
}

void TestSideIdentityHelpers()
{
    const game::SectorAuthoringSideId front{17, game::SectorTopologySideKind::Front};
    const game::SectorAuthoringSideId sameFront{17, game::SectorTopologySideKind::Front};
    const game::SectorAuthoringSideId back{17, game::SectorTopologySideKind::Back};

    Check(game::SectorAuthoringSideIdsEqual(front, sameFront), "same line and side compare equal");
    Check(!game::SectorAuthoringSideIdsEqual(front, back), "different side does not compare equal");
    Check(game::SectorAuthoringSideIdsEqual(
                  game::OppositeSectorAuthoringSideId(front),
                  back),
          "opposite authoring side helper flips side only");

    game::SectorAuthoringGraph graph;
    graph.lines.push_back(game::SectorAuthoringLine{17, 1, 2});
    game::SectorAuthoringLineSide side;
    side.id = front;
    side.wall.textureId = "wall";
    graph.lineSides.push_back(side);

    const game::SectorAuthoringLineSide* found = game::FindSectorAuthoringLineSide(graph, front);
    Check(found != nullptr, "find authoring side by line plus side");
    if (found != nullptr) {
        Check(found->wall.textureId == "wall", "authoring side carries wall material metadata");
        Check(found->lower.textureId.empty(), "authoring side lower material defaults match topology defaults");
        Check(found->upper.textureId.empty(), "authoring side upper material defaults match topology defaults");
        Check(found->middle.textureId.empty(), "authoring side middle material defaults match topology defaults");
    }
}

void TestFaceAnchorDefaultsMatchTopologyDefaults()
{
    game::SectorAuthoringFaceAnchor anchor;
    game::SectorTopologySector sector;

    Check(anchor.floorZ == sector.floorZ, "face anchor floor height default matches topology sector");
    Check(anchor.ceilingZ == sector.ceilingZ, "face anchor ceiling height default matches topology sector");
    Check(anchor.floorTextureId == sector.floorTextureId, "face anchor floor texture default matches topology sector");
    Check(anchor.ceilingTextureId == sector.ceilingTextureId, "face anchor ceiling texture default matches topology sector");
    Check(anchor.ceilingSky == sector.ceilingSky, "face anchor sky default matches topology sector");
    Check(anchor.ambientColor.r == sector.ambientColor.r
                  && anchor.ambientColor.g == sector.ambientColor.g
                  && anchor.ambientColor.b == sector.ambientColor.b
                  && anchor.ambientColor.a == sector.ambientColor.a,
          "face anchor ambient color default matches topology sector");
    Check(anchor.ambientIntensity == sector.ambientIntensity, "face anchor ambient intensity default matches topology sector");
    Check(anchor.defaultWall.textureId == sector.defaultWall.textureId, "face anchor default wall material matches topology sector");
    Check(anchor.defaultLower.textureId == sector.defaultLower.textureId, "face anchor default lower material matches topology sector");
    Check(anchor.defaultUpper.textureId == sector.defaultUpper.textureId, "face anchor default upper material matches topology sector");
}

void TestImportEmptyTopologyMap()
{
    const game::SectorTopologyMap map;
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    Check(graph.vertices.empty(), "empty topology import has no authoring vertices");
    Check(graph.lines.empty(), "empty topology import has no authoring lines");
    Check(graph.lineSides.empty(), "empty topology import has no authoring sides");
    Check(graph.faceAnchors.empty(), "empty topology import has no face anchors");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "empty imported graph validates");
}

void TestImportSingleSectorSquare()
{
    const game::SectorTopologyMap map = MakeSingleSectorSquareMap();
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    Check(graph.vertices.size() == map.vertices.size(), "single sector import preserves vertex count");
    Check(graph.lines.size() == map.lineDefs.size(), "single sector import preserves line count");
    Check(graph.lineSides.size() == map.sideDefs.size(), "single sector import preserves side count");
    Check(graph.faceAnchors.size() == map.sectors.size(), "single sector import preserves sector anchor count");

    const game::SectorAuthoringVertex* vertex = game::FindSectorAuthoringVertex(graph, 3);
    Check(vertex != nullptr && vertex->x == 64 && vertex->y == 64, "import preserves vertex ID and coordinates");

    const game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, 10);
    Check(line != nullptr, "import preserves linedef ID as authoring line ID");
    if (line != nullptr) {
        Check(line->startVertexId == 1 && line->endVertexId == 2, "import preserves linedef orientation");
        Check(line->flags.blocksPlayer, "import preserves linedef blocksPlayer flag");
    }

    const game::SectorAuthoringLineSide* side = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* originalSide = game::FindSectorTopologySideDef(map, 100);
    Check(side != nullptr && originalSide != nullptr, "import preserves front side identity");
    if (side != nullptr && originalSide != nullptr) {
        CheckWallPartCopied(side->wall, originalSide->wall, "import copies wall material metadata");
        CheckWallPartCopied(side->lower, originalSide->lower, "import copies lower material metadata");
        CheckWallPartCopied(side->upper, originalSide->upper, "import copies upper material metadata");
        CheckWallPartCopied(side->middle, originalSide->middle, "import copies middle material metadata");
    }

    const game::SectorAuthoringFaceAnchor* anchor = game::FindSectorAuthoringFaceAnchor(graph, 200);
    const game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 200);
    Check(anchor != nullptr && sector != nullptr, "import preserves sector ID as face anchor ID");
    if (anchor != nullptr && sector != nullptr) {
        CheckSectorPropertiesCopied(*anchor, *sector, "import copies sector properties to face anchor");
        Check(anchor->x == 32 && anchor->y == 32, "import assigns deterministic face anchor center");
    }

    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "single sector imported graph validates");
}

void TestImportAdjacentSectors()
{
    const game::SectorTopologyMap map = MakeAdjacentSectorMap();
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    const game::SectorAuthoringLine* portalLine = game::FindSectorAuthoringLine(graph, 11);
    Check(portalLine != nullptr, "adjacent import preserves shared linedef");
    if (portalLine != nullptr) {
        Check(portalLine->startVertexId == 2 && portalLine->endVertexId == 5, "adjacent import preserves shared linedef orientation");
        Check(portalLine->flags.blocksPlayer, "adjacent import preserves shared linedef blocksPlayer");
    }

    const game::SectorAuthoringLineSide* front = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Front});
    const game::SectorAuthoringLineSide* back = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Back});
    Check(front != nullptr, "adjacent import keeps front side metadata for shared linedef");
    Check(back != nullptr, "adjacent import keeps back side metadata for shared linedef");
    if (front != nullptr) {
        Check(front->wall.textureId == "portal_left_wall", "adjacent import copies front wall metadata");
    }
    if (back != nullptr) {
        Check(back->wall.textureId == "portal_right_wall", "adjacent import copies back wall metadata");
    }

    Check(game::FindSectorAuthoringFaceAnchor(graph, 200) != nullptr, "adjacent import keeps first sector anchor");
    Check(game::FindSectorAuthoringFaceAnchor(graph, 201) != nullptr, "adjacent import keeps second sector anchor");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "adjacent imported graph validates");
}

void TestPlanarizeCrossingLines()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 64},
            {0, 64}, {64, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "crossing lines planarize without diagnostics");
    Check(result.vertices.size() == 5, "crossing lines add one intersection vertex");
    Check(result.edges.size() == 4, "crossing lines split into four planar edges");
    Check(FindPlanarVertexAt(result, 32, 32) != nullptr, "crossing intersection point is deterministic");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "first crossing source line maps to two planar edges");
    Check(CountPlanarEdgesForLine(result, 11) == 2, "second crossing source line maps to two planar edges");
}

void TestPlanarizeTJunction()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "T-junction planarizes without diagnostics");
    Check(result.vertices.size() == 4, "T-junction reuses endpoint as split vertex");
    Check(result.edges.size() == 3, "T-junction splits only the containing segment");
    Check(FindPlanarVertexAt(result, 32, 0) != nullptr, "T-junction keeps endpoint-on-segment point");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "T-junction containing source line maps to two edges");
    Check(CountPlanarEdgesForLine(result, 11) == 1, "T-junction endpoint source line stays one edge");
}

void TestPlanarizeEndpointOnSegment()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, -32}, {32, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "endpoint-on-segment planarizes without diagnostics");
    Check(result.edges.size() == 3, "endpoint-on-segment splits containing segment");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "endpoint-on-segment maps containing source line to two edges");
    Check(CountPlanarEdgesForLine(result, 11) == 1, "endpoint-on-segment source endpoint line stays one edge");
}

void TestPlanarizeNonCollinearCoincidentEndpointDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::CoincidentEndpoint),
          "non-collinear endpoint touch with distinct vertex IDs produces diagnostic");
}

void TestPlanarizeMultipleCrossingsOnOneLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {96, 0},
            {32, -32}, {32, 32},
            {64, -32}, {64, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "multiple crossings planarize without diagnostics");
    Check(result.vertices.size() == 8, "multiple crossings insert two vertices");
    Check(result.edges.size() == 7, "multiple crossings split all crossing segments");
    Check(CountPlanarEdgesForLine(result, 10) == 3, "source line with two crossings maps to ordered child segments");
    Check(FindPlanarVertexAt(result, 32, 0) != nullptr, "first crossing exists");
    Check(FindPlanarVertexAt(result, 64, 0) != nullptr, "second crossing exists");
}

void TestPlanarizeNonIntersectingLines()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {0, 16}, {32, 16}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "non-intersecting lines planarize without diagnostics");
    Check(result.vertices.size() == 4, "non-intersecting lines keep only endpoints");
    Check(result.edges.size() == 2, "non-intersecting lines keep one edge per source line");
}

void TestPlanarizeDuplicateLineDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::DuplicateLine),
          "duplicate lines produce diagnostic");
    Check(result.edges.empty(), "duplicate lines are not silently emitted as planar edges");
}

void TestPlanarizeCollinearOverlapDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {96, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::CollinearOverlap),
          "collinear overlapping lines produce diagnostic");
    Check(result.edges.empty(), "collinear overlapping lines are not silently emitted as planar edges");
}

void TestPlanarizeZeroLengthLineDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {8, 8}, {8, 8}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::ZeroLengthLine),
          "zero-length line produces diagnostic");
    Check(result.edges.empty(), "zero-length line emits no planar edges");
}

void TestPlanarizeNearMissDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 1}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::NearMiss),
          "near-miss endpoint within one grid coordinate produces deterministic diagnostic");
    Check(result.edges.size() == 2, "near-miss does not auto-snap or split");
}

void TestPlanarizeMetadataMapping()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 64},
            {0, 64}, {64, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    for (const game::SectorAuthoringPlanarEdge& edge : result.edges) {
        Check(edge.sourceLineId == 10 || edge.sourceLineId == 11, "planar edge preserves source authoring line ID");
        Check(edge.followsSourceLineDirection, "planar edge records source direction mapping");
    }
}

} // namespace

int main()
{
    TestEmptyGraph();
    TestStablePositiveIdAllocation();
    TestAddVerticesAndLines();
    TestInvalidEndpointDiagnosticsAndRejection();
    TestValidateAuthoringGraphRejectsDuplicateVertexIds();
    TestValidateAuthoringGraphRejectsDuplicateLineIds();
    TestValidateAuthoringGraphRejectsDuplicateFaceAnchorIds();
    TestValidateAuthoringGraphRejectsDuplicateSideIdentity();
    TestSideIdentityHelpers();
    TestFaceAnchorDefaultsMatchTopologyDefaults();
    TestImportEmptyTopologyMap();
    TestImportSingleSectorSquare();
    TestImportAdjacentSectors();
    TestPlanarizeCrossingLines();
    TestPlanarizeTJunction();
    TestPlanarizeEndpointOnSegment();
    TestPlanarizeNonCollinearCoincidentEndpointDiagnostic();
    TestPlanarizeMultipleCrossingsOnOneLine();
    TestPlanarizeNonIntersectingLines();
    TestPlanarizeDuplicateLineDiagnostic();
    TestPlanarizeCollinearOverlapDiagnostic();
    TestPlanarizeZeroLengthLineDiagnostic();
    TestPlanarizeNearMissDiagnostic();
    TestPlanarizeMetadataMapping();

    if (failures != 0) {
        std::cerr << failures << " authoring graph test(s) failed\n";
        return 1;
    }

    std::cout << "Sector authoring graph tests passed\n";
    return 0;
}
