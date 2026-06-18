#pragma once

#include <raylib.h>

namespace game {

constexpr float kSectorAuthoringUnitsPerLegacyWorldUnit = 8.0f;
constexpr float kSectorWorldUnitsPerAuthoringUnit = 1.0f / kSectorAuthoringUnitsPerLegacyWorldUnit;

inline float SectorAuthoringToWorldDistance(float authoringDistance)
{
    return authoringDistance * kSectorWorldUnitsPerAuthoringUnit;
}

inline float SectorWorldToAuthoringDistance(float worldDistance)
{
    return worldDistance * kSectorAuthoringUnitsPerLegacyWorldUnit;
}

inline Vector2 SectorAuthoringToWorldPosition(Vector2 authoringPosition)
{
    return Vector2{
            SectorAuthoringToWorldDistance(authoringPosition.x),
            SectorAuthoringToWorldDistance(authoringPosition.y)
    };
}

inline Vector2 SectorWorldToAuthoringPosition(Vector2 worldPosition)
{
    return Vector2{
            SectorWorldToAuthoringDistance(worldPosition.x),
            SectorWorldToAuthoringDistance(worldPosition.y)
    };
}

inline Vector3 SectorAuthoringToWorldPosition(Vector3 authoringPosition)
{
    return Vector3{
            SectorAuthoringToWorldDistance(authoringPosition.x),
            SectorAuthoringToWorldDistance(authoringPosition.y),
            SectorAuthoringToWorldDistance(authoringPosition.z)
    };
}

inline Vector3 SectorWorldToAuthoringPosition(Vector3 worldPosition)
{
    return Vector3{
            SectorWorldToAuthoringDistance(worldPosition.x),
            SectorWorldToAuthoringDistance(worldPosition.y),
            SectorWorldToAuthoringDistance(worldPosition.z)
    };
}

inline Vector3 SectorAuthoringToWorldPosition(float authoringX, float authoringHeight, float authoringY)
{
    return Vector3{
            SectorAuthoringToWorldDistance(authoringX),
            SectorAuthoringToWorldDistance(authoringHeight),
            SectorAuthoringToWorldDistance(authoringY)
    };
}

} // namespace game
