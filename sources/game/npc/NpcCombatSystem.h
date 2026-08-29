#pragma once

#include "engine/systems/AnimatedModelRaycast.h"
#include "engine/ecs/Entity.h"
#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <array>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorCollisionWorld;
class SectorNavigationWorld;
struct NpcNavigationRuntime;
struct NpcAudioRuntime;
struct NpcAiRuntime;
struct NpcBodyPartDamageState;
struct SectorDynamicDoorCollider;
struct SectorStaticModelCollider;
struct SectorTopologyMap;

enum class WeaponImpactKind {
    None,
    Blood,
    SurfaceDebris
};

struct WeaponImpactEvent {
    WeaponImpactKind kind = WeaponImpactKind::None;
    Vector3 position{};
    Vector3 normal{};
    Vector3 localPosition{};
    int sectorId = 0;
    engine::Entity attachedEntity = engine::NullEntity();
    engine::AnimatedModelSurfaceAnchor surfaceAnchor;
    FpsWeaponImpactParticlesDefinition particles;
};

struct WeaponPelletVolleyResult {
    int pelletCount = 0;
    int hitCount = 0;
    std::array<FpsShotResult, MaxFpsWeaponPellets> shots{};
    std::array<WeaponImpactEvent, MaxFpsWeaponPellets> impacts{};
};

struct NpcCombatRuntime {
    std::vector<engine::Entity> deferredDestroy;
};

struct NpcBodyPartDamageMatch {
    float multiplier = 1.0f;
    int rowIndex = -1;
    bool matched = false;
};

NpcBodyPartDamageMatch ClassifyNpcBodyPartDamage(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelSurfaceAnchor& anchor,
        NpcBodyPartDamageState& state);

int ScaleNpcBodyPartDamage(int baseDamage, float multiplier);

void InitializeNpcCombatRuntime(NpcCombatRuntime& runtime, size_t npcCapacity);
void ClearNpcCombatRuntime(NpcCombatRuntime& runtime);

bool ResolvePlayerWeaponShot(
        engine::World& world,
        const engine::AssetManager* assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 rayOrigin,
        Vector3 rayDirection,
        float maximumDistance,
        const FpsWeaponImpactDefinition& impact,
        FpsShotResult& outShot,
        WeaponImpactEvent& outImpact,
        NpcAudioRuntime* npcAudio = nullptr,
        NpcAiRuntime* npcAi = nullptr);

bool ResolvePlayerWeaponPelletVolley(
        engine::World& world,
        const engine::AssetManager* assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 rayOrigin,
        Vector3 aimDirection,
        uint64_t shotSequence,
        const FpsWeaponFiringDefinition& firing,
        WeaponPelletVolleyResult& outVolley,
        NpcAudioRuntime* npcAudio = nullptr,
        NpcAiRuntime* npcAi = nullptr);

bool UpdateNpcCombatSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const SectorTopologyMap* map,
        NpcCombatRuntime& runtime,
        float dt);

} // namespace game
