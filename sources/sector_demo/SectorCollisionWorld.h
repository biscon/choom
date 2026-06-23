#pragma once

#include "sector_demo/SectorTopologyTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorTopologyMap;

struct SectorCollisionHeights {
    float floorZ = 0.0f;
    float ceilingZ = 0.0f;
};

enum class SectorCollisionEdgeKind {
    BlockingWall,
    Portal
};

struct SectorCollisionEdge {
    Vector2 a = {};
    Vector2 b = {};
    int lineDefId = 0;
    int sideDefId = 0;
    int sectorId = 0;
    int neighborSectorId = 0;
    SectorCollisionEdgeKind kind = SectorCollisionEdgeKind::BlockingWall;
};

struct SectorCollisionLoop {
    std::vector<Vector2> points;
    std::vector<int> vertexIds;
    std::vector<int> sideDefIds;
};

struct SectorCollisionSector {
    int sectorId = 0;
    SectorCollisionHeights heights;
    SectorCollisionLoop outerLoop;
    std::vector<SectorCollisionLoop> holeLoops;
    std::vector<SectorCollisionEdge> edges;
    std::vector<int> portalNeighbors;
};

class SectorCollisionWorld {
public:
    bool BuildFromTopology(const SectorTopologyMap& map, std::string* errorMessage = nullptr);

    const SectorCollisionSector* FindSector(int sectorId) const;
    bool GetSectorFloorCeiling(int sectorId, SectorCollisionHeights* out) const;
    const std::vector<SectorCollisionEdge>* GetSectorEdges(int sectorId) const;
    const std::vector<int>* GetPortalNeighbors(int sectorId) const;

    int FindSectorContainingPoint(Vector2 xz) const;
    int FindSectorContainingPointPreferCurrent(Vector2 xz, int currentSectorId) const;

private:
    bool SectorContainsPoint(const SectorCollisionSector& sector, Vector2 xz) const;

    std::vector<SectorCollisionSector> sectors;
};

} // namespace game
