#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <set>
#include <string>
#include <vector>

namespace game {

bool ReconcileSectorEditorAuthoringCandidateDoors(
        const SectorTopologyMap& currentMap,
        const SectorAuthoringDerivationResult& currentDerivation,
        const SectorAuthoringDerivationResult& candidateDerivation,
        const std::set<int>& removedAuthoringLineIds,
        const std::set<int>& rejectDoorAuthoringLineIds,
        SectorTopologyMap& candidateMapData,
        std::vector<int>& outRemovedDoorIds,
        std::string& outError);

bool ValidateSectorEditorAuthoringCandidateDoorPortalSpans(
        const SectorTopologyMap& currentMap,
        const SectorAuthoringDerivationResult& currentDerivation,
        const SectorAuthoringGraph& candidateGraph,
        std::string& outError);

} // namespace game
