#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
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

struct RuntimePortalDynamicBlocker {
    int lineDefId = -1;
    int sideDefId = -1;
    int fromSectorId = -1;
    int toSectorId = -1;
    bool blocksPortal = false;
};

struct RuntimePortalVisibilityResult {
    int startSectorId = -1;
    std::vector<int> startSectorIds;
    std::vector<int> visibleSectorIds;
    // Static geometry immediately behind a visible dynamically blocked portal.
    // These sectors are terminal: they are drawn/pickable but do not expose
    // runtime objects, lights, atmosphere, or further portal traversal.
    std::vector<int> boundarySurfaceSectorIds;
    std::vector<int> traversedPortalLineDefIds;
    size_t totalSectorCount = 0;
    bool validStartSector = false;
    bool fallbackDrawAll = false;
    std::string mode;
    std::string status;
};

inline bool ShouldDrawRuntimeSectorForVisibility(
        int sectorId,
        const RuntimePortalVisibilityResult& visibility)
{
    if (!visibility.validStartSector || visibility.fallbackDrawAll) {
        return true;
    }
    if (sectorId <= 0) {
        return false;
    }
    return std::binary_search(
            visibility.visibleSectorIds.begin(),
            visibility.visibleSectorIds.end(),
            sectorId);
}

inline bool ShouldDrawRuntimeSectorGeometryForVisibility(
        int sectorId,
        const RuntimePortalVisibilityResult& visibility)
{
    if (ShouldDrawRuntimeSectorForVisibility(sectorId, visibility)) {
        return true;
    }
    if (!visibility.validStartSector
            || visibility.fallbackDrawAll
            || sectorId <= 0) {
        return false;
    }
    return std::binary_search(
            visibility.boundarySurfaceSectorIds.begin(),
            visibility.boundarySurfaceSectorIds.end(),
            sectorId);
}

bool BuildRuntimeSectorVisibilityGraph(
        const SectorTopologyMap& map,
        RuntimeSectorVisibilityGraph& outGraph,
        std::string* outError = nullptr);

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibility(
        const RuntimeSectorVisibilityGraph& graph,
        int startSectorId,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers = nullptr);

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibilityFromSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        const std::vector<int>& startSectorIds,
        int preferredStartSectorId = 0,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers = nullptr);

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromPoint(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        int preferredStartSectorId = 0,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers = nullptr);

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromView(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        int preferredStartSectorId = 0,
        size_t iterationCap = 0,
        float visibilitySeedRadiusWorld = 0.0f,
        float eyeYWorld = 0.0f,
        bool validateEyeY = false,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers = nullptr);

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromViewSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        const std::vector<int>& startSectorIds,
        int preferredStartSectorId = 0,
        size_t iterationCap = 0,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers = nullptr);

bool IsRuntimePortalDynamicallyBlocked(
        const RuntimePortalEdge& edge,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers);

float ClampRuntimeVisibilitySeedRadiusWorld(float playerRadiusWorld);

float ComputeRuntimePortalVisibilityHorizontalFovRadians(
        float verticalFovRadians,
        float aspectRatio,
        float pitchRadians);

std::string FormatRuntimePortalVisibilityDebugText(
        const RuntimePortalVisibilityResult& result);

const RuntimeSectorNode* FindRuntimeSectorVisibilityNode(
        const RuntimeSectorVisibilityGraph& graph,
        int sectorId);

} // namespace game
