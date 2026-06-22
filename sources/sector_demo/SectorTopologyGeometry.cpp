#include "sector_demo/SectorTopologyGeometry.h"

#include <algorithm>

namespace game {
namespace {

__int128 Cross(
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

} // namespace

bool SectorTopologyPointOnSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b)
{
    return Cross(a, b, point) == 0
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

} // namespace game
