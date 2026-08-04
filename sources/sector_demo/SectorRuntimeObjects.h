#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace game {

constexpr size_t kSectorRuntimeObjectInitialCapacity = 128;

struct SectorPlacedRuntimeObjectEntity {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
};

struct SectorObjectTransform {
    Vector3 position = {};
    float yawRadians = 0.0f;
};

struct SectorObject {
    int currentSectorId = -1;
    bool visible = true;
};

struct SectorObjectLighting {
    BakedObjectLightingSample baked = {};
};

struct SectorStaticModel {
    engine::ModelHandle model = engine::NullModelHandle();
    int placedObjectId = 0;
    Vector3 containingSectorAmbient = {0.15f, 0.15f, 0.15f};
    float scale = 1.0f;
    float environmentExposure = 0.15f;
};

} // namespace game

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorBillboardRuntime.h"

namespace game {

struct SectorDoorAnchorDiagnostic {
    int placedObjectId = 0;
    int lineDefId = 0;
    std::string message;
};

struct SectorRuntimeObjectState {
    engine::AssetScopeHandle runtimeObjectAssetScope = engine::NullAssetScopeHandle();
    std::vector<SectorPlacedRuntimeObjectEntity> placedObjectEntities;
    std::vector<SectorDoorAnchorDiagnostic> doorAnchorDiagnostics;
    std::vector<SectorDynamicDoorCollider> dynamicDoorColliders;
    std::vector<SectorStaticModelCollider> staticModelColliders;
    std::vector<RuntimePortalDynamicBlocker> dynamicPortalBlockers;
    size_t placedObjectCount = 0;
    size_t spawnedObjectCount = 0;
    size_t skippedObjectCount = 0;
    size_t doorObjectCount = 0;
    size_t validDoorAnchorCount = 0;
    size_t invalidDoorAnchorCount = 0;
    size_t spriteAnimationRequestedCount = 0;
    size_t spriteAnimationReadyCount = 0;
    size_t spriteAnimationPendingCount = 0;
    size_t spriteAnimationFailedCount = 0;
    size_t staticModelRequestedCount = 0;
    size_t staticModelReadyCount = 0;
    size_t staticModelPendingCount = 0;
    size_t staticModelFailedCount = 0;
    size_t staticModelUnassignedCount = 0;
    size_t directionalClipResolvedCount = 0;
    size_t directionalClipMissingCount = 0;
    size_t directionalClipFallbackCount = 0;
    size_t singleClipResolvedCount = 0;
    size_t singleClipMissingCount = 0;
    size_t singleClipFallbackCount = 0;
    std::string placedObjectStatus;
    std::string placedObjectWarning;
    SectorBakedObjectLightProbeRuntimeData objectLightProbes;
    std::string objectProbeStatus;
    SectorCollisionWorld objectSectorLookupWorld;
    bool objectSectorLookupWorldValid = false;
    std::string objectSectorLookupWarning;
    bool worldReserved = false;
};

void ReserveSectorRuntimeObjectWorld(
        engine::World& world,
        size_t objectCapacity = kSectorRuntimeObjectInitialCapacity);

void EnsureSectorRuntimeObjectWorldReserved(
        engine::World& world,
        SectorRuntimeObjectState& state,
        size_t objectCapacity = kSectorRuntimeObjectInitialCapacity);

void ClearSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state);

void RefreshSectorRuntimeObjectMapData(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map);

void ResetSectorRuntimeObjectsForMap(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map);

void SpawnPlacedRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map);

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition = nullptr);

void UpdateSectorObjectCurrentSectorSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld);

void UpdateSectorObjectBakedLightingSystem(
        engine::World& world,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback);

} // namespace game
