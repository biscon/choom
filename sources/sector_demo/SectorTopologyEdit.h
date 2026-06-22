#pragma once

#include "sector_demo/SectorTopologyCreation.h"

#include <string>

namespace game {

struct SectorTopologySplitLineResult {
    int newVertexId = -1;
    int midpointVertexId = -1;

    int firstLineDefId = -1;
    int secondLineDefId = -1;

    int firstFrontSideDefId = -1;
    int firstBackSideDefId = -1;

    int secondFrontSideDefId = -1;
    int secondBackSideDefId = -1;
};

struct SectorTopologyDeleteSectorResult {
    int deletedSectorId = -1;
    int removedSideDefCount = 0;
    int removedLineDefCount = 0;
    int removedVertexCount = 0;
};

struct SectorTopologyMergeVerticesResult {
    int mergedVertexId = 0;
    int removedVertexId = 0;
};

struct SectorTopologyDissolveVertexResult {
    int removedVertexId = 0;
    int replacementLineDefId = 0;
    int replacementFrontSideDefId = 0;
    int replacementBackSideDefId = 0;
};

struct SectorTopologyBoundaryCutPoint {
    int vertexId = -1;
    int lineDefId = -1;
    SectorTopologyCoordPoint point;
};

struct SectorTopologyCutSectorResult {
    int originalSectorId = -1;
    int newSectorId = -1;
    int firstEndpointVertexId = -1;
    int secondEndpointVertexId = -1;
    int cutLineDefId = -1;
    int originalSectorSideDefId = -1;
    int newSectorSideDefId = -1;
};

bool MoveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint newPosition,
        std::string* outError = nullptr);

bool MergeSectorTopologyVertices(
        SectorTopologyMap& map,
        int sourceVertexId,
        int targetVertexId,
        SectorTopologyMergeVerticesResult* outResult = nullptr,
        std::string* outError = nullptr);

bool DissolveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyDissolveVertexResult* outResult = nullptr,
        std::string* outError = nullptr);

bool SplitSectorTopologyLineDef(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologySplitLineResult* outResult = nullptr,
        std::string* outError = nullptr);

bool SplitSectorTopologyLineDefAtPoint(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologyCoordPoint point,
        SectorTopologySplitLineResult* outResult = nullptr,
        std::string* outError = nullptr);

bool DeleteSectorTopologySector(
        SectorTopologyMap& map,
        int sectorId,
        SectorTopologyDeleteSectorResult* outResult = nullptr,
        std::string* outError = nullptr);

bool ValidateSectorTopologySectorBoundaryCutPoint(
        const SectorTopologyMap& map,
        int sectorId,
        SectorTopologyBoundaryCutPoint point,
        std::string* outError = nullptr);

bool CutSectorTopologySectorBetweenBoundaryPoints(
        SectorTopologyMap& map,
        int sectorId,
        SectorTopologyBoundaryCutPoint firstPoint,
        SectorTopologyBoundaryCutPoint secondPoint,
        SectorTopologyCutSectorResult* outResult = nullptr,
        std::string* outError = nullptr);

} // namespace game
