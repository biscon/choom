#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

bool ValidateSectorTriggerPolygon(
        const std::vector<SectorTriggerPoint>& points,
        SectorTriggerShapeKind shape,
        std::string* outError = nullptr);

bool SectorTriggerContainsAuthoringPoint(
        const std::vector<SectorTriggerPoint>& points,
        float authoringX,
        float authoringZ);

bool SectorTriggerContainsWorldPoint(
        const std::vector<SectorTriggerPoint>& points,
        float worldX,
        float worldZ);

} // namespace game
