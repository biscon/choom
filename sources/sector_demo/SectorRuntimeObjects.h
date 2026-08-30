#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "game/npc/NpcDefinitions.h"
#include "game/items/ItemAssets.h"
#include "game/items/ItemDefinitions.h"
#include "game/items/ItemPresentation.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorSwingDoorCatalog.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

constexpr size_t kSectorRuntimeObjectInitialCapacity = 128;

float ComputeSectorModelEnvironmentExposure(
        const SectorTopologyMap& map,
        int sectorId);

Vector3 ComputeSectorModelAmbient(
        const SectorTopologyMap& map,
        int sectorId);

struct SectorPlacedRuntimeObjectEntity {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
};

struct SectorObjectTransform {
    Vector3 position = {};
    float yawRadians = 0.0f;
    float rotationXRadians = 0.0f;
    float rotationZRadians = 0.0f;
};

struct SectorObject {
    int currentSectorId = -1;
    bool visible = true;
};

struct SectorObjectLighting {
    BakedObjectLightingSample baked = {};
    BakedObjectLightingVerticalSample vertical = {};
};

// Presentation-only offset. Physical position, collision, sector lookup, and
// lighting continue to use SectorObjectTransform.
struct SectorObjectVisualOffset {
    Vector3 position = {};
};

struct SectorStaticModel {
    engine::ModelHandle model = engine::NullModelHandle();
    int placedObjectId = 0;
    Vector3 containingSectorAmbient = {0.15f, 0.15f, 0.15f};
    float scale = 1.0f;
    float environmentExposure = 0.15f;
    bool castsShadow = true;
    std::string instanceId;
    float emissiveScale = 1.0f;
};

struct SectorDynamicModel {
    int placedObjectId = 0;
    std::string instanceId;
    Vector3 containingSectorAmbient = {0.15f, 0.15f, 0.15f};
    float scale = 1.0f;
    float environmentExposure = 0.15f;
    std::string requestedAnimation;
    bool animationResolved = false;
    bool animationFallback = false;
    float opacity = 1.0f;
    SectorDynamicModelShadowMode shadowMode = SectorDynamicModelShadowMode::Contact;
    std::string useTitle = "object";
    float useDistance = 1.5f;
    std::string onUseScript;
    bool singleUse = false;
    bool useConsumed = false;
    float emissiveScale = 1.0f;
};

enum class SectorItemOrigin {
    Authored,
    SessionDrop
};

struct SectorItem {
    engine::ModelHandle model = engine::NullModelHandle();
    int placedObjectId = 0;
    std::string definitionId;
    std::string title;
    std::string instanceId;
    std::uint64_t quantity = 1;
    float takeDistance = 1.5f;
    std::string onTakeScript;
    std::string onUseScript;
    Vector3 containingSectorAmbient = {0.15f, 0.15f, 0.15f};
    float scale = 1.0f;
    float environmentExposure = 0.15f;
    SectorDynamicModelShadowMode shadowMode =
            SectorDynamicModelShadowMode::Contact;
    SectorItemOrigin origin = SectorItemOrigin::Authored;
    bool takePending = false;
    ItemPresentationState presentation;
};

struct SectorWindow {
    int placedObjectId = 0;
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    float width = 0.0f;
    float height = 0.0f;
    float thickness = 0.04f;
    Color tint = WHITE;
    float opacity = 0.06f;
    float roughness = 0.08f;
    float indexOfRefraction = 1.5f;
    bool visible = true;
};

Matrix BuildSectorWindowModelMatrix(
        const SectorObjectTransform& transform,
        const SectorWindow& window);

} // namespace game

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorBillboardRuntime.h"

namespace game {

struct SectorDoorAnchorDiagnostic {
    int placedObjectId = 0;
    int lineDefId = 0;
    std::string message;
};

struct SectorDoorFallbackDiagnostic {
    int placedObjectId = 0;
    std::string modelAssetId;
    std::string message;
};

struct SectorRuntimeObjectState {
    engine::AssetScopeHandle runtimeObjectAssetScope = engine::NullAssetScopeHandle();
    std::vector<SectorPlacedRuntimeObjectEntity> placedObjectEntities;
    std::vector<SectorDoorAnchorDiagnostic> doorAnchorDiagnostics;
    SectorSwingDoorCatalog swingDoorCatalog;
    NpcDefinitionCatalog npcDefinitionCatalog;
    std::vector<SectorDoorFallbackDiagnostic> doorFallbackDiagnostics;
    std::vector<SectorDynamicDoorCollider> dynamicDoorColliders;
    std::vector<SectorDoorPlayerObstacle> doorObstacles;
    std::vector<SectorStaticModelCollider> staticModelColliders;
    std::vector<SectorStaticModelCollider> dynamicModelColliders;
    // Kept separate so physical glass can block movement and weapons without
    // becoming opaque to perception, audio, or light queries.
    std::vector<SectorStaticModelCollider> windowColliders;
    std::vector<SectorStaticModelCollider> physicalModelColliders;
    std::vector<RuntimePortalDynamicBlocker> dynamicPortalBlockers;
    size_t placedObjectCount = 0;
    size_t spawnedObjectCount = 0;
    size_t skippedObjectCount = 0;
    size_t doorObjectCount = 0;
    size_t validDoorAnchorCount = 0;
    size_t invalidDoorAnchorCount = 0;
    size_t doorFallbackCount = 0;
    size_t doorFrameFailureCount = 0;
    size_t spriteAnimationRequestedCount = 0;
    size_t spriteAnimationReadyCount = 0;
    size_t spriteAnimationPendingCount = 0;
    size_t spriteAnimationFailedCount = 0;
    size_t staticModelRequestedCount = 0;
    size_t staticModelReadyCount = 0;
    size_t staticModelPendingCount = 0;
    size_t staticModelFailedCount = 0;
    size_t staticModelUnassignedCount = 0;
    size_t itemModelRequestedCount = 0;
    size_t itemModelReadyCount = 0;
    size_t itemModelPendingCount = 0;
    size_t itemModelFailedCount = 0;
    size_t directionalClipResolvedCount = 0;
    size_t directionalClipMissingCount = 0;
    size_t directionalClipFallbackCount = 0;
    size_t singleClipResolvedCount = 0;
    size_t singleClipMissingCount = 0;
    size_t singleClipFallbackCount = 0;
    std::string placedObjectStatus;
    std::string placedObjectWarning;
    bool swingDoorCatalogLoaded = false;
    uint64_t swingDoorCatalogRevision = 0;
    std::string swingDoorCatalogStatus;
    std::string swingDoorCatalogWarning;
    uint64_t npcDefinitionCatalogRevision = 0;
    std::string npcDefinitionCatalogStatus;
    std::string npcDefinitionCatalogWarning;
    SectorBakedObjectLightProbeRuntimeData objectLightProbes;
    std::string objectProbeStatus;
    uint64_t staticLightingRevision = 1;
    SectorCollisionWorld objectSectorLookupWorld;
    bool objectSectorLookupWorldValid = false;
    bool doorSpatialStateChanged = true;
    bool doorCollisionCacheInitialized = false;
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

void ReloadSectorSwingDoorCatalog(SectorRuntimeObjectState& state);
void ReloadSectorNpcDefinitionCatalog(SectorRuntimeObjectState& state);

void ResetSectorRuntimeObjectsForMap(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const ItemRegistry* itemRegistry = nullptr,
        const ItemModelAssetState* itemAssets = nullptr);

void SpawnPlacedRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const ItemRegistry* itemRegistry = nullptr,
        const ItemModelAssetState* itemAssets = nullptr);

bool SpawnSectorItemRuntimeObject(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& placedObject,
        const ItemRegistry& itemRegistry,
        const ItemModelAssetState& itemAssets,
        engine::Entity* outEntity = nullptr);

bool QueueRemoveSectorRuntimeObjectByEntity(
        engine::World& world,
        SectorRuntimeObjectState& state,
        engine::Entity entity);

void RefreshSectorDoorSpatialCaches(
        engine::World& world,
        SectorRuntimeObjectState& state);

void CollectSectorWindowColliders(
        engine::World& world,
        std::vector<SectorStaticModelCollider>& colliders);

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition = nullptr,
        const SectorDoorPlayerObstacle* playerObstacle = nullptr,
        const std::vector<SectorDoorPlayerObstacle>* doorObstacles = nullptr);

void UpdateSectorObjectCurrentSectorSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld);

void UpdateSectorObjectBakedLightingSystem(
        engine::World& world,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback);

// Synchronizes one already-spawned authored prop/item after an editor-only
// transform change. This performs no ECS structural changes or asset requests.
bool SynchronizeSectorPlacedRuntimeObjectTransform(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& placedObject);

} // namespace game
