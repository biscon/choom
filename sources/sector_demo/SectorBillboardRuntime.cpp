#include "sector_demo/SectorBillboardRuntime.h"

#include "sector_demo/SectorRuntimeObjects.h"

#include <cmath>
#include <cstdio>
#include <limits>

#include <raymath.h>

namespace game {

SectorBillboardFrameUvs BuildSectorBillboardFrameUvs(
        Rectangle source,
        int atlasWidth,
        int atlasHeight)
{
    if (atlasWidth <= 0 || atlasHeight <= 0) {
        return SectorBillboardFrameUvs{};
    }

    const float invWidth = 1.0f / static_cast<float>(atlasWidth);
    const float invHeight = 1.0f / static_cast<float>(atlasHeight);
    const float u0 = source.x * invWidth;
    const float u1 = (source.x + source.width) * invWidth;
    const float v0 = source.y * invHeight;
    const float v1 = (source.y + source.height) * invHeight;
    return SectorBillboardFrameUvs{
            Vector2{u0, v0},
            Vector2{u1, v0},
            Vector2{u1, v1},
            Vector2{u0, v1}};
}

SectorBillboardQuad BuildSectorBillboardQuad(
        Vector3 position,
        Vector2 sizeWorld,
        Vector2 originNormalized,
        Vector3 cameraRight)
{
    Vector3 right = cameraRight;
    if (Vector3LengthSqr(right) <= 0.000001f) {
        right = Vector3{1.0f, 0.0f, 0.0f};
    }

    const Vector2 origin = {
        sizeWorld.x * originNormalized.x,
        sizeWorld.y * (1.0f - originNormalized.y)
    };
    const Vector3 up = Vector3{0.0f, 1.0f, 0.0f};
    const Vector3 bottomLeft = Vector3Add(
            position,
            Vector3Add(
                    Vector3Scale(right, -origin.x),
                    Vector3Scale(up, -origin.y)));
    const Vector3 topLeft = Vector3Add(bottomLeft, Vector3Scale(up, sizeWorld.y));
    const Vector3 topRight = Vector3Add(topLeft, Vector3Scale(right, sizeWorld.x));
    const Vector3 bottomRight = Vector3Add(bottomLeft, Vector3Scale(right, sizeWorld.x));

    return SectorBillboardQuad{bottomLeft, bottomRight, topRight, topLeft};
}

namespace {

uint32_t FindClipIndexInAsset(const engine::SpriteAnimationAsset& asset, const char* name)
{
    if (name == nullptr) {
        return engine::InvalidSpriteClipIndex;
    }

    for (uint32_t i = 0; i < asset.clips.size(); ++i) {
        if (asset.clips[i].name == name) {
            return i;
        }
    }

    return engine::InvalidSpriteClipIndex;
}

uint32_t FindFallbackClipIndex(const engine::SpriteAnimationAsset& asset)
{
    return FindClipIndexInAsset(asset, "Default");
}

uint32_t FindFirstFallbackClipIndex(const engine::SpriteAnimationAsset& asset)
{
    const uint32_t defaultClip = FindFallbackClipIndex(asset);
    if (defaultClip != engine::InvalidSpriteClipIndex) {
        return defaultClip;
    }

    return asset.clips.empty() ? engine::InvalidSpriteClipIndex : 0;
}

uint32_t ResolveDirectionalClipIndex(
        const engine::SpriteAnimationAsset& asset,
        const char* name,
        const char* directionLabel,
        bool& usedFallback)
{
    const uint32_t clipIndex = FindClipIndexInAsset(asset, name);
    if (clipIndex != engine::InvalidSpriteClipIndex) {
        return clipIndex;
    }

    const uint32_t fallback = FindFallbackClipIndex(asset);
    if (fallback != engine::InvalidSpriteClipIndex) {
        usedFallback = true;
        std::fprintf(stderr,
                "[SectorRuntimeObjects WARNING] Missing billboard %s clip '%s'; using clip %u as fallback\n",
                directionLabel,
                name != nullptr ? name : "<null>",
                fallback);
    }
    return fallback;
}

} // namespace

SectorBillboardDirectionalClipNames SectorBillboardStoredDirectionalClipNames(const SectorBillboardDirectionalClips& clips)
{
    return SectorBillboardDirectionalClipNames{
            clips.frontName.c_str(),
            clips.backName.c_str(),
            clips.leftName.c_str(),
            clips.rightName.c_str()};
}

void StoreSectorBillboardDirectionalClipNames(
        SectorBillboardDirectionalClips& clips,
        const SectorBillboardDirectionalClipNames& names)
{
    clips.frontName = names.front != nullptr ? names.front : "";
    clips.backName = names.back != nullptr ? names.back : "";
    clips.leftName = names.left != nullptr ? names.left : "";
    clips.rightName = names.right != nullptr ? names.right : "";
}

namespace {

void ClearDirectionalClipResolution(SectorBillboardDirectionalClips& clips)
{
    clips.front = engine::InvalidSpriteClipIndex;
    clips.back = engine::InvalidSpriteClipIndex;
    clips.left = engine::InvalidSpriteClipIndex;
    clips.right = engine::InvalidSpriteClipIndex;
    clips.resolved = false;
    clips.usedFallback = false;
}

void ClearSingleClipResolution(SectorBillboardSingleClip& clip)
{
    clip.clip = engine::InvalidSpriteClipIndex;
    clip.resolved = false;
    clip.usedFallback = false;
}
float WrapRadiansPi(float angle)
{
    constexpr float TwoPi = 6.28318530717958647692f;
    constexpr float Pi = 3.14159265358979323846f;
    while (angle <= -Pi) {
        angle += TwoPi;
    }
    while (angle > Pi) {
        angle -= TwoPi;
    }
    return angle;
}

} // namespace

bool ResolveSectorBillboardDirectionalClipsFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips)
{
    StoreSectorBillboardDirectionalClipNames(clips, names);
    ClearDirectionalClipResolution(clips);
    if (asset.clips.empty()) {
        return false;
    }

    bool usedFallback = false;
    clips.front = ResolveDirectionalClipIndex(asset, clips.frontName.c_str(), "front", usedFallback);
    clips.back = ResolveDirectionalClipIndex(asset, clips.backName.c_str(), "back", usedFallback);
    clips.left = ResolveDirectionalClipIndex(asset, clips.leftName.c_str(), "left", usedFallback);
    clips.right = ResolveDirectionalClipIndex(asset, clips.rightName.c_str(), "right", usedFallback);
    clips.usedFallback = usedFallback;
    clips.resolved = clips.front != engine::InvalidSpriteClipIndex
            && clips.back != engine::InvalidSpriteClipIndex
            && clips.left != engine::InvalidSpriteClipIndex
            && clips.right != engine::InvalidSpriteClipIndex;
    return clips.resolved;
}

bool ResolveSectorBillboardDirectionalClips(
        engine::AssetManager& assets,
        engine::SpriteAnimationHandle animation,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips)
{
    StoreSectorBillboardDirectionalClipNames(clips, names);
    ClearDirectionalClipResolution(clips);
    if (engine::IsNull(animation)) {
        return false;
    }

    const engine::SpriteAnimationAsset* asset = assets.GetSpriteAnimation(animation);
    if (asset == nullptr) {
        if (assets.HasFailed(animation)) {
            std::fprintf(stderr,
                    "[SectorRuntimeObjects WARNING] Cannot resolve billboard directional clips; animation asset failed\n");
        }
        return false;
    }

    return ResolveSectorBillboardDirectionalClipsFromAsset(*asset, names, clips);
}

bool ResolveSectorBillboardSingleClipFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const char* name,
        SectorBillboardSingleClip& clip)
{
    clip.name = name != nullptr ? name : "";
    ClearSingleClipResolution(clip);
    if (asset.clips.empty()) {
        return false;
    }

    if (clip.name.empty()) {
        const uint32_t defaultClip = FindFirstFallbackClipIndex(asset);
        if (defaultClip == engine::InvalidSpriteClipIndex) {
            return false;
        }

        clip.clip = defaultClip;
        clip.resolved = true;
        return true;
    }

    {
        const uint32_t clipIndex = FindClipIndexInAsset(asset, clip.name.c_str());
        if (clipIndex != engine::InvalidSpriteClipIndex) {
            clip.clip = clipIndex;
            clip.resolved = true;
            return true;
        }
    }

    const uint32_t fallback = FindFirstFallbackClipIndex(asset);
    if (fallback == engine::InvalidSpriteClipIndex) {
        return false;
    }

    clip.clip = fallback;
    clip.usedFallback = true;
    clip.resolved = true;
    std::fprintf(stderr,
            "[SectorRuntimeObjects WARNING] Missing billboard single clip '%s'; using clip %u as fallback\n",
            clip.name.empty() ? "<default>" : clip.name.c_str(),
            fallback);
    return true;
}

bool ResolveSectorBillboardSingleClip(
        engine::AssetManager& assets,
        engine::SpriteAnimationHandle animation,
        const char* name,
        SectorBillboardSingleClip& clip)
{
    clip.name = name != nullptr ? name : "";
    ClearSingleClipResolution(clip);
    if (engine::IsNull(animation)) {
        return false;
    }

    const engine::SpriteAnimationAsset* asset = assets.GetSpriteAnimation(animation);
    if (asset == nullptr) {
        if (assets.HasFailed(animation)) {
            std::fprintf(stderr,
                    "[SectorRuntimeObjects WARNING] Cannot resolve billboard single clip; animation asset failed\n");
        }
        return false;
    }

    return ResolveSectorBillboardSingleClipFromAsset(*asset, name, clip);
}

uint32_t SelectSectorBillboardDirectionalClip(
        const SectorObjectTransform& transform,
        Vector3 cameraPosition,
        const SectorBillboardDirectionalClips& clips)
{
    if (!clips.resolved) {
        return engine::InvalidSpriteClipIndex;
    }

    const float toCameraX = cameraPosition.x - transform.position.x;
    const float toCameraZ = cameraPosition.z - transform.position.z;
    const float distanceSq = toCameraX * toCameraX + toCameraZ * toCameraZ;
    if (distanceSq <= std::numeric_limits<float>::epsilon()) {
        return clips.front;
    }

    constexpr float Pi = 3.14159265358979323846f;
    constexpr float QuarterTurn = Pi * 0.5f;
    const float angleToCamera = std::atan2(toCameraZ, toCameraX);
    const float relativeAngle = WrapRadiansPi(angleToCamera - transform.yawRadians);

    if (std::fabs(relativeAngle) <= QuarterTurn * 0.5f) {
        return clips.front;
    }
    if (std::fabs(relativeAngle) >= Pi - QuarterTurn * 0.5f) {
        return clips.back;
    }

    return relativeAngle < 0.0f ? clips.left : clips.right;
}
void AdvanceSectorBillboardAnimatorSystem(engine::World& world, float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    world.ForEach<SectorBillboardAnimator>(
            [dt](engine::Entity, SectorBillboardAnimator& animator) {
                if (!animator.playing || animator.finished || animator.speed <= 0.0f) {
                    return;
                }

                animator.timeSeconds += dt * animator.speed;
            });
}

} // namespace game
