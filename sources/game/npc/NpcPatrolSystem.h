#pragma once

#include "engine/ecs/Entity.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace engine { class World; }

namespace game {

class SectorCollisionWorld;
class SectorNavigationWorld;
struct NpcNavigationRuntime;
struct NpcPatrolState;
struct SectorCompiledPatrol;
struct SectorTopologyMap;

struct NpcPatrolParticipant {
    engine::Entity entity = engine::NullEntity();
    int placedObjectId = 0;
    int patrolEditorId = 0;
    size_t waypointIndex = 0;
    bool claimsSlot = false;
};

struct NpcPatrolRuntime {
    std::vector<NpcPatrolParticipant> participants;
    bool growthWarned = false;
};

void InitializeNpcPatrolRuntime(NpcPatrolRuntime& runtime, size_t capacity);
void ClearNpcPatrolRuntime(NpcPatrolRuntime& runtime);

void InitializeNpcPatrolTraversal(
        NpcPatrolState& state,
        const SectorCompiledPatrol& patrol,
        bool randomStart,
        bool reverse,
        uint32_t randomSeed);

void AdvanceNpcPatrolWaypoint(
        NpcPatrolState& state,
        const SectorCompiledPatrol& patrol);

void NotifyNpcPatrolScriptMoveStarted(
        engine::World& world,
        const NpcNavigationRuntime& navigationRuntime,
        std::string_view instanceId);

void UpdateNpcPatrolSystem(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& navigationRuntime,
        NpcPatrolRuntime& runtime,
        const SectorTopologyMap& map,
        float dt,
        bool frozen);

} // namespace game
