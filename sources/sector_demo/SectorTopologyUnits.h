#pragma once

#include <raylib.h>

#include <cstdint>

namespace game {

using SectorCoord = int32_t;

constexpr int SectorCoordSubdivisions = 16;

float SectorCoordToVisibleAuthoring(SectorCoord value);
bool VisibleAuthoringToSectorCoord(float value, SectorCoord& outValue);

float SectorCoordToWorldDistance(SectorCoord value);
Vector2 SectorCoordToWorldPosition2(SectorCoord x, SectorCoord y);
Vector3 SectorCoordToWorldPosition3(SectorCoord x, float heightAuthoring, SectorCoord y);

} // namespace game
