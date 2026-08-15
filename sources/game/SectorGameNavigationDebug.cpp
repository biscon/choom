#include "game/SectorGameNavigationDebug.h"

#include "engine/assets/AssetManager.h"
#include "engine/ui/UI.h"
#include "game/SectorScriptBindings.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcRuntime.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <string_view>

namespace game {
namespace {

const NpcNavigationRecord* FindFocusedNpc(
        const NpcNavigationRuntime& runtime,
        std::string_view preferredInstanceId)
{
    if (!preferredInstanceId.empty()) {
        for (const NpcNavigationRecord& record : runtime.records) {
            if (record.occupied && record.instanceId == preferredInstanceId) {
                return &record;
            }
        }
    }
    for (const NpcNavigationRecord& record : runtime.records) {
        if (record.occupied
                && record.authority == NpcMoveAuthority::Script
                && record.phase == NpcMovePhase::FollowingPath) {
            return &record;
        }
    }
    return nullptr;
}

} // namespace

void DrawSectorGameNavigationDebugPanel(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        const SectorScriptHost& scriptHost)
{
    engine::UIConfig textConfig = config;
    textConfig.fontSize = 20.0f;
    textConfig.textSpacing = 1.0f;
    textConfig.paddingX = 8.0f;
    textConfig.paddingY = 4.0f;
    const float panelWidth = 620.0f;
    const float panelHeight = 460.0f;
    const Rectangle bounds = config.overlayBounds;
    const Rectangle panel{
            bounds.x + std::max(16.0f, bounds.width - panelWidth - 24.0f),
            bounds.y + 24.0f,
            panelWidth,
            panelHeight};
    DrawRectangleRec(panel, Color{12, 15, 20, 220});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    float y = panel.y + 10.0f;
    const float rowHeight = 29.0f;
    const auto line = [&](const char* text, Color color) {
        engine::Text(
                textConfig,
                assets,
                Rectangle{panel.x + 12.0f, y, panel.width - 24.0f, rowHeight},
                smallFont,
                text,
                engine::UITextJustify::Left,
                color,
                true);
        y += rowHeight;
    };

    line("Nav diagnostics (F8)", textConfig.accentColor);
    line(TextFormat(
            "state %s | stage %s | revision %llu",
            SectorNavigationStateName(navigation.State()),
            SectorNavigationBuildStageName(navigation.BuildStage()),
            static_cast<unsigned long long>(
                    navigation.DebugCache().navigationRevision)),
            textConfig.textColor);
    line(TextFormat(
            "agent radius %.2f | height %.2f | climb %.2f (map step)",
            navigation.Settings().agentRadius,
            navigation.Settings().agentHeight,
            navigation.Settings().agentMaximumClimb),
            textConfig.textColor);
    size_t clearDoorLinks = 0;
    size_t openingDoorLinks = 0;
    size_t disabledDoorLinks = 0;
    uint32_t doorHolders = 0;
    for (const SectorNavigationDebugDoorLink& link :
            navigation.DebugCache().doorLinks) {
        doorHolders += link.holderCount;
        if (link.state == SectorNavigationDoorLinkState::Clear) ++clearDoorLinks;
        else if (link.state == SectorNavigationDoorLinkState::Disabled) ++disabledDoorLinks;
        else ++openingDoorLinks;
    }
    line(TextFormat(
            "doors %zu clear | %zu require open | %zu disabled | %u holders",
            clearDoorLinks, openingDoorLinks, disabledDoorLinks, doorHolders),
            textConfig.textColor);
    const SectorNavigationDynamicObstacleStatistics& obstacleStats =
            navigation.DynamicObstacleStatistics();
    line(TextFormat(
            "obstacles %zu active | %zu pending | %zu fast | %llu tile updates",
            obstacleStats.activeCount,
            obstacleStats.pendingCount + obstacleStats.removingCount,
            obstacleStats.fastSuppressedCount,
            static_cast<unsigned long long>(obstacleStats.updatedTiles)),
            textConfig.textColor);
    const SectorNavigationCrowdStatistics& crowd =
            navigation.CrowdStatistics();
    line(TextFormat(
            "Crowd %zu / %zu | high avoidance | %d velocity samples",
            crowd.activeAgentCount,
            navigation.Capacities().agentCapacity,
            crowd.lastVelocitySampleCount),
            textConfig.textColor);
    line(TextFormat(
            "Crowd sync %llu | failures %llu | %.3f / %.3f ms",
            static_cast<unsigned long long>(crowd.reconciliations),
            static_cast<unsigned long long>(crowd.attachmentFailures),
            crowd.lastUpdateMilliseconds,
            crowd.peakUpdateMilliseconds),
            textConfig.textColor);
    line(TextFormat(
            "script moves %zu active | %llu ok | %llu failed | %llu cancelled",
            static_cast<size_t>(std::count_if(
                    scriptHost.npcMoves.begin(),
                    scriptHost.npcMoves.end(),
                    [](const SectorScriptNpcMove& move) {
                        return move.active;
                    })),
            static_cast<unsigned long long>(
                    scriptHost.npcMoveDiagnostics.successes),
            static_cast<unsigned long long>(
                    scriptHost.npcMoveDiagnostics.failures),
            static_cast<unsigned long long>(
                    scriptHost.npcMoveDiagnostics.cancellations)),
            textConfig.textColor);
    line(TextFormat(
            "latest %s | %s",
            scriptHost.npcMoveDiagnostics.lastInstanceId.data(),
            scriptHost.npcMoveDiagnostics.lastOutcome.data()),
            textConfig.mutedTextColor);

    const NpcNavigationRecord* focused = FindFocusedNpc(
            npcNavigation,
            scriptHost.npcMoveDiagnostics.lastInstanceId.data());
    if (focused == nullptr) {
        line("focused NPC: none", textConfig.mutedTextColor);
        return;
    }
    line(TextFormat(
            "%s | %s | %s | %s | request %llu",
            focused->instanceId.c_str(),
            NpcMoveAuthorityName(focused->authority),
            NpcMovePhaseName(focused->phase),
            NpcMoveGaitName(focused->gait),
            static_cast<unsigned long long>(focused->requestId)),
            textConfig.textColor);
    line(TextFormat(
            "destination %.2f %.2f | %zu corners | query %s",
            focused->requestedDestinationXZ.x,
            focused->requestedDestinationXZ.y,
            focused->cornerCount > focused->nextCorner
                    ? focused->cornerCount - focused->nextCorner : 0,
            SectorNavigationQueryStatusName(focused->lastQueryStatus)),
            textConfig.textColor);
    line(TextFormat(
            "preferred %.2f | steered %.2f | actual %.2f",
            Vector2Length(focused->preferredVelocity),
            Vector2Length(focused->desiredVelocity),
            Vector2Length(focused->actualVelocity)),
            textConfig.textColor);
    line(TextFormat(
            "neighbors %d | nearest %.2f | player avoid %s | stall %.2f | replans %u",
            focused->crowdNeighborCount,
            focused->crowdNearestNeighborDistance,
            focused->playerAvoidanceActive ? "active" : "off",
            focused->stallSeconds,
            focused->replanCount),
            textConfig.textColor);
    line(TextFormat(
            "door %d | %s | %s | wait %.2fs | %s",
            focused->doorId,
            NpcDoorTraversalPhaseName(focused->doorPhase),
            SectorNavigationDoorDirectionName(focused->doorDirection),
            focused->doorWaitSeconds,
            focused->holdsDoor ? "holding" : "not holding"),
            textConfig.textColor);
    line(focused->diagnostic.data(), textConfig.mutedTextColor);
}

} // namespace game
