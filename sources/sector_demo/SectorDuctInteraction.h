#pragma once

#include "engine/ecs/Entity.h"
#include "game/PlayerDuctTraversal.h"
#include "sector_demo/SectorFpsController.h"

namespace engine { class World; }

namespace game {

class SectorCollisionWorld;
struct SectorDuctAccess;
struct SectorTopologyMap;

enum class SectorDuctTraversalPhase {
    Inactive,
    Entering,
    Crawling,
    Exiting
};

struct SectorDuctTraversalState {
    SectorDuctTraversalPhase phase = SectorDuctTraversalPhase::Inactive;
    engine::Entity accessEntity = engine::NullEntity();
    int crawlspaceSectorId = 0;
    Vector3 transitionStartFeet = {};
    Vector3 transitionTargetFeet = {};
    float transitionStartYawRadians = 0.0f;
    float transitionStartPitchRadians = 0.0f;
    float transitionTargetYawRadians = 0.0f;
    float transitionStartEyeHeightWorld = 0.32f;
    float transitionTargetEyeHeightWorld = 0.32f;
    float transitionElapsedSeconds = 0.0f;
    float crawlRadiusWorld = 0.20f;
    float crawlHeightWorld = 0.40f;
    float viewEyeHeightWorld = 0.32f;
    bool exitArmed = false;
    bool weaponHolsterInitialized = false;
};

bool IsSectorDuctTraversalActive(const SectorDuctTraversalState& state);
bool IsSectorDuctCrawling(const SectorDuctTraversalState& state);
void ResetSectorDuctTraversal(SectorDuctTraversalState& state);
SectorFpsControllerConfig SectorDuctCrawlControllerConfig(
        SectorFpsControllerConfig base,
        const PlayerDuctTraversalApplicationSettings& settings);
SectorFpsControllerConfig SectorDuctViewControllerConfig(
        SectorFpsControllerConfig base,
        const SectorDuctTraversalState& traversal);
bool BeginSectorDuctTraversal(
        SectorDuctTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& normalConfig,
        const SectorDuctAccess& access,
        engine::Entity accessEntity,
        const PlayerDuctTraversalApplicationSettings& settings);
bool UpdateSectorDuctTraversal(
        engine::World& world,
        SectorDuctTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& normalConfig,
        const SectorFpsControllerInput& input,
        const PlayerDuctTraversalApplicationSettings& settings,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        float dt);

} // namespace game
