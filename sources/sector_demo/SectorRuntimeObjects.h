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

namespace game {

struct SectorBakedObjectLightProbeRuntimeData;
struct SectorTopologyMap;

constexpr size_t kSectorRuntimeObjectInitialCapacity = 128;
constexpr float kSectorBillboardDefaultAlphaCutoff = 0.5f;

struct SectorRuntimeObjectState {
    engine::Entity temporaryGoblinDebugSpawnEntity = engine::NullEntity();
    engine::AssetScopeHandle runtimeObjectAssetScope = engine::NullAssetScopeHandle();
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

SectorBillboardFrameUvs BuildSectorBillboardFrameUvs(
        Rectangle source,
        int atlasWidth,
        int atlasHeight);

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

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt);

bool ToggleTemporaryGoblinDebugSpawn(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const Camera3D& camera,
        const SectorTopologyMap& map);

bool HasTemporaryGoblinDebugSpawn(
        const engine::World& world,
        const SectorRuntimeObjectState& state);

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
