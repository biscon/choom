#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <vector>

namespace game {

struct SelectionState {
    TopologySelectionKind topologySelectionKind = TopologySelectionKind::None;
    int selectedTopologySectorId = -1;
    int selectedTopologyVertexId = -1;
    int selectedTopologySideDefId = -1;
    int selectedTopologyLineDefId = -1;
    SectorTopologySideKind selectedTopologySideKind = SectorTopologySideKind::Front;
    TopologyWallPart selectedTopologyWallPart = TopologyWallPart::Wall;
    TopologyMaterialLayer activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
    int selectedTopologyLightId = -1;
    int selectedTopologyStaticSpotLightId = -1;
    int selectedTopologyDynamicLightId = -1;
    int selectedTopologyDynamicSpotLightId = -1;
    int selectedRuntimeObjectId = -1;
    int inspectedTopologyVertexId = -1;
    SectorAuthoringSelectionTarget selectedAuthoring;
    std::vector<int> selectedAuthoringFaceAnchorIds;
    int hoveredTopologyLightId = -1;
    int hoveredTopologyStaticSpotLightId = -1;
    int hoveredTopologyDynamicLightId = -1;
    int hoveredTopologyDynamicSpotLightId = -1;
    bool hasHoveredVertex = false;
    int hoveredTopologyVertexId = -1;
    SectorTopologyCoordPoint hoveredTopologyVertexPoint = {};
    SectorAuthoringSelectionTarget hoveredAuthoring;
};

} // namespace game
