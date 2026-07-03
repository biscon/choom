#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/ecs/World.h"
#include "engine/components/SpriteAnimator.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace game {

struct SectorBakedObjectLightProbeRuntimeData;
constexpr size_t kSectorRuntimeObjectInitialCapacity = 128;
constexpr float kSectorBillboardDefaultAlphaCutoff = 0.5f;
constexpr float kSectorDoorPortalBlockEpsilon = 0.001f;
constexpr float kSectorDoorAutoOpenFallbackDistance = 2.0f;

struct SectorPlacedRuntimeObjectEntity {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
};

struct SectorDoorAnchorDiagnostic {
    int placedObjectId = 0;
    int lineDefId = 0;
    std::string message;
};

struct SectorDynamicDoorCollider {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    Vector2 center = {};
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
};

struct SectorRuntimeObjectState {
    engine::AssetScopeHandle runtimeObjectAssetScope = engine::NullAssetScopeHandle();
    std::vector<SectorPlacedRuntimeObjectEntity> placedObjectEntities;
    std::vector<SectorDoorAnchorDiagnostic> doorAnchorDiagnostics;
    std::vector<SectorDynamicDoorCollider> dynamicDoorColliders;
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

struct SectorBillboardSprite {
    engine::SpriteAnimationHandle animation = engine::NullSpriteAnimationHandle();
    uint32_t clipIndex = engine::InvalidSpriteClipIndex;
    Rectangle source = {};
    engine::TextureHandle texture = engine::NullTextureHandle();
    Vector2 sizeWorld = {1.0f, 1.0f};
    Vector2 originNormalized = {0.5f, 1.0f};
    float alphaCutoff = kSectorBillboardDefaultAlphaCutoff;
    Color tint = WHITE;
    bool visible = true;
};

struct SectorBillboardAnimator {
    std::string animationId;
    float timeSeconds = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool finished = false;
};

struct SectorBillboardDirectionalClipNames {
    const char* front = "Front";
    const char* back = "Back";
    const char* left = "Left";
    const char* right = "Right";
};

struct SectorBillboardDirectionalClips {
    std::string frontName = "Front";
    std::string backName = "Back";
    std::string leftName = "Left";
    std::string rightName = "Right";
    uint32_t front = engine::InvalidSpriteClipIndex;
    uint32_t back = engine::InvalidSpriteClipIndex;
    uint32_t left = engine::InvalidSpriteClipIndex;
    uint32_t right = engine::InvalidSpriteClipIndex;
    bool resolved = false;
    bool usedFallback = false;
};

struct SectorBillboardSingleClip {
    std::string name;
    uint32_t clip = engine::InvalidSpriteClipIndex;
    bool resolved = false;
    bool usedFallback = false;
};

struct SectorDoor {
    int placedObjectId = 0;
    bool enabled = true;
};

struct SectorDoorResolvedAnchor {
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    int frontSideDefId = 0;
    int backSideDefId = 0;
    Vector2 endpointA = {};
    Vector2 endpointB = {};
    Vector2 midpoint = {};
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    float openBottom = 0.0f;
    float openTop = 0.0f;
    float portalWidth = 0.0f;
    float portalHeight = 0.0f;
};

struct SectorDoorMotion {
    SectorDoorMotionType motion = SectorDoorMotionType::SlideVertical;
    float openFraction = 0.0f;
    float targetOpenFraction = 0.0f;
    float openDistance = 0.0f;
    float speed = 1.5f;
};

struct SectorDoorInteraction {
    bool autoOpen = false;
    float interactionDistance = 1.5f;
    float autoOpenDistance = kSectorDoorAutoOpenFallbackDistance;
};

struct SectorDoorRender {
    float width = 0.0f;
    float height = 0.0f;
    float thickness = 0.25f;
    float normalOffset = 0.0f;
    std::string textureId;
    Color tint = WHITE;
    bool visible = true;
};

struct SectorDoorCollider {
    Vector2 center = {};
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
    bool enabled = true;
};

struct SectorDoorPortalBlocker {
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    int frontSideDefId = 0;
    int backSideDefId = 0;
    bool blocksPortal = true;
};

struct SectorBillboardFrameUvs {
    Vector2 topLeft = {};
    Vector2 topRight = {};
    Vector2 bottomRight = {};
    Vector2 bottomLeft = {};
};

struct SectorBillboardQuad {
    Vector3 bottomLeft = {};
    Vector3 bottomRight = {};
    Vector3 topRight = {};
    Vector3 topLeft = {};
};

SectorBillboardFrameUvs BuildSectorBillboardFrameUvs(
        Rectangle source,
        int atlasWidth,
        int atlasHeight);

SectorBillboardQuad BuildSectorBillboardQuad(
        Vector3 position,
        Vector2 sizeWorld,
        Vector2 originNormalized,
        Vector3 cameraRight);

inline engine::SpriteAnimationHandle RequestSectorBillboardSpriteAnimation(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        const char* animationId,
        const char* jsonPath,
        SectorBillboardSprite& sprite,
        SectorBillboardAnimator& animator)
{
    if (animationId == nullptr || animationId[0] == '\0' || jsonPath == nullptr || jsonPath[0] == '\0') {
        sprite.animation = engine::NullSpriteAnimationHandle();
        sprite.clipIndex = engine::InvalidSpriteClipIndex;
        sprite.source = {};
        sprite.texture = engine::NullTextureHandle();
        animator.animationId.clear();
        animator.timeSeconds = 0.0f;
        animator.finished = false;
        return sprite.animation;
    }

    sprite.animation = assets.RequestSpriteAnimation(
            scope,
            animationId,
            jsonPath,
            engine::TextureLoad_PointFilter);
    sprite.clipIndex = engine::InvalidSpriteClipIndex;
    sprite.source = {};
    sprite.texture = engine::NullTextureHandle();
    animator.animationId = animationId;
    animator.timeSeconds = 0.0f;
    animator.finished = false;
    return sprite.animation;
}

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

bool ResolveSectorBillboardDirectionalClipsFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips);

bool ResolveSectorBillboardDirectionalClips(
        engine::AssetManager& assets,
        engine::SpriteAnimationHandle animation,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips);

bool ResolveSectorBillboardSingleClipFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const char* name,
        SectorBillboardSingleClip& clip);

bool ResolveSectorBillboardSingleClip(
        engine::AssetManager& assets,
        engine::SpriteAnimationHandle animation,
        const char* name,
        SectorBillboardSingleClip& clip);

uint32_t SelectSectorBillboardDirectionalClip(
        const SectorObjectTransform& transform,
        Vector3 cameraPosition,
        const SectorBillboardDirectionalClips& clips);

void UpdateSectorObjectCurrentSectorSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld);

void UpdateSectorObjectBakedLightingSystem(
        engine::World& world,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback);

void UpdateSectorDoorAutoOpenSystem(
        engine::World& world,
        const Vector3& playerPosition);

bool ToggleTargetedSectorDoorInteractionSystem(
        engine::World& world,
        const Vector3& playerPosition,
        const Vector3& playerForward);

void AdvanceSectorDoorMotionSystem(engine::World& world, float dt);

void UpdateSectorDoorDerivedStateSystem(engine::World& world);

void CollectSectorDoorDynamicColliders(
        engine::World& world,
        std::vector<SectorDynamicDoorCollider>& colliders);

void CollectSectorDoorDynamicPortalBlockers(
        engine::World& world,
        std::vector<RuntimePortalDynamicBlocker>& blockers);

SectorCollisionMoveResult ResolveSectorDoorDynamicCollidersForPlayerMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& staticResult,
        const SectorCollisionMoveConfig& config,
        const std::vector<SectorDynamicDoorCollider>& colliders);

void AdvanceSectorBillboardAnimatorSystem(engine::World& world, float dt);

} // namespace game
