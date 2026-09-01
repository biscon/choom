#pragma once

#include "sector_demo/SectorFpsController.h"

namespace game {

class SectorCollisionWorld;
struct SectorTopologyMap;

enum class SectorLadderEndpoint {
    Bottom,
    Top
};

enum class SectorLadderTraversalPhase {
    Inactive,
    Mounting,
    Climbing,
    Dismounting
};

struct SectorLadderTraversalState {
    SectorLadderTraversalPhase phase = SectorLadderTraversalPhase::Inactive;
    int ladderPrimitiveId = -1;
    SectorLadderEndpoint mountEndpoint = SectorLadderEndpoint::Bottom;
    Vector3 transitionStartFeet = {};
    Vector3 transitionTargetFeet = {};
    Vector2 railXZ = {};
    Vector2 ladderCenterXZ = {};
    Vector2 front = {0.0f, 1.0f};
    float ladderHalfDepth = 0.0f;
    float bottomY = 0.0f;
    float topY = 0.0f;
    float facingYawRadians = 0.0f;
    float transitionStartYawRadians = 0.0f;
    float transitionStartPitchRadians = 0.0f;
    float transitionElapsedSeconds = 0.0f;
};

inline constexpr float SectorLadderTransitionSeconds = 0.30f;
inline constexpr float SectorLadderClimbSpeedWorld = 1.5f;
inline constexpr float SectorLadderLookYawArcRadians = 60.0f * DEG2RAD;

bool IsSectorLadderTraversalActive(const SectorLadderTraversalState& state);
void ResetSectorLadderTraversal(SectorLadderTraversalState& state);
bool BeginSectorLadderTraversal(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        int ladderPrimitiveId,
        SectorLadderEndpoint endpoint);
bool UpdateSectorLadderTraversal(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        float dt);

} // namespace game
