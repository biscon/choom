#include "game/navigation/SectorNavigationDebugDraw.h"

#include "engine/render/ColorTransfer.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>
#include <raymath.h>

namespace game {
namespace {

Color LinearOverlaySwatch(Color color)
{
    return engine::SrgbColorBytesToLinearSceneUnorm(color);
}

Vector3 OffsetNavigationSurfaceDebugPoint(Vector3 point)
{
    point.y += 0.005f;
    return point;
}

} // namespace

void DrawSectorNavigationDebugWorld(
        const SectorNavigationDebugDrawSettings& settings,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        const SectorMeshRenderer& renderer)
{
    if (!renderer.IsRendererReady()) return;
    const SectorNavigationDebugCache& debug = navigation.DebugCache();
    if (debug.navigationRevision == 0) return;

    const Color surfaceColor = LinearOverlaySwatch(Color{62, 202, 132, 72});
    const Color edgeColor = LinearOverlaySwatch(Color{88, 242, 164, 225});
    const Color tileColor = LinearOverlaySwatch(Color{84, 156, 255, 180});
    const Color obstacleColor = LinearOverlaySwatch(Color{255, 154, 72, 235});
    const Color dynamicObstacleColor = LinearOverlaySwatch(Color{255, 92, 92, 245});
    const Color pendingObstacleColor = LinearOverlaySwatch(Color{255, 214, 78, 245});
    const Color suppressedObstacleColor = LinearOverlaySwatch(Color{156, 166, 180, 220});
    const Color doorColor = LinearOverlaySwatch(Color{244, 214, 78, 245});
    const Color stepColor = LinearOverlaySwatch(Color{104, 226, 255, 245});
    const Color pathColor = LinearOverlaySwatch(Color{255, 102, 214, 245});
    const Color agentColor = LinearOverlaySwatch(Color{255, 238, 132, 245});
    const Color preferredColor = LinearOverlaySwatch(Color{96, 220, 255, 245});
    BeginMode3D(renderer.RenderCamera());
    if (settings.showSurface) {
        for (const SectorNavigationDebugTriangle& triangle : debug.walkableTriangles) {
            const Vector3 a = OffsetNavigationSurfaceDebugPoint(triangle.a);
            const Vector3 b = OffsetNavigationSurfaceDebugPoint(triangle.b);
            const Vector3 c = OffsetNavigationSurfaceDebugPoint(triangle.c);
            DrawTriangle3D(a, b, c, surfaceColor);
            DrawTriangle3D(c, b, a, surfaceColor);
        }
    }
    if (settings.showEdges) {
        for (const SectorNavigationDebugSegment& edge : debug.polygonEdges) {
            DrawLine3D(OffsetNavigationSurfaceDebugPoint(edge.a),
                    OffsetNavigationSurfaceDebugPoint(edge.b), edgeColor);
        }
    }
    if (settings.showTileBounds) {
        for (const SectorNavigationDebugTileBounds& tile : debug.tileBounds) {
            DrawBoundingBox(tile.bounds, tileColor);
        }
    }
    const auto drawObstacle = [](const SectorNavigationDebugObstacle& obstacle,
                                 Color color) {
            const auto corner = [&](float x, float z, float y) {
                return Vector3{
                        obstacle.center.x + obstacle.axisX.x * x
                                + obstacle.axisZ.x * z,
                        y,
                        obstacle.center.y + obstacle.axisX.y * x
                                + obstacle.axisZ.y * z};
            };
            const float ex = obstacle.halfExtents.x;
            const float ez = obstacle.halfExtents.y;
            const Vector3 vertices[8]{
                    corner(-ex, -ez, obstacle.bottom),
                    corner(ex, -ez, obstacle.bottom),
                    corner(ex, ez, obstacle.bottom),
                    corner(-ex, ez, obstacle.bottom),
                    corner(-ex, -ez, obstacle.top),
                    corner(ex, -ez, obstacle.top),
                    corner(ex, ez, obstacle.top),
                    corner(-ex, ez, obstacle.top)};
            constexpr int edges[12][2] = {
                    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (const auto& edge : edges) {
                DrawLine3D(vertices[edge[0]], vertices[edge[1]], color);
            }
    };
    if (settings.showStaticObstacles) {
        for (const SectorNavigationDebugObstacle& obstacle : debug.staticObstacles) {
            drawObstacle(obstacle, obstacleColor);
        }
    }
    if (settings.showDynamicObstacles) {
        for (const SectorNavigationDebugDynamicObstacle& obstacle :
                debug.dynamicObstacles) {
            const Color color = obstacle.state
                            == SectorNavigationDynamicObstacleState::Active
                    ? dynamicObstacleColor
                    : obstacle.state
                                    == SectorNavigationDynamicObstacleState::FastSuppressed
                              || obstacle.state
                                    == SectorNavigationDynamicObstacleState::Failed
                            ? suppressedObstacleColor
                            : pendingObstacleColor;
            drawObstacle(obstacle, color);
        }
    }
    if (settings.showDoorPlaceholders) {
        for (const SectorNavigationDebugDoorPlaceholder& door : debug.doorPlaceholders) {
            Vector3 aBottom = door.a;
            Vector3 bBottom = door.b;
            Vector3 aTop = door.a;
            Vector3 bTop = door.b;
            aBottom.y = bBottom.y = door.bottom;
            aTop.y = bTop.y = door.top;
            DrawLine3D(aBottom, bBottom, doorColor);
            DrawLine3D(aTop, bTop, doorColor);
            DrawLine3D(aBottom, aTop, doorColor);
            DrawLine3D(bBottom, bTop, doorColor);
        }
        for (const SectorNavigationDebugDoorLink& link : debug.doorLinks) {
            const Color linkColor = LinearOverlaySwatch(
                    link.state == SectorNavigationDoorLinkState::Clear
                            ? Color{82, 232, 132, 245}
                            : link.state == SectorNavigationDoorLinkState::Disabled
                                    ? Color{236, 72, 72, 245}
                                    : Color{244, 174, 68, 245});
            DrawLine3D(link.frontStage, link.backStage, linkColor);
            DrawSphere(link.frontStage, 0.06f, linkColor);
            DrawSphere(link.backStage, 0.06f, linkColor);
        }
    }
    if (settings.showStepConnections) {
        for (const SectorNavigationDebugSegment& connection : debug.stepConnections) {
            DrawLine3D(connection.a, connection.b, stepColor);
            DrawSphere(connection.a, 0.035f, stepColor);
            DrawSphere(connection.b, 0.035f, stepColor);
        }
    }
    for (const NpcNavigationRecord& agent : npcNavigation.records) {
        if (!agent.occupied
                || (settings.showFocusedNpcOnly
                    && agent.placedObjectId != settings.focusedPlacedObjectId)) {
            continue;
        }
        if (settings.showNpcAgents) {
            const float height = navigation.Settings().agentHeight;
            const float radius = navigation.Settings().agentRadius;
            const Vector3 top{
                    agent.physicalPosition.x,
                    agent.physicalPosition.y + height,
                    agent.physicalPosition.z};
            DrawCylinderWiresEx(
                    agent.physicalPosition,
                    top,
                    radius,
                    radius,
                    12,
                    agentColor);
            DrawLine3D(
                    agent.physicalPosition,
                    Vector3Add(
                            agent.physicalPosition,
                            Vector3{agent.preferredVelocity.x * 0.2f, 0.0f,
                                    agent.preferredVelocity.y * 0.2f}),
                    preferredColor);
            DrawLine3D(
                    agent.physicalPosition,
                    Vector3Add(
                            agent.physicalPosition,
                            Vector3{agent.desiredVelocity.x * 0.2f, 0.0f,
                                    agent.desiredVelocity.y * 0.2f}),
                    pathColor);
            DrawLine3D(
                    agent.physicalPosition,
                    Vector3Add(
                            agent.physicalPosition,
                            Vector3{agent.actualVelocity.x * 0.2f, 0.0f,
                                    agent.actualVelocity.y * 0.2f}),
                    agentColor);
            DrawLine3D(agent.physicalPosition, agent.visualPosition, stepColor);
        }
        if (settings.showNpcPaths && agent.nextCorner < agent.cornerCount) {
            Vector3 previous = agent.physicalPosition;
            for (size_t cornerIndex = agent.nextCorner;
                    cornerIndex < agent.cornerCount;
                    ++cornerIndex) {
                DrawLine3D(previous, agent.corners[cornerIndex], pathColor);
                DrawSphere(agent.corners[cornerIndex], 0.045f, pathColor);
                previous = agent.corners[cornerIndex];
            }
            if (agent.doorPhase != NpcDoorTraversalPhase::None) {
                DrawLine3D(agent.corners[agent.nextCorner],
                        agent.doorLanding, doorColor);
                DrawSphere(agent.doorLanding, 0.06f, doorColor);
            }
        }
    }
    EndMode3D();
}

} // namespace game
