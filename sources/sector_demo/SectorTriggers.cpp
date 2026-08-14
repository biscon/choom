#include "sector_demo/SectorTriggers.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game {
namespace {

using WideCoord = __int128_t;

WideCoord Cross(SectorTriggerPoint a, SectorTriggerPoint b, SectorTriggerPoint c)
{
    return (static_cast<WideCoord>(b.x) - a.x) * (static_cast<WideCoord>(c.z) - a.z)
            - (static_cast<WideCoord>(b.z) - a.z) * (static_cast<WideCoord>(c.x) - a.x);
}

bool OnSegment(SectorTriggerPoint a, SectorTriggerPoint b, SectorTriggerPoint p)
{
    return Cross(a, b, p) == 0
            && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x)
            && p.z >= std::min(a.z, b.z) && p.z <= std::max(a.z, b.z);
}

bool SegmentsIntersect(SectorTriggerPoint a, SectorTriggerPoint b,
                       SectorTriggerPoint c, SectorTriggerPoint d)
{
    const WideCoord abC = Cross(a, b, c);
    const WideCoord abD = Cross(a, b, d);
    const WideCoord cdA = Cross(c, d, a);
    const WideCoord cdB = Cross(c, d, b);
    if (((abC > 0 && abD < 0) || (abC < 0 && abD > 0))
            && ((cdA > 0 && cdB < 0) || (cdA < 0 && cdB > 0))) {
        return true;
    }
    return (abC == 0 && OnSegment(a, b, c))
            || (abD == 0 && OnSegment(a, b, d))
            || (cdA == 0 && OnSegment(c, d, a))
            || (cdB == 0 && OnSegment(c, d, b));
}

} // namespace

bool ValidateSectorTriggerPolygon(
        const std::vector<SectorTriggerPoint>& points,
        SectorTriggerShapeKind shape,
        std::string* outError)
{
    const auto fail = [outError](const char* message) {
        if (outError != nullptr) *outError = message;
        return false;
    };
    if (points.size() < 3) return fail("trigger needs at least three points");
    if (shape == SectorTriggerShapeKind::Rectangle && points.size() != 4) {
        return fail("rectangle trigger needs exactly four points");
    }
    WideCoord twiceArea = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        const SectorTriggerPoint a = points[i];
        const SectorTriggerPoint b = points[(i + 1) % points.size()];
        if (a.x == b.x && a.z == b.z) return fail("trigger has a zero-length edge");
        twiceArea += static_cast<WideCoord>(a.x) * b.z
                - static_cast<WideCoord>(b.x) * a.z;
    }
    if (twiceArea == 0) return fail("trigger area must be non-zero");
    for (size_t i = 0; i < points.size(); ++i) {
        const size_t iNext = (i + 1) % points.size();
        for (size_t j = i + 1; j < points.size(); ++j) {
            const size_t jNext = (j + 1) % points.size();
            if (i == j || iNext == j || jNext == i) continue;
            if (SegmentsIntersect(points[i], points[iNext], points[j], points[jNext])) {
                return fail("trigger polygon must not self-intersect");
            }
        }
    }
    if (shape == SectorTriggerShapeKind::Rectangle) {
        for (size_t i = 0; i < 4; ++i) {
            const SectorTriggerPoint a = points[i];
            const SectorTriggerPoint b = points[(i + 1) % 4];
            if (a.x != b.x && a.z != b.z) {
                return fail("rectangle trigger edges must be axis-aligned");
            }
        }
    }
    if (outError != nullptr) outError->clear();
    return true;
}

bool SectorTriggerContainsAuthoringPoint(
        const std::vector<SectorTriggerPoint>& points,
        float authoringX,
        float authoringZ)
{
    if (points.size() < 3
            || !std::isfinite(authoringX)
            || !std::isfinite(authoringZ)) {
        return false;
    }
    const double px = static_cast<double>(authoringX) * SectorCoordSubdivisions;
    const double pz = static_cast<double>(authoringZ) * SectorCoordSubdivisions;
    bool inside = false;
    for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
        const double ax = points[j].x;
        const double az = points[j].z;
        const double bx = points[i].x;
        const double bz = points[i].z;
        const double cross = (bx - ax) * (pz - az) - (bz - az) * (px - ax);
        if (std::fabs(cross) <= 1e-7
                && px >= std::min(ax, bx) && px <= std::max(ax, bx)
                && pz >= std::min(az, bz) && pz <= std::max(az, bz)) {
            return true;
        }
        if ((az > pz) != (bz > pz)) {
            const double intersectionX = ax + (pz - az) * (bx - ax) / (bz - az);
            if (px < intersectionX) inside = !inside;
        }
    }
    return inside;
}

bool SectorTriggerContainsWorldPoint(
        const std::vector<SectorTriggerPoint>& points,
        float worldX,
        float worldZ)
{
    if (!std::isfinite(worldX) || !std::isfinite(worldZ)) return false;
    const Vector2 authoring = SectorWorldToAuthoringPosition(Vector2{worldX, worldZ});
    return SectorTriggerContainsAuthoringPoint(points, authoring.x, authoring.y);
}

} // namespace game
