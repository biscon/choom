#pragma once

#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorEditorTopologyActionResult {
    bool changed = false;
    std::string status;
};

struct SectorEditorAddStaticLightResult {
    bool changed = false;
    int lightId = -1;
    std::string status;
};

struct SectorEditorMergeVerticesResult {
    bool changed = false;
    SectorTopologyMergeVerticesResult merge;
    std::string status;
};

struct SectorEditorDissolveVertexResult {
    bool changed = false;
    SectorTopologyDissolveVertexResult dissolve;
    std::string status;
};

struct SectorEditorSplitLineDefResult {
    bool changed = false;
    SectorTopologySplitLineResult split;
    std::string status;
};

struct SectorEditorCreateSectorResult {
    bool changed = false;
    int sectorId = -1;
    std::string status;
};

struct SectorEditorDeleteSectorResult {
    bool changed = false;
    SectorTopologyDeleteSectorResult deleted;
    std::string status;
};

struct SectorEditorCutSectorResult {
    bool changed = false;
    SectorTopologyCutSectorResult cut;
    std::string status;
};

struct SectorEditorJoinSectorsResult {
    bool changed = false;
    SectorTopologyJoinSectorsResult join;
    std::string status;
};

SectorEditorTopologyActionResult MoveTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint originalPoint,
        SectorTopologyCoordPoint targetPoint);

SectorEditorMergeVerticesResult MergeTopologyVertices(
        SectorTopologyMap& map,
        int sourceVertexId,
        int targetVertexId);

SectorEditorDissolveVertexResult DissolveTopologyVertex(
        SectorTopologyMap& map,
        int vertexId);

SectorEditorSplitLineDefResult SplitTopologyLineDef(
        SectorTopologyMap& map,
        int lineDefId);

SectorEditorSplitLineDefResult SplitTopologyLineDefAtPoint(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologyCoordPoint point);

SectorEditorCreateSectorResult CreateTopologySector(
        SectorTopologyMap& map,
        const std::vector<SectorTopologyCoordPoint>& points,
        const SectorTopologyCreatePolygonOptions& options);

SectorEditorCreateSectorResult InsertTopologySectorInside(
        SectorTopologyMap& map,
        int parentSectorId,
        const std::vector<SectorTopologyCoordPoint>& points,
        const SectorTopologyInsertPolygonOptions& options);

SectorEditorDeleteSectorResult DeleteTopologySector(
        SectorTopologyMap& map,
        int sectorId);

SectorEditorCutSectorResult CutTopologySector(
        SectorTopologyMap& map,
        int sectorId,
        SectorTopologyBoundaryCutPoint firstPoint,
        SectorTopologyBoundaryCutPoint secondPoint);

SectorEditorJoinSectorsResult JoinTopologySectors(
        SectorTopologyMap& map,
        int winnerSectorId,
        int otherSectorId);

SectorEditorAddStaticLightResult AddStaticLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteStaticLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorTopologyActionResult FinishMoveStaticLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition);

SectorEditorTopologyActionResult SetPortalBlocksPlayer(
        SectorTopologyMap& map,
        int lineDefId,
        bool blocksPlayer);

} // namespace game
