#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorUnits.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTopologyRenderCache.h"

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

bool HasFaceDiagnostic(
        const std::vector<game::SectorAuthoringFaceDiagnostic>& diagnostics,
        game::SectorAuthoringFaceDiagnosticKind kind)
{
    for (const game::SectorAuthoringFaceDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

bool HasDerivationDiagnostic(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

bool HasDerivationDiagnosticFor(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind,
        int objectId)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind && diagnostic.objectId == objectId) {
            return true;
        }
    }
    return false;
}

const game::SectorAuthoringDerivationDiagnostic* FindDerivationDiagnosticFor(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind,
        int objectId)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind && diagnostic.objectId == objectId) {
            return &diagnostic;
        }
    }
    return nullptr;
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

bool TextContains(const char* text, const char* expected)
{
    return text != nullptr && std::string{text}.find(expected) != std::string::npos;
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

game::SectorAuthoringGraph MakeGraphFromConnectedLines(
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& vertices,
        const std::vector<std::pair<int, int>>& lines)
{
    game::SectorAuthoringGraph graph;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        AddAuthoringVertexWithId(
                graph,
                static_cast<int>(index) + 1,
                vertices[index].first,
                vertices[index].second);
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        AddAuthoringLineWithId(
                graph,
                10 + static_cast<int>(index),
                lines[index].first,
                lines[index].second);
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

int CountDerivedLineMappingsForAuthoringLine(
        const game::SectorAuthoringDerivationMapping& mapping,
        int lineId)
{
    int count = 0;
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : mapping.lines) {
        if (lineMapping.authoringLineId == lineId) {
            ++count;
        }
    }
    return count;
}

const game::SectorAuthoringDerivedSectorMapping* FindSectorMappingForAnchor(
        const game::SectorAuthoringDerivationMapping& mapping,
        int anchorId)
{
    for (const game::SectorAuthoringDerivedSectorMapping& sectorMapping : mapping.sectors) {
        if (sectorMapping.faceAnchorId == anchorId) {
            return &sectorMapping;
        }
    }
    return nullptr;
}

const game::SectorTopologyLineDef* FindDerivedLineDefForAuthoringLine(
        const game::SectorAuthoringDerivationResult& result,
        int authoringLineId)
{
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId == authoringLineId) {
            return game::FindSectorTopologyLineDef(result.topology, lineMapping.topologyLineDefId);
        }
    }
    return nullptr;
}

const game::SectorTopologySideDef* FindDerivedSideDefForAuthoringSide(
        const game::SectorAuthoringDerivationResult& result,
        int authoringLineId,
        game::SectorTopologySideKind side)
{
    for (const game::SectorAuthoringDerivedSideMapping& sideMapping : result.mapping.sides) {
        if (sideMapping.authoringLineId == authoringLineId && sideMapping.authoringSide == side) {
            return game::FindSectorTopologySideDef(result.topology, sideMapping.topologySideDefId);
        }
    }
    return nullptr;
}

game::SectorAuthoringFaceExtractionResult ExtractFacesFromGraph(const game::SectorAuthoringGraph& graph)
{
    const game::SectorAuthoringPlanarizationResult planar = game::PlanarizeSectorAuthoringGraph(graph);
    Check(planar.diagnostics.empty(), "face extraction test graph planarizes without diagnostics");
    return game::ExtractSectorAuthoringFaces(planar);
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

void TestExtractFacesSingleSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "single square produces one bounded face");
    Check(result.faces[0].boundary.size() == 4, "single square face has four boundary edges");
    Check(result.faces[0].signedArea > 0.0, "single square face has deterministic positive winding");
    Check(result.diagnostics.empty(), "single square produces no face diagnostics");
    for (const game::SectorAuthoringFaceBoundaryEdge& edge : result.faces[0].boundary) {
        Check(edge.sourceLineId >= 10 && edge.sourceLineId <= 13, "face boundary preserves source line mapping");
        Check(edge.sourceSide == game::SectorTopologySideKind::Front, "CCW square maps bounded face to front sides");
    }
}

void TestExtractFacesAdjacentSquares()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "two adjacent squares produce two bounded faces");
    Check(result.diagnostics.empty(), "two adjacent squares produce no face diagnostics");
    Check(result.faces[0].id == 1 && result.faces[1].id == 2, "adjacent square face IDs are deterministic");
}

void TestExtractFacesRectangleCutByLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 64}, {0, 64}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "rectangle cut by a boundary-to-boundary line produces two faces");
    Check(result.diagnostics.empty(), "rectangle cut by line produces no face diagnostics");
}

void TestExtractFacesCrossingDiagonals()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringPlanarizationResult planar = game::PlanarizeSectorAuthoringGraph(graph);
    const game::SectorAuthoringFaceExtractionResult result = game::ExtractSectorAuthoringFaces(planar);

    Check(planar.diagnostics.empty(), "crossing diagonals planarize without diagnostics");
    Check(result.faces.size() == 4, "crossing diagonals in a square produce four bounded faces");
    Check(result.diagnostics.empty(), "crossing diagonals produce no face diagnostics");
}

void TestExtractFacesOpenChainDiagnostics()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.empty(), "open chain produces no bounded faces");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "open chain produces dangling edge diagnostics");
}

void TestExtractFacesDanglingLineDoesNotCorruptSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {96, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {3, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "dangling line attached to square keeps the square face");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "dangling line attached to square produces dangling edge diagnostic");
}

void TestExtractFacesRejectsOuterFace()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "outer face is not returned as a bounded face");
}

void TestExtractFacesSliverThreshold()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {1, 0}, {0, 1}},
            {{1, 2}, {2, 3}, {3, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.empty(), "sub-grid sliver triangle is not returned as a face");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::TinySliverFace),
          "sub-grid sliver triangle produces a sliver diagnostic");
}

void TestExtractFacesNestedDisconnectedLoopsAreDeferred()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}, {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.empty(), "nested disconnected loops are not returned as guessed faces");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::AmbiguousTopology),
          "nested disconnected loops produce an ambiguous topology diagnostic");
}

void TestExtractDisconnectedClosedLoopsProducesFaces()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {128, 0}, {192, 0}, {192, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "disconnected closed loops are returned as independent faces");
    Check(result.diagnostics.empty(), "disconnected closed loops produce no face diagnostics");
}

void TestExtractDisconnectedClosedLoopsWithDanglingLineReportsOnlyDanglingLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64},
             {128, 0}, {192, 0}, {192, 64}, {128, 64},
             {224, 0}, {256, 0}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "dangling line does not corrupt disconnected closed loop faces");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "dangling line in disconnected graph reports dangling diagnostic");
    Check(!HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::AmbiguousTopology),
          "dangling line in disconnected graph does not report ambiguous topology");
}

void CheckDerivedTopologyIsValid(
        const game::SectorAuthoringDerivationResult& result,
        const char* description)
{
    Check(result.success, description);
    Check(result.diagnostics.empty(), description);
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(result.topology)),
          description);
}

void TestDeriveSingleSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "single square derives valid topology");
    Check(result.topology.vertices.size() == 4, "single square derives four vertices");
    Check(result.topology.lineDefs.size() == 4, "single square derives four linedefs");
    Check(result.topology.sideDefs.size() == 4, "single square derives four sidedefs");
    Check(result.topology.sectors.size() == 1, "single square derives one sector");
    Check(result.mapping.vertices.size() == 4, "single square records vertex mapping");
    Check(result.mapping.lines.size() == 4, "single square records line mapping");
    Check(result.mapping.sides.size() == 4, "single square records side mapping");
    Check(result.mapping.sectors.size() == 1, "single square records sector mapping");
}

void TestDeriveAdjacentSquares()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "adjacent squares derive valid topology");
    Check(result.topology.sectors.size() == 2, "adjacent squares derive two sectors");
    Check(result.topology.lineDefs.size() == 7, "adjacent squares share one derived linedef");

    int twoSidedLineCount = 0;
    for (const game::SectorTopologyLineDef& lineDef : result.topology.lineDefs) {
        if (lineDef.frontSideDefId > 0 && lineDef.backSideDefId > 0) {
            ++twoSidedLineCount;
        }
    }
    Check(twoSidedLineCount == 1, "adjacent squares derive one two-sided linedef");
}

void TestDeriveCrossingDiagonals()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "crossing diagonals derive valid topology");
    Check(result.topology.vertices.size() == 5, "crossing diagonals derive inserted intersection vertex");
    Check(result.topology.sectors.size() == 4, "crossing diagonals derive four sectors after planarization");
}

void TestDeriveDisconnectedClosedLoopsProducesSectors()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {128, 0}, {192, 0}, {192, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "disconnected closed loops derive valid topology");
    Check(result.topology.sectors.size() == 2, "disconnected closed loops derive two sectors");
    Check(result.mapping.sectors.size() == 2, "disconnected closed loops record two sector mappings");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "disconnected closed loops derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "disconnected closed loops generate surfaces");
}

void TestDeriveNonIntegerIntersectionFails()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {64, 33}},
            {{1, 2}, {2, 5}, {5, 3}, {3, 4}, {4, 1}, {1, 5}, {4, 2}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    bool hasNonIntegerPlanarVertex = false;
    for (const game::SectorAuthoringPlanarVertex& vertex : result.planar.vertices) {
        if (!game::SectorAuthoringPlanarRationalIsInteger(vertex.point.x)
                || !game::SectorAuthoringPlanarRationalIsInteger(vertex.point.y)) {
            hasNonIntegerPlanarVertex = true;
            break;
        }
    }

    Check(!result.success, "non-integer intersection fails derivation");
    Check(result.topology.vertices.empty(), "non-integer intersection returns no half-valid topology vertices");
    Check(result.topology.lineDefs.empty(), "non-integer intersection returns no half-valid topology linedefs");
    Check(result.topology.sideDefs.empty(), "non-integer intersection returns no half-valid topology sidedefs");
    Check(result.topology.sectors.empty(), "non-integer intersection returns no half-valid topology sectors");
    Check(result.mapping.vertices.empty(), "non-integer intersection returns no half-valid vertex mapping");
    Check(result.mapping.lines.empty(), "non-integer intersection returns no half-valid line mapping");
    Check(result.mapping.sides.empty(), "non-integer intersection returns no half-valid side mapping");
    Check(result.mapping.sectors.empty(), "non-integer intersection returns no half-valid sector mapping");
    Check(result.planar.diagnostics.empty(), "non-integer intersection is not rejected during planarization");
    Check(!result.faces.faces.empty(), "non-integer intersection reaches face extraction before failing");
    Check(result.faces.diagnostics.empty(), "non-integer intersection is not rejected during face extraction");
    Check(hasNonIntegerPlanarVertex, "non-integer intersection keeps fractional planar vertex coordinates");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex),
          "non-integer intersection reports non-integer vertex diagnostic");
}

void TestDeriveSquareWithMixedLineDirections()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {3, 2}, {3, 4}, {4, 1}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "mixed-direction square derives valid topology");
    Check(result.topology.vertices.size() == 4, "mixed-direction square derives four vertices");
    Check(result.topology.lineDefs.size() == 4, "mixed-direction square derives four linedefs");
    Check(result.topology.sideDefs.size() == 4, "mixed-direction square derives four sidedefs");
    Check(result.topology.sectors.size() == 1, "mixed-direction square derives one sector");

    bool hasBackBoundarySide = false;
    for (const game::SectorTopologyLineDef& lineDef : result.topology.lineDefs) {
        const bool hasFront = lineDef.frontSideDefId > 0;
        const bool hasBack = lineDef.backSideDefId > 0;
        Check(hasFront != hasBack, "mixed-direction square derives exactly one side per boundary linedef");
        hasBackBoundarySide = hasBackBoundarySide || hasBack;
    }
    Check(hasBackBoundarySide, "mixed-direction square keeps reversed authored edge on back side");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "mixed-direction square derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "mixed-direction square derived topology generates surfaces");
}

void TestDeriveOpenGraphFails()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "open graph fails derivation");
    Check(result.topology.sectors.empty(), "open graph does not return half-valid topology");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::FaceExtraction),
          "open graph reports face extraction diagnostic");
}

void TestDeriveDuplicateLinesFail()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "duplicate line graph fails derivation");
    Check(result.topology.lineDefs.empty(), "duplicate line graph returns no derived linedefs");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DuplicateLine),
          "duplicate line graph reports duplicate line diagnostic");
}

void TestDeriveDiagnosticsUseSpecificKindsAndSources()
{
    const game::SectorAuthoringGraph openGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}});
    const game::SectorAuthoringDerivationResult openResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(openGraph);
    Check(HasDerivationDiagnostic(
                  openResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DanglingLine),
          "open graph derivation reports explicit dangling line diagnostic");

    const game::SectorAuthoringGraph duplicateGraph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});
    const game::SectorAuthoringDerivationResult duplicateResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(duplicateGraph);
    Check(HasDerivationDiagnosticFor(
                  duplicateResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DuplicateLine,
                  10),
          "duplicate line derivation diagnostic includes source line ID");
    Check(!duplicateResult.diagnostics.empty()
                  && duplicateResult.diagnostics.front().relatedObjectId == 11,
          "duplicate line derivation diagnostic includes related line ID");

    const game::SectorAuthoringGraph overlapGraph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {96, 0}});
    const game::SectorAuthoringDerivationResult overlapResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(overlapGraph);
    Check(HasDerivationDiagnostic(
                  overlapResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::CollinearOverlap),
          "overlap derivation reports explicit collinear overlap diagnostic");
}

void TestDeriveDanglingLineDiagnosticUsesAuthoringLineId()
{
    const int danglingLineId = 14;
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {96, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {3, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "dangling line graph fails derivation");
    const game::SectorAuthoringDerivationDiagnostic* diagnostic = FindDerivationDiagnosticFor(
            result.diagnostics,
            game::SectorAuthoringDerivationDiagnosticKind::DanglingLine,
            danglingLineId);
    Check(diagnostic != nullptr, "dangling line diagnostic uses authoring line ID");

    int danglingPlanarEdgeId = -1;
    for (const game::SectorAuthoringPlanarEdge& edge : result.planar.edges) {
        if (edge.sourceLineId == danglingLineId) {
            danglingPlanarEdgeId = edge.id;
            break;
        }
    }
    Check(danglingPlanarEdgeId > 0, "dangling line planar edge is available for comparison");
    if (diagnostic != nullptr && danglingPlanarEdgeId > 0) {
        Check(diagnostic->objectId != danglingPlanarEdgeId,
              "dangling line diagnostic does not use internal planar edge ID as primary object");
        Check(diagnostic->relatedObjectId == danglingPlanarEdgeId,
              "dangling line diagnostic keeps internal planar edge ID as related data");
    }
}

void TestDeriveDisconnectedClosedLoopsWithDanglingLineFailsCleanly()
{
    const int danglingLineId = 18;
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64},
             {128, 0}, {192, 0}, {192, 64}, {128, 64},
             {224, 0}, {256, 0}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "disconnected loops plus dangling line fail derivation");
    Check(result.topology.sectors.empty(), "dangling disconnected graph returns no half-valid topology");
    Check(result.mapping.sectors.empty(), "dangling disconnected graph returns no half-valid mapping");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DanglingLine,
                  danglingLineId),
          "dangling disconnected graph reports dangling authoring line ID");
    Check(!HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::FaceExtraction),
          "dangling disconnected graph does not report generic face extraction ambiguity");
}

void TestDeriveSplitLineMapping()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "split-line mapping graph derives valid topology");
    Check(CountDerivedLineMappingsForAuthoringLine(result.mapping, 14) == 2,
          "planarized diagonal maps one authoring line to two derived linedefs");
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId == 14) {
            Check(lineMapping.sourceLineId == 14, "line mapping records derived back-source line ID");
            Check(lineMapping.topologyLineDefId > 0, "split line mapping records derived linedef ID");
        }
    }
}

void TestDeriveFaceAnchorMapping()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = 200;
    anchor.x = 32;
    anchor.y = 32;
    graph.faceAnchors.push_back(anchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "anchored square derives valid topology");
    const game::SectorAuthoringDerivedSectorMapping* sectorMapping =
            FindSectorMappingForAnchor(result.mapping, 200);
    Check(sectorMapping != nullptr, "face anchor maps to derived sector");
    if (sectorMapping != nullptr) {
        Check(sectorMapping->extractedFaceId > 0, "sector mapping records extracted face ID");
        Check(sectorMapping->faceAnchorId == 200, "sector mapping records face anchor ID");
        Check(sectorMapping->topologySectorId == 200, "derived sector uses unambiguous face anchor ID");
    }
}

void TestDeriveFaceAnchorDiagnostics()
{
    game::SectorAuthoringGraph unresolvedGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor unresolvedAnchor;
    unresolvedAnchor.id = 300;
    unresolvedAnchor.x = 128;
    unresolvedAnchor.y = 128;
    unresolvedGraph.faceAnchors.push_back(unresolvedAnchor);

    const game::SectorAuthoringDerivationResult unresolvedResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(unresolvedGraph);
    Check(!unresolvedResult.success, "unresolved face anchor fails derivation cleanly");
    Check(HasDerivationDiagnosticFor(
                  unresolvedResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor,
                  300),
          "unresolved face anchor reports explicit diagnostic with anchor ID");

    game::SectorAuthoringGraph ambiguousGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor firstAnchor;
    firstAnchor.id = 400;
    firstAnchor.x = 32;
    firstAnchor.y = 32;
    ambiguousGraph.faceAnchors.push_back(firstAnchor);
    game::SectorAuthoringFaceAnchor secondAnchor;
    secondAnchor.id = 401;
    secondAnchor.x = 48;
    secondAnchor.y = 48;
    ambiguousGraph.faceAnchors.push_back(secondAnchor);

    const game::SectorAuthoringDerivationResult ambiguousResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(ambiguousGraph);
    Check(!ambiguousResult.success, "multiple anchors in one face fail derivation cleanly");
    Check(HasDerivationDiagnostic(
                  ambiguousResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor),
          "multiple anchors in one face report ambiguous face anchor diagnostic");
}

void TestDeriveInvalidSideProjectionDiagnostic()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringLineSide side;
    side.id.lineId = 999;
    side.id.side = game::SectorTopologySideKind::Front;
    graph.lineSides.push_back(side);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "invalid side projection fails derivation cleanly");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection,
                  999),
          "invalid side projection reports explicit diagnostic with source line ID");
}

void TestDeriveProjectsFaceAnchorProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = 200;
    anchor.name = "projected";
    anchor.x = 32;
    anchor.y = 32;
    anchor.floorZ = -4.0f;
    anchor.ceilingZ = 48.0f;
    anchor.floorTextureId = "floor_projected";
    anchor.ceilingTextureId = "ceiling_projected";
    anchor.ceilingSky = true;
    anchor.floorUv.scale = Vector2{2.0f, 3.0f};
    anchor.floorUv.offset = Vector2{4.0f, 5.0f};
    anchor.ceilingUv.scale = Vector2{6.0f, 7.0f};
    anchor.ceilingUv.offset = Vector2{8.0f, 9.0f};
    anchor.floorDecal.textureId = "floor_decal_projected";
    anchor.ceilingDecal.textureId = "ceiling_decal_projected";
    anchor.ambientColor = Color{12, 24, 36, 255};
    anchor.ambientIntensity = 0.45f;
    anchor.defaultWall = WallPart("anchor_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    anchor.defaultLower = WallPart("anchor_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    anchor.defaultUpper = WallPart("anchor_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    graph.faceAnchors.push_back(anchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "face anchor property projection derives valid topology");
    const game::SectorTopologySector* sector = game::FindSectorTopologySector(result.topology, 200);
    Check(sector != nullptr, "projected face anchor uses anchor ID as derived sector ID");
    if (sector != nullptr) {
        Check(anchor.id == sector->id, "projected sector keeps anchor ID");
        Check(anchor.name == sector->name, "projected sector keeps anchor name");
        Check(anchor.floorZ == sector->floorZ, "projected sector keeps floor height");
        Check(anchor.ceilingZ == sector->ceilingZ, "projected sector keeps ceiling height");
        Check(anchor.floorTextureId == sector->floorTextureId, "projected sector keeps floor texture");
        Check(anchor.ceilingTextureId == sector->ceilingTextureId, "projected sector keeps ceiling texture");
        Check(sector->ceilingSky, "projected sector keeps ceiling sky");
        Check(SameUv(anchor.floorUv, sector->floorUv), "projected sector keeps floor UV");
        Check(SameUv(anchor.ceilingUv, sector->ceilingUv), "projected sector keeps ceiling UV");
        Check(anchor.floorDecal.textureId == sector->floorDecal.textureId, "projected sector keeps floor decal");
        Check(anchor.ceilingDecal.textureId == sector->ceilingDecal.textureId, "projected sector keeps ceiling decal");
        Check(anchor.ambientColor.r == sector->ambientColor.r
                      && anchor.ambientColor.g == sector->ambientColor.g
                      && anchor.ambientColor.b == sector->ambientColor.b
                      && anchor.ambientColor.a == sector->ambientColor.a,
              "projected sector keeps ambient color");
        Check(anchor.ambientIntensity == sector->ambientIntensity, "projected sector keeps ambient intensity");
        CheckWallPartCopied(sector->defaultWall, anchor.defaultWall, "projected sector keeps default wall");
        CheckWallPartCopied(sector->defaultLower, anchor.defaultLower, "projected sector keeps default lower");
        CheckWallPartCopied(sector->defaultUpper, anchor.defaultUpper, "projected sector keeps default upper");
    }
}

void TestDeriveProjectsSideMaterialsAndLineFlags()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    if (game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, 10)) {
        line->flags.blocksPlayer = true;
    }
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("side_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    side.lower = WallPart("side_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    side.upper = WallPart("side_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    side.middle = WallPart("side_middle", 4.0f, 5.0f, 6.0f, 7.0f);
    side.middle.decal.textureId = "side_middle_decal";
    side.middle.decal.opacity = 0.25f;
    graph.lineSides.push_back(side);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "side material projection derives valid topology");
    const game::SectorTopologyLineDef* lineDef = FindDerivedLineDefForAuthoringLine(result, 10);
    Check(lineDef != nullptr && lineDef->flags.blocksPlayer, "authoring line blocksPlayer projects to derived linedef");
    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            result,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "authoring side maps to a derived sidedef");
    if (sideDef != nullptr) {
        CheckWallPartCopied(sideDef->wall, side.wall, "authoring side wall projects to derived sidedef");
        CheckWallPartCopied(sideDef->lower, side.lower, "authoring side lower projects to derived sidedef");
        CheckWallPartCopied(sideDef->upper, side.upper, "authoring side upper projects to derived sidedef");
        CheckWallPartCopied(sideDef->middle, side.middle, "authoring side middle projects to derived sidedef");
    }
}

void TestDeriveSplitLineDuplicatesProjectedProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});
    if (game::SectorAuthoringLine* diagonal = game::FindSectorAuthoringLine(graph, 14)) {
        diagonal->flags.blocksPlayer = true;
    }
    game::SectorAuthoringLineSide frontSide;
    frontSide.id = game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Front};
    frontSide.wall = WallPart("split_front_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    graph.lineSides.push_back(frontSide);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "split-line property projection derives valid topology");
    int projectedLineCount = 0;
    int projectedFrontSideCount = 0;
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId != 14) {
            continue;
        }
        const game::SectorTopologyLineDef* lineDef =
                game::FindSectorTopologyLineDef(result.topology, lineMapping.topologyLineDefId);
        Check(lineDef != nullptr && lineDef->flags.blocksPlayer, "split derived linedef keeps source line flag");
        ++projectedLineCount;
    }
    for (const game::SectorAuthoringDerivedSideMapping& sideMapping : result.mapping.sides) {
        if (sideMapping.authoringLineId != 14
                || sideMapping.authoringSide != game::SectorTopologySideKind::Front) {
            continue;
        }
        const game::SectorTopologySideDef* sideDef =
                game::FindSectorTopologySideDef(result.topology, sideMapping.topologySideDefId);
        Check(sideDef != nullptr && sideDef->wall.textureId == "split_front_wall",
              "split derived sidedef keeps source side wall material");
        ++projectedFrontSideCount;
    }
    Check(projectedLineCount == 2, "split source line projects flags to both child linedefs");
    Check(projectedFrontSideCount == 2, "split source side projects materials to both child sidedefs");
}

void TestDeriveUnresolvedAnchorPreservesAuthoringProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor unresolvedAnchor;
    unresolvedAnchor.id = 300;
    unresolvedAnchor.x = 128;
    unresolvedAnchor.y = 128;
    unresolvedAnchor.floorTextureId = "unresolved_floor";
    graph.faceAnchors.push_back(unresolvedAnchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "unresolved property anchor fails derivation");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor,
                  300),
          "unresolved property anchor reports diagnostic");
    const game::SectorAuthoringFaceAnchor* preserved =
            game::FindSectorAuthoringFaceAnchor(graph, 300);
    Check(preserved != nullptr && preserved->floorTextureId == "unresolved_floor",
          "unresolved anchor properties remain in authoring graph");
}

void TestDerivedTopologyBuildsGeometry()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    CheckDerivedTopologyIsValid(result, "geometry derivation input is valid");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "derived topology generates surfaces");
}

void TestFreshDerivedTopologyUsesDefaultMaterials()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    CheckDerivedTopologyIsValid(result, "fresh material default derivation input is valid");

    Check(result.topology.sectors.size() == 1, "fresh material default graph derives one sector");
    if (!result.topology.sectors.empty()) {
        const game::SectorTopologySector& sector = result.topology.sectors.front();
        Check(sector.floorTextureId == "floor", "fresh derived sector gets default floor texture");
        Check(sector.ceilingTextureId == "ceiling", "fresh derived sector gets default ceiling texture");
        Check(sector.defaultWall.textureId == "wall", "fresh derived sector gets default wall texture");
        Check(sector.defaultLower.textureId == "wall", "fresh derived sector gets default lower wall texture");
        Check(sector.defaultUpper.textureId == "wall", "fresh derived sector gets default upper wall texture");
    }

    for (const game::SectorTopologySideDef& sideDef : result.topology.sideDefs) {
        Check(sideDef.wall.textureId == "wall", "fresh derived sidedef gets default wall texture");
        Check(sideDef.lower.textureId == "wall", "fresh derived sidedef gets default lower wall texture");
        Check(sideDef.upper.textureId == "wall", "fresh derived sidedef gets default upper wall texture");
    }

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "fresh derived default-material topology builds generated geometry");
    bool sawFloor = false;
    bool sawCeiling = false;
    bool sawWall = false;
    bool sawEmptyTexture = false;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        sawEmptyTexture = sawEmptyTexture || surface.textureId.empty();
        sawFloor = sawFloor || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Floor
                && surface.textureId == "floor");
        sawCeiling = sawCeiling || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Ceiling
                && surface.textureId == "ceiling");
        sawWall = sawWall || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                && surface.textureId == "wall");
    }
    Check(!sawEmptyTexture, "fresh derived generated geometry surfaces have non-empty textures");
    Check(sawFloor, "fresh derived generated geometry has default floor texture");
    Check(sawCeiling, "fresh derived generated geometry has default ceiling texture");
    Check(sawWall, "fresh derived generated geometry has default wall texture");
}

void TestEditorAuthoringGraphMutationMarksDirtyAndStale()
{
    game::SectorEditorState state;
    game::InitializeSectorEditorAuthoringStateFromTopology(state, game::SectorTopologyMap{});
    const uint64_t originalRevision = state.topologyRenderRevision;

    int firstVertexId = -1;
    Check(game::AddSectorAuthoringVertex(state.authoringGraph, 0, 0, &firstVertexId),
          "editor authoring mutation test adds vertex");
    game::MarkSectorEditorAuthoringGraphEdited(state, "authoring graph changed");

    Check(state.topologyDocumentDirty, "graph mutation marks document dirty");
    Check(state.authoringDerivedTopologyStale, "graph mutation marks derived topology stale");
    Check(state.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidNoDerived,
          "graph mutation without last valid topology has invalid/no-derived state");
    Check(!state.topologyRenderCache.valid, "graph mutation invalidates topology render cache");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "graph mutation bumps topology render revision");
}

void TestAuthoringOverlayRenderCacheIncludesLooseGraph()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(!derivation.success, "loose authoring line has no valid derived topology");

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    derivation,
                    7);

    Check(cache.valid, "authoring overlay cache builds without valid derived topology");
    Check(cache.revision == 7, "authoring overlay cache stores revision");
    Check(cache.sectors.empty(), "authoring overlay cache does not require derived sector fills");
    Check(cache.authoringVertices.size() == 2, "authoring overlay cache includes authoring vertices");
    Check(cache.authoringLines.size() == 1, "authoring overlay cache includes loose authoring line");
    if (!cache.authoringLines.empty()) {
        Check(cache.authoringLines.front().lineId == 10, "authoring overlay cache preserves line ID");
        Check(cache.authoringLines.front().validEndpoints, "authoring overlay cache resolves loose line endpoints");
    }
}

void TestAuthoringDiagnosticRenderCacheDoesNotRequireDerivedTopology()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringVertexWithId(graph, 3, 64, 64);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    AddAuthoringLineWithId(graph, 11, 2, 3);

    game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(!derivation.success, "open authoring graph fails derivation for diagnostic cache test");

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    derivation,
                    8);

    Check(!cache.authoringDiagnostics.empty(),
          "authoring diagnostic cache includes failed-derivation diagnostics");
    bool foundPositionedDiagnostic = false;
    for (const game::CachedAuthoringDiagnosticDraw& diagnostic : cache.authoringDiagnostics) {
        if (diagnostic.hasPosition) {
            foundPositionedDiagnostic = true;
            break;
        }
    }
    Check(foundPositionedDiagnostic,
          "authoring diagnostic cache maps at least one diagnostic to an authoring graph position");
}

void TestAuthoringOverlayRenderCacheIncludesReferenceDiagnostics()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringLineWithId(graph, 10, 1, 99);

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    game::SectorAuthoringDerivationResult{},
                    9);

    Check(cache.authoringLines.size() == 1, "authoring overlay cache includes line with missing endpoint");
    if (!cache.authoringLines.empty()) {
        Check(!cache.authoringLines.front().validEndpoints,
              "authoring overlay cache marks missing endpoint line invalid");
        Check(cache.authoringLines.front().hasPartialEndpoint,
              "authoring overlay cache keeps visible partial endpoint for invalid line");
    }
    Check(!cache.authoringDiagnostics.empty(),
          "authoring diagnostic cache includes reference validation diagnostics");
}

void TestEditorAuthoringSuccessfulDerivationUpdatesState()
{
    game::SectorEditorState state;
    state.topologyMap.texturesById.emplace("wall", game::SectorTextureDefinition{"wall", "assets/images/wall.png"});
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::MarkSectorEditorAuthoringGraphEdited(state, "authoring square changed");

    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "successful editor derivation returns true");
    Check(state.authoringDerivation.success, "successful editor derivation stores successful result");
    Check(state.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "successful editor derivation marks state current");
    Check(!state.authoringDerivedTopologyStale, "successful editor derivation clears stale flag");
    Check(state.lastValidAuthoringDerivedTopology.has_value(),
          "successful editor derivation stores memory-only last-valid topology");
    Check(state.topologyMap.sectors.size() == 1,
          "successful editor derivation updates current derived topology");
    Check(state.authoringDerivation.mapping.sectors.size() == 1,
          "successful editor derivation preserves mapping for editor state");
    Check(state.topologyMap.texturesById.find("wall") != state.topologyMap.texturesById.end(),
          "successful editor derivation preserves map-level texture data");
}

void TestEditorAuthoringFailedDerivationKeepsGraphAndDiagnostics()
{
    game::SectorEditorState state;
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "failed editor derivation setup creates last valid topology");
    const game::SectorTopologyMap lastValid = state.topologyMap;

    const int addedLineId = 99;
    AddAuthoringVertexWithId(state.authoringGraph, 5, 128, 0);
    AddAuthoringLineWithId(state.authoringGraph, addedLineId, 2, 5);
    game::MarkSectorEditorAuthoringGraphEdited(state, "dangling authoring line added");

    Check(!game::RefreshSectorEditorAuthoringDerivation(state),
          "failed editor derivation returns false");
    Check(!state.authoringDerivation.success, "failed editor derivation stores failed result");
    Check(state.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed editor derivation keeps invalid/last-valid state");
    Check(state.authoringDerivedTopologyStale, "failed editor derivation leaves stale flag set");
    Check(!state.authoringDerivation.diagnostics.empty(),
          "failed editor derivation records diagnostics");
    Check(game::FindSectorAuthoringLine(state.authoringGraph, addedLineId) != nullptr,
          "failed editor derivation keeps edited graph data");
    Check(state.topologyMap.sectors.size() == lastValid.sectors.size(),
          "failed editor derivation keeps current topology unchanged");
    Check(state.lastValidAuthoringDerivedTopology.has_value(),
          "failed editor derivation keeps memory-only last-valid topology");
}

void TestEditorAuthoringPreviewAndBakeGateAllowsCurrentDerivedTopology()
{
    game::SectorEditorState state;
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "preview gate setup derives a valid topology");

    std::string previewMessage = "not cleared";
    Check(game::CanUseCurrentAuthoringDerivedTopologyForPreview(state, &previewMessage),
          "preview gate allows current valid derived topology");
    Check(previewMessage.empty(), "preview gate clears message when allowed");

    std::string bakeMessage = "not cleared";
    Check(game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &bakeMessage),
          "bake gate allows current valid derived topology");
    Check(bakeMessage.empty(), "bake gate clears message when allowed");
    Check(!state.topologyMap.sectors.empty(), "allowed gate still uses derived topology map");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsInvalidNoDerived()
{
    game::SectorEditorState state;
    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(state, &previewMessage),
          "preview gate rejects invalid/no-derived graph");
    Check(previewMessage.find("no valid derived topology") != std::string::npos,
          "preview gate reports missing derived topology");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &bakeMessage),
          "bake gate rejects invalid/no-derived graph");
    Check(bakeMessage.find("no valid derived topology") != std::string::npos,
          "bake gate reports missing derived topology");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsStaleDerivedTopology()
{
    game::SectorEditorState state;
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "stale gate setup derives a valid topology");
    game::MarkSectorEditorAuthoringGraphEdited(state, "authoring graph edited for stale gate test");

    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(state, &previewMessage),
          "preview gate rejects stale derived topology");
    Check(previewMessage.find("re-derive") != std::string::npos,
          "preview gate reports stale derivation");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &bakeMessage),
          "bake gate rejects stale derived topology");
    Check(bakeMessage.find("re-derive") != std::string::npos,
          "bake gate reports stale derivation");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsFailedDerivation()
{
    game::SectorEditorState state;
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "failed gate setup creates last valid topology");
    AddAuthoringVertexWithId(state.authoringGraph, 5, 128, 0);
    AddAuthoringLineWithId(state.authoringGraph, 99, 2, 5);
    game::MarkSectorEditorAuthoringGraphEdited(state, "dangling authoring line added");
    Check(!game::RefreshSectorEditorAuthoringDerivation(state),
          "failed gate setup records failed derivation");

    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(state, &previewMessage),
          "preview gate rejects failed derivation without crashing");
    Check(previewMessage.find("derivation failed") != std::string::npos,
          "preview gate reports failed derivation");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &bakeMessage),
          "bake gate rejects failed derivation without crashing");
    Check(bakeMessage.find("derivation failed") != std::string::npos,
          "bake gate reports failed derivation");
}

void TestEditorAuthoringSuccessfulDerivationClearsBakedLightmapMetadata()
{
    game::SectorEditorState state;
    state.topologyMap.bakedLightmap.path = "assets/levels/test/lightmap.png";
    state.topologyMap.bakedLightmap.width = 128;
    state.topologyMap.bakedLightmap.height = 128;
    state.topologyMap.bakedLightmap.sourceHash = "old-hash";
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "lightmap stale setup derives valid topology");
    Check(state.topologyMap.bakedLightmap.path.empty(),
          "successful derivation clears baked lightmap path");
    Check(state.topologyMap.bakedLightmap.width == 0 && state.topologyMap.bakedLightmap.height == 0,
          "successful derivation clears baked lightmap dimensions");
    Check(state.topologyMap.bakedLightmap.sourceHash.empty(),
          "successful derivation clears baked lightmap source hash");
}

void TestEditorAuthoringSelectionTargetsRepresentLineAndVertex()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);

    const game::SectorAuthoringSelectionTarget lineTarget =
            game::MakeSectorAuthoringLineSelectionTarget(10);
    const game::SectorAuthoringSelectionTarget vertexTarget =
            game::MakeSectorAuthoringVertexSelectionTarget(1);

    Check(lineTarget.kind == game::SectorAuthoringSelectionKind::Line,
          "authoring selection target can represent a line");
    Check(lineTarget.lineId == 10 && lineTarget.vertexId == -1,
          "authoring line selection target stores only the line ID");
    Check(vertexTarget.kind == game::SectorAuthoringSelectionKind::Vertex,
          "authoring selection target can represent a vertex");
    Check(vertexTarget.vertexId == 1 && vertexTarget.lineId == -1,
          "authoring vertex selection target stores only the vertex ID");
    Check(game::IsSectorAuthoringSelectionTargetValid(state.authoringGraph, lineTarget),
          "authoring line target validates against the graph");
    Check(game::IsSectorAuthoringSelectionTargetValid(state.authoringGraph, vertexTarget),
          "authoring vertex target validates against the graph");
    Check(state.topologyMap.vertices.empty() && state.topologyMap.lineDefs.empty(),
          "authoring selection target helpers do not mutate derived topology");
}

void TestEditorAuthoringSelectionHelpersSetClearAndRejectMissingTargets()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);

    Check(game::SelectSectorEditorAuthoringLine(state, 10),
          "select authoring line helper accepts an existing line");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line,
          "select authoring line helper records line selection");
    Check(!game::SelectSectorEditorAuthoringLine(state, 99),
          "select authoring line helper rejects missing line");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && state.selectedAuthoring.lineId == 10,
          "rejected authoring line selection leaves previous selection intact");
    Check(!game::SelectSectorEditorAuthoringLine(state, 0),
          "select authoring line helper rejects zero line ID");
    Check(!game::SelectSectorEditorAuthoringLine(state, -1),
          "select authoring line helper rejects negative line ID");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && state.selectedAuthoring.lineId == 10,
          "invalid authoring line selection leaves previous selection intact");

    Check(game::SelectSectorEditorAuthoringVertex(state, 1),
          "select authoring vertex helper accepts an existing vertex");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex,
          "select authoring vertex helper records vertex selection");
    Check(!game::SelectSectorEditorAuthoringVertex(state, 99),
          "select authoring vertex helper rejects missing vertex");
    Check(!game::SelectSectorEditorAuthoringVertex(state, 0),
          "select authoring vertex helper rejects zero vertex ID");
    Check(!game::SelectSectorEditorAuthoringVertex(state, -1),
          "select authoring vertex helper rejects negative vertex ID");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && state.selectedAuthoring.vertexId == 1,
          "invalid authoring vertex selection leaves previous selection intact");

    game::ClearSectorEditorAuthoringSelection(state);
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "clear authoring selection helper clears selection kind");
    Check(state.topologyMap.vertices.empty() && state.topologyMap.lineDefs.empty(),
          "authoring selection helpers do not mutate derived topology");
}

void TestEditorAuthoringHoverAndPruneUseGraphValidity()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);

    Check(game::SelectSectorEditorAuthoringLine(state, 10),
          "prune setup selects authoring line");
    Check(game::SetHoveredSectorEditorAuthoringVertex(state, 2),
          "hover authoring vertex helper accepts existing vertex");
    Check(!game::SetHoveredSectorEditorAuthoringVertex(state, 0),
          "hover authoring vertex helper rejects zero vertex ID");
    Check(!game::SetHoveredSectorEditorAuthoringVertex(state, -1),
          "hover authoring vertex helper rejects negative vertex ID");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && state.hoveredAuthoring.vertexId == 2,
          "invalid authoring vertex hover leaves previous hover intact");
    Check(game::SetHoveredSectorEditorAuthoringLine(state, 10),
          "hover authoring line helper accepts existing line");
    Check(!game::SetHoveredSectorEditorAuthoringLine(state, 0),
          "hover authoring line helper rejects zero line ID");
    Check(!game::SetHoveredSectorEditorAuthoringLine(state, -1),
          "hover authoring line helper rejects negative line ID");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && state.hoveredAuthoring.lineId == 10,
          "invalid authoring line hover leaves previous hover intact");
    Check(game::SetHoveredSectorEditorAuthoringVertex(state, 2),
          "prune setup restores authoring vertex hover");

    state.authoringGraph.lines.clear();
    state.authoringGraph.vertices.erase(
            std::remove_if(
                    state.authoringGraph.vertices.begin(),
                    state.authoringGraph.vertices.end(),
                    [](const game::SectorAuthoringVertex& vertex) {
                        return vertex.id == 2;
                    }),
            state.authoringGraph.vertices.end());

    game::PruneSectorEditorAuthoringSelectionToGraph(state);
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection prune clears deleted line selection");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection prune clears deleted vertex hover");
    Check(state.topologyMap.vertices.empty() && state.topologyMap.lineDefs.empty(),
          "authoring hover/prune helpers do not mutate derived topology");
}

void TestEditorAuthoringLineDrawHelperCreatesLooseLineAndMarksDirty()
{
    game::SectorEditorState state;
    game::InitializeSectorEditorAuthoringStateFromTopology(state, game::SectorTopologyMap{});
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = state.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = state.topologyMap.lineDefs.size();

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "authoring line draw helper creates a loose line");

    Check(game::IsValidSectorAuthoringId(lineId), "authoring line draw helper returns a valid line ID");
    Check(state.authoringGraph.vertices.size() == 2,
          "authoring line draw helper creates endpoint vertices");
    Check(state.authoringGraph.lines.size() == 1,
          "authoring line draw helper creates one authoring line");
    const game::SectorAuthoringLine* line =
            game::FindSectorAuthoringLine(state.authoringGraph, lineId);
    Check(line != nullptr && line->startVertexId != line->endVertexId,
          "authoring line draw helper connects distinct endpoint vertices");
    Check(state.topologyDocumentDirty, "authoring line draw helper marks document dirty");
    Check(state.authoringDerivedTopologyStale,
          "authoring line draw helper marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "authoring line draw helper invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "authoring line draw helper bumps topology render revision");
    Check(state.topologyMap.vertices.size() == originalTopologyVertexCount
                  && state.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "authoring line draw helper does not directly mutate derived topology");
}

void TestEditorAuthoringLineDrawHelperReusesVerticesAndRejectsZeroLength()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 7, 0, 0);
    const uint64_t originalRevision = state.topologyRenderRevision;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "authoring line draw helper creates a line from an existing endpoint");
    Check(state.authoringGraph.vertices.size() == 2,
          "authoring line draw helper reuses exact-coordinate endpoint vertex");
    const game::SectorAuthoringLine* line =
            game::FindSectorAuthoringLine(state.authoringGraph, lineId);
    Check(line != nullptr && line->startVertexId == 7,
          "authoring line draw helper connects reused start vertex");

    const std::size_t vertexCount = state.authoringGraph.vertices.size();
    const std::size_t lineCount = state.authoringGraph.lines.size();
    Check(!game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 0}),
          "authoring line draw helper rejects zero-length lines");
    Check(state.authoringGraph.vertices.size() == vertexCount
                  && state.authoringGraph.lines.size() == lineCount,
          "rejected zero-length authoring line leaves graph unchanged");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "rejected zero-length authoring line does not invalidate cache again");
    Check(state.topologyMap.vertices.empty() && state.topologyMap.lineDefs.empty(),
          "authoring line draw helper zero-length rejection does not mutate derived topology");
}

void TestEditorAuthoringLinePickingFindsNearestValidLine()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringVertexWithId(graph, 3, 0, 64);
    AddAuthoringVertexWithId(graph, 4, 64, 64);
    AddAuthoringLineWithId(graph, 20, 1, 2);
    AddAuthoringLineWithId(graph, 10, 3, 4);
    AddAuthoringLineWithId(graph, 30, 1, 99);

    int lineId = -1;
    Check(game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 0.125f},
                  0.25f,
                  &lineId),
          "authoring line picking finds a nearby valid line");
    Check(lineId == 20, "authoring line picking returns the nearest line ID");

    lineId = -1;
    Check(!game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 8.0f},
                  0.25f,
                  &lineId),
          "authoring line picking rejects points outside the threshold");
    Check(lineId == -1, "authoring line picking leaves output unchanged on miss");

    Check(!game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  -1.0f),
          "authoring line picking rejects negative thresholds");
}

void TestEditorAuthoringDeleteSelectedLineOnlyMutatesGraphAndInvalidates()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 3, 64, 64);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);
    AddAuthoringLineWithId(state.authoringGraph, 20, 2, 3);

    game::SectorAuthoringLineSide frontSide;
    frontSide.id.lineId = 10;
    frontSide.id.side = game::SectorTopologySideKind::Front;
    state.authoringGraph.lineSides.push_back(frontSide);
    game::SectorAuthoringLineSide otherSide;
    otherSide.id.lineId = 20;
    otherSide.id.side = game::SectorTopologySideKind::Back;
    state.authoringGraph.lineSides.push_back(otherSide);

    Check(game::SelectSectorEditorAuthoringLine(state, 10),
          "delete selected authoring line setup selects line");
    Check(game::SetHoveredSectorEditorAuthoringLine(state, 10),
          "delete selected authoring line setup hovers line");
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = state.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = state.topologyMap.lineDefs.size();

    Check(game::DeleteSectorEditorSelectedAuthoringLine(state),
          "delete selected authoring line helper deletes the selected line");
    Check(game::FindSectorAuthoringLine(state.authoringGraph, 10) == nullptr,
          "delete selected authoring line removes the line from the graph");
    Check(game::FindSectorAuthoringLine(state.authoringGraph, 20) != nullptr,
          "delete selected authoring line preserves other lines");
    Check(state.authoringGraph.vertices.size() == 3,
          "delete selected authoring line leaves endpoint vertices for vertex pass");
    Check(state.authoringGraph.lineSides.size() == 1
                  && state.authoringGraph.lineSides.front().id.lineId == 20,
          "delete selected authoring line removes side metadata for the deleted line");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete selected authoring line prunes deleted selection");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete selected authoring line prunes deleted hover");
    Check(state.topologyDocumentDirty, "delete selected authoring line marks document dirty");
    Check(state.authoringDerivedTopologyStale,
          "delete selected authoring line marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "delete selected authoring line invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "delete selected authoring line bumps topology render revision");
    Check(state.topologyMap.vertices.size() == originalTopologyVertexCount
                  && state.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "delete selected authoring line does not directly mutate derived topology");

    Check(!game::DeleteSectorEditorSelectedAuthoringLine(state),
          "delete selected authoring line helper rejects missing selection");
}

void TestEditorAuthoringVertexPickingFindsNearestValidVertex()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 20, 0, 0);
    AddAuthoringVertexWithId(graph, 10, 64, 0);

    constexpr float testViewZoom = 48.0f;
    const float practicalPickDistance = game::SectorWorldToAuthoringDistance(
            game::ScreenVertexSnapPixels / testViewZoom);

    int vertexId = -1;
    game::SectorTopologyCoordPoint point{};
    Check(game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(64), 0.125f},
                  0.25f,
                  &vertexId,
                  &point),
          "authoring vertex picking finds a nearby vertex");
    Check(vertexId == 10 && point.x == 64 && point.y == 0,
          "authoring vertex picking returns nearest vertex ID and point");

    vertexId = -1;
    Check(game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(64), 1.0f},
                  practicalPickDistance,
                  &vertexId,
                  &point),
          "authoring vertex picking succeeds within practical screen-space radius");
    Check(vertexId == 10 && point.x == 64 && point.y == 0,
          "authoring practical radius picking returns nearest vertex ID and point");

    vertexId = -1;
    Check(!game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 4.0f},
                  practicalPickDistance,
                  &vertexId),
          "authoring vertex picking rejects points outside threshold");
    Check(vertexId == -1, "authoring vertex picking leaves output unchanged on miss");

    Check(!game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  -1.0f),
          "authoring vertex picking rejects negative thresholds");
}

void TestEditorAuthoringSelectionPickingPrefersVerticesThenLines()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    constexpr float testViewZoom = 48.0f;
    const float practicalPickDistance = game::SectorWorldToAuthoringDistance(
            game::ScreenVertexSnapPixels / testViewZoom);

    game::SectorAuthoringSelectionTarget target;
    game::SectorTopologyCoordPoint vertexPoint{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  0.25f,
                  0.25f,
                  &target,
                  &vertexPoint),
          "authoring selection picking finds an authoring target");
    Check(target.kind == game::SectorAuthoringSelectionKind::Vertex
                  && target.vertexId == 1,
          "authoring selection picking prefers vertex over overlapping line");
    Check(vertexPoint.x == 0 && vertexPoint.y == 0,
          "authoring selection picking returns the picked vertex point");

    target = game::SectorAuthoringSelectionTarget{};
    vertexPoint = game::SectorTopologyCoordPoint{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{1.0f, 0.0f},
                  practicalPickDistance,
                  practicalPickDistance,
                  &target,
                  &vertexPoint),
          "authoring selection practical radius finds an authoring target");
    Check(target.kind == game::SectorAuthoringSelectionKind::Vertex
                  && target.vertexId == 1,
          "authoring selection practical radius prefers vertex over nearby line");
    Check(vertexPoint.x == 0 && vertexPoint.y == 0,
          "authoring selection practical radius returns picked vertex point");

    target = game::SectorAuthoringSelectionTarget{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 0.0f},
                  0.25f,
                  0.25f,
                  &target),
          "authoring selection picking finds an authoring line");
    Check(target.kind == game::SectorAuthoringSelectionKind::Line
                  && target.lineId == 10
                  && target.vertexId == -1,
          "authoring selection picking returns only authoring line targets");

    target = game::SectorAuthoringSelectionTarget{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 1.0f},
                  0.25f,
                  practicalPickDistance,
                  &target),
          "authoring selection picking finds line outside vertex tolerance");
    Check(target.kind == game::SectorAuthoringSelectionKind::Line
                  && target.lineId == 10,
          "authoring selection line fallback works outside vertex tolerance");

    target = game::MakeSectorAuthoringLineSelectionTarget(10);
    Check(!game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 4.0f},
                  practicalPickDistance,
                  practicalPickDistance,
                  &target),
          "authoring selection picking misses empty space");
    Check(target.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection picking clears target on miss");
}

void TestEditorAuthoringMoveVertexUpdatesConnectedLinesAndInvalidates()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 3, 0, 64);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);
    AddAuthoringLineWithId(state.authoringGraph, 20, 3, 1);
    Check(game::SelectSectorEditorAuthoringVertex(state, 1),
          "move authoring vertex setup selects vertex");
    Check(game::SetHoveredSectorEditorAuthoringVertex(state, 1),
          "move authoring vertex setup hovers vertex");
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = state.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = state.topologyMap.lineDefs.size();

    Check(game::MoveSectorEditorAuthoringVertex(
                  state,
                  1,
                  game::SectorTopologyCoordPoint{16, 16}),
          "move authoring vertex helper moves the selected vertex");

    const game::SectorAuthoringVertex* moved =
            game::FindSectorAuthoringVertex(state.authoringGraph, 1);
    Check(moved != nullptr && moved->x == 16 && moved->y == 16,
          "move authoring vertex updates vertex coordinates");
    const game::SectorAuthoringLine* firstLine =
            game::FindSectorAuthoringLine(state.authoringGraph, 10);
    const game::SectorAuthoringLine* secondLine =
            game::FindSectorAuthoringLine(state.authoringGraph, 20);
    Check(firstLine != nullptr && firstLine->startVertexId == 1 && firstLine->endVertexId == 2,
          "move authoring vertex preserves first connected line endpoint IDs");
    Check(secondLine != nullptr && secondLine->startVertexId == 3 && secondLine->endVertexId == 1,
          "move authoring vertex preserves second connected line endpoint IDs");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && state.selectedAuthoring.vertexId == 1,
          "move authoring vertex preserves valid selection");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && state.hoveredAuthoring.vertexId == 1,
          "move authoring vertex preserves valid hover");
    Check(state.topologyDocumentDirty, "move authoring vertex marks document dirty");
    Check(state.hasUnsavedChanges, "move authoring vertex marks unsaved changes");
    Check(state.authoringDerivedTopologyStale,
          "move authoring vertex marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "move authoring vertex invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "move authoring vertex bumps topology render revision");
    Check(state.topologyMap.vertices.size() == originalTopologyVertexCount
                  && state.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "move authoring vertex does not directly mutate derived topology");

    const uint64_t afterMoveRevision = state.topologyRenderRevision;
    Check(!game::MoveSectorEditorAuthoringVertex(
                  state,
                  1,
                  game::SectorTopologyCoordPoint{16, 16}),
          "move authoring vertex helper treats unchanged coordinates as no-op");
    Check(state.topologyRenderRevision == afterMoveRevision,
          "unchanged authoring vertex move does not invalidate again");
}

void TestEditorAuthoringDeleteConnectedVertexIsExplicitlyRejected()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);
    Check(game::SelectSectorEditorAuthoringVertex(state, 1),
          "delete connected authoring vertex setup selects vertex");
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(!game::DeleteSectorEditorSelectedAuthoringVertex(state),
          "delete selected authoring vertex rejects connected vertices");
    Check(game::FindSectorAuthoringVertex(state.authoringGraph, 1) != nullptr,
          "delete connected authoring vertex leaves vertex in graph");
    Check(game::FindSectorAuthoringLine(state.authoringGraph, 10) != nullptr,
          "delete connected authoring vertex leaves connected line in graph");
    Check(!state.topologyDocumentDirty,
          "delete connected authoring vertex does not mark document dirty");
    Check(!state.hasUnsavedChanges,
          "delete connected authoring vertex does not mark unsaved changes");
    Check(state.topologyRenderRevision == originalRevision,
          "delete connected authoring vertex does not invalidate cache");
    Check(state.authoringDerivationStatus == "Authoring vertex is connected; delete its lines first.",
          "delete connected authoring vertex records explicit safe-delete status");
    Check(state.topologyMap.vertices.empty() && state.topologyMap.lineDefs.empty(),
          "delete connected authoring vertex does not directly mutate derived topology");
}

void TestEditorAuthoringDeleteIsolatedVertexOnlyMutatesGraphAndInvalidates()
{
    game::SectorEditorState state;
    AddAuthoringVertexWithId(state.authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(state.authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(state.authoringGraph, 10, 1, 2);
    Check(game::SelectSectorEditorAuthoringVertex(state, 2),
          "delete isolated authoring vertex setup selects vertex");
    Check(game::SetHoveredSectorEditorAuthoringVertex(state, 2),
          "delete isolated authoring vertex setup hovers vertex");
    Check(game::DeleteSectorEditorSelectedAuthoringLine(state) == false,
          "delete isolated authoring vertex setup keeps line because vertex is selected");
    state.authoringGraph.lines.clear();
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = state.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = state.topologyMap.lineDefs.size();

    Check(game::DeleteSectorEditorSelectedAuthoringVertex(state),
          "delete selected authoring vertex deletes isolated vertex");
    Check(game::FindSectorAuthoringVertex(state.authoringGraph, 2) == nullptr,
          "delete isolated authoring vertex removes vertex from graph");
    Check(game::FindSectorAuthoringVertex(state.authoringGraph, 1) != nullptr,
          "delete isolated authoring vertex preserves other vertices");
    Check(state.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete isolated authoring vertex prunes deleted selection");
    Check(state.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete isolated authoring vertex prunes deleted hover");
    Check(state.topologyDocumentDirty, "delete isolated authoring vertex marks document dirty");
    Check(state.hasUnsavedChanges, "delete isolated authoring vertex marks unsaved changes");
    Check(state.authoringDerivedTopologyStale,
          "delete isolated authoring vertex marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "delete isolated authoring vertex invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "delete isolated authoring vertex bumps topology render revision");
    Check(state.topologyMap.vertices.size() == originalTopologyVertexCount
                  && state.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "delete isolated authoring vertex does not directly mutate derived topology");
}

void TestEditorAuthoringEditsRefreshValidCrossingDerivation()
{
    game::SectorEditorState state;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "crossing refresh setup adds south boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "crossing refresh setup adds east boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 64},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "crossing refresh setup adds north boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 64},
                  game::SectorTopologyCoordPoint{0, 0},
                  &lineId),
          "crossing refresh setup adds west boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "crossing refresh setup adds first diagonal");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "crossing refresh setup adds second diagonal");

    Check(state.authoringDerivation.success,
          "completed crossing graph edit refreshes successful derivation");
    Check(state.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "completed crossing graph edit marks derivation current");
    Check(!state.authoringDerivedTopologyStale,
          "completed crossing graph edit clears stale flag");
    Check(state.topologyMap.sectors.size() == 4,
          "completed crossing graph edit derives four sectors");
    Check(state.topologyMap.vertices.size() == 5,
          "completed crossing graph edit derives inserted intersection vertex");
    Check(state.lastValidAuthoringDerivedTopology.has_value(),
          "completed crossing graph edit records last-valid topology");
}

void TestEditorAuthoringFailedRefreshDoesNotReplaceLastValidTopology()
{
    game::SectorEditorState state;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "failed refresh setup adds south boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "failed refresh setup adds east boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 64},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "failed refresh setup adds north boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{0, 64},
                  game::SectorTopologyCoordPoint{0, 0},
                  &lineId),
          "failed refresh setup adds west boundary");

    Check(state.authoringDerivation.success,
          "failed refresh setup creates current derived topology");
    const game::SectorTopologyMap lastValid = state.topologyMap;

    Check(game::AddSectorEditorAuthoringLineSegment(
                  state,
                  game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{128, 0},
                  &lineId),
          "failed refresh test adds dangling line");

    Check(!state.authoringDerivation.success,
          "failed refresh records failed derivation");
    Check(state.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed refresh keeps invalid/last-valid state");
    Check(state.authoringDerivedTopologyStale,
          "failed refresh keeps derived topology stale");
    Check(!state.authoringDerivation.diagnostics.empty(),
          "failed refresh records diagnostics");
    Check(state.topologyMap.sectors.size() == lastValid.sectors.size()
                  && state.topologyMap.lineDefs.size() == lastValid.lineDefs.size()
                  && state.topologyMap.vertices.size() == lastValid.vertices.size(),
          "failed refresh does not replace current derived topology");
    Check(state.lastValidAuthoringDerivedTopology.has_value()
                  && state.lastValidAuthoringDerivedTopology->sectors.size() == lastValid.sectors.size(),
          "failed refresh keeps memory-only last-valid topology");
}

void TestEditorAuthoringToolPaneNamingAndHelpDistinguishGraphAndLegacyTools()
{
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringLine), "Authoring"),
          "authoring line tool name is exposed as an authoring graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringLine), "snapped points"),
          "authoring line tool help describes mouse-driven line creation");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Select), "authoring"),
          "select tool help includes authoring graph selection targets");
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringMove), "Move"),
          "authoring move tool name is exposed as a graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringMove), "authoring vertices"),
          "authoring move tool help describes graph vertex movement");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::Select),
          "select is classified as a graph-authoring tool");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringLine),
          "authoring line is classified as a graph-authoring tool");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringMove),
          "authoring move is classified as a graph-authoring tool");

    Check(TextContains(game::ToolName(game::SectorEditorTool::Sector), "Legacy"),
          "closed-sector polygon tool is labeled as legacy in the tools pane");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Sector), "Legacy"),
          "closed-sector polygon help distinguishes legacy topology mutation");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Move), "legacy topology"),
          "topology move help distinguishes legacy topology mutation");
    Check(!game::IsGraphAuthoringTool(game::SectorEditorTool::Move),
          "legacy topology move is not classified as a graph-authoring tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Erase), "legacy topology"),
          "topology erase help distinguishes legacy topology mutation");
}

void TestEditorAuthoringLastValidTopologyIsNotPersisted()
{
    game::SectorEditorState state;
    state.authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state),
          "last-valid persistence setup derives topology");

    game::SectorAuthoringDocument document;
    document.graph = state.authoringGraph;
    document.mapData = state.topologyMap;
    document.derivation = state.authoringDerivation;

    std::string json;
    std::string error;
    Check(game::SaveSectorAuthoringDocumentToJsonString(document, json, &error),
          "authoring document serializes without editor last-valid state");
    Check(json.find("lastValid") == std::string::npos,
          "serialized authoring document omits editor last-valid topology");
    Check(json.find("authoringGraph") != std::string::npos,
          "serialized authoring document keeps authoring graph source");
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
    TestExtractFacesSingleSquare();
    TestExtractFacesAdjacentSquares();
    TestExtractFacesRectangleCutByLine();
    TestExtractFacesCrossingDiagonals();
    TestExtractFacesOpenChainDiagnostics();
    TestExtractFacesDanglingLineDoesNotCorruptSquare();
    TestExtractFacesRejectsOuterFace();
    TestExtractFacesSliverThreshold();
    TestExtractFacesNestedDisconnectedLoopsAreDeferred();
    TestExtractDisconnectedClosedLoopsProducesFaces();
    TestExtractDisconnectedClosedLoopsWithDanglingLineReportsOnlyDanglingLine();
    TestDeriveSingleSquare();
    TestDeriveAdjacentSquares();
    TestDeriveCrossingDiagonals();
    TestDeriveDisconnectedClosedLoopsProducesSectors();
    TestDeriveNonIntegerIntersectionFails();
    TestDeriveSquareWithMixedLineDirections();
    TestDeriveOpenGraphFails();
    TestDeriveDuplicateLinesFail();
    TestDeriveDiagnosticsUseSpecificKindsAndSources();
    TestDeriveDanglingLineDiagnosticUsesAuthoringLineId();
    TestDeriveDisconnectedClosedLoopsWithDanglingLineFailsCleanly();
    TestDeriveSplitLineMapping();
    TestDeriveFaceAnchorMapping();
    TestDeriveFaceAnchorDiagnostics();
    TestDeriveInvalidSideProjectionDiagnostic();
    TestDeriveProjectsFaceAnchorProperties();
    TestDeriveProjectsSideMaterialsAndLineFlags();
    TestDeriveSplitLineDuplicatesProjectedProperties();
    TestDeriveUnresolvedAnchorPreservesAuthoringProperties();
    TestDerivedTopologyBuildsGeometry();
    TestFreshDerivedTopologyUsesDefaultMaterials();
    TestEditorAuthoringGraphMutationMarksDirtyAndStale();
    TestAuthoringOverlayRenderCacheIncludesLooseGraph();
    TestAuthoringDiagnosticRenderCacheDoesNotRequireDerivedTopology();
    TestAuthoringOverlayRenderCacheIncludesReferenceDiagnostics();
    TestEditorAuthoringSuccessfulDerivationUpdatesState();
    TestEditorAuthoringFailedDerivationKeepsGraphAndDiagnostics();
    TestEditorAuthoringPreviewAndBakeGateAllowsCurrentDerivedTopology();
    TestEditorAuthoringPreviewAndBakeGateRejectsInvalidNoDerived();
    TestEditorAuthoringPreviewAndBakeGateRejectsStaleDerivedTopology();
    TestEditorAuthoringPreviewAndBakeGateRejectsFailedDerivation();
    TestEditorAuthoringSuccessfulDerivationClearsBakedLightmapMetadata();
    TestEditorAuthoringSelectionTargetsRepresentLineAndVertex();
    TestEditorAuthoringSelectionHelpersSetClearAndRejectMissingTargets();
    TestEditorAuthoringHoverAndPruneUseGraphValidity();
    TestEditorAuthoringLineDrawHelperCreatesLooseLineAndMarksDirty();
    TestEditorAuthoringLineDrawHelperReusesVerticesAndRejectsZeroLength();
    TestEditorAuthoringLinePickingFindsNearestValidLine();
    TestEditorAuthoringDeleteSelectedLineOnlyMutatesGraphAndInvalidates();
    TestEditorAuthoringVertexPickingFindsNearestValidVertex();
    TestEditorAuthoringSelectionPickingPrefersVerticesThenLines();
    TestEditorAuthoringMoveVertexUpdatesConnectedLinesAndInvalidates();
    TestEditorAuthoringDeleteConnectedVertexIsExplicitlyRejected();
    TestEditorAuthoringDeleteIsolatedVertexOnlyMutatesGraphAndInvalidates();
    TestEditorAuthoringEditsRefreshValidCrossingDerivation();
    TestEditorAuthoringFailedRefreshDoesNotReplaceLastValidTopology();
    TestEditorAuthoringToolPaneNamingAndHelpDistinguishGraphAndLegacyTools();
    TestEditorAuthoringLastValidTopologyIsNotPersisted();

    if (failures != 0) {
        std::cerr << failures << " authoring graph test(s) failed\n";
        return 1;
    }

    std::cout << "Sector authoring graph tests passed\n";
    return 0;
}
