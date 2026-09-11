#pragma once

#include "engine/scripting/ScriptData.h"
#include "engine/ecs/Entity.h"
#include <raylib.h>

namespace engine { class World; class AssetManager; }
namespace game {
class SectorCollisionWorld;
struct SectorFpsControllerState;
struct SectorFpsControllerConfig;

struct SectorConversationState {
    engine::Entity npc = engine::NullEntity();
    engine::ScriptTaskHandle owner{};
    engine::ScriptOperationHandle preparation{};
    uint64_t token = 0;
    bool active = false;
    bool reposition = true;
    bool preparing = false;
    bool acquiredControls = false;
    bool retreatStopped = false;
    float elapsed = 0;
    float retreatElapsed = 0;
    float stalledSeconds = 0;
    float startYaw = 0;
    float startPitch = 0;
    float npcStartYaw = 0;
    Vector2 previousPlayerXZ{};
    Vector2 requestedDelta{};
};

bool IsConversationNpcAvailable(const engine::World& world, engine::Entity entity);
Vector3 SectorConversationTalkPoint(engine::World& world, engine::AssetManager& assets,
        engine::Entity npc);
// Preparation produces a backwards external movement delta. The caller applies
// it through the ordinary player controller before finishing the frame.
Vector2 PrepareSectorConversationFrame(SectorConversationState& state,
        const SectorFpsControllerState& player, const SectorFpsControllerConfig& config,
        Vector3 npcPosition, const SectorCollisionWorld* collision, float dt);
bool FinishSectorConversationFrame(SectorConversationState& state,
        SectorFpsControllerState& player, const SectorFpsControllerConfig& config,
        Vector3 npcPosition, Vector3 talkPoint, float& npcYaw, float dt);
} // namespace game
