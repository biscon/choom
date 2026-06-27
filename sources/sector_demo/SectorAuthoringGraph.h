#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorAuthoringVertex {
    int id = -1;
    SectorCoord x = 0;
    SectorCoord y = 0;
};

struct SectorAuthoringLineSpecialData {
    int type = 0;
    std::string tag;
};

struct SectorAuthoringLine {
    int id = -1;
    int startVertexId = -1;
    int endVertexId = -1;
    SectorTopologyLineDefFlags flags;
    SectorAuthoringLineSpecialData special;
};

struct SectorAuthoringSideId {
    int lineId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
};

struct SectorAuthoringLineSide {
    SectorAuthoringSideId id;
    SectorTopologyWallPartSettings wall;
    SectorTopologyWallPartSettings lower;
    SectorTopologyWallPartSettings upper;
    SectorTopologyWallPartSettings middle;
};

struct SectorAuthoringFaceAnchor {
    int id = -1;
    std::string name;
    SectorCoord x = 0;
    SectorCoord y = 0;

    float floorZ = 0.0f;
    float ceilingZ = 24.0f;

    std::string floorTextureId;
    std::string ceilingTextureId;
    bool ceilingSky = false;

    SectorTopologyUvSettings floorUv;
    SectorTopologyUvSettings ceilingUv;
    SectorTopologyDecalLayer floorDecal;
    SectorTopologyDecalLayer ceilingDecal;

    Color ambientColor = WHITE;
    float ambientIntensity = 1.0f;

    SectorTopologyWallPartSettings defaultWall;
    SectorTopologyWallPartSettings defaultLower;
    SectorTopologyWallPartSettings defaultUpper;
};

struct SectorAuthoringGraph {
    std::vector<SectorAuthoringVertex> vertices;
    std::vector<SectorAuthoringLine> lines;
    std::vector<SectorAuthoringLineSide> lineSides;
    std::vector<SectorAuthoringFaceAnchor> faceAnchors;
};

enum class SectorAuthoringValidationSeverity {
    Warning,
    Error
};

enum class SectorAuthoringObjectKind {
    Graph,
    Vertex,
    Line,
    Side,
    FaceAnchor
};

struct SectorAuthoringValidationIssue {
    SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error;
    SectorAuthoringObjectKind objectKind = SectorAuthoringObjectKind::Graph;
    int objectId = -1;
    std::string message;
};

struct SectorAuthoringPlanarRational {
    int64_t numerator = 0;
    int64_t denominator = 1;
};

struct SectorAuthoringPlanarPoint {
    SectorAuthoringPlanarRational x;
    SectorAuthoringPlanarRational y;
};

struct SectorAuthoringPlanarVertex {
    int id = -1;
    SectorAuthoringPlanarPoint point;
    int sourceVertexId = -1;
};

struct SectorAuthoringPlanarEdge {
    int id = -1;
    int startVertexId = -1;
    int endVertexId = -1;
    int sourceLineId = -1;
    bool followsSourceLineDirection = true;
};

enum class SectorAuthoringPlanarDiagnosticKind {
    MissingVertex,
    ZeroLengthLine,
    DuplicateLine,
    CollinearOverlap,
    NearMiss,
    CoincidentEndpoint
};

struct SectorAuthoringPlanarDiagnostic {
    SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error;
    SectorAuthoringPlanarDiagnosticKind kind = SectorAuthoringPlanarDiagnosticKind::MissingVertex;
    int lineId = -1;
    int otherLineId = -1;
    std::string message;
};

struct SectorAuthoringPlanarizationResult {
    std::vector<SectorAuthoringPlanarVertex> vertices;
    std::vector<SectorAuthoringPlanarEdge> edges;
    std::vector<SectorAuthoringPlanarDiagnostic> diagnostics;
};

bool IsValidSectorAuthoringId(int id);

int AllocateSectorAuthoringVertexId(const SectorAuthoringGraph& graph);
int AllocateSectorAuthoringLineId(const SectorAuthoringGraph& graph);
int AllocateSectorAuthoringFaceAnchorId(const SectorAuthoringGraph& graph);

const SectorAuthoringVertex* FindSectorAuthoringVertex(const SectorAuthoringGraph& graph, int id);
SectorAuthoringVertex* FindSectorAuthoringVertex(SectorAuthoringGraph& graph, int id);

const SectorAuthoringLine* FindSectorAuthoringLine(const SectorAuthoringGraph& graph, int id);
SectorAuthoringLine* FindSectorAuthoringLine(SectorAuthoringGraph& graph, int id);

const SectorAuthoringLineSide* FindSectorAuthoringLineSide(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSideId id);
SectorAuthoringLineSide* FindSectorAuthoringLineSide(
        SectorAuthoringGraph& graph,
        SectorAuthoringSideId id);

const SectorAuthoringFaceAnchor* FindSectorAuthoringFaceAnchor(
        const SectorAuthoringGraph& graph,
        int id);
SectorAuthoringFaceAnchor* FindSectorAuthoringFaceAnchor(SectorAuthoringGraph& graph, int id);

bool SectorAuthoringSideIdsEqual(SectorAuthoringSideId lhs, SectorAuthoringSideId rhs);
SectorAuthoringSideId OppositeSectorAuthoringSideId(SectorAuthoringSideId id);

bool AddSectorAuthoringVertex(
        SectorAuthoringGraph& graph,
        SectorCoord x,
        SectorCoord y,
        int* outVertexId = nullptr);

bool AddSectorAuthoringLine(
        SectorAuthoringGraph& graph,
        int startVertexId,
        int endVertexId,
        int* outLineId = nullptr);

std::vector<SectorAuthoringValidationIssue> ValidateSectorAuthoringGraphReferences(
        const SectorAuthoringGraph& graph);

bool HasSectorAuthoringValidationErrors(
        const std::vector<SectorAuthoringValidationIssue>& issues);

bool SectorAuthoringPlanarRationalsEqual(
        SectorAuthoringPlanarRational lhs,
        SectorAuthoringPlanarRational rhs);
bool SectorAuthoringPlanarPointsEqual(
        const SectorAuthoringPlanarPoint& lhs,
        const SectorAuthoringPlanarPoint& rhs);
bool SectorAuthoringPlanarRationalIsInteger(SectorAuthoringPlanarRational value);
SectorCoord SectorAuthoringPlanarRationalToSectorCoord(SectorAuthoringPlanarRational value);

SectorAuthoringPlanarizationResult PlanarizeSectorAuthoringGraph(
        const SectorAuthoringGraph& graph);

SectorAuthoringGraph ImportSectorTopologyMapToAuthoringGraph(const SectorTopologyMap& map);

} // namespace game
