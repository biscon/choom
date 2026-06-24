#pragma once

#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>

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
