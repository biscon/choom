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

} // namespace game
