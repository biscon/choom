#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <cstddef>
#include <string>
#include <vector>

namespace game {

class SectorCollisionWorld;

struct RuntimePortalEdge {
    int lineDefId = -1;
    int sideDefId = -1;
    int fromSectorId = -1;
    int toSectorId = -1;
    // Portal segment endpoints are stored in rendered world XZ units.
    Vector2 a{};
    Vector2 b{};
    // Vertical interval is stored in rendered world Y units.
    float openBottom = 0.0f;
    float openTop = 0.0f;
    bool open = false;
};

struct RuntimeSectorNode {
    int sectorId = -1;
    std::vector<int> outgoingPortalEdgeIndices;
};

struct RuntimeSectorVisibilityGraph {
    std::vector<RuntimeSectorNode> sectors;
    std::vector<RuntimePortalEdge> portals;
};

struct RuntimePortalVisibilityResult {
    int startSectorId = -1;
    std::vector<int> visibleSectorIds;
    std::vector<int> traversedPortalLineDefIds;
    size_t totalSectorCount = 0;
    bool validStartSector = false;
    bool fallbackDrawAll = false;
    std::string mode;
    std::string status;
};

bool BuildRuntimeSectorVisibilityGraph(
        const SectorTopologyMap& map,
        RuntimeSectorVisibilityGraph& outGraph,
        std::string* outError = nullptr);

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibility(
        const RuntimeSectorVisibilityGraph& graph,
        int startSectorId);

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromPoint(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        int preferredStartSectorId = 0);

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromView(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        int preferredStartSectorId = 0,
        size_t iterationCap = 0);

std::string FormatRuntimePortalVisibilityDebugText(
        const RuntimePortalVisibilityResult& result);

const RuntimeSectorNode* FindRuntimeSectorVisibilityNode(
        const RuntimeSectorVisibilityGraph& graph,
        int sectorId);

} // namespace game
