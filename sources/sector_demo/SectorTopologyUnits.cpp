#include "sector_demo/SectorTopologyUnits.h"

#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <limits>

namespace game {

float SectorCoordToVisibleAuthoring(SectorCoord value)
{
    return static_cast<float>(value) / static_cast<float>(SectorCoordSubdivisions);
}

bool VisibleAuthoringToSectorCoord(float value, SectorCoord& outValue)
{
    if (!std::isfinite(value)) {
        return false;
    }

    const double scaledValue = std::round(
            static_cast<double>(value) * static_cast<double>(SectorCoordSubdivisions));
    if (scaledValue < static_cast<double>(std::numeric_limits<SectorCoord>::min())
        || scaledValue > static_cast<double>(std::numeric_limits<SectorCoord>::max())) {
        return false;
    }

    outValue = static_cast<SectorCoord>(scaledValue);
    return true;
}

float SectorCoordToWorldDistance(SectorCoord value)
{
    return SectorAuthoringToWorldDistance(SectorCoordToVisibleAuthoring(value));
}

Vector2 SectorCoordToWorldPosition2(SectorCoord x, SectorCoord y)
{
    return Vector2{
            SectorCoordToWorldDistance(x),
            SectorCoordToWorldDistance(y)
    };
}

Vector3 SectorCoordToWorldPosition3(SectorCoord x, float heightAuthoring, SectorCoord y)
{
    return Vector3{
            SectorCoordToWorldDistance(x),
            SectorAuthoringToWorldDistance(heightAuthoring),
            SectorCoordToWorldDistance(y)
    };
}

} // namespace game
