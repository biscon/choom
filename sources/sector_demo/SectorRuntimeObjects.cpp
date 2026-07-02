#include "sector_demo/SectorRuntimeObjects.h"

#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <raymath.h>

namespace game {

void ReserveSectorRuntimeObjectWorld(engine::World& world, size_t objectCapacity)
{
    world.ReserveEntities(objectCapacity);
    world.ReserveComponentTypes(7);
    world.ReserveComponent<SectorObjectTransform>(objectCapacity);
    world.ReserveComponent<SectorObject>(objectCapacity);
    world.ReserveComponent<SectorObjectLighting>(objectCapacity);
    world.ReserveComponent<SectorBillboardSprite>(objectCapacity);
    world.ReserveComponent<SectorBillboardAnimator>(objectCapacity);
    world.ReserveComponent<SectorBillboardDirectionalClips>(objectCapacity);
    world.ReserveComponent<SectorBillboardSingleClip>(objectCapacity);
    world.LockComponentRegistration();
}

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

constexpr const char* SectorRuntimeObjectAssetScopeName = "sector_runtime_objects";

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

SectorBillboardDirectionalClipNames StoredDirectionalClipNames(const SectorBillboardDirectionalClips& clips)
{
    return SectorBillboardDirectionalClipNames{
            clips.frontName.c_str(),
            clips.backName.c_str(),
            clips.leftName.c_str(),
            clips.rightName.c_str()};
}

void StoreDirectionalClipNames(
        SectorBillboardDirectionalClips& clips,
        const SectorBillboardDirectionalClipNames& names)
{
    clips.frontName = names.front != nullptr ? names.front : "";
    clips.backName = names.back != nullptr ? names.back : "";
    clips.leftName = names.left != nullptr ? names.left : "";
    clips.rightName = names.right != nullptr ? names.right : "";
}

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

Vector3 PlacedRuntimeObjectAuthoringToWorldPosition(Vector3 authoringPosition)
{
    // Runtime object placements are saved in the editor's authored coordinate space.
    // X/Z are editor-plane coordinates; Y currently stores authored sector floor height.
    return Vector3{
            SectorAuthoringToWorldDistance(authoringPosition.x),
            SectorAuthoringToWorldDistance(authoringPosition.y),
            SectorAuthoringToWorldDistance(authoringPosition.z)};
}

void RefreshPlacedRuntimeObjectDiagnostics(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state)
{
    size_t requestedCount = 0;
    size_t readyCount = 0;
    size_t pendingCount = 0;
    size_t failedCount = 0;
    size_t clipResolvedCount = 0;
    size_t clipMissingCount = 0;
    size_t clipFallbackCount = 0;
    size_t singleClipResolvedCount = 0;
    size_t singleClipMissingCount = 0;
    size_t singleClipFallbackCount = 0;

    world.ForEach<SectorObject, SectorBillboardSprite>(
            [&assets,
             &requestedCount,
             &readyCount,
             &pendingCount,
             &failedCount](
                    engine::Entity,
                    SectorObject&,
                    SectorBillboardSprite& sprite) {
                if (!engine::IsNull(sprite.animation)) {
                    ++requestedCount;
                    if (assets.IsReady(sprite.animation)) {
                        ++readyCount;
                    } else if (!assets.IsFinished(sprite.animation)) {
                        ++pendingCount;
                    } else if (assets.HasFailed(sprite.animation)) {
                        ++failedCount;
                    } else {
                        ++pendingCount;
                    }
                }
            });

    world.ForEach<SectorObject, SectorBillboardDirectionalClips>(
            [&clipResolvedCount, &clipMissingCount, &clipFallbackCount](
                    engine::Entity,
                    SectorObject&,
                    SectorBillboardDirectionalClips& directionalClips) {
                if (directionalClips.resolved) {
                    ++clipResolvedCount;
                    if (directionalClips.usedFallback) {
                        ++clipFallbackCount;
                    }
                } else {
                    ++clipMissingCount;
                }
            });

    world.ForEach<SectorObject, SectorBillboardSingleClip>(
            [&singleClipResolvedCount, &singleClipMissingCount, &singleClipFallbackCount](
                    engine::Entity,
                    SectorObject&,
                    SectorBillboardSingleClip& singleClip) {
                if (singleClip.resolved) {
                    ++singleClipResolvedCount;
                    if (singleClip.usedFallback) {
                        ++singleClipFallbackCount;
                    }
                } else {
                    ++singleClipMissingCount;
                }
            });

    state.spriteAnimationRequestedCount = requestedCount;
    state.spriteAnimationReadyCount = readyCount;
    state.spriteAnimationPendingCount = pendingCount;
    state.spriteAnimationFailedCount = failedCount;
    state.directionalClipResolvedCount = clipResolvedCount;
    state.directionalClipMissingCount = clipMissingCount;
    state.directionalClipFallbackCount = clipFallbackCount;
    state.singleClipResolvedCount = singleClipResolvedCount;
    state.singleClipMissingCount = singleClipMissingCount;
    state.singleClipFallbackCount = singleClipFallbackCount;

    state.placedObjectStatus = TextFormat(
            "Runtime objects: %zu placed / %zu spawned, %zu skipped | sprites %zu ready, %zu pending, %zu failed | clips %zu resolved, %zu missing",
            state.placedObjectCount,
            state.spawnedObjectCount,
            state.skippedObjectCount,
            readyCount,
            pendingCount,
            failedCount,
            clipResolvedCount + singleClipResolvedCount,
            clipMissingCount + singleClipMissingCount);

    if (state.skippedObjectCount == 0) {
        state.placedObjectWarning.clear();
    }

    if (failedCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu sprite animation asset(s) failed",
                failedCount);
    } else if (clipMissingCount + singleClipMissingCount > 0 && pendingCount == 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu billboard object(s) have missing clips",
                clipMissingCount + singleClipMissingCount);
    } else if (clipFallbackCount + singleClipFallbackCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu billboard object(s) used fallback clips",
                clipFallbackCount + singleClipFallbackCount);
    }
}

bool EnsureSectorRuntimeObjectAssetScope(
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state)
{
    if (!engine::IsNull(state.runtimeObjectAssetScope)) {
        return true;
    }

    state.runtimeObjectAssetScope = assets.CreateScope(SectorRuntimeObjectAssetScopeName);
    if (engine::IsNull(state.runtimeObjectAssetScope)) {
        std::fprintf(stderr, "[SectorRuntimeObjects WARNING] Could not create runtime object asset scope\n");
        return false;
    }
    return true;
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

void EnsureSectorRuntimeObjectWorldReserved(
        engine::World& world,
        SectorRuntimeObjectState& state,
        size_t objectCapacity)
{
    if (state.worldReserved) {
        return;
    }

    ReserveSectorRuntimeObjectWorld(world, objectCapacity);
    state.worldReserved = true;
}

void ClearSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state)
{
    std::vector<engine::Entity> sectorObjects;
    sectorObjects.reserve(kSectorRuntimeObjectInitialCapacity);
    world.ForEach<SectorObject>(
            [&sectorObjects](engine::Entity entity, SectorObject&) {
                sectorObjects.push_back(entity);
            });

    for (engine::Entity entity : sectorObjects) {
        world.DestroyLater(entity);
    }
    world.FlushDestroyedEntities();

    if (!engine::IsNull(state.runtimeObjectAssetScope)) {
        assets.UnloadScope(state.runtimeObjectAssetScope);
    }

    const bool keepReservation = state.worldReserved;
    state = SectorRuntimeObjectState{};
    state.worldReserved = keepReservation;
}

void RefreshSectorRuntimeObjectMapData(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    state.objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
    state.objectProbeStatus.clear();
    if (map.bakedLightmap.objectProbes.path.empty()) {
        state.objectProbeStatus = "Object probes: none";
    } else {
        std::string objectProbeError;
        if (!LoadSectorBakedObjectLightProbeRuntimeData(map, state.objectLightProbes, objectProbeError)) {
            state.objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
            state.objectProbeStatus = objectProbeError.empty()
                    ? "Object probes: unavailable"
                    : objectProbeError;
        } else {
            state.objectProbeStatus = TextFormat(
                    "Object probes: %zu loaded",
                    state.objectLightProbes.probes.size());
        }
    }

    std::string collisionError;
    state.objectSectorLookupWorldValid = state.objectSectorLookupWorld.BuildFromTopology(map, &collisionError);
    state.objectSectorLookupWarning = state.objectSectorLookupWorldValid
            ? std::string{}
            : (collisionError.empty()
                    ? "Object sector lookup build failed"
                    : "Object sector lookup build failed: " + collisionError);
}

void ResetSectorRuntimeObjectsForMap(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    ClearSectorRuntimeObjects(world, assets, state);
    RefreshSectorRuntimeObjectMapData(state, map);
    SpawnPlacedRuntimeObjects(world, assets, state, map);
}

void SpawnPlacedRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    EnsureSectorRuntimeObjectWorldReserved(world, state);

    for (const SectorPlacedRuntimeObjectEntity& entry : state.placedObjectEntities) {
        if (world.IsAlive(entry.entity)) {
            world.DestroyLater(entry.entity);
        }
    }
    world.FlushDestroyedEntities();
    state.placedObjectEntities.clear();
    state.placedObjectCount = map.runtimeObjects.size();
    state.spawnedObjectCount = 0;
    state.skippedObjectCount = 0;
    state.placedObjectWarning.clear();

    size_t spawnedCount = 0;
    size_t skippedCount = 0;
    const auto recordWarning = [&state](const std::string& warning) {
        if (state.placedObjectWarning.empty()) {
            state.placedObjectWarning = "Runtime object warnings: " + warning;
        }
    };
    for (const SectorPlacedRuntimeObject& placedObject : map.runtimeObjects) {
        if (!placedObject.definitionId.empty()) {
            const std::string warning = TextFormat(
                    "legacy definitionId '%s' for placed object %d is unsupported",
                    placedObject.definitionId.c_str(),
                    placedObject.id);
            std::fprintf(stderr,
                    "[SectorRuntimeObjects WARNING] %s\n",
                    warning.c_str());
            recordWarning(warning);
            ++skippedCount;
            continue;
        }

        if (placedObject.kind != "billboard") {
            const std::string warning = TextFormat(
                    "unsupported kind '%s' for placed object %d",
                    placedObject.kind.c_str(),
                    placedObject.id);
            std::fprintf(stderr,
                    "[SectorRuntimeObjects WARNING] %s\n",
                    warning.c_str());
            recordWarning(warning);
            ++skippedCount;
            continue;
        }

        if (placedObject.billboard.spriteAnimationPath.empty()) {
            const std::string warning = TextFormat(
                    "missing billboard sprite animation path for placed object %d",
                    placedObject.id);
            std::fprintf(stderr,
                    "[SectorRuntimeObjects WARNING] %s\n",
                    warning.c_str());
            recordWarning(warning);
            ++skippedCount;
            continue;
        }

        if (!EnsureSectorRuntimeObjectAssetScope(assets, state)) {
            recordWarning(TextFormat("asset scope unavailable for placed object %d", placedObject.id));
            ++skippedCount;
            continue;
        }

        SectorBillboardSprite sprite;
        SectorBillboardAnimator animator;
        const std::string animationPath = ResolveSectorAssetPath(
                placedObject.billboard.spriteAnimationPath);
        RequestSectorBillboardSpriteAnimation(
                assets,
                state.runtimeObjectAssetScope,
                placedObject.billboard.spriteAnimationPath.c_str(),
                animationPath.c_str(),
                sprite,
                animator);
        sprite.sizeWorld = placedObject.billboard.sizeWorld;
        sprite.originNormalized = placedObject.billboard.originNormalized;
        sprite.alphaCutoff = kSectorBillboardDefaultAlphaCutoff;
        sprite.tint = WHITE;
        animator.playing = placedObject.billboard.playing;

        const Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(placedObject.position);
        SectorObject object;
        if (state.objectSectorLookupWorldValid) {
            const int foundSectorId = state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                    Vector2{worldPosition.x, worldPosition.z},
                    -1);
            object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
        }

        const engine::Entity entity = world.CreateEntity();
        world.Add(entity, SectorObjectTransform{worldPosition, placedObject.yawRadians});
        world.Add(entity, object);
        world.Add(entity, SectorObjectLighting{SampleBakedObjectLighting(
                state.objectLightProbes,
                worldPosition,
                object.currentSectorId,
                &map)});
        world.Add(entity, sprite);
        world.Add(entity, animator);
        if (placedObject.billboard.directional) {
            SectorBillboardDirectionalClips directionalClips;
            StoreDirectionalClipNames(
                    directionalClips,
                    SectorBillboardDirectionalClipNames{
                            placedObject.billboard.frontClip.c_str(),
                            placedObject.billboard.backClip.c_str(),
                            placedObject.billboard.leftClip.c_str(),
                            placedObject.billboard.rightClip.c_str()});
            world.Add(entity, directionalClips);
        } else {
            SectorBillboardSingleClip singleClip;
            singleClip.name = placedObject.billboard.clip;
            world.Add(entity, singleClip);
        }
        state.placedObjectEntities.push_back(SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
        ++spawnedCount;
    }

    state.spawnedObjectCount = spawnedCount;
    state.skippedObjectCount = skippedCount;
    RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
}

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt)
{
    AdvanceSectorBillboardAnimatorSystem(world, dt);
    world.ForEach<SectorBillboardSprite, SectorBillboardDirectionalClips>(
            [&assets](engine::Entity, SectorBillboardSprite& sprite, SectorBillboardDirectionalClips& directionalClips) {
                if (!directionalClips.resolved) {
                    ResolveSectorBillboardDirectionalClips(
                            assets,
                            sprite.animation,
                            StoredDirectionalClipNames(directionalClips),
                            directionalClips);
                }
            });
    world.ForEach<SectorBillboardSprite, SectorBillboardSingleClip>(
            [&assets](engine::Entity, SectorBillboardSprite& sprite, SectorBillboardSingleClip& singleClip) {
                if (!singleClip.resolved) {
                    ResolveSectorBillboardSingleClip(
                            assets,
                            sprite.animation,
                            singleClip.name.c_str(),
                            singleClip);
                    if (singleClip.resolved) {
                        sprite.clipIndex = singleClip.clip;
                    }
                }
            });
    if (state.objectSectorLookupWorldValid) {
        UpdateSectorObjectCurrentSectorSystem(world, state.objectSectorLookupWorld);
    }
    UpdateSectorObjectBakedLightingSystem(world, state.objectLightProbes, &map);
    RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
}

bool ResolveSectorBillboardDirectionalClipsFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips)
{
    StoreDirectionalClipNames(clips, names);
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
    StoreDirectionalClipNames(clips, names);
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

void UpdateSectorObjectCurrentSectorSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld)
{
    world.ForEach<SectorObjectTransform, SectorObject>(
            [&collisionWorld](engine::Entity, SectorObjectTransform& transform, SectorObject& object) {
                const int foundSectorId = collisionWorld.FindSectorContainingPointPreferCurrent(
                        Vector2{transform.position.x, transform.position.z},
                        object.currentSectorId);
                object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
            });
}

void UpdateSectorObjectBakedLightingSystem(
        engine::World& world,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback)
{
    world.ForEach<SectorObjectTransform, SectorObject, SectorObjectLighting>(
            [&objectLightProbes, mapForFallback](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting) {
                lighting.baked = SampleBakedObjectLighting(
                        objectLightProbes,
                        transform.position,
                        object.currentSectorId,
                        mapForFallback);
            });
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
