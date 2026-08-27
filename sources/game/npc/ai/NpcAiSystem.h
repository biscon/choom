#pragma once

#include "engine/ecs/Entity.h"
#include "game/PlayerLightLevel.h"
#include "game/PlayerSneak.h"
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

struct NpcPursuitSlot {
    engine::Entity owner = engine::NullEntity();
    Vector3 requestedPosition{};
    Vector3 resolvedPosition{};
    int index = -1;
    int ring = -1;
    NpcPursuitSlotKind kind = NpcPursuitSlotKind::None;
    bool claimed = false;
    bool projected = false;
};

struct NpcPursuitParticipant {
    engine::Entity entity = engine::NullEntity();
    int placedObjectId = 0;
    float attackRangeWorld = 0.0f;
};

struct NpcAiRuntime {
    std::vector<NpcSoundEvent> playerSounds;
    std::vector<NpcPursuitSlot> pursuitSlots;
    std::vector<NpcPursuitParticipant> pursuitParticipants;
    uint64_t nextSoundSequence = 1;
    float pursuitOrbitPhaseRadians = 0.0f;
    bool capacityWarningPrinted = false;
    bool pursuitCapacityWarningPrinted = false;
    PlayerLightLevelSample playerLightLevel;
    float playerLightDetectionFactor = 1.0f;
    float playerCrouchBlend = 0.0f;
    float playerMovementNoiseMultiplier = 1.0f;
    bool playerSneaking = false;
};

using NpcAiScriptTakeoverFn = void (*)(
        void* userData,
        engine::Entity entity,
        const char* instanceId);
using NpcAiPlayerDamagedFn = void (*)(
        void* userData,
        int appliedDamage);
using NpcAiPlayerAttackHitFn = void (*)(
        void* userData,
        int appliedDamage,
        const NpcAttackCameraImpactDefinition& cameraImpact,
        Vector2 directionFromAttackerToPlayerWorld);

struct NpcAiGameplayContext {
    Vector3 playerFeetPosition{};
    Vector3 playerEyePosition{};
    Health* playerHealth = nullptr;
    Vector2* playerKnockbackVelocity = nullptr;
    float* playerStunRemainingSeconds = nullptr;
    void* scriptUserData = nullptr;
    NpcAiScriptTakeoverFn interruptScriptMovement = nullptr;
    void* playerDamageUserData = nullptr;
    NpcAiPlayerDamagedFn playerDamaged = nullptr;
    void* playerAttackHitUserData = nullptr;
    NpcAiPlayerAttackHitFn playerAttackHit = nullptr;
    bool godMode = false;
    bool frozen = false;
    bool playerGrounded = false;
    bool playerSneaking = false;
    float playerRadiusWorld = 0.25f;
    float playerNormalizedLightLevel = 1.0f;
    float playerCrouchBlend = 0.0f;
    float playerMovementNoiseMultiplier = 1.0f;
    const PlayerSneakApplicationSettings* playerSneakSettings = nullptr;
    const PlayerLightLevelSample* playerLightLevel = nullptr;
};

void InitializeNpcAiRuntime(
        NpcAiRuntime& runtime,
        size_t soundCapacity = 64,
        size_t pursuitCapacity = 128);
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
int ApplyNpcAiPlayerDamage(
        const NpcAiGameplayContext& gameplay,
        int damage);
int ApplyNpcAiPlayerAttackEffects(
        const NpcAiGameplayContext& gameplay,
        const NpcActionDefinition& attack,
        Vector2 directionFromAttackerToPlayer);
bool IsNpcAiCommittedMeleeHitInRange(
        float playerDistanceWorld,
        float attackRangeWorld);

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
