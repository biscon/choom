#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/ecs/World.h"
#include "engine/components/SpriteAnimator.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmapTypes.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace game {

struct SectorBakedObjectLightProbeRuntimeData;
struct SectorTopologyMap;

constexpr size_t kSectorRuntimeObjectInitialCapacity = 128;
constexpr float kSectorBillboardDefaultAlphaCutoff = 0.5f;

struct SectorPlacedRuntimeObjectEntity {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
};

struct SectorRuntimeObjectState {
    engine::AssetScopeHandle runtimeObjectAssetScope = engine::NullAssetScopeHandle();
    std::vector<SectorPlacedRuntimeObjectEntity> placedObjectEntities;
    size_t placedObjectCount = 0;
    size_t spawnedObjectCount = 0;
    size_t skippedObjectCount = 0;
    size_t spriteAnimationRequestedCount = 0;
    size_t spriteAnimationReadyCount = 0;
    size_t spriteAnimationPendingCount = 0;
    size_t spriteAnimationFailedCount = 0;
    size_t directionalClipResolvedCount = 0;
    size_t directionalClipMissingCount = 0;
    size_t directionalClipFallbackCount = 0;
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

struct SectorRuntimeObjectBillboardDefinition {
    std::string spriteAnimationAssetPath;
    Vector2 sizeWorld = {1.0f, 1.0f};
    Vector2 originNormalized = {0.5f, 1.0f};
    std::string frontClipName = "Front";
    std::string backClipName = "Back";
    std::string leftClipName = "Left";
    std::string rightClipName = "Right";
};

struct SectorRuntimeObjectDefinition {
    std::string id;
    std::string kind;
    SectorRuntimeObjectBillboardDefinition billboard;
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
        float dt);

const std::vector<SectorRuntimeObjectDefinition>& GetSectorRuntimeObjectDefinitions();

const SectorRuntimeObjectDefinition* FindSectorRuntimeObjectDefinition(const std::string& definitionId);

bool ResolveSectorBillboardDirectionalClipsFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips);

bool ResolveSectorBillboardDirectionalClips(
        engine::AssetManager& assets,
        engine::SpriteAnimationHandle animation,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips);

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

void AdvanceSectorBillboardAnimatorSystem(engine::World& world, float dt);

} // namespace game
