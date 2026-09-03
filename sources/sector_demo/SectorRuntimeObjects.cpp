#include "sector_demo/SectorRuntimeObjects.h"

#include "game/npc/NpcRuntime.h"
#include "game/npc/NpcPatrolSystem.h"
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
#include <limits>
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

Matrix BuildSectorWindowModelMatrix(
        const SectorObjectTransform& transform,
        const SectorWindow& window)
{
    // Portal normals point from the front sector toward the back sector. With
    // tangent and world-up, that direction forms a mirrored basis. A centered
    // cube occupies the same slab volume when its local depth axis is reversed,
    // while the resulting right-handed transform preserves face winding and
    // outward normals for backface culling and Fresnel shading.
    return Matrix{
            window.tangent.x * window.width, 0.0f,
                    -window.normal.x * window.thickness, transform.position.x,
            0.0f, window.height, 0.0f, transform.position.y,
            window.tangent.y * window.width, 0.0f,
                    -window.normal.y * window.thickness, transform.position.z,
            0.0f, 0.0f, 0.0f, 1.0f};
}

SectorDuctCoverRenderBasis BuildSectorDuctCoverRenderBasis(
        const SectorDuctAccess& access)
{
    Vector3 outward{
            -access.outsideToCrawlspaceNormal.x,
            0.0f,
            -access.outsideToCrawlspaceNormal.y};
    if (Vector3LengthSqr(outward) <= 0.000001f) {
        outward = Vector3{0.0f, 0.0f, 1.0f};
    } else {
        outward = Vector3Normalize(outward);
    }
    // Cross(horizontal, up) == outward, so both portal orientations remain
    // right-handed and the grille front always faces the outside sector.
    const Vector3 horizontal{outward.z, 0.0f, -outward.x};
    return SectorDuctCoverRenderBasis{
            horizontal, Vector3{0.0f, 1.0f, 0.0f}, outward};
}

Matrix BuildSectorDuctCoverModelMatrix(
        const SectorDuctAccess& access,
        Vector3 position)
{
    const SectorDuctCoverRenderBasis basis =
            BuildSectorDuctCoverRenderBasis(access);
    return Matrix{
            basis.horizontal.x, basis.up.x, basis.outward.x, position.x,
            basis.horizontal.y, basis.up.y, basis.outward.y, position.y,
            basis.horizontal.z, basis.up.z, basis.outward.z, position.z,
            0.0f, 0.0f, 0.0f, 1.0f};
}

bool IsSectorDuctCoverBlocking(const SectorDuctAccess& access)
{
    return access.cover.enabled
            && access.coverPhase == SectorDuctCoverPhase::Attached;
}

bool IsSectorDuctCoverClear(const SectorDuctAccess& access)
{
    return !access.cover.enabled
            || access.coverPhase == SectorDuctCoverPhase::Falling
            || access.coverPhase == SectorDuctCoverPhase::Settled;
}

bool BeginSectorDuctCoverRemoval(
        SectorDuctAccess& access,
        Vector3 actorPosition)
{
    if (!IsSectorDuctCoverBlocking(access)) return false;
    const Vector2 actorOffset{
            actorPosition.x - access.centerXZ.x,
            actorPosition.z - access.centerXZ.y};
    const float side = actorOffset.x * access.outsideToCrawlspaceNormal.x
            + actorOffset.y * access.outsideToCrawlspaceNormal.y;
    access.removalSide = side <= 0.0f
            ? SectorDuctCoverRemovalSide::Outside
            : SectorDuctCoverRemovalSide::Crawlspace;
    access.coverPhase = SectorDuctCoverPhase::Removing;
    access.coverMotionElapsedSeconds = 0.0f;
    access.coverOffset = {};
    return true;
}


void ReserveSectorRuntimeObjectWorld(engine::World& world, size_t objectCapacity)
{
    world.ReserveEntities(objectCapacity);
    world.ReserveComponentTypes(35);
    world.ReserveComponent<SectorObjectTransform>(objectCapacity);
    world.ReserveComponent<SectorObject>(objectCapacity);
    world.ReserveComponent<SectorObjectLighting>(objectCapacity);
    world.ReserveComponent<SectorObjectVisualOffset>(objectCapacity);
    world.ReserveComponent<SectorStaticModel>(objectCapacity);
    world.ReserveComponent<SectorDynamicModel>(objectCapacity);
    world.ReserveComponent<SectorItem>(objectCapacity);
    world.ReserveComponent<SectorWindow>(objectCapacity);
    world.ReserveComponent<SectorDuctAccess>(objectCapacity);
    world.ReserveComponent<NpcRuntimeInstance>(objectCapacity);
    world.ReserveComponent<NpcPatrolState>(objectCapacity);
    world.ReserveComponent<NpcAiState>(objectCapacity);
    world.ReserveComponent<NpcAnimationState>(objectCapacity);
    world.ReserveComponent<NpcHeadLookState>(objectCapacity);
    world.ReserveComponent<Health>(objectCapacity);
    world.ReserveComponent<NpcCombatState>(objectCapacity);
    world.ReserveComponent<NpcBodyPartDamageState>(objectCapacity);
    world.ReserveComponent<NpcBoneImpactState>(objectCapacity);
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
    world.ReserveComponent<SectorDoorOpenControl>(objectCapacity);
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

void RefreshPhysicalModelColliders(SectorRuntimeObjectState& state)
{
    state.physicalModelColliders.clear();
    state.physicalModelColliders.insert(
            state.physicalModelColliders.end(),
            state.staticModelColliders.begin(),
            state.staticModelColliders.end());
    state.physicalModelColliders.insert(
            state.physicalModelColliders.end(),
            state.windowColliders.begin(),
            state.windowColliders.end());
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
    size_t itemRequestedCount = 0;
    size_t itemReadyCount = 0;
    size_t itemPendingCount = 0;
    size_t itemFailedCount = 0;

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

    world.ForEach<SectorObject, SectorItem>(
            [&assets,
             &itemRequestedCount,
             &itemReadyCount,
             &itemPendingCount,
             &itemFailedCount](
                    engine::Entity, SectorObject&, SectorItem& item) {
                if (engine::IsNull(item.model)) {
                    ++itemFailedCount;
                } else {
                    ++itemRequestedCount;
                    if (assets.IsReady(item.model)) ++itemReadyCount;
                    else if (assets.HasFailed(item.model)) ++itemFailedCount;
                    else ++itemPendingCount;
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
    state.itemModelRequestedCount = itemRequestedCount;
    state.itemModelReadyCount = itemReadyCount;
    state.itemModelPendingCount = itemPendingCount;
    state.itemModelFailedCount = itemFailedCount;
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
    if (itemRequestedCount + itemFailedCount > 0) {
        state.placedObjectStatus += TextFormat(
                " | items %zu ready, %zu pending, %zu failed",
                itemReadyCount,
                itemPendingCount,
                itemFailedCount);
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
    } else if (itemFailedCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu item model asset(s) failed",
                itemFailedCount);
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

                const size_t clipCount =
                        engine::ModelAnimationClipCount(*asset);
                if (clipCount == 0) {
                    dynamicModel.animationResolved = true;
                    return;
                }

                int clipIndex = engine::FindModelAnimationClipIndex(
                        *asset,
                        dynamicModel.requestedAnimation.c_str());
                if (clipIndex < 0) {
                    clipIndex = 0;
                    dynamicModel.animationFallback = !dynamicModel.requestedAnimation.empty();
                    if (dynamicModel.animationFallback) {
                        const char* fallbackName =
                                engine::ModelAnimationClipName(*asset, 0);
                        std::fprintf(
                                stderr,
                                "[SectorRuntimeObjects WARNING] Animation '%s' was not found for dynamic model object %d; using '%s'.\n",
                                dynamicModel.requestedAnimation.c_str(),
                                dynamicModel.placedObjectId,
                                fallbackName != nullptr
                                        ? fallbackName
                                        : "");
                    }
                }

                engine::SetAnimatedModelClip(
                        animator,
                        *asset,
                        static_cast<uint32_t>(clipIndex));
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

bool SpawnItemEntity(
        engine::World& world,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& placedObject,
        const ItemRegistry& itemRegistry,
        const ItemModelAssetState& itemAssets,
        engine::Entity& outEntity)
{
    outEntity = engine::NullEntity();
    if (placedObject.kind != "item" || placedObject.id <= 0) return false;
    const ItemDefinition* definition = FindItemDefinition(
            itemRegistry, placedObject.item.definitionId);
    if (definition == nullptr) return false;
    engine::ModelHandle model = engine::NullModelHandle();
    if (const ItemModelAssetEntry* entry = FindItemModelAsset(
                itemAssets, definition->id)) {
        model = entry->model;
    }
    Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(
            placedObject.position);
    worldPosition.y += placedObject.item.heightOffsetWorld;
    SectorObject object;
    if (state.objectSectorLookupWorldValid) {
        const int foundSectorId =
                state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                        Vector2{worldPosition.x, worldPosition.z}, -1);
        object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
    }
    const Vector3 sectorAmbient = StaticModelSectorAmbient(
            map, object.currentSectorId);
    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, SectorObjectTransform{
            worldPosition,
            placedObject.yawRadians,
            placedObject.item.rotationXRadians,
            placedObject.item.rotationZRadians});
    world.Add(entity, SectorObjectVisualOffset{});
    world.Add(entity, object);
    world.Add(entity, SampleSectorObjectLighting(
            state.objectLightProbes,
            worldPosition,
            object.currentSectorId,
            &map));
    SectorItem runtimeItem;
    runtimeItem.model = model;
    runtimeItem.placedObjectId = placedObject.id;
    runtimeItem.definitionId = definition->id;
    runtimeItem.title = definition->title;
    runtimeItem.instanceId = placedObject.item.instanceId;
    runtimeItem.quantity = static_cast<std::uint64_t>(
            placedObject.item.quantity);
    runtimeItem.takeDistance = placedObject.item.takeDistance;
    runtimeItem.onTakeScript = placedObject.item.onTakeScript;
    runtimeItem.onUseScript = definition->type == ItemType::Object
            ? placedObject.item.onUseScript : std::string{};
    runtimeItem.containingSectorAmbient = sectorAmbient;
    runtimeItem.scale = placedObject.item.scale;
    runtimeItem.environmentExposure = StaticModelEnvironmentExposure(
            map, object.currentSectorId, sectorAmbient);
    runtimeItem.shadowMode = placedObject.item.shadowMode;
    runtimeItem.origin = placedObject.item.sessionDrop
            ? SectorItemOrigin::SessionDrop
            : SectorItemOrigin::Authored;
    if (definition->type != ItemType::Object
            && !placedObject.item.onUseScript.empty()) {
        std::fprintf(
                stderr,
                "[SectorRuntimeObjects WARNING] item '%s' ignores onUseScript because its definition is not Object\n",
                runtimeItem.instanceId.c_str());
    }
    world.Add(entity, std::move(runtimeItem));
    outEntity = entity;
    return true;
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
    const uint64_t nextStaticLightingRevision =
            state.staticLightingRevision == std::numeric_limits<uint64_t>::max()
            ? 1
            : state.staticLightingRevision + 1;
    state = SectorRuntimeObjectState{};
    state.worldReserved = keepReservation;
    state.swingDoorCatalogRevision = keepSwingDoorCatalogRevision;
    state.npcDefinitionCatalogRevision = keepNpcDefinitionCatalogRevision;
    state.staticLightingRevision = nextStaticLightingRevision;
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
    state.staticLightingRevision =
            state.staticLightingRevision == std::numeric_limits<uint64_t>::max()
            ? 1
            : state.staticLightingRevision + 1;
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
        const SectorTopologyMap& map,
        const ItemRegistry* itemRegistry,
        const ItemModelAssetState* itemAssets)
{
    ClearSectorRuntimeObjects(world, assets, state);
    RefreshSectorRuntimeObjectMapData(state, map);
    SpawnPlacedRuntimeObjects(
            world, assets, state, map, itemRegistry, itemAssets);
}

bool SpawnSectorItemRuntimeObject(
        engine::World& world,
        engine::AssetManager&,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& placedObject,
        const ItemRegistry& itemRegistry,
        const ItemModelAssetState& itemAssets,
        engine::Entity* outEntity)
{
    EnsureSectorRuntimeObjectWorldReserved(world, state);
    engine::Entity entity = engine::NullEntity();
    if (!SpawnItemEntity(
                world,
                state,
                map,
                placedObject,
                itemRegistry,
                itemAssets,
                entity)) {
        return false;
    }
    if (state.placedObjectEntities.size()
            == state.placedObjectEntities.capacity()) {
        std::fprintf(
                stderr,
                "[SectorRuntimeObjects WARNING] placed object tracking capacity exceeded during item drop\n");
    }
    state.placedObjectEntities.push_back(
            SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
    ++state.placedObjectCount;
    ++state.spawnedObjectCount;
    if (outEntity != nullptr) *outEntity = entity;
    return true;
}

void SpawnPlacedRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const ItemRegistry* itemRegistry,
        const ItemModelAssetState* itemAssets)
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
    state.doorObstacles.clear();
    state.doorObstacles.reserve(map.runtimeObjects.size() + 1u);
    state.dynamicPortalBlockers.clear();
    state.dynamicPortalBlockers.reserve(map.runtimeObjects.size() * 2);
    state.doorCollisionCacheInitialized = false;
    state.staticModelColliders.clear();
    state.staticModelColliders.reserve(map.runtimeObjects.size());
    state.dynamicModelColliders.clear();
    state.dynamicModelColliders.reserve(map.runtimeObjects.size());
    state.windowColliders.clear();
    state.windowColliders.reserve(map.runtimeObjects.size());
    state.ductAccessGateColliders.clear();
    state.ductAccessGateColliders.reserve(map.runtimeObjects.size());
    state.physicalModelColliders.clear();
    state.physicalModelColliders.reserve(map.runtimeObjects.size() * 2u);
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
                    placedObject.door.materialId,
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
            SectorDoor runtimeDoor{placedObject.id, true};
            runtimeDoor.instanceId = placedObject.door.instanceId;
            world.Add(entity, std::move(runtimeDoor));
            world.Add(entity, runtimeAnchor);
            world.Add(entity, runtimeMotion);
            world.Add(entity, SectorDoorOpenControl{});
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
                    placedObject.door.autoOpenDistance,
                    placedObject.door.useTitle,
                    placedObject.door.canOpenScript,
                    placedObject.door.canCloseScript});
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

        if (placedObject.kind == "window") {
            const SectorResolvedWindowAnchor resolved =
                    ResolveSectorWindowAnchor(map, placedObject.window);
            if (!resolved.valid) {
                const std::string warning = TextFormat(
                        "window object %d anchor on linedef %d is invalid: %s",
                        placedObject.id,
                        placedObject.window.anchor.lineDefId,
                        resolved.diagnostic.empty()
                                ? "unknown anchor error"
                                : resolved.diagnostic.c_str());
                std::fprintf(stderr,
                        "[SectorRuntimeObjects WARNING] %s\n",
                        warning.c_str());
                recordWarning(warning);
                ++skippedCount;
                continue;
            }
            const Vector2 centerXZ{
                    resolved.midpoint.x
                            + resolved.tangent.x
                                    * placedObject.window.horizontalOffsetWorld
                            + resolved.normal.x
                                    * placedObject.window.normalOffset,
                    resolved.midpoint.y
                            + resolved.tangent.y
                                    * placedObject.window.horizontalOffsetWorld
                            + resolved.normal.y
                                    * placedObject.window.normalOffset};
            const Vector3 worldPosition{
                    centerXZ.x,
                    resolved.openBottom + resolved.portalHeight * 0.5f
                            + placedObject.window.verticalOffsetWorld,
                    centerXZ.y};
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int found = state.objectSectorLookupWorld
                        .FindSectorContainingPointPreferCurrent(
                                centerXZ, resolved.frontSectorId);
                object.currentSectorId = found != 0
                        ? found : resolved.frontSectorId;
            } else {
                object.currentSectorId = resolved.frontSectorId;
            }
            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{
                    worldPosition, SectorDoorAnchorYawRadians(resolved)});
            world.Add(entity, object);
            world.Add(entity, SampleSectorObjectLighting(
                    state.objectLightProbes,
                    worldPosition,
                    object.currentSectorId,
                    &map));
            world.Add(entity, SectorWindow{
                    placedObject.id,
                    resolved.lineDefId,
                    resolved.frontSectorId,
                    resolved.backSectorId,
                    resolved.tangent,
                    resolved.normal,
                    resolved.width,
                    resolved.height,
                    placedObject.window.thickness,
                    placedObject.window.tint,
                    placedObject.window.opacity,
                    placedObject.window.roughness,
                    placedObject.window.surfaceHaze,
                    placedObject.window.imperfectionStrength,
                    placedObject.window.indexOfRefraction,
                    true});
            if (placedObject.window.collision) {
                SectorStaticModelCollider collider;
                collider.placedObjectId = placedObject.id;
                collider.center = centerXZ;
                collider.axisX = resolved.tangent;
                collider.axisZ = resolved.normal;
                collider.halfExtents = Vector2{
                        resolved.width * 0.5f,
                        placedObject.window.thickness * 0.5f};
                collider.bottom = worldPosition.y - resolved.height * 0.5f;
                collider.top = worldPosition.y + resolved.height * 0.5f;
                collider.resolvedPosition = worldPosition;
                collider.resolvedYawRadians =
                        SectorDoorAnchorYawRadians(resolved);
                collider.resolvedScale = 1.0f;
                collider.resolved = true;
                collider.entity = entity;
                world.Add(entity, collider);
            }
            state.placedObjectEntities.push_back(
                    SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
            ++spawnedCount;
            continue;
        }

        if (placedObject.kind == "duct_access") {
            const SectorResolvedDuctAccessAnchor resolved =
                    ResolveSectorDuctAccessAnchor(map, placedObject.ductAccess);
            if (!resolved.valid) {
                const std::string warning = TextFormat(
                        "Duct Access %d on linedef %d is invalid: %s",
                        placedObject.id,
                        placedObject.ductAccess.anchor.lineDefId,
                        resolved.diagnostic.c_str());
                std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n",
                        warning.c_str());
                recordWarning(warning);
                ++skippedCount;
                continue;
            }
            const Vector2 centerXZ{
                    resolved.midpoint.x
                            + resolved.tangent.x
                                    * placedObject.ductAccess.horizontalOffsetWorld
                            + resolved.outsideToCrawlspaceNormal.x
                                    * placedObject.ductAccess.normalOffset,
                    resolved.midpoint.y
                            + resolved.tangent.y
                                    * placedObject.ductAccess.horizontalOffsetWorld
                            + resolved.outsideToCrawlspaceNormal.y
                                    * placedObject.ductAccess.normalOffset};
            const float centerY = resolved.openBottom
                    + resolved.height * 0.5f
                    + placedObject.ductAccess.verticalOffsetWorld;
            SectorObject object;
            object.currentSectorId = resolved.outsideSectorId;
            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{
                    Vector3{centerXZ.x, centerY, centerXZ.y},
                    SectorDoorAnchorYawRadians(resolved)});
            world.Add(entity, object);
            world.Add(entity, SampleSectorObjectLighting(
                    state.objectLightProbes,
                    Vector3{centerXZ.x, centerY, centerXZ.y},
                    resolved.outsideSectorId, &map));
            SectorDuctAccess access;
            access.placedObjectId = placedObject.id;
            access.lineDefId = resolved.lineDefId;
            access.outsideSectorId = resolved.outsideSectorId;
            access.crawlspaceSectorId = resolved.crawlspaceSectorId;
            access.centerXZ = centerXZ;
            access.tangent = resolved.tangent;
            access.outsideToCrawlspaceNormal =
                    resolved.outsideToCrawlspaceNormal;
            access.openingBottom = resolved.openBottom
                    + placedObject.ductAccess.verticalOffsetWorld;
            access.openingTop = access.openingBottom + resolved.height;
            access.width = resolved.width;
            access.height = resolved.height;
            access.thickness = placedObject.ductAccess.thickness;
            access.cover = placedObject.ductAccess.cover;
            world.Add(entity, std::move(access));
            SectorStaticModelCollider gate;
            gate.placedObjectId = placedObject.id;
            gate.center = centerXZ;
            gate.axisX = resolved.tangent;
            gate.axisZ = resolved.normal;
            gate.halfExtents = Vector2{
                    resolved.portalWidth * 0.5f,
                    std::max(0.01f, placedObject.ductAccess.thickness * 0.5f)};
            gate.bottom = resolved.openBottom;
            gate.top = resolved.openTop;
            gate.resolvedPosition = Vector3{centerXZ.x, centerY, centerXZ.y};
            gate.resolvedYawRadians = SectorDoorAnchorYawRadians(resolved);
            gate.resolvedScale = 1.0f;
            gate.resolved = true;
            gate.entity = entity;
            state.ductAccessGateColliders.push_back(gate);
            state.placedObjectEntities.push_back(
                    SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
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
                            sectorAmbient),
                    placedObject.staticModel.castsShadow,
                    placedObject.staticModel.instanceId,
                    1.0f});
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
                    definition->canOpenDoors,
                    GetNpcAction(*definition, NpcAction::Walk).movementSpeed,
                    GetNpcAction(*definition, NpcAction::Run).movementSpeed,
                    false});
            if (placedObject.npc.patrolEditorId > 0
                    && FindSectorCompiledPatrol(
                            map, placedObject.npc.patrolEditorId) != nullptr) {
                const SectorCompiledPatrol& compiledPatrol =
                        *FindSectorCompiledPatrol(
                                map, placedObject.npc.patrolEditorId);
                NpcPatrolState patrol;
                patrol.patrolEditorId = placedObject.npc.patrolEditorId;
                patrol.scriptMoveStopsPatrol =
                        placedObject.npc.scriptMoveStopsPatrol;
                const uint32_t randomSeed = static_cast<uint32_t>(
                        GetRandomValue(1, std::numeric_limits<int>::max()))
                        ^ static_cast<uint32_t>(placedObject.id * 0x9e3779b9u);
                InitializeNpcPatrolTraversal(
                        patrol, compiledPatrol,
                        placedObject.npc.randomPatrolStart,
                        placedObject.npc.reversePatrol,
                        randomSeed);
                world.Add(entity, std::move(patrol));
            }
            if (!definition->aiType.empty()) {
                NpcAiState ai;
                ai.aiType = definition->aiType;
                ai.perception = definition->perception;
                ai.attack = GetNpcAction(*definition, NpcAction::Attack);
                if (!ai.attack.attackSoundPath.empty()) {
                    const std::string soundPath = ResolveSectorAudioAssetPath(
                            ai.attack.attackSoundPath);
                    ai.attackSound = assets.RequestSound(
                            state.runtimeObjectAssetScope,
                            soundPath.c_str());
                }
                if (!ai.attack.soundPath.empty()) {
                    const std::string soundPath = ResolveSectorAudioAssetPath(
                            ai.attack.soundPath);
                    ai.attackImpactSound = assets.RequestSound(
                            state.runtimeObjectAssetScope,
                            soundPath.c_str());
                }
                world.Add(entity, std::move(ai));
            }
            world.Add(entity, MakeHealth(definition->baseHealth));
            NpcCombatState npcCombat;
            npcCombat.despawnOnDeath = definition->despawnOnDeath;
            npcCombat.corpseDespawnDelaySeconds =
                    definition->corpseDespawnDelaySeconds;
            npcCombat.corpseFadeDurationSeconds =
                    definition->corpseFadeDurationSeconds;
            world.Add(entity, npcCombat);
            if (!definition->bodyPartDamage.empty()) {
                NpcBodyPartDamageState bodyPartDamage;
                bodyPartDamage.rows.reserve(definition->bodyPartDamage.size());
                for (const NpcBodyPartDamageDefinition& authoredRow
                        : definition->bodyPartDamage) {
                    NpcBodyPartDamageRuntimeRow runtimeRow;
                    runtimeRow.boneName = authoredRow.boneName;
                    runtimeRow.damageMultiplier =
                            authoredRow.damageMultiplier;
                    bodyPartDamage.rows.push_back(std::move(runtimeRow));
                }
                world.Add(entity, std::move(bodyPartDamage));
            }
            if (definition->boneImpact.enabled) {
                NpcBoneImpactState boneImpact;
                boneImpact.impulseDegreesPerSecond =
                        definition->boneImpact.impulseDegreesPerSecond;
                boneImpact.springFrequencyHz =
                        definition->boneImpact.springFrequencyHz;
                boneImpact.springDampingRatio =
                        definition->boneImpact.springDampingRatio;
                boneImpact.maxAngleDegrees =
                        definition->boneImpact.maxAngleDegrees;
                world.Add(entity, std::move(boneImpact));
            }
            NpcAnimationState npcAnimation;
            npcAnimation.blendSeconds = definition->animationBlendSeconds;
            for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
                npcAnimation.animationSpeeds[static_cast<size_t>(metadata.action)] =
                        GetNpcAction(*definition, metadata.action).animationSpeed;
            }
            world.Add(entity, npcAnimation);
            if (!definition->hostile && definition->headLook.enabled) {
                NpcHeadLookState headLook;
                headLook.boneName = definition->headLook.boneName;
                headLook.rangeWorld = definition->headLook.rangeWorld;
                headLook.maxYawDegrees =
                        definition->headLook.maxYawDegrees;
                headLook.maxPitchDegrees =
                        definition->headLook.maxPitchDegrees;
                world.Add(entity, std::move(headLook));
            }
            world.Add(entity, SectorObjectVisualOffset{});
            world.Add(entity, SectorDynamicModel{
                    placedObject.id,
                    {},
                    sectorAmbient,
                    placedObject.npc.scale,
                    StaticModelEnvironmentExposure(
                            map,
                            object.currentSectorId,
                            sectorAmbient),
                    idle.animation,
                    false,
                    false,
                    1.0f,
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

        if (placedObject.kind == "item") {
            engine::Entity entity = engine::NullEntity();
            if (itemRegistry == nullptr || itemAssets == nullptr
                    || !SpawnItemEntity(
                            world,
                            state,
                            map,
                            placedObject,
                            *itemRegistry,
                            *itemAssets,
                            entity)) {
                const std::string warning = TextFormat(
                        "item object %d references missing definition '%s'",
                        placedObject.id,
                        placedObject.item.definitionId.c_str());
                std::fprintf(stderr, "[SectorRuntimeObjects WARNING] %s\n", warning.c_str());
                recordWarning(warning);
                ++skippedCount;
                continue;
            }
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
                    placedObject.dynamicModel.instanceId,
                    sectorAmbient,
                    placedObject.dynamicModel.scale,
                    StaticModelEnvironmentExposure(map, object.currentSectorId, sectorAmbient),
                    placedObject.dynamicModel.animation,
                    false,
                    false,
                    1.0f,
                    placedObject.dynamicModel.shadowMode,
                    placedObject.dynamicModel.useTitle,
                    placedObject.dynamicModel.useDistance,
                    placedObject.dynamicModel.onUseScript,
                    placedObject.dynamicModel.singleUse,
                    false});
            engine::AnimatedModelInstance animatedModel{model};
            animatedModel.poseSource =
                    engine::AnimatedModelPoseSource::GltfScene;
            world.Add(entity, std::move(animatedModel));
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
    CollectSectorDynamicModelColliders(world, state.dynamicModelColliders);
    CollectSectorWindowColliders(world, state.windowColliders);
    RefreshPhysicalModelColliders(state);
    RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
}

void CollectSectorWindowColliders(
        engine::World& world,
        std::vector<SectorStaticModelCollider>& colliders)
{
    colliders.clear();
    world.ForEach<SectorWindow, SectorStaticModelCollider>(
            [&colliders](engine::Entity entity,
                    SectorWindow&,
                    SectorStaticModelCollider& collider) {
                if (!collider.resolved || collider.failed) return;
                SectorStaticModelCollider copy = collider;
                copy.entity = entity;
                colliders.push_back(copy);
            });
}

void RefreshSectorDoorSpatialCaches(
        engine::World& world,
        SectorRuntimeObjectState& state)
{
    UpdateSectorDoorDerivedStateSystem(world);
    state.dynamicDoorColliders.clear();
    CollectSectorDoorDynamicColliders(world, state.dynamicDoorColliders);
    state.dynamicPortalBlockers.clear();
    CollectSectorDoorDynamicPortalBlockers(world, state.dynamicPortalBlockers);
    state.doorSpatialStateChanged = true;
    state.doorCollisionCacheInitialized = true;
}

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition,
        const SectorDoorPlayerObstacle* playerObstacle,
        const std::vector<SectorDoorPlayerObstacle>* doorObstacles)
{
    world.ForEach<
            SectorDuctAccess,
            SectorObject,
            SectorObjectTransform,
            SectorObjectLighting>(
            [dt, &map, &state](engine::Entity,
                    SectorDuctAccess& access,
                    SectorObject& object,
                    SectorObjectTransform& transform,
                    SectorObjectLighting& lighting) {
                const bool coverMoved =
                        access.coverPhase == SectorDuctCoverPhase::Removing
                        || access.coverPhase == SectorDuctCoverPhase::Falling;
                if (access.coverPhase == SectorDuctCoverPhase::Removing) {
                    object.currentSectorId = access.removalSide
                                    == SectorDuctCoverRemovalSide::Outside
                            ? access.outsideSectorId
                            : access.crawlspaceSectorId;
                    const float pop = access.cover.thickness + 0.05f;
                    const float slide = access.width + 0.05f;
                    const float duration = std::max(pop, slide)
                            / std::max(0.05f, access.cover.removalSpeedWorld);
                    access.coverMotionElapsedSeconds += std::max(0.0f, dt);
                    const float raw = std::clamp(
                            access.coverMotionElapsedSeconds
                                    / std::max(0.01f, duration),
                            0.0f, 1.0f);
                    const float eased = raw * raw * (3.0f - 2.0f * raw);
                    const float towardActor = access.removalSide
                                    == SectorDuctCoverRemovalSide::Outside
                            ? -1.0f : 1.0f;
                    const float along = access.cover.slideSide
                                    == SectorDuctCoverSlideSide::PortalStart
                            ? -1.0f : 1.0f;
                    access.coverOffset = Vector3{
                            access.outsideToCrawlspaceNormal.x
                                            * towardActor * pop * eased
                                    + access.tangent.x * along * slide * eased,
                            0.0f,
                            access.outsideToCrawlspaceNormal.y
                                            * towardActor * pop * eased
                                    + access.tangent.y * along * slide * eased};
                    if (raw >= 1.0f) {
                        access.coverPhase = SectorDuctCoverPhase::Falling;
                        access.coverMotionElapsedSeconds = 0.0f;
                    }
                } else if (access.coverPhase == SectorDuctCoverPhase::Falling) {
                    object.currentSectorId = access.removalSide
                                    == SectorDuctCoverRemovalSide::Outside
                            ? access.outsideSectorId
                            : access.crawlspaceSectorId;
                    access.coverMotionElapsedSeconds += std::max(0.0f, dt);
                    const int floorSectorId = access.removalSide
                                    == SectorDuctCoverRemovalSide::Outside
                            ? access.outsideSectorId : access.crawlspaceSectorId;
                    const SectorTopologySector* sector =
                            FindSectorTopologySector(map, floorSectorId);
                    const float floorY = sector != nullptr
                            ? SectorAuthoringToWorldDistance(sector->floorZ)
                            : access.openingBottom;
                    const float startBottom = access.openingBottom;
                    const float drop = 0.5f * 25.0f
                            * access.coverMotionElapsedSeconds
                            * access.coverMotionElapsedSeconds;
                    access.coverOffset.y = std::max(
                            floorY - startBottom, -drop);
                    if (startBottom + access.coverOffset.y <= floorY + 0.0001f) {
                        access.coverOffset.y = floorY - startBottom;
                        access.coverPhase = SectorDuctCoverPhase::Settled;
                    }
                }
                if (coverMoved) {
                    lighting = SampleSectorObjectLighting(
                            state.objectLightProbes,
                            Vector3Add(transform.position, access.coverOffset),
                            object.currentSectorId,
                            &map);
                }
            });
    AdvanceSectorBillboardAnimatorSystem(world, dt);
    ResolveDynamicModelAnimations(world, assets);
    if (playerPosition != nullptr) {
        UpdateSectorDoorAutoOpenSystem(world, *playerPosition);
    }
    state.doorSpatialStateChanged = doorObstacles != nullptr
            ? AdvanceSectorDoorMotionSystem(
                    world, dt, doorObstacles->data(), doorObstacles->size())
            : AdvanceSectorDoorMotionSystem(world, dt, playerObstacle);
    if (state.doorSpatialStateChanged) {
        UpdateSectorDoorDerivedStateSystem(world);
    }
    if (UpdateSectorStaticModelColliderSystem(world, assets)) {
        CollectSectorStaticModelColliders(world, state.staticModelColliders);
        CollectSectorDynamicModelColliders(world, state.dynamicModelColliders);
        RefreshPhysicalModelColliders(state);
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
            || state.staticModelPendingCount > 0
            || state.itemModelPendingCount > 0) {
        RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
    }
}

bool QueueRemoveSectorRuntimeObjectByEntity(
        engine::World& world,
        SectorRuntimeObjectState& state,
        engine::Entity entity)
{
    const auto found = std::find_if(
            state.placedObjectEntities.begin(),
            state.placedObjectEntities.end(),
            [entity](const SectorPlacedRuntimeObjectEntity& entry) {
                return entry.entity == entity;
            });
    if (found == state.placedObjectEntities.end()
            || !world.IsAlive(entity)) {
        return false;
    }
    world.DestroyLater(entity);
    state.placedObjectEntities.erase(found);
    if (state.placedObjectCount > 0) --state.placedObjectCount;
    if (state.spawnedObjectCount > 0) --state.spawnedObjectCount;
    return true;
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

bool SynchronizeSectorPlacedRuntimeObjectTransform(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& placedObject)
{
    if (placedObject.kind != "static_model"
            && placedObject.kind != "dynamic_model"
            && placedObject.kind != "item") {
        return false;
    }
    const auto entry = std::find_if(
            state.placedObjectEntities.begin(),
            state.placedObjectEntities.end(),
            [&placedObject](const SectorPlacedRuntimeObjectEntity& value) {
                return value.placedObjectId == placedObject.id;
            });
    if (entry == state.placedObjectEntities.end()
            || !world.IsAlive(entry->entity)
            || !world.Has<SectorObjectTransform>(entry->entity)
            || !world.Has<SectorObject>(entry->entity)) {
        return false;
    }

    float heightOffsetWorld = 0.0f;
    float rotationXRadians = 0.0f;
    float rotationZRadians = 0.0f;
    if (placedObject.kind == "static_model") {
        heightOffsetWorld = placedObject.staticModel.heightOffsetWorld;
        rotationXRadians = placedObject.staticModel.rotationXRadians;
        rotationZRadians = placedObject.staticModel.rotationZRadians;
    } else if (placedObject.kind == "dynamic_model") {
        heightOffsetWorld = placedObject.dynamicModel.heightOffsetWorld;
        rotationXRadians = placedObject.dynamicModel.rotationXRadians;
        rotationZRadians = placedObject.dynamicModel.rotationZRadians;
    } else {
        heightOffsetWorld = placedObject.item.heightOffsetWorld;
        rotationXRadians = placedObject.item.rotationXRadians;
        rotationZRadians = placedObject.item.rotationZRadians;
    }

    Vector3 worldPosition = PlacedRuntimeObjectAuthoringToWorldPosition(
            placedObject.position);
    worldPosition.y += heightOffsetWorld;
    SectorObjectTransform& transform =
            world.Get<SectorObjectTransform>(entry->entity);
    const bool transformChanged = transform.position.x != worldPosition.x
            || transform.position.y != worldPosition.y
            || transform.position.z != worldPosition.z
            || transform.yawRadians != placedObject.yawRadians
            || transform.rotationXRadians != rotationXRadians
            || transform.rotationZRadians != rotationZRadians;
    transform = SectorObjectTransform{
            worldPosition,
            placedObject.yawRadians,
            rotationXRadians,
            rotationZRadians};

    SectorObject& object = world.Get<SectorObject>(entry->entity);
    if (state.objectSectorLookupWorldValid) {
        const int foundSectorId =
                state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                        Vector2{worldPosition.x, worldPosition.z},
                        object.currentSectorId);
        object.currentSectorId = foundSectorId != 0 ? foundSectorId : -1;
    }
    if (world.Has<SectorObjectLighting>(entry->entity)) {
        world.Get<SectorObjectLighting>(entry->entity) = SampleSectorObjectLighting(
                state.objectLightProbes,
                worldPosition,
                object.currentSectorId,
                &map);
    }

    const Vector3 ambient = StaticModelSectorAmbient(map, object.currentSectorId);
    const float exposure = StaticModelEnvironmentExposure(
            map, object.currentSectorId, ambient);
    if (world.Has<SectorStaticModel>(entry->entity)) {
        SectorStaticModel& model = world.Get<SectorStaticModel>(entry->entity);
        model.containingSectorAmbient = ambient;
        model.environmentExposure = exposure;
    }
    if (world.Has<SectorDynamicModel>(entry->entity)) {
        SectorDynamicModel& model = world.Get<SectorDynamicModel>(entry->entity);
        model.containingSectorAmbient = ambient;
        model.environmentExposure = exposure;
    }
    if (world.Has<SectorItem>(entry->entity)) {
        SectorItem& item = world.Get<SectorItem>(entry->entity);
        item.containingSectorAmbient = ambient;
        item.environmentExposure = exposure;
    }

    if (transformChanged
            && world.Has<SectorStaticModelCollider>(entry->entity)) {
        SectorStaticModelCollider& collider =
                world.Get<SectorStaticModelCollider>(entry->entity);
        collider.resolved = false;
        collider.failed = false;
        UpdateSectorStaticModelColliderSystem(world, assets);
        CollectSectorStaticModelColliders(world, state.staticModelColliders);
        CollectSectorDynamicModelColliders(world, state.dynamicModelColliders);
    }
    return true;
}


} // namespace game
