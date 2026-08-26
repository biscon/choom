#pragma once

#include "engine/ecs/Entity.h"
#include "game/npc/NpcRuntime.h"

#include <raylib.h>

#include <cstdint>
#include <vector>

namespace engine {
class AssetManager;
class AudioSystem;
class World;
}

namespace game {

class SectorCollisionWorld;
class SectorNavigationWorld;
struct Health;
struct NpcNavigationRuntime;
struct SectorDynamicDoorCollider;
struct SectorStaticModelCollider;

struct NpcSoundEvent {
    uint64_t sequence = 0;
    Vector3 positionWorld{};
    float radiusWorld = 0.0f;
    float remainingSeconds = 0.0f;
};

struct NpcAiRuntime {
    std::vector<NpcSoundEvent> playerSounds;
    uint64_t nextSoundSequence = 1;
    bool capacityWarningPrinted = false;
};

using NpcAiScriptTakeoverFn = void (*)(
        void* userData,
        engine::Entity entity,
        const char* instanceId);

struct NpcAiGameplayContext {
    Vector3 playerFeetPosition{};
    Vector3 playerEyePosition{};
    Health* playerHealth = nullptr;
    Vector2* playerKnockbackVelocity = nullptr;
    float* playerStunRemainingSeconds = nullptr;
    void* scriptUserData = nullptr;
    NpcAiScriptTakeoverFn interruptScriptMovement = nullptr;
    bool godMode = false;
    bool frozen = false;
};

void InitializeNpcAiRuntime(NpcAiRuntime& runtime, size_t soundCapacity = 64);
void ClearNpcAiRuntime(NpcAiRuntime& runtime);
void EmitNpcPlayerSound(
        NpcAiRuntime& runtime,
        Vector3 positionWorld,
        float radiusWorld,
        float lifetimeSeconds = 1.5f);
void AlertNpcToPlayerPosition(
        engine::World& world,
        engine::Entity entity,
        Vector3 playerPositionWorld);

void UpdateNpcAiSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        NpcAiRuntime& runtime,
        const NpcAiGameplayContext& gameplay,
        float dt);

} // namespace game
