#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cstddef>

namespace game {

struct SectorEditorAssetPruneOptions {
    bool pruneTextures = true;
    bool pruneSounds = true;
};

struct SectorEditorAssetPruneResult {
    std::size_t removedTextureCount = 0;
    std::size_t removedSoundCount = 0;
};

SectorEditorAssetPruneResult PruneUnusedSectorEditorAssets(
        const SectorAuthoringGraph& authoringGraph,
        SectorTopologyMap& map,
        const SectorEditorAssetPruneOptions& options);

} // namespace game
