#include "game/dialogue/SectorConversation.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/components/AnimatedModel.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace game {
namespace {
constexpr float TalkingDistance = 1.5f;
constexpr float TurnSeconds = 0.75f;
constexpr float MaximumPreparationSeconds = 2.0f;

float SafeDt(float dt) { return std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.1f) : 0.0f; }
float Turn(float start, float target, float blend)
{
    return start + std::remainder(target - start, 2.0f * PI) * blend;
}
}

bool IsConversationNpcAvailable(const engine::World& world, engine::Entity entity)
{
    return world.IsAlive(entity) && world.Has<NpcRuntimeInstance>(entity)
            && world.Has<SectorObjectTransform>(entity)
            && !world.Get<NpcRuntimeInstance>(entity).hostile
            && (!world.Has<Health>(entity) || !IsDepleted(world.Get<Health>(entity)))
            && (!world.Has<NpcCombatState>(entity) || !world.Get<NpcCombatState>(entity).dead);
}

Vector3 SectorConversationTalkPoint(engine::World& world, engine::AssetManager& assets,
        engine::Entity npc)
{
    const auto& transform = world.Get<SectorObjectTransform>(npc);
    const float scale = world.Has<SectorDynamicModel>(npc)
            ? world.Get<SectorDynamicModel>(npc).scale : 1.0f;
    Vector3 local{0, 1.53f, 0};
    if (world.Has<engine::AnimatedModelInstance>(npc)) {
        const auto* asset = assets.GetModelAsset(world.Get<engine::AnimatedModelInstance>(npc).model);
        if (asset && asset->hasLocalBounds) {
            // Stable model bounds avoid following every gesture's animated bounds.
            const auto& bounds = asset->localBounds;
            local = {(bounds.min.x + bounds.max.x) * 0.5f,
                    bounds.min.y + (bounds.max.y - bounds.min.y) * 0.85f,
                    (bounds.min.z + bounds.max.z) * 0.5f};
        }
    }
    return Vector3Transform(local, BuildSectorStaticModelAuthoredTransform(transform.position,
            transform.rotationXRadians, transform.yawRadians, transform.rotationZRadians, scale));
}

Vector2 PrepareSectorConversationFrame(SectorConversationState& state,
        const SectorFpsControllerState& player, const SectorFpsControllerConfig& config,
        Vector3 npcPosition, const SectorCollisionWorld* collision, float rawDt)
{
    state.requestedDelta = {};
    if (!state.active || !state.preparing) return {};
    const float dt = SafeDt(rawDt);
    state.elapsed += dt;
    state.previousPlayerXZ = {player.feetPosition.x, player.feetPosition.z};
    const Vector2 away{player.feetPosition.x - npcPosition.x, player.feetPosition.z - npcPosition.z};
    const float distance = Vector2Length(away);
    if (state.retreatStopped || distance >= TalkingDistance - 0.02f || distance < 0.001f
            || !player.grounded || !collision || dt <= 0) return {};
    const Vector2 direction = Vector2Scale(away, 1.0f / distance);
    const Vector2 facing{std::cos(player.yawRadians), std::sin(player.yawRadians)};
    // Turn toward the NPC first if necessary, never toward the retreat route.
    if (Vector2DotProduct(facing, direction) > -0.8f) return {};
    state.retreatElapsed += dt;
    const float remaining = TalkingDistance - distance;
    const float speed = std::min({1.0f, state.retreatElapsed / 0.2f, remaining / 0.2f});
    const Vector2 delta = Vector2Scale(direction, std::min(remaining, speed * dt));
    const Vector2 destination = Vector2Add(state.previousPlayerXZ, delta);
    const int sector = collision->FindSectorContainingPointPreferCurrent(destination, player.currentSectorId);
    SectorCollisionHeights heights;
    const SectorCollisionVerticalQuery query{destination, player.feetPosition.y,
            config.playerRadius, config.playerHeight, config.stepHeight, true};
    if (sector == 0 || !collision->ResolveActorVerticalContext(sector, query, &heights)
            || heights.floorZ < player.feetPosition.y - 0.05f
            || heights.floorZ > player.feetPosition.y + config.stepHeight
            || !collision->AllowsPrismPlacement(destination, config.playerRadius,
                    heights.floorZ, heights.floorZ + config.playerHeight, sector)) {
        state.retreatStopped = true;
        return {};
    }
    state.requestedDelta = delta;
    return delta;
}

bool FinishSectorConversationFrame(SectorConversationState& state,
        SectorFpsControllerState& player, const SectorFpsControllerConfig& config,
        Vector3 npcPosition, Vector3 talkPoint, float& npcYaw, float rawDt)
{
    if (!state.active || !state.preparing) return false;
    const float dt = SafeDt(rawDt);
    const float t = std::clamp(state.elapsed / TurnSeconds, 0.0f, 1.0f);
    const float blend = t * t * (3.0f - 2.0f * t);
    const Vector3 delta = Vector3Subtract(talkPoint, SectorFpsControllerEyePosition(player, config));
    const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (horizontal > 0.001f) {
        player.yawRadians = Turn(state.startYaw, std::atan2(delta.z, delta.x), blend);
        player.pitchRadians = ClampSectorFpsPitch(state.startPitch
                + (std::atan2(delta.y, horizontal) - state.startPitch) * blend);
    }
    const Vector2 fromNpc{player.feetPosition.x - npcPosition.x, player.feetPosition.z - npcPosition.z};
    if (Vector2Length(fromNpc) > 0.001f)
        npcYaw = Turn(state.npcStartYaw, std::atan2(fromNpc.x, fromNpc.y), blend);
    const float moved = Vector2Distance(state.previousPlayerXZ, {player.feetPosition.x, player.feetPosition.z});
    if (Vector2Length(state.requestedDelta) > 0.0001f && moved < Vector2Length(state.requestedDelta) * 0.1f)
        state.stalledSeconds += dt;
    else state.stalledSeconds = 0;
    if (state.stalledSeconds >= 0.2f) state.retreatStopped = true;
    const bool positioned = state.retreatStopped || !player.grounded
            || Vector2Length(fromNpc) >= TalkingDistance - 0.02f;
    if ((t >= 1.0f && positioned) || state.elapsed >= MaximumPreparationSeconds) {
        state.preparing = false;
        return true;
    }
    return false;
}
} // namespace game
