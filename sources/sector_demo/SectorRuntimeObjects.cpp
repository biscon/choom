#include "sector_demo/SectorRuntimeObjects.h"

#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include <raymath.h>

namespace game {

void ReserveSectorRuntimeObjectWorld(engine::World& world, size_t objectCapacity)
{
    world.ReserveEntities(objectCapacity);
    world.ReserveComponentTypes(6);
    world.ReserveComponent<SectorObjectTransform>(objectCapacity);
    world.ReserveComponent<SectorObject>(objectCapacity);
    world.ReserveComponent<SectorObjectLighting>(objectCapacity);
    world.ReserveComponent<SectorBillboardSprite>(objectCapacity);
    world.ReserveComponent<SectorBillboardAnimator>(objectCapacity);
    world.ReserveComponent<SectorBillboardDirectionalClips>(objectCapacity);
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

namespace {

constexpr float TemporaryGoblinDebugSpawnDistance = 2.0f;
constexpr float TemporaryGoblinDebugSpawnEyeToFeet = 1.25f;
constexpr const char* TemporaryGoblinDebugAnimationId = "temporary_goblin_billboard";
constexpr const char* TemporaryGoblinDebugAnimationPath = "assets/sprites/goblin.json";

Vector3 FlattenedCameraForward(const Camera3D& camera)
{
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    forward.y = 0.0f;
    const float lengthSq = Vector3LengthSqr(forward);
    if (lengthSq <= 0.000001f) {
        return Vector3{1.0f, 0.0f, 0.0f};
    }
    return Vector3Scale(forward, 1.0f / std::sqrt(lengthSq));
}

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
    const uint32_t defaultClip = FindClipIndexInAsset(asset, "Default");
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

void ClearDirectionalClips(SectorBillboardDirectionalClips& clips)
{
    clips = SectorBillboardDirectionalClips{};
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
                            SectorBillboardDirectionalClipNames{},
                            directionalClips);
                }
            });
    if (state.objectSectorLookupWorldValid) {
        UpdateSectorObjectCurrentSectorSystem(world, state.objectSectorLookupWorld);
    }
    UpdateSectorObjectBakedLightingSystem(world, state.objectLightProbes, &map);
}

bool HasTemporaryGoblinDebugSpawn(
        const engine::World& world,
        const SectorRuntimeObjectState& state)
{
    return !engine::IsNull(state.temporaryGoblinDebugSpawnEntity)
            && world.IsAlive(state.temporaryGoblinDebugSpawnEntity);
}

bool ToggleTemporaryGoblinDebugSpawn(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const Camera3D& camera,
        const SectorTopologyMap& map)
{
    EnsureSectorRuntimeObjectWorldReserved(world, state);

    // TODO_REMOVE_BILLBOARD_TEST_SPAWN: temporary goblin billboard test spawn.
    // Remove this when real sector object placement/NPC spawning exists.
    if (HasTemporaryGoblinDebugSpawn(world, state)) {
        world.DestroyLater(state.temporaryGoblinDebugSpawnEntity);
        world.FlushDestroyedEntities();
        state.temporaryGoblinDebugSpawnEntity = engine::NullEntity();
        std::fprintf(stderr, "[SectorRuntimeObjects] Removed temporary goblin billboard test spawn\n");
        return false;
    }

    if (engine::IsNull(state.runtimeObjectAssetScope)) {
        state.runtimeObjectAssetScope = assets.CreateScope("sector_runtime_objects");
        if (engine::IsNull(state.runtimeObjectAssetScope)) {
            std::fprintf(stderr, "[SectorRuntimeObjects WARNING] Could not create runtime object asset scope\n");
            return false;
        }
    }

    SectorBillboardSprite sprite;
    SectorBillboardAnimator animator;
    const std::string animationPath = ResolveSectorAssetPath(TemporaryGoblinDebugAnimationPath);
    RequestSectorBillboardSpriteAnimation(
            assets,
            state.runtimeObjectAssetScope,
            TemporaryGoblinDebugAnimationId,
            animationPath.c_str(),
            sprite,
            animator);
    sprite.sizeWorld = Vector2{0.8f, 1.2f};
    sprite.originNormalized = Vector2{0.5f, 1.0f};
    sprite.alphaCutoff = kSectorBillboardDefaultAlphaCutoff;
    sprite.tint = WHITE;

    const Vector3 forward = FlattenedCameraForward(camera);
    Vector3 spawnPosition = Vector3Add(
            camera.position,
            Vector3Scale(forward, TemporaryGoblinDebugSpawnDistance));
    spawnPosition.y = camera.position.y - TemporaryGoblinDebugSpawnEyeToFeet;

    SectorObject object;
    if (state.objectSectorLookupWorldValid) {
        const int foundSectorId = state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                Vector2{spawnPosition.x, spawnPosition.z},
                -1);
        object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
    }

    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, SectorObjectTransform{
            spawnPosition,
            std::atan2(camera.position.z - spawnPosition.z, camera.position.x - spawnPosition.x)});
    world.Add(entity, object);
    world.Add(entity, SectorObjectLighting{SampleBakedObjectLighting(
            state.objectLightProbes,
            spawnPosition,
            object.currentSectorId,
            &map)});
    world.Add(entity, sprite);
    world.Add(entity, animator);
    world.Add(entity, SectorBillboardDirectionalClips{});
    state.temporaryGoblinDebugSpawnEntity = entity;

    std::fprintf(stderr,
            "[SectorRuntimeObjects] Spawned temporary goblin billboard test spawn at %.2f %.2f %.2f\n",
            spawnPosition.x,
            spawnPosition.y,
            spawnPosition.z);
    return true;
}

bool ResolveSectorBillboardDirectionalClipsFromAsset(
        const engine::SpriteAnimationAsset& asset,
        const SectorBillboardDirectionalClipNames& names,
        SectorBillboardDirectionalClips& clips)
{
    ClearDirectionalClips(clips);
    if (asset.clips.empty()) {
        return false;
    }

    bool usedFallback = false;
    clips.front = ResolveDirectionalClipIndex(asset, names.front, "front", usedFallback);
    clips.back = ResolveDirectionalClipIndex(asset, names.back, "back", usedFallback);
    clips.left = ResolveDirectionalClipIndex(asset, names.left, "left", usedFallback);
    clips.right = ResolveDirectionalClipIndex(asset, names.right, "right", usedFallback);
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
    ClearDirectionalClips(clips);
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
