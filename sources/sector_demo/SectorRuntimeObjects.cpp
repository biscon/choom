#include "sector_demo/SectorRuntimeObjects.h"

#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorSwingDoorCatalog.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"
#include "engine/systems/AnimatedModelSystem.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <raymath.h>

namespace game {
namespace {

SectorObjectLighting SampleSectorObjectLighting(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        const SectorTopologyMap* mapForFallback)
{
    const BakedObjectLightingVerticalSample vertical =
            SampleBakedObjectLightingVertical(
                    probes,
                    worldPosition,
                    preferredSectorId,
                    mapForFallback);
    return SectorObjectLighting{
            ResolveBakedObjectLightingVerticalSample(vertical, worldPosition.y),
            vertical};
}

} // namespace


void ReserveSectorRuntimeObjectWorld(engine::World& world, size_t objectCapacity)
{
    world.ReserveEntities(objectCapacity);
    world.ReserveComponentTypes(24);
    world.ReserveComponent<SectorObjectTransform>(objectCapacity);
    world.ReserveComponent<SectorObject>(objectCapacity);
    world.ReserveComponent<SectorObjectLighting>(objectCapacity);
    world.ReserveComponent<SectorObjectVisualOffset>(objectCapacity);
    world.ReserveComponent<SectorStaticModel>(objectCapacity);
    world.ReserveComponent<SectorDynamicModel>(objectCapacity);
    world.ReserveComponent<NpcRuntimeInstance>(objectCapacity);
    world.ReserveComponent<NpcAnimationState>(objectCapacity);
    world.ReserveComponent<engine::AnimatedModelInstance>(objectCapacity);
    world.ReserveComponent<engine::AnimatedModelAnimator>(objectCapacity);
    world.ReserveComponent<SectorStaticModelCollider>(objectCapacity);
    world.ReserveComponent<SectorBillboardSprite>(objectCapacity);
    world.ReserveComponent<SectorBillboardAnimator>(objectCapacity);
    world.ReserveComponent<SectorBillboardDirectionalClips>(objectCapacity);
    world.ReserveComponent<SectorBillboardSingleClip>(objectCapacity);
    world.ReserveComponent<SectorDoor>(objectCapacity);
    world.ReserveComponent<SectorDoorResolvedAnchor>(objectCapacity);
    world.ReserveComponent<SectorDoorMotion>(objectCapacity);
    world.ReserveComponent<SectorDoorAudio>(objectCapacity);
    world.ReserveComponent<SectorDoorInteraction>(objectCapacity);
    world.ReserveComponent<SectorDoorRender>(objectCapacity);
    world.ReserveComponent<SectorDoorCollider>(objectCapacity);
    world.ReserveComponent<SectorDoorPortalBlocker>(objectCapacity);
    world.ReserveComponent<SectorDoorModelRender>(objectCapacity);
    world.LockComponentRegistration();
}


namespace {

constexpr const char* SectorRuntimeObjectAssetScopeName = "sector_runtime_objects";


Vector3 PlacedRuntimeObjectAuthoringToWorldPosition(Vector3 authoringPosition)
{
    // Runtime object placements are saved in the editor's authored coordinate space.
    // X/Z are editor-plane coordinates; Y currently stores authored sector floor height.
    return Vector3{
            SectorAuthoringToWorldDistance(authoringPosition.x),
            SectorAuthoringToWorldDistance(authoringPosition.y),
            SectorAuthoringToWorldDistance(authoringPosition.z)};
}

Vector3 StaticModelSectorAmbient(
        const SectorTopologyMap& map,
        int sectorId)
{
    const SectorTopologySector* sector =
            FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return Vector3{0.15f, 0.15f, 0.15f};
    }
    const float scale = std::max(0.0f, sector->ambientIntensity) / 255.0f;
    return Vector3{
            static_cast<float>(sector->ambientColor.r) * scale,
            static_cast<float>(sector->ambientColor.g) * scale,
            static_cast<float>(sector->ambientColor.b) * scale};
}

float StaticModelEnvironmentExposure(
        const SectorTopologyMap& map,
        int sectorId,
        Vector3 ambient)
{
    const SectorTopologySector* sector =
            FindSectorTopologySector(map, sectorId);
    if (sector != nullptr && sector->ceilingSky) {
        return 1.0f;
    }
    const float luminance = ambient.x * 0.2126f
            + ambient.y * 0.7152f
            + ambient.z * 0.0722f;
    return std::clamp(luminance, 0.08f, 0.35f);
}

void RefreshPlacedRuntimeObjectDiagnostics(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state)
{
    const std::string spawnWarning = state.placedObjectWarning;
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
    size_t modelRequestedCount = 0;
    size_t modelReadyCount = 0;
    size_t modelPendingCount = 0;
    size_t modelFailedCount = 0;
    size_t modelUnassignedCount = 0;

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

    world.ForEach<SectorObject, SectorStaticModel>(
            [&assets,
             &modelRequestedCount,
             &modelReadyCount,
             &modelPendingCount,
             &modelFailedCount,
             &modelUnassignedCount](
                    engine::Entity,
                    SectorObject&,
                    SectorStaticModel& staticModel) {
                if (engine::IsNull(staticModel.model)) {
                    ++modelUnassignedCount;
                } else {
                    ++modelRequestedCount;
                    if (assets.IsReady(staticModel.model)) {
                        ++modelReadyCount;
                    } else if (!assets.IsFinished(staticModel.model)) {
                        ++modelPendingCount;
                    } else if (assets.HasFailed(staticModel.model)) {
                        ++modelFailedCount;
                    } else {
                        ++modelPendingCount;
                    }
                }
            });

    state.spriteAnimationRequestedCount = requestedCount;
    state.spriteAnimationReadyCount = readyCount;
    state.spriteAnimationPendingCount = pendingCount;
    state.spriteAnimationFailedCount = failedCount;
    state.staticModelRequestedCount = modelRequestedCount;
    state.staticModelReadyCount = modelReadyCount;
    state.staticModelPendingCount = modelPendingCount;
    state.staticModelFailedCount = modelFailedCount;
    state.staticModelUnassignedCount = modelUnassignedCount;
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
    if (modelRequestedCount + modelUnassignedCount > 0) {
        state.placedObjectStatus += TextFormat(
                " | models %zu ready, %zu pending, %zu failed, %zu unassigned",
                modelReadyCount,
                modelPendingCount,
                modelFailedCount,
                modelUnassignedCount);
    }
    if (state.doorObjectCount > 0) {
        state.placedObjectStatus += TextFormat(
                " | doors %zu valid, %zu invalid anchors, %zu fallback, %zu frame failures",
                state.validDoorAnchorCount,
                state.invalidDoorAnchorCount,
                state.doorFallbackCount,
                state.doorFrameFailureCount);
    }

    if (state.invalidDoorAnchorCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu door object(s) have invalid anchors",
                state.invalidDoorAnchorCount);
    } else if (!state.swingDoorCatalogWarning.empty()) {
        state.placedObjectWarning = state.swingDoorCatalogWarning;
    } else if (state.doorFallbackCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu door object(s) use procedural fallback",
                state.doorFallbackCount);
    } else if (state.doorFrameFailureCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu door frame model asset(s) failed",
                state.doorFrameFailureCount);
    } else if (modelFailedCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu static model asset(s) failed",
                modelFailedCount);
    } else if (modelUnassignedCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu static model object(s) have no model assigned",
                modelUnassignedCount);
    } else if (failedCount > 0) {
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
    } else if (state.skippedObjectCount > 0) {
        state.placedObjectWarning = spawnWarning;
    } else {
        state.placedObjectWarning.clear();
    }
}

void RefreshDoorAnchorDiagnostics(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    state.doorAnchorDiagnostics.clear();
    state.doorAnchorDiagnostics.reserve(map.runtimeObjects.size());
    state.doorObjectCount = 0;
    state.validDoorAnchorCount = 0;
    state.invalidDoorAnchorCount = 0;

    for (const SectorPlacedRuntimeObject& placedObject : map.runtimeObjects) {
        if (placedObject.kind != "door") {
            continue;
        }

        ++state.doorObjectCount;
        const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(map, placedObject.door);
        if (resolved.valid) {
            ++state.validDoorAnchorCount;
            continue;
        }

        ++state.invalidDoorAnchorCount;
        SectorDoorAnchorDiagnostic diagnostic;
        diagnostic.placedObjectId = placedObject.id;
        diagnostic.lineDefId = placedObject.door.anchor.lineDefId;
        diagnostic.message = TextFormat(
                "door object %d anchor on linedef %d is invalid: %s",
                placedObject.id,
                placedObject.door.anchor.lineDefId,
                resolved.diagnostic.empty() ? "unknown anchor error" : resolved.diagnostic.c_str());
        state.doorAnchorDiagnostics.push_back(std::move(diagnostic));
    }
}

void ReloadSwingDoorCatalogData(SectorRuntimeObjectState& state)
{
    ++state.swingDoorCatalogRevision;
    state.swingDoorCatalog = SectorSwingDoorCatalog{};
    state.swingDoorCatalogLoaded = false;
    state.swingDoorCatalogStatus.clear();
    state.swingDoorCatalogWarning.clear();

    std::string error;
    const std::string resolvedPath = ResolveSectorAssetPath(
            kSectorSwingDoorCatalogAssetPath);
    if (!LoadSectorSwingDoorCatalog(resolvedPath, state.swingDoorCatalog, error)) {
        state.swingDoorCatalogStatus = "Swing door catalog: unavailable";
        state.swingDoorCatalogWarning = "Runtime object warnings: swing door catalog unavailable: "
                + (error.empty() ? std::string{"unknown catalog error"} : error);
        std::fprintf(
                stderr,
                "[SectorRuntimeObjects WARNING] %s\n",
                state.swingDoorCatalogWarning.c_str());
        return;
    }

    state.swingDoorCatalogLoaded = true;
    state.swingDoorCatalogStatus = TextFormat(
            "Swing door catalog: %zu styles",
            state.swingDoorCatalog.assets.size());
}

void ReloadNpcDefinitionCatalogData(SectorRuntimeObjectState& state)
{
    ++state.npcDefinitionCatalogRevision;
    state.npcDefinitionCatalog = NpcDefinitionCatalog{};
    state.npcDefinitionCatalogStatus.clear();
    state.npcDefinitionCatalogWarning.clear();

    const std::filesystem::path root = ResolveSectorAssetPath(
            kNpcDefinitionsAssetRoot);
    const bool valid = DiscoverNpcDefinitions(root, state.npcDefinitionCatalog);
    state.npcDefinitionCatalogStatus = TextFormat(
            "NPC definitions: %zu loaded",
            state.npcDefinitionCatalog.definitions.size());
    if (!valid && !state.npcDefinitionCatalog.errors.empty()) {
        const NpcDefinitionCatalogError& error =
                state.npcDefinitionCatalog.errors.front();
        state.npcDefinitionCatalogWarning =
                "Runtime object warnings: NPC catalog: "
                + error.path + ": " + error.message;
        std::fprintf(
                stderr,
                "[SectorRuntimeObjects WARNING] %s\n",
                state.npcDefinitionCatalogWarning.c_str());
    }
}

void RefreshDoorFallbackDiagnostics(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    state.doorFallbackDiagnostics.clear();
    state.doorFallbackDiagnostics.reserve(map.runtimeObjects.size());
    state.doorFallbackCount = 0;
    state.doorFrameFailureCount = 0;

    for (const SectorPlacedRuntimeObject& placedObject : map.runtimeObjects) {
        if (placedObject.kind != "door"
                || placedObject.door.visual != SectorDoorVisualType::Model) {
            continue;
        }

        SectorDoorFallbackDiagnostic diagnostic;
        diagnostic.placedObjectId = placedObject.id;
        diagnostic.modelAssetId = placedObject.door.modelAssetId;
        if (placedObject.door.visual == SectorDoorVisualType::Model) {
            SectorSwingDoorCatalogAsset asset;
            if (!state.swingDoorCatalogLoaded) {
                diagnostic.message = TextFormat(
                        "door object %d model style '%s' is unavailable because the swing door catalog failed; using procedural fallback",
                        placedObject.id,
                        placedObject.door.modelAssetId.c_str());
            } else if (!FindSectorSwingDoorCatalogAsset(
                               state.swingDoorCatalog,
                               placedObject.door.modelAssetId,
                               asset)) {
                diagnostic.message = TextFormat(
                        "door object %d model style '%s' is missing from the swing door catalog; using procedural fallback",
                        placedObject.id,
                        placedObject.door.modelAssetId.c_str());
            } else {
                const SectorResolvedDoorAnchor resolved =
                        ResolveSectorDoorAnchor(map, placedObject.door);
                const SectorSwingDoorFitResult fit = resolved.valid
                        ? ComputeSectorSwingDoorFit(
                                asset,
                                resolved.width,
                                resolved.height,
                                placedObject.door.modelFit,
                                placedObject.door.modelScale)
                        : SectorSwingDoorFitResult{};
                if (fit.status != SectorSwingDoorFitStatus::InvalidInput) {
                    continue;
                }
                diagnostic.message = TextFormat(
                        "door object %d model style '%s' has invalid fit inputs; using procedural fallback",
                        placedObject.id,
                        placedObject.door.modelAssetId.c_str());
            }
        }
        state.doorFallbackDiagnostics.push_back(std::move(diagnostic));
        ++state.doorFallbackCount;
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

void ResolveDynamicModelAnimations(
        engine::World& world,
        engine::AssetManager& assets)
{
    world.ForEach<SectorDynamicModel, engine::AnimatedModelInstance, engine::AnimatedModelAnimator>(
            [&assets](
                    engine::Entity,
                    SectorDynamicModel& dynamicModel,
                    engine::AnimatedModelInstance& instance,
                    engine::AnimatedModelAnimator& animator) {
                if (dynamicModel.animationResolved) {
                    return;
                }
                const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
                if (asset == nullptr) {
                    return;
                }

                if (asset->animationCount <= 0 || asset->animations == nullptr) {
                    dynamicModel.animationResolved = true;
                    return;
                }

                uint32_t animationIndex = engine::FindModelAnimationIndex(
                        *asset,
                        dynamicModel.requestedAnimation.c_str());
                if (animationIndex == engine::InvalidModelAnimationIndex) {
                    animationIndex = 0;
                    dynamicModel.animationFallback = !dynamicModel.requestedAnimation.empty();
                    if (dynamicModel.animationFallback) {
                        std::fprintf(
                                stderr,
                                "[SectorRuntimeObjects WARNING] Animation '%s' was not found for dynamic model object %d; using '%s'.\n",
                                dynamicModel.requestedAnimation.c_str(),
                                dynamicModel.placedObjectId,
                                asset->animations[0].name);
                    }
                }

                engine::SetAnimatedModelAnimation(animator, animationIndex);
                if (!animator.loop) {
                    animator.playing = false;
                    animator.frame = 0.0f;
                    animator.poseDirty = true;
                }
                dynamicModel.animationResolved = true;
            });
}

void UpdateSectorDoorModelLighting(
        engine::World& world,
        const SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    world.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorObjectLighting,
            SectorDoorResolvedAnchor,
            SectorDoorModelRender>(
            [&state, &map](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorModelRender& model) {
                int containingSectorId = 0;
                if (state.objectSectorLookupWorldValid) {
                    containingSectorId =
                            state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                                    Vector2{transform.position.x, transform.position.z},
                                    object.currentSectorId);
                }
                object.currentSectorId = ResolveSectorDoorAdjacentLightingSector(
                        anchor, transform.position, containingSectorId);
                lighting = SampleSectorObjectLighting(
                        state.objectLightProbes,
                        transform.position,
                        object.currentSectorId,
                        &map);
                model.containingSectorAmbient = StaticModelSectorAmbient(
                        map, object.currentSectorId);
                model.environmentExposure = StaticModelEnvironmentExposure(
                        map,
                        object.currentSectorId,
                        model.containingSectorAmbient);
            });
}

bool RefreshSectorDoorModelFailureDiagnostics(
        engine::World& world,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    bool changed = false;
    world.ForEach<SectorDoor, SectorDoorModelRender>(
            [&state, &map, &changed](
                    engine::Entity,
                    SectorDoor& door,
                    SectorDoorModelRender& model) {
                if (!model.modelVisualRequested) {
                    return;
                }
                const bool reportLeafFailure =
                        model.leafFailed && !model.leafFailureReported;
                const bool reportFrameFailure =
                        model.frameFailed && !model.frameFailureReported;
                if (!reportLeafFailure && !reportFrameFailure) {
                    return;
                }
                const SectorPlacedRuntimeObject* placed =
                        FindSectorPlacedRuntimeObject(map, door.placedObjectId);
                const std::string modelAssetId = placed != nullptr
                        ? placed->door.modelAssetId : std::string{};
                if (reportLeafFailure) {
                    model.leafFailureReported = true;
                    state.doorFallbackDiagnostics.push_back(SectorDoorFallbackDiagnostic{
                            door.placedObjectId,
                            modelAssetId,
                            TextFormat(
                                    "door object %d model style '%s' leaf asset failed; using procedural fallback",
                                    door.placedObjectId,
                                    modelAssetId.c_str())});
                    ++state.doorFallbackCount;
                    changed = true;
                }
                if (reportFrameFailure) {
                    model.frameFailureReported = true;
                    state.doorFallbackDiagnostics.push_back(SectorDoorFallbackDiagnostic{
                            door.placedObjectId,
                            modelAssetId,
                            TextFormat(
                                    "door object %d model style '%s' frame asset failed; drawing the leaf without its frame",
                                    door.placedObjectId,
                                    modelAssetId.c_str())});
                    ++state.doorFrameFailureCount;
                    std::fprintf(
                            stderr,
                            "[SectorRuntimeObjects WARNING] Door object %d model style '%s' frame asset failed; drawing the leaf without its frame\n",
                            door.placedObjectId,
                            modelAssetId.c_str());
                    changed = true;
                }
            });
    return changed;
}


} // namespace

float ComputeSectorModelEnvironmentExposure(
        const SectorTopologyMap& map,
        int sectorId)
{
    const Vector3 ambient = StaticModelSectorAmbient(map, sectorId);
    return StaticModelEnvironmentExposure(map, sectorId, ambient);
}

Vector3 ComputeSectorModelAmbient(
        const SectorTopologyMap& map,
        int sectorId)
{
    return StaticModelSectorAmbient(map, sectorId);
}

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
    sectorObjects.reserve(std::max(
            kSectorRuntimeObjectInitialCapacity,
            state.placedObjectEntities.size()));
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
    const uint64_t keepSwingDoorCatalogRevision = state.swingDoorCatalogRevision;
    const uint64_t keepNpcDefinitionCatalogRevision =
            state.npcDefinitionCatalogRevision;
    state = SectorRuntimeObjectState{};
    state.worldReserved = keepReservation;
    state.swingDoorCatalogRevision = keepSwingDoorCatalogRevision;
    state.npcDefinitionCatalogRevision = keepNpcDefinitionCatalogRevision;
}

void ReloadSectorSwingDoorCatalog(SectorRuntimeObjectState& state)
{
    ReloadSwingDoorCatalogData(state);
}

void ReloadSectorNpcDefinitionCatalog(SectorRuntimeObjectState& state)
{
    ReloadNpcDefinitionCatalogData(state);
}

void RefreshSectorRuntimeObjectMapData(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    ReloadSwingDoorCatalogData(state);
    ReloadNpcDefinitionCatalogData(state);
    RefreshDoorAnchorDiagnostics(state, map);
    RefreshDoorFallbackDiagnostics(state, map);

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
    // Explicit object refreshes may follow authored door edits without a map-data
    // reload. Re-evaluate CPU diagnostics against the retained catalog, but do
    // not perform catalog file I/O here.
    RefreshDoorAnchorDiagnostics(state, map);
    RefreshDoorFallbackDiagnostics(state, map);

    for (const SectorPlacedRuntimeObjectEntity& entry : state.placedObjectEntities) {
        if (world.IsAlive(entry.entity)) {
            world.DestroyLater(entry.entity);
        }
    }
    world.FlushDestroyedEntities();
    state.placedObjectEntities.clear();
    state.placedObjectEntities.reserve(map.runtimeObjects.size());
    state.dynamicDoorColliders.clear();
    state.dynamicDoorColliders.reserve(map.runtimeObjects.size());
    state.dynamicPortalBlockers.clear();
    state.dynamicPortalBlockers.reserve(map.runtimeObjects.size() * 2);
    state.doorCollisionCacheInitialized = false;
    state.staticModelColliders.clear();
    state.staticModelColliders.reserve(map.runtimeObjects.size());
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

        if (placedObject.kind == "door") {
            const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(map, placedObject.door);
            if (!resolved.valid) {
                const std::string warning = TextFormat(
                        "door object %d anchor on linedef %d is invalid: %s",
                        placedObject.id,
                        placedObject.door.anchor.lineDefId,
                        resolved.diagnostic.empty() ? "unknown anchor error" : resolved.diagnostic.c_str());
                std::fprintf(stderr,
                        "[SectorRuntimeObjects WARNING] %s\n",
                        warning.c_str());
                recordWarning(warning);
                ++skippedCount;
                continue;
            }

            float fallbackWidth = resolved.width;
            float fallbackHeight = resolved.height;
            float fallbackThickness = placedObject.door.thickness;
            SectorDoorModelRender modelRender;
            modelRender.modelVisualRequested =
                    placedObject.door.visual == SectorDoorVisualType::Model;
            modelRender.actualWidth = fallbackWidth;
            modelRender.actualHeight = fallbackHeight;
            modelRender.actualThickness = fallbackThickness;
            if (modelRender.modelVisualRequested) {
                modelRender.fallbackReason = SectorDoorModelFallbackReason::CatalogUnavailable;
                SectorSwingDoorCatalogAsset catalogAsset;
                if (state.swingDoorCatalogLoaded
                        && FindSectorSwingDoorCatalogAsset(
                                state.swingDoorCatalog,
                                placedObject.door.modelAssetId,
                                catalogAsset)) {
                    modelRender.fallbackReason = SectorDoorModelFallbackReason::InvalidFit;
                    const SectorSwingDoorFitResult fit = ComputeSectorSwingDoorFit(
                            catalogAsset,
                            resolved.width,
                            resolved.height,
                            placedObject.door.modelFit,
                            placedObject.door.modelScale);
                    if (fit.status != SectorSwingDoorFitStatus::InvalidInput) {
                        fallbackWidth = fit.actualWidth;
                        fallbackHeight = fit.actualHeight;
                        fallbackThickness = fit.actualThickness;
                        modelRender.catalogResolved = true;
                        modelRender.effectiveScale = fit.effectiveScale;
                        modelRender.nominalWidth = catalogAsset.nominalWidth;
                        modelRender.nominalHeight = catalogAsset.nominalHeight;
                        modelRender.nominalThickness = catalogAsset.nominalThickness;
                        modelRender.actualWidth = fit.actualWidth;
                        modelRender.actualHeight = fit.actualHeight;
                        modelRender.actualThickness = fit.actualThickness;
                        modelRender.frameDeclared = catalogAsset.hasFrame;
                        modelRender.frameOuterWidth = catalogAsset.frameOuterWidth;
                        modelRender.frameOuterHeight = catalogAsset.frameOuterHeight;
                        modelRender.leafHingeToFrameCenter =
                                catalogAsset.leafHingeToFrameCenter;
                        modelRender.leafBottomOffset = catalogAsset.leafBottomOffset;
                        modelRender.fallbackReason = SectorDoorModelFallbackReason::None;

                        if (!EnsureSectorRuntimeObjectAssetScope(assets, state)) {
                            modelRender.fallbackReason =
                                    SectorDoorModelFallbackReason::AssetScopeUnavailable;
                        } else {
                            const std::string leafPath = ResolveSectorAssetPath(
                                    catalogAsset.leafModelPath);
                            modelRender.leafModel = assets.RequestModel(
                                    state.runtimeObjectAssetScope,
                                    catalogAsset.leafModelPath.c_str(),
                                    leafPath.c_str());
                            if (engine::IsNull(modelRender.leafModel)) {
                                modelRender.fallbackReason =
                                        SectorDoorModelFallbackReason::LeafRequestFailed;
                            }
                            if (catalogAsset.hasFrame) {
                                const std::string framePath = ResolveSectorAssetPath(
                                        catalogAsset.frameModelPath);
                                modelRender.frameModel = assets.RequestModel(
                                        state.runtimeObjectAssetScope,
                                        catalogAsset.frameModelPath.c_str(),
                                        framePath.c_str());
                            }
                        }
                    }
                } else if (state.swingDoorCatalogLoaded) {
                    modelRender.fallbackReason =
                            SectorDoorModelFallbackReason::MissingCatalogAsset;
                }

                if (modelRender.fallbackReason
                                == SectorDoorModelFallbackReason::AssetScopeUnavailable
                        || modelRender.fallbackReason
                                == SectorDoorModelFallbackReason::LeafRequestFailed) {
                    SectorDoorFallbackDiagnostic diagnostic;
                    diagnostic.placedObjectId = placedObject.id;
                    diagnostic.modelAssetId = placedObject.door.modelAssetId;
                    diagnostic.message = TextFormat(
                            "door object %d model style '%s' could not request its leaf model; using procedural fallback",
                            placedObject.id,
                            placedObject.door.modelAssetId.c_str());
                    state.doorFallbackDiagnostics.push_back(std::move(diagnostic));
                    ++state.doorFallbackCount;
                }
            }

            const SectorDoorResolvedAnchor runtimeAnchor = ToSectorRuntimeDoorAnchor(resolved);
            const SectorDoorMotion runtimeMotion{
                    placedObject.door.motion,
                    placedObject.door.initialOpenFraction,
                    placedObject.door.initialOpenFraction,
                    placedObject.door.motion == SectorDoorMotionType::Swing
                            ? placedObject.door.openAngleDegrees * DEG2RAD
                            : SectorDoorResolvedOpenDistance(resolved, placedObject.door),
                    placedObject.door.motion == SectorDoorMotionType::Swing
                            ? placedObject.door.angularSpeedDegrees * DEG2RAD
                            : placedObject.door.speed,
                    placedObject.door.hinge,
                    placedObject.door.swingSide};
            SectorDoorRender runtimeRender{
                    fallbackWidth,
                    fallbackHeight,
                    fallbackThickness,
                    placedObject.door.normalOffset,
                    placedObject.door.heightOffsetWorld,
                    placedObject.door.textureId,
                    placedObject.door.faceUvs,
                    WHITE,
                    true};
            if (modelRender.catalogResolved && modelRender.frameDeclared) {
                runtimeRender.leafHingeToFrameCenter =
                        modelRender.leafHingeToFrameCenter
                        * modelRender.effectiveScale;
                runtimeRender.leafBottomOffset = modelRender.leafBottomOffset
                        * modelRender.effectiveScale;
                runtimeRender.alignLeafToFrame = true;
            }
            const Vector3 worldPosition = placedObject.door.motion == SectorDoorMotionType::Swing
                    ? BuildSectorDoorSwingPose(
                            runtimeAnchor,
                            runtimeMotion,
                            runtimeRender,
                            modelRender.effectiveScale,
                            runtimeMotion.openFraction).center
                    : Vector3Add(
                            SectorDoorClosedCenter(runtimeAnchor, runtimeRender),
                            SectorDoorMotionOffset(runtimeAnchor, runtimeMotion));
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int foundSectorId = state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                        Vector2{worldPosition.x, worldPosition.z},
                        resolved.frontSectorId);
                object.currentSectorId = ResolveSectorDoorAdjacentLightingSector(
                        runtimeAnchor, worldPosition, foundSectorId);
            } else {
                object.currentSectorId = resolved.frontSectorId;
            }
            modelRender.containingSectorAmbient = StaticModelSectorAmbient(
                    map, object.currentSectorId);
            modelRender.environmentExposure = StaticModelEnvironmentExposure(
                    map,
                    object.currentSectorId,
                    modelRender.containingSectorAmbient);

            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{worldPosition, SectorDoorAnchorYawRadians(resolved)});
            world.Add(entity, object);
            world.Add(entity, SampleSectorObjectLighting(
                    state.objectLightProbes,
                    worldPosition,
                    object.currentSectorId,
                    &map));
            world.Add(entity, SectorDoor{placedObject.id, true});
            world.Add(entity, runtimeAnchor);
            world.Add(entity, runtimeMotion);
            world.Add(entity, SectorDoorAudio{
                    placedObject.door.openSoundId,
                    placedObject.door.closeSoundId,
                    engine::NullSoundHandle(),
                    engine::NullSoundHandle(),
                    runtimeMotion.targetOpenFraction > 0.5f,
                    SectorDoorAudioEvent::None});
            world.Add(entity, SectorDoorInteraction{
                    placedObject.door.autoOpen,
                    placedObject.door.interactionDistance,
                    placedObject.door.autoOpenDistance});
            world.Add(entity, runtimeRender);
            world.Add(entity, SectorDoorCollider{});
            world.Add(entity, SectorDoorPortalBlocker{
                    resolved.lineDefId,
                    resolved.frontSectorId,
                    resolved.backSectorId,
                    resolved.frontSideDefId,
                    resolved.backSideDefId,
                    placedObject.door.initialOpenFraction <= kSectorDoorPortalBlockEpsilon});
            if (placedObject.door.motion == SectorDoorMotionType::Swing) {
                world.Add(entity, modelRender);
            }
            state.placedObjectEntities.push_back(SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
            ++spawnedCount;
            continue;
        }

        if (placedObject.kind == "static_model") {
            engine::ModelHandle model = engine::NullModelHandle();
            if (!placedObject.staticModel.modelPath.empty()) {
                if (!EnsureSectorRuntimeObjectAssetScope(assets, state)) {
                    recordWarning(TextFormat("asset scope unavailable for placed object %d", placedObject.id));
                    ++skippedCount;
                    continue;
                }
                const std::string modelPath = ResolveSectorAssetPath(
                        placedObject.staticModel.modelPath);
                model = assets.RequestModel(
                        state.runtimeObjectAssetScope,
                        placedObject.staticModel.modelPath.c_str(),
                        modelPath.c_str());
                if (engine::IsNull(model)) {
                    const std::string warning = TextFormat(
                            "could not request static model '%s' for placed object %d",
                            placedObject.staticModel.modelPath.c_str(),
                            placedObject.id);
                    std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n", warning.c_str());
                    recordWarning(warning);
                }
            }

            Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(
                    placedObject.position);
            worldPosition.y += placedObject.staticModel.heightOffsetWorld;
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int foundSectorId =
                        state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                                Vector2{worldPosition.x, worldPosition.z},
                                -1);
                object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
            }

            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{
                    worldPosition,
                    placedObject.yawRadians,
                    placedObject.staticModel.rotationXRadians,
                    placedObject.staticModel.rotationZRadians});
            world.Add(entity, object);
            const Vector3 sectorAmbient =
                    StaticModelSectorAmbient(map, object.currentSectorId);
            world.Add(entity, SectorStaticModel{
                    model,
                    placedObject.id,
                    sectorAmbient,
                    placedObject.staticModel.scale,
                    StaticModelEnvironmentExposure(
                            map,
                            object.currentSectorId,
                            sectorAmbient)});
            if (placedObject.staticModel.collision) {
                world.Add(entity, SectorStaticModelCollider{
                        placedObject.id});
            }
            state.placedObjectEntities.push_back(
                    SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
            ++spawnedCount;
            continue;
        }

        if (placedObject.kind == "npc") {
            const NpcDefinition* definition = FindNpcDefinition(
                    state.npcDefinitionCatalog,
                    placedObject.npc.definitionId);
            if (definition == nullptr) {
                const std::string warning = TextFormat(
                        "NPC definition '%s' was not found for placed object %d",
                        placedObject.npc.definitionId.c_str(),
                        placedObject.id);
                std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n", warning.c_str());
                recordWarning(warning);
                ++skippedCount;
                continue;
            }

            engine::ModelHandle model = engine::NullModelHandle();
            if (!EnsureSectorRuntimeObjectAssetScope(assets, state)) {
                recordWarning(TextFormat(
                        "asset scope unavailable for NPC object %d",
                        placedObject.id));
                ++skippedCount;
                continue;
            }
            const std::string modelPath = ResolveSectorAssetPath(
                    definition->modelPath);
            model = assets.RequestModel(
                    state.runtimeObjectAssetScope,
                    definition->modelPath.c_str(),
                    modelPath.c_str(),
                    engine::ModelLoad_Animations);
            if (engine::IsNull(model)) {
                const std::string warning = TextFormat(
                        "could not request NPC model '%s' for placed object %d",
                        definition->modelPath.c_str(),
                        placedObject.id);
                std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n", warning.c_str());
                recordWarning(warning);
            }

            const NpcActionDefinition& idle = GetNpcAction(
                    *definition,
                    NpcAction::Idle);
            Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(
                    placedObject.position);
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int foundSectorId =
                        state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                                Vector2{worldPosition.x, worldPosition.z}, -1);
                object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
            }
            const Vector3 sectorAmbient = StaticModelSectorAmbient(
                    map,
                    object.currentSectorId);

            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{
                    worldPosition,
                    placedObject.yawRadians,
                    0.0f,
                    0.0f});
            world.Add(entity, object);
            world.Add(entity, SampleSectorObjectLighting(
                    state.objectLightProbes,
                    worldPosition,
                    object.currentSectorId,
                    &map));
            world.Add(entity, NpcRuntimeInstance{
                    definition->id,
                    placedObject.npc.instanceId,
                    NpcAction::Idle,
                    definition->hostile,
                    GetNpcAction(*definition, NpcAction::Walk).movementSpeed,
                    GetNpcAction(*definition, NpcAction::Run).movementSpeed});
            NpcAnimationState npcAnimation;
            npcAnimation.blendSeconds = definition->animationBlendSeconds;
            for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
                npcAnimation.animationSpeeds[static_cast<size_t>(metadata.action)] =
                        GetNpcAction(*definition, metadata.action).animationSpeed;
            }
            world.Add(entity, npcAnimation);
            world.Add(entity, SectorObjectVisualOffset{});
            world.Add(entity, SectorDynamicModel{
                    placedObject.id,
                    sectorAmbient,
                    placedObject.npc.scale,
                    StaticModelEnvironmentExposure(
                            map,
                            object.currentSectorId,
                            sectorAmbient),
                    idle.animation,
                    false,
                    false,
                    placedObject.npc.shadowMode});
            world.Add(entity, engine::AnimatedModelInstance{model});
            engine::AnimatedModelAnimator animator;
            animator.speed = idle.animationSpeed;
            animator.loop = true;
            animator.playing = true;
            world.Add(entity, animator);
            state.placedObjectEntities.push_back(
                    SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
            ++spawnedCount;
            continue;
        }

        if (placedObject.kind == "dynamic_model") {
            engine::ModelHandle model = engine::NullModelHandle();
            if (!placedObject.dynamicModel.modelPath.empty()) {
                if (!EnsureSectorRuntimeObjectAssetScope(assets, state)) {
                    recordWarning(TextFormat("asset scope unavailable for placed object %d", placedObject.id));
                    ++skippedCount;
                    continue;
                }
                const std::string modelPath = ResolveSectorAssetPath(
                        placedObject.dynamicModel.modelPath);
                model = assets.RequestModel(
                        state.runtimeObjectAssetScope,
                        placedObject.dynamicModel.modelPath.c_str(),
                        modelPath.c_str(),
                        engine::ModelLoad_Animations);
                if (engine::IsNull(model)) {
                    const std::string warning = TextFormat(
                            "could not request dynamic model '%s' for placed object %d",
                            placedObject.dynamicModel.modelPath.c_str(),
                            placedObject.id);
                    std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n", warning.c_str());
                    recordWarning(warning);
                }
            }

            Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(
                    placedObject.position);
            worldPosition.y += placedObject.dynamicModel.heightOffsetWorld;
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int foundSectorId =
                        state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                                Vector2{worldPosition.x, worldPosition.z}, -1);
                object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
            }
            const Vector3 sectorAmbient = StaticModelSectorAmbient(map, object.currentSectorId);

            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{
                    worldPosition,
                    placedObject.yawRadians,
                    placedObject.dynamicModel.rotationXRadians,
                    placedObject.dynamicModel.rotationZRadians});
            world.Add(entity, object);
            world.Add(entity, SampleSectorObjectLighting(
                    state.objectLightProbes,
                    worldPosition,
                    object.currentSectorId,
                    &map));
            world.Add(entity, SectorDynamicModel{
                    placedObject.id,
                    sectorAmbient,
                    placedObject.dynamicModel.scale,
                    StaticModelEnvironmentExposure(map, object.currentSectorId, sectorAmbient),
                    placedObject.dynamicModel.animation,
                    false,
                    false,
                    placedObject.dynamicModel.shadowMode});
            world.Add(entity, engine::AnimatedModelInstance{model});
            engine::AnimatedModelAnimator animator;
            animator.speed = placedObject.dynamicModel.animationSpeed;
            animator.loop = placedObject.dynamicModel.loop;
            animator.playing = placedObject.dynamicModel.loop;
            world.Add(entity, animator);
            if (placedObject.dynamicModel.collision) {
                world.Add(entity, SectorStaticModelCollider{placedObject.id});
            }
            state.placedObjectEntities.push_back(
                    SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
            ++spawnedCount;
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
        world.Add(entity, SampleSectorObjectLighting(
                state.objectLightProbes,
                worldPosition,
                object.currentSectorId,
                &map));
        world.Add(entity, sprite);
        world.Add(entity, animator);
        if (placedObject.billboard.directional) {
            SectorBillboardDirectionalClips directionalClips;
            StoreSectorBillboardDirectionalClipNames(
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
    UpdateSectorDoorDerivedStateSystem(world);
    ResolveDynamicModelAnimations(world, assets);
    UpdateSectorStaticModelColliderSystem(world, assets);
    CollectSectorStaticModelColliders(world, state.staticModelColliders);
    RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
}

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition,
        const SectorDoorPlayerObstacle* playerObstacle)
{
    AdvanceSectorBillboardAnimatorSystem(world, dt);
    ResolveDynamicModelAnimations(world, assets);
    if (playerPosition != nullptr) {
        UpdateSectorDoorAutoOpenSystem(world, *playerPosition);
    }
    state.doorSpatialStateChanged = AdvanceSectorDoorMotionSystem(
            world, dt, playerObstacle);
    if (state.doorSpatialStateChanged) {
        UpdateSectorDoorDerivedStateSystem(world);
    }
    if (UpdateSectorStaticModelColliderSystem(world, assets)) {
        CollectSectorStaticModelColliders(world, state.staticModelColliders);
    }
    const bool doorModelReadinessChanged =
            RefreshSectorDoorModelReadinessSystem(world, assets);
    if (doorModelReadinessChanged) {
        UpdateSectorDoorDerivedStateSystem(world);
    }
    if (state.doorSpatialStateChanged) {
        UpdateSectorDoorModelLighting(world, state, map);
    }
    if (RefreshSectorDoorModelFailureDiagnostics(world, state, map)) {
        RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
    }
    world.ForEach<SectorBillboardSprite, SectorBillboardDirectionalClips>(
            [&assets](engine::Entity, SectorBillboardSprite& sprite, SectorBillboardDirectionalClips& directionalClips) {
                if (!directionalClips.resolved) {
                    ResolveSectorBillboardDirectionalClips(
                            assets,
                            sprite.animation,
                            SectorBillboardStoredDirectionalClipNames(directionalClips),
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
    // Placed object transforms are immutable between explicit map/object
    // refreshes. Sector membership and baked probes are assigned at spawn, so
    // avoid rescanning every object during the steady frame.
    (void)map;
    if (state.spriteAnimationPendingCount > 0
            || state.staticModelPendingCount > 0) {
        RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
    }
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
                lighting = SampleSectorObjectLighting(
                        objectLightProbes,
                        transform.position,
                        object.currentSectorId,
                        mapForFallback);
            });
}


} // namespace game
