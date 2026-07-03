#pragma once

#include "sector_demo/SectorMath.h"

#include <raylib.h>

#include <algorithm>
#include <limits>

namespace game {

struct SectorAabb3 {
    Vector3 min = {};
    Vector3 max = {};
};

inline SectorAabb3 EmptySectorAabb3()
{
    return SectorAabb3{
            Vector3{
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()},
            Vector3{
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()}};
}

inline SectorAabb3 SectorAabb3FromPoint(Vector3 point)
{
    return SectorAabb3{point, point};
}

inline bool IsFiniteSectorAabb3(const SectorAabb3& bounds)
{
    return IsFiniteVector3(bounds.min) && IsFiniteVector3(bounds.max);
}

inline bool IsValidSectorAabb3(const SectorAabb3& bounds)
{
    return IsFiniteSectorAabb3(bounds)
            && bounds.min.x <= bounds.max.x
            && bounds.min.y <= bounds.max.y
            && bounds.min.z <= bounds.max.z;
}

inline void ExpandSectorAabb3(SectorAabb3& bounds, Vector3 point)
{
    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

inline Vector3 SectorAabb3Center(const SectorAabb3& bounds)
{
    return Vector3{
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f,
            (bounds.min.z + bounds.max.z) * 0.5f};
}

inline Vector3 SectorAabb3Extents(const SectorAabb3& bounds)
{
    return Vector3{
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z};
}

inline Vector3 ClosestPointOnSectorAabb3(const SectorAabb3& bounds, Vector3 point)
{
    return Vector3{
            std::clamp(point.x, bounds.min.x, bounds.max.x),
            std::clamp(point.y, bounds.min.y, bounds.max.y),
            std::clamp(point.z, bounds.min.z, bounds.max.z)};
}

} // namespace game
