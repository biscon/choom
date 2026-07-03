#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/components/SpriteAnimator.h"
#include "engine/ecs/World.h"

#include <raylib.h>

#include <cstdint>
#include <string>

namespace game {

struct SectorObjectTransform;

constexpr float kSectorBillboardDefaultAlphaCutoff = 0.5f;

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

SectorBillboardDirectionalClipNames SectorBillboardStoredDirectionalClipNames(
        const SectorBillboardDirectionalClips& clips);

void StoreSectorBillboardDirectionalClipNames(
        SectorBillboardDirectionalClips& clips,
        const SectorBillboardDirectionalClipNames& names);

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

void AdvanceSectorBillboardAnimatorSystem(engine::World& world, float dt);

} // namespace game
