#pragma once

namespace game {

class SectorMeshRenderer;
class SectorNavigationWorld;
struct NpcNavigationRuntime;

struct SectorNavigationDebugDrawSettings {
    bool showSurface = false;
    bool showEdges = true;
    bool showTileBounds = false;
    bool showStaticObstacles = true;
    bool showDynamicObstacles = true;
    bool showDoorPlaceholders = true;
    bool showStepConnections = true;
    bool showNpcPaths = true;
    bool showNpcAgents = true;
    bool showFocusedNpcOnly = false;
    int focusedPlacedObjectId = 0;
};

void DrawSectorNavigationDebugWorld(
        const SectorNavigationDebugDrawSettings& settings,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        const SectorMeshRenderer& renderer);

} // namespace game
