#pragma once

#include "sector_demo/SectorTopologyCreation.h"

namespace game {

enum class SectorTopologySegmentIntersectionKind {
    None,
    Touch,
    Proper,
    CollinearOverlap
};

enum class SectorTopologyPointContainment {
    Outside,
    Inside,
    Boundary
};

__int128 SectorTopologyCross(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c);

bool SectorTopologyPointOnSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b);

bool SectorTopologyPointStrictlyInsideSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b);

SectorTopologySegmentIntersectionKind SectorTopologySegmentIntersection(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c,
        SectorTopologyCoordPoint d);

SectorTopologyPointContainment SectorTopologyClassifyPointInPolygon(
        const std::vector<SectorTopologyCoordPoint>& polygon,
        SectorTopologyCoordPoint point);

} // namespace game
