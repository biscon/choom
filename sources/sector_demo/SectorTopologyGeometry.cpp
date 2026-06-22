#include "sector_demo/SectorTopologyGeometry.h"

#include <algorithm>

namespace game {
namespace {

int Sign(__int128 value)
{
    return (value > 0) - (value < 0);
}

bool RangesOverlapWithLength(SectorCoord a, SectorCoord b, SectorCoord c, SectorCoord d)
{
    return std::max(std::min(a, b), std::min(c, d))
           < std::min(std::max(a, b), std::max(c, d));
}

} // namespace

__int128 SectorTopologyCross(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c)
{
    const int64_t abX = static_cast<int64_t>(b.x) - static_cast<int64_t>(a.x);
    const int64_t abY = static_cast<int64_t>(b.y) - static_cast<int64_t>(a.y);
    const int64_t acX = static_cast<int64_t>(c.x) - static_cast<int64_t>(a.x);
    const int64_t acY = static_cast<int64_t>(c.y) - static_cast<int64_t>(a.y);
    return static_cast<__int128>(abX) * static_cast<__int128>(acY)
           - static_cast<__int128>(abY) * static_cast<__int128>(acX);
}

bool SectorTopologyPointOnSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b)
{
    return SectorTopologyCross(a, b, point) == 0
           && point.x >= std::min(a.x, b.x)
           && point.x <= std::max(a.x, b.x)
           && point.y >= std::min(a.y, b.y)
           && point.y <= std::max(a.y, b.y);
}

bool SectorTopologyPointStrictlyInsideSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b)
{
    return SectorTopologyPointOnSegment(point, a, b)
           && !(point.x == a.x && point.y == a.y)
           && !(point.x == b.x && point.y == b.y);
}

SectorTopologySegmentIntersectionKind SectorTopologySegmentIntersection(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c,
        SectorTopologyCoordPoint d)
{
    const int abC = Sign(SectorTopologyCross(a, b, c));
    const int abD = Sign(SectorTopologyCross(a, b, d));
    const int cdA = Sign(SectorTopologyCross(c, d, a));
    const int cdB = Sign(SectorTopologyCross(c, d, b));

    if (abC == 0 && abD == 0 && cdA == 0 && cdB == 0) {
        const bool hasPositiveOverlap = RangesOverlapWithLength(a.x, b.x, c.x, d.x)
                                        || RangesOverlapWithLength(a.y, b.y, c.y, d.y);
        if (hasPositiveOverlap) {
            return SectorTopologySegmentIntersectionKind::CollinearOverlap;
        }
        if (SectorTopologyPointOnSegment(c, a, b)
                || SectorTopologyPointOnSegment(d, a, b)
                || SectorTopologyPointOnSegment(a, c, d)
                || SectorTopologyPointOnSegment(b, c, d)) {
            return SectorTopologySegmentIntersectionKind::Touch;
        }
        return SectorTopologySegmentIntersectionKind::None;
    }

    if (abC * abD < 0 && cdA * cdB < 0) {
        return SectorTopologySegmentIntersectionKind::Proper;
    }
    if ((abC == 0 && SectorTopologyPointOnSegment(c, a, b))
            || (abD == 0 && SectorTopologyPointOnSegment(d, a, b))
            || (cdA == 0 && SectorTopologyPointOnSegment(a, c, d))
            || (cdB == 0 && SectorTopologyPointOnSegment(b, c, d))) {
        return SectorTopologySegmentIntersectionKind::Touch;
    }
    return SectorTopologySegmentIntersectionKind::None;
}

SectorTopologyPointContainment SectorTopologyClassifyPointInPolygon(
        const std::vector<SectorTopologyCoordPoint>& polygon,
        SectorTopologyCoordPoint point)
{
    bool inside = false;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const SectorTopologyCoordPoint a = polygon[i];
        const SectorTopologyCoordPoint b = polygon[(i + 1) % polygon.size()];
        if (SectorTopologyPointOnSegment(point, a, b)) {
            return SectorTopologyPointContainment::Boundary;
        }

        if ((a.y > point.y) != (b.y > point.y)) {
            const __int128 orientation = SectorTopologyCross(a, b, point);
            const bool crossesToRight = b.y > a.y ? orientation > 0 : orientation < 0;
            if (crossesToRight) {
                inside = !inside;
            }
        }
    }
    return inside ? SectorTopologyPointContainment::Inside : SectorTopologyPointContainment::Outside;
}

} // namespace game
