#pragma once

#include "sector_demo/SectorTopologyTypes.h"

namespace game {

enum class TopologyWallPart {
    Wall,
    Lower,
    Upper,
    Middle
};

enum class TopologyMaterialLayer {
    Base,
    Decal
};

enum class TopologySectorTextureField {
    None,
    Floor,
    Ceiling,
    DefaultWall,
    DefaultLower,
    DefaultUpper
};

enum class TopologySurfaceEditTargetKind {
    None,
    SectorFloor,
    SectorCeiling,
    SideDefWall,
    SideDefLower,
    SideDefUpper,
    SideDefMiddle
};

struct TopologySurfaceEditTarget {
    TopologySurfaceEditTargetKind kind = TopologySurfaceEditTargetKind::None;
    int sectorId = -1;
    int lineDefId = -1;
    int sideDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
};

} // namespace game
