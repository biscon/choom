#pragma once

#include "game/npc/NpcDefinitions.h"
#include "engine/ecs/Entity.h"
#include "game/navigation/SectorNavigationTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace game {

struct NpcRuntimeInstance {
    std::string definitionId;
    std::string instanceId;
    NpcAction action = NpcAction::Idle;
    bool hostile = false;
    float walkSpeed = 1.5f;
    float runSpeed = 3.0f;
};

struct NpcAnimationState {
    std::array<uint32_t, kNpcActionCount> animationIndices{
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX};
    std::array<float, kNpcActionCount> animationSpeeds{1.0f, 1.0f, 1.0f};
    float blendSeconds = kDefaultNpcAnimationBlendSeconds;
    NpcAction appliedAction = NpcAction::Idle;
    NpcAction pendingAction = NpcAction::Idle;
    uint8_t missingAnimationMask = 0;
    bool resolved = false;
    bool hasPendingAction = false;
};

enum class NpcMoveGait : uint8_t {
    Walk,
    Run
};

enum class NpcMovePhase : uint8_t {
    Idle,
    FollowingPath,
    Arrived,
    Cancelled,
    Failed
};

struct NpcNavigationRecord {
    std::string instanceId;
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    SectorNavigationAgentHandle agentHandle;
    SectorNavigationPathHandle pathHandle;
    NpcMovePhase phase = NpcMovePhase::Idle;
    NpcMoveGait gait = NpcMoveGait::Walk;
    SectorNavigationQueryStatus lastQueryStatus =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector2 requestedDestinationXZ = {};
    Vector3 projectedDestination = {};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> corners{};
    size_t cornerCount = 0;
    size_t nextCorner = 0;
    Vector2 desiredVelocity = {};
    Vector2 actualVelocity = {};
    Vector3 physicalPosition = {};
    Vector3 visualPosition = {};
    float stallSeconds = 0.0f;
    float replanCooldownSeconds = 0.0f;
    float driftCheckSeconds = 0.0f;
    uint32_t replanCount = 0;
    std::array<char, 192> diagnostic{};
    bool occupied = false;
};

struct NpcNavigationCounters {
    uint64_t requests = 0;
    uint64_t arrivals = 0;
    uint64_t cancellations = 0;
    uint64_t replans = 0;
    uint64_t stalls = 0;
    uint64_t failures = 0;
    uint64_t capacityWarnings = 0;
};

struct NpcNavigationRuntime {
    std::vector<NpcNavigationRecord> records;
    NpcNavigationCounters counters;
    bool growthWarned = false;
};

struct NpcMoveRequestResult {
    bool accepted = false;
    SectorNavigationQueryStatus status =
            SectorNavigationQueryStatus::NavigationUnavailable;
    std::string message;
};

struct NpcMoveStatus {
    bool found = false;
    NpcMovePhase phase = NpcMovePhase::Idle;
    NpcMoveGait gait = NpcMoveGait::Walk;
    SectorNavigationQueryStatus queryStatus =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector2 requestedDestinationXZ = {};
    Vector3 projectedDestination = {};
    size_t remainingCorners = 0;
    Vector2 desiredVelocity = {};
    Vector2 actualVelocity = {};
    float stallSeconds = 0.0f;
    uint32_t replanCount = 0;
    std::string message;
};

bool IsValidNpcInstanceId(std::string_view id);
const char* NpcMovePhaseName(NpcMovePhase phase);
const char* NpcMoveGaitName(NpcMoveGait gait);

} // namespace game
