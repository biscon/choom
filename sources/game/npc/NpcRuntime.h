#pragma once

#include "game/npc/NpcDefinitions.h"
#include "game/npc/NpcCollision.h"
#include "game/npc/ai/NpcAiTypes.h"
#include "game/Health.h"
#include "engine/ecs/Entity.h"
#include "engine/assets/AssetHandles.h"
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
    bool canOpenDoors = true;
    float walkSpeed = 1.5f;
    float runSpeed = 3.0f;
    bool actionLockedByAi = false;
};

enum class NpcAwarenessState : uint8_t {
    Unaware,
    InvestigatingTravel,
    InvestigatingSearch,
    Detected
};

enum class NpcPursuitSlotKind : uint8_t {
    None,
    Melee,
    Orbit,
    Invalid
};

enum class NpcVisualDetectionReason : uint8_t {
    NoPlayer,
    OutsideRange,
    OutsideCone,
    Occluded,
    Darkness,
    Building,
    Decaying,
    Detected
};

struct NpcAiState {
    std::string aiType;
    NpcPerceptionDefinition perception;
    NpcActionDefinition attack;
    engine::SoundHandle attackSound = engine::NullSoundHandle();
    engine::SoundHandle attackImpactSound = engine::NullSoundHandle();
    NpcAwarenessState awareness = NpcAwarenessState::Unaware;
    Vector3 lastKnownPlayerPosition{};
    float searchRemainingSeconds = 0.0f;
    float retargetRemainingSeconds = 0.0f;
    float searchTurnDirection = 1.0f;
    uint64_t lastHeardSoundSequence = 0;
    NpcAiIntent previousIntent = NpcAiIntent::Idle;
    float attackPhase = 0.0f;
    bool attackCommitted = false;
    bool attackHitResolved = false;
    bool scriptTakeoverPending = false;
    bool directAlertPending = false;
    int pursuitSlotIndex = -1;
    int pursuitSlotRing = -1;
    NpcPursuitSlotKind pursuitSlotKind = NpcPursuitSlotKind::None;
    float pursuitOrbitCooldownSeconds = 0.0f;
    float visualDetectionProgress = 0.0f;
    float visualLightDetectionFactor = 0.0f;
    float visualProximityDetectionFactor = 0.0f;
    float visualDetectionRateFactor = 0.0f;
    NpcVisualDetectionReason visualDetectionReason =
            NpcVisualDetectionReason::NoPlayer;
    bool playerInGeometricSight = false;
    bool playerDetectionAudioPending = false;
    bool pursuitRetargetFailed = false;
};

struct NpcCombatState {
    Vector2 knockbackVelocity{};
    float staggerRemainingSeconds = 0.0f;
    float corpseElapsedSeconds = 0.0f;
    float corpseDespawnDelaySeconds = kDefaultNpcCorpseDespawnDelaySeconds;
    float corpseFadeDurationSeconds = kDefaultNpcCorpseFadeDurationSeconds;
    bool despawnOnDeath = false;
    bool dead = false;
    bool hurtAnimationRequested = false;
    bool hurtAnimationPlaying = false;
    bool deathAnimationRequested = false;
    bool deathAnimationComplete = false;
};

struct NpcAnimationState {
    std::array<uint32_t, kNpcActionCount> animationIndices{
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX};
    std::array<float, kNpcActionCount> animationSpeeds{
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
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

enum class NpcMoveAuthority : uint8_t {
    None,
    Programmatic,
    Script,
    Patrol,
    Ai
};

enum class NpcPatrolPhase : uint8_t {
    Moving,
    Waiting,
    SuspendedAi,
    SuspendedScript,
    Complete,
    Failed,
    StoppedByScript
};

struct NpcPatrolState {
    int patrolEditorId = 0;
    size_t waypointIndex = 0;
    int direction = 1;
    std::vector<size_t> shuffleOrder;
    size_t shuffleCursor = 0;
    uint32_t randomState = 0x6d2b79f5u;
    NpcPatrolPhase phase = NpcPatrolPhase::Moving;
    NpcPatrolPhase resumePhase = NpcPatrolPhase::Moving;
    float waitRemainingSeconds = 0.0f;
    float lookOffsetRadians = 0.0f;
    float lookDirection = 1.0f;
    float retryRemainingSeconds = 0.0f;
    uint64_t requestId = 0;
    int slotIndex = -1;
    Vector2 destinationXZ{};
    bool scriptMoveStopsPatrol = false;
    bool scriptOverrideActive = false;
    bool stoppedByScript = false;
    bool destinationInitialized = false;
};

enum class NpcMovePhase : uint8_t {
    Idle,
    FollowingPath,
    Arrived,
    Cancelled,
    Failed
};

enum class NpcDoorTraversalPhase : uint8_t {
    None,
    Approaching,
    WaitingForClearance,
    Crossing
};

struct NpcNavigationRecord {
    std::string instanceId;
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    SectorNavigationAgentHandle agentHandle;
    SectorNavigationPathHandle pathHandle;
    NpcMovePhase phase = NpcMovePhase::Idle;
    NpcMoveGait gait = NpcMoveGait::Walk;
    NpcMoveAuthority authority = NpcMoveAuthority::None;
    uint64_t requestId = 0;
    SectorNavigationQueryStatus lastQueryStatus =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector2 requestedDestinationXZ = {};
    Vector3 projectedDestination = {};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> corners{};
    std::array<int, SectorNavigationMaximumStraightPathCorners> cornerDoorIds{};
    std::array<SectorNavigationDoorDirection,
            SectorNavigationMaximumStraightPathCorners> cornerDoorDirections{};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> cornerDoorLandings{};
    std::array<SectorNavigationTileKey,
            SectorNavigationMaximumCorridorTiles> corridorTiles{};
    size_t corridorTileCount = 0;
    uint64_t pathTileRevision = 0;
    size_t cornerCount = 0;
    size_t nextCorner = 0;
    Vector2 desiredVelocity = {};
    Vector2 preferredVelocity = {};
    Vector2 actualVelocity = {};
    int crowdNeighborCount = 0;
    float crowdNearestNeighborDistance = 0.0f;
    bool crowdAttached = false;
    bool playerAvoidanceActive = false;
    Vector3 physicalPosition = {};
    Vector3 visualPosition = {};
    float footstepDistanceWorld = 0.0f;
    float stallSeconds = 0.0f;
    float bestCornerDistance = 0.0f;
    size_t trackedCornerIndex = SIZE_MAX;
    float replanCooldownSeconds = 0.0f;
    float driftCheckSeconds = 0.0f;
    uint32_t replanCount = 0;
    NpcDoorTraversalPhase doorPhase = NpcDoorTraversalPhase::None;
    int doorId = 0;
    SectorNavigationDoorDirection doorDirection =
            SectorNavigationDoorDirection::None;
    Vector3 doorLanding = {};
    float doorWaitSeconds = 0.0f;
    bool holdsDoor = false;
    bool steeringRecoveryActive = false;
    bool tileReplanPending = false;
    bool footstepEvent = false;
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
    std::vector<NpcCollisionCylinder> collisionCylinders;
    NpcNavigationCounters counters;
    uint64_t nextRequestId = 1;
    bool growthWarned = false;
};

struct NpcMoveRequestResult {
    bool accepted = false;
    SectorNavigationQueryStatus status =
            SectorNavigationQueryStatus::NavigationUnavailable;
    uint64_t requestId = 0;
    std::string message;
};

struct NpcMoveStatus {
    bool found = false;
    NpcMovePhase phase = NpcMovePhase::Idle;
    NpcMoveGait gait = NpcMoveGait::Walk;
    NpcMoveAuthority authority = NpcMoveAuthority::None;
    uint64_t requestId = 0;
    SectorNavigationQueryStatus queryStatus =
            SectorNavigationQueryStatus::NavigationUnavailable;
    Vector2 requestedDestinationXZ = {};
    Vector3 projectedDestination = {};
    size_t remainingCorners = 0;
    Vector2 desiredVelocity = {};
    Vector2 actualVelocity = {};
    float stallSeconds = 0.0f;
    uint32_t replanCount = 0;
    std::array<char, 192> message{};
};

bool IsValidNpcInstanceId(std::string_view id);
const char* NpcMovePhaseName(NpcMovePhase phase);
const char* NpcMoveGaitName(NpcMoveGait gait);
const char* NpcMoveAuthorityName(NpcMoveAuthority authority);
const char* NpcDoorTraversalPhaseName(NpcDoorTraversalPhase phase);
const char* NpcPursuitSlotKindName(NpcPursuitSlotKind kind);
const char* NpcVisualDetectionReasonName(NpcVisualDetectionReason reason);

} // namespace game
