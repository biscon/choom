#pragma once

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorFpsController.h"

#include <raylib.h>

#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorObjectTransform;
struct SectorStaticModel;

struct SectorStaticModelCollider {
    int placedObjectId = 0;
    Vector2 center = {};
    Vector2 axisX = {1.0f, 0.0f};
    Vector2 axisZ = {0.0f, 1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
    bool resolved = false;
    bool failed = false;
};

bool BuildSectorStaticModelCollider(
        int placedObjectId,
        BoundingBox localBounds,
        const SectorObjectTransform& transform,
        float scale,
        SectorStaticModelCollider& outCollider);

void UpdateSectorStaticModelColliderSystem(
        engine::World& world,
        engine::AssetManager& assets);

void CollectSectorStaticModelColliders(
        engine::World& world,
        std::vector<SectorStaticModelCollider>& colliders);

SectorCollisionMoveResult ResolveSectorStaticModelCollidersForPlayerMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& sectorAndDoorResult,
        const SectorCollisionMoveConfig& moveConfig,
        const SectorFpsVerticalContext& sectorContext,
        const std::vector<SectorStaticModelCollider>& colliders);

SectorFpsVerticalContext BuildSectorStaticModelVerticalContext(
        const SectorFpsVerticalContext& sectorContext,
        const SectorFpsControllerState& playerState,
        const SectorFpsControllerConfig& playerConfig,
        const std::vector<SectorStaticModelCollider>& colliders);

} // namespace game
