#include "sector_demo/SectorRuntimeObjects.h"

#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <raymath.h>

namespace game {

void ReserveSectorRuntimeObjectWorld(engine::World& world, size_t objectCapacity)
{
    world.ReserveEntities(objectCapacity);
    world.ReserveComponentTypes(14);
    world.ReserveComponent<SectorObjectTransform>(objectCapacity);
    world.ReserveComponent<SectorObject>(objectCapacity);
    world.ReserveComponent<SectorObjectLighting>(objectCapacity);
    world.ReserveComponent<SectorBillboardSprite>(objectCapacity);
    world.ReserveComponent<SectorBillboardAnimator>(objectCapacity);
    world.ReserveComponent<SectorBillboardDirectionalClips>(objectCapacity);
    world.ReserveComponent<SectorBillboardSingleClip>(objectCapacity);
    world.ReserveComponent<SectorDoor>(objectCapacity);
    world.ReserveComponent<SectorDoorResolvedAnchor>(objectCapacity);
    world.ReserveComponent<SectorDoorMotion>(objectCapacity);
    world.ReserveComponent<SectorDoorInteraction>(objectCapacity);
    world.ReserveComponent<SectorDoorRender>(objectCapacity);
    world.ReserveComponent<SectorDoorCollider>(objectCapacity);
    world.ReserveComponent<SectorDoorPortalBlocker>(objectCapacity);
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

SectorDoorSlabGeometry BuildSectorDoorSlabGeometry(
        const SectorObjectTransform& transform,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render)
{
    Vector3 tangent = Vector3{anchor.tangent.x, 0.0f, anchor.tangent.y};
    if (Vector3LengthSqr(tangent) <= 0.000001f) {
        tangent = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        tangent = Vector3Normalize(tangent);
    }

    Vector3 normal = Vector3{anchor.normal.x, 0.0f, anchor.normal.y};
    if (Vector3LengthSqr(normal) <= 0.000001f) {
        normal = Vector3{0.0f, 0.0f, -1.0f};
    } else {
        normal = Vector3Normalize(normal);
    }

    const Vector3 tangentHalf = Vector3Scale(tangent, render.width * 0.5f);
    const Vector3 normalHalf = Vector3Scale(normal, render.thickness * 0.5f);
    const float bottomY = transform.position.y - render.height * 0.5f;
    const float topY = transform.position.y + render.height * 0.5f;

    const Vector3 leftFront = Vector3Add(Vector3Subtract(transform.position, tangentHalf), normalHalf);
    const Vector3 rightFront = Vector3Add(Vector3Add(transform.position, tangentHalf), normalHalf);
    const Vector3 rightBack = Vector3Subtract(Vector3Add(transform.position, tangentHalf), normalHalf);
    const Vector3 leftBack = Vector3Subtract(Vector3Subtract(transform.position, tangentHalf), normalHalf);

    return SectorDoorSlabGeometry{
            tangent,
            normal,
            Vector3{leftFront.x, bottomY, leftFront.z},
            Vector3{rightFront.x, bottomY, rightFront.z},
            Vector3{rightBack.x, bottomY, rightBack.z},
            Vector3{leftBack.x, bottomY, leftBack.z},
            Vector3{leftFront.x, topY, leftFront.z},
            Vector3{rightFront.x, topY, rightFront.z},
            Vector3{rightBack.x, topY, rightBack.z},
            Vector3{leftBack.x, topY, leftBack.z}};
}

SectorDoorSlabMeshData BuildSectorDoorSlabMeshData(const SectorDoorRender& render)
{
    SectorDoorSlabMeshData data;
    if (render.width <= 0.0f
            || render.height <= 0.0f
            || render.thickness <= 0.0f
            || !std::isfinite(render.width)
            || !std::isfinite(render.height)
            || !std::isfinite(render.thickness)) {
        return data;
    }

    data.vertices.reserve(24);
    data.indices.reserve(36);

    const float halfWidth = render.width * 0.5f;
    const float halfHeight = render.height * 0.5f;
    const float halfThickness = render.thickness * 0.5f;

    const Vector3 bottomFrontLeft{-halfWidth, -halfHeight, halfThickness};
    const Vector3 bottomFrontRight{halfWidth, -halfHeight, halfThickness};
    const Vector3 bottomBackRight{halfWidth, -halfHeight, -halfThickness};
    const Vector3 bottomBackLeft{-halfWidth, -halfHeight, -halfThickness};
    const Vector3 topFrontLeft{-halfWidth, halfHeight, halfThickness};
    const Vector3 topFrontRight{halfWidth, halfHeight, halfThickness};
    const Vector3 topBackRight{halfWidth, halfHeight, -halfThickness};
    const Vector3 topBackLeft{-halfWidth, halfHeight, -halfThickness};

    const auto appendFace = [&data](
            Vector3 normal,
            Vector3 a,
            Vector3 b,
            Vector3 c,
            Vector3 d,
            float uScale,
            float vScale) {
        const uint16_t base = static_cast<uint16_t>(data.vertices.size());
        data.vertices.push_back(SectorDoorSlabMeshVertex{a, normal, Vector2{0.0f, vScale}, WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{b, normal, Vector2{uScale, vScale}, WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{c, normal, Vector2{uScale, 0.0f}, WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{d, normal, Vector2{0.0f, 0.0f}, WHITE});
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 3);
    };

    appendFace(
            Vector3{0.0f, 0.0f, 1.0f},
            bottomFrontLeft,
            bottomFrontRight,
            topFrontRight,
            topFrontLeft,
            render.width,
            render.height);
    appendFace(
            Vector3{0.0f, 0.0f, -1.0f},
            bottomBackRight,
            bottomBackLeft,
            topBackLeft,
            topBackRight,
            render.width,
            render.height);
    appendFace(
            Vector3{1.0f, 0.0f, 0.0f},
            bottomFrontRight,
            bottomBackRight,
            topBackRight,
            topFrontRight,
            render.thickness,
            render.height);
    appendFace(
            Vector3{-1.0f, 0.0f, 0.0f},
            bottomBackLeft,
            bottomFrontLeft,
            topFrontLeft,
            topBackLeft,
            render.thickness,
            render.height);
    appendFace(
            Vector3{0.0f, 1.0f, 0.0f},
            topFrontLeft,
            topFrontRight,
            topBackRight,
            topBackLeft,
            render.width,
            render.thickness);
    appendFace(
            Vector3{0.0f, -1.0f, 0.0f},
            bottomBackLeft,
            bottomBackRight,
            bottomFrontRight,
            bottomFrontLeft,
            render.width,
            render.thickness);

    return data;
}

Matrix BuildSectorDoorSlabModelMatrix(
        const SectorObjectTransform& transform,
        const SectorDoorResolvedAnchor& anchor)
{
    Vector3 tangent = Vector3{anchor.tangent.x, 0.0f, anchor.tangent.y};
    if (Vector3LengthSqr(tangent) <= 0.000001f) {
        tangent = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        tangent = Vector3Normalize(tangent);
    }

    Vector3 normal = Vector3{anchor.normal.x, 0.0f, anchor.normal.y};
    if (Vector3LengthSqr(normal) <= 0.000001f) {
        normal = Vector3{0.0f, 0.0f, -1.0f};
    } else {
        normal = Vector3Normalize(normal);
    }

    const Vector3 up = Vector3{0.0f, 1.0f, 0.0f};
    return Matrix{
            tangent.x, up.x, normal.x, transform.position.x,
            tangent.y, up.y, normal.y, transform.position.y,
            tangent.z, up.z, normal.z, transform.position.z,
            0.0f, 0.0f, 0.0f, 1.0f};
}

Vector3 EvaluateSectorObjectAmbientCubeLighting(
        const BakedObjectLightingSample& sample,
        Vector3 worldNormal)
{
    if (Vector3LengthSqr(worldNormal) <= 0.000001f) {
        worldNormal = Vector3{0.0f, 1.0f, 0.0f};
    } else {
        worldNormal = Vector3Normalize(worldNormal);
    }

    const Vector3 absNormal = Vector3{std::fabs(worldNormal.x), std::fabs(worldNormal.y), std::fabs(worldNormal.z)};
    int face = 0;
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        face = worldNormal.y >= 0.0f ? 2 : 3;
    } else if (absNormal.z >= absNormal.x) {
        face = worldNormal.z >= 0.0f ? 4 : 5;
    } else {
        face = worldNormal.x >= 0.0f ? 0 : 1;
    }
    return sample.ambientCube[face];
}

namespace {

Color QuantizeStaticLightingColor(Vector3 lighting)
{
    const auto channel = [](float value) -> unsigned char {
        if (!std::isfinite(value)) {
            value = 0.0f;
        }
        value = std::clamp(value, 0.0f, 1.0f);
        return static_cast<unsigned char>(std::round(value * 255.0f));
    };
    return Color{channel(lighting.x), channel(lighting.y), channel(lighting.z), 255};
}

Vector3 TransformDoorLocalDirection(Matrix model, Vector3 localDirection)
{
    const Vector3 origin = Vector3Transform(Vector3{}, model);
    Vector3 direction = Vector3Subtract(Vector3Transform(localDirection, model), origin);
    if (Vector3LengthSqr(direction) <= 0.000001f) {
        return Vector3{0.0f, 1.0f, 0.0f};
    }
    return Vector3Normalize(direction);
}

} // namespace

bool BuildSectorDoorStaticLightingColors(
        const SectorDoorSlabMeshData& meshData,
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoorResolvedAnchor& anchor,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback,
        std::vector<Color>& outColors)
{
    outColors.clear();
    if (meshData.vertices.empty()) {
        return false;
    }

    const Matrix model = BuildSectorDoorSlabModelMatrix(transform, anchor);
    outColors.reserve(meshData.vertices.size());
    for (const SectorDoorSlabMeshVertex& vertex : meshData.vertices) {
        const Vector3 worldPosition = Vector3Transform(vertex.position, model);
        const Vector3 worldNormal = TransformDoorLocalDirection(model, vertex.normal);
        const BakedObjectLightingSample sample = SampleBakedObjectLighting(
                objectLightProbes,
                worldPosition,
                object.currentSectorId,
                mapForFallback);
        outColors.push_back(QuantizeStaticLightingColor(
                EvaluateSectorObjectAmbientCubeLighting(sample, worldNormal)));
    }
    return outColors.size() == meshData.vertices.size();
}

bool AppendSectorDoorReceiverBounds(
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorReceiverBounds>& outBounds)
{
    const auto isFiniteVector3 = [](Vector3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const auto expandBounds = [](Vector3 point, Vector3& min, Vector3& max) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    };

    if (!object.visible || !door.enabled || !render.visible) {
        return false;
    }
    if (render.width <= 0.0f
            || render.height <= 0.0f
            || render.thickness <= 0.0f
            || !std::isfinite(render.width)
            || !std::isfinite(render.height)
            || !std::isfinite(render.thickness)
            || !isFiniteVector3(transform.position)) {
        return false;
    }

    const SectorDoorSlabGeometry slab = BuildSectorDoorSlabGeometry(transform, anchor, render);
    const Vector3 corners[] = {
            slab.bottomFrontLeft,
            slab.bottomFrontRight,
            slab.bottomBackRight,
            slab.bottomBackLeft,
            slab.topFrontLeft,
            slab.topFrontRight,
            slab.topBackRight,
            slab.topBackLeft};

    Vector3 min = corners[0];
    Vector3 max = corners[0];
    for (const Vector3& corner : corners) {
        if (!isFiniteVector3(corner)) {
            return false;
        }
        expandBounds(corner, min, max);
    }

    const std::size_t beginIndex = outBounds.size();
    const auto alreadyAppended = [&outBounds, beginIndex](int sectorId) {
        for (std::size_t i = beginIndex; i < outBounds.size(); ++i) {
            if (outBounds[i].sectorId == sectorId) {
                return true;
            }
        }
        return false;
    };
    const auto appendForSector = [&](int sectorId) {
        if (sectorId <= 0 || alreadyAppended(sectorId)) {
            return;
        }
        outBounds.push_back(SectorReceiverBounds{sectorId, min, max});
    };

    appendForSector(anchor.frontSectorId);
    appendForSector(anchor.backSectorId);
    if (outBounds.size() == beginIndex) {
        appendForSector(object.currentSectorId);
    }
    return outBounds.size() > beginIndex;
}

void CollectSectorDoorReceiverBounds(
        engine::World& world,
        std::vector<SectorReceiverBounds>& outBounds)
{
    world.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [&outBounds](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                AppendSectorDoorReceiverBounds(transform, object, door, anchor, render, outBounds);
            });
}

bool AppendSectorDoorShadowCaster(
        engine::Entity entity,
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorDoorShadowCaster>& outCasters)
{
    const auto isFiniteVector3 = [](Vector3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };

    if (!object.visible || !door.enabled || !render.visible) {
        return false;
    }
    if (render.width <= 0.0f
            || render.height <= 0.0f
            || render.thickness <= 0.0f
            || !std::isfinite(render.width)
            || !std::isfinite(render.height)
            || !std::isfinite(render.thickness)
            || !isFiniteVector3(transform.position)) {
        return false;
    }

    outCasters.push_back(SectorDoorShadowCaster{
            door.placedObjectId,
            entity,
            BuildSectorDoorSlabModelMatrix(transform, anchor),
            transform.position,
            render.width,
            render.height,
            render.thickness});
    return true;
}

void CollectSectorDoorShadowCasters(
        engine::World& world,
        std::vector<SectorDoorShadowCaster>& outCasters)
{
    world.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [&outCasters](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                AppendSectorDoorShadowCaster(entity, transform, object, door, anchor, render, outCasters);
            });
}

namespace {

constexpr const char* SectorRuntimeObjectAssetScopeName = "sector_runtime_objects";
constexpr float DoorInteractionFacingDotThreshold = 0.5f;
constexpr float DoorDynamicCollisionEpsilon = 0.0001f;
// Park fully-open sliding slabs just inside the exact open distance to avoid coplanar z-fighting.
constexpr float kSectorDoorOpenParkingEpsilonWorld = 0.01f;

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
    if (state.doorObjectCount > 0) {
        state.placedObjectStatus += TextFormat(
                " | doors %zu valid, %zu invalid anchors",
                state.validDoorAnchorCount,
                state.invalidDoorAnchorCount);
    }

    if (state.skippedObjectCount == 0) {
        state.placedObjectWarning.clear();
    }

    if (state.invalidDoorAnchorCount > 0) {
        state.placedObjectWarning = TextFormat(
                "Runtime object warnings: %zu door object(s) have invalid anchors",
                state.invalidDoorAnchorCount);
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
    }
}

void RefreshDoorAnchorDiagnostics(
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map)
{
    state.doorAnchorDiagnostics.clear();
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

float ResolvedDoorOpenDistance(const SectorResolvedDoorAnchor& resolved, const SectorPlacedDoor& door)
{
    if (door.openDistance > 0.0f) {
        return door.openDistance;
    }

    switch (door.motion) {
        case SectorDoorMotionType::SlideVertical:
            return resolved.height;
        case SectorDoorMotionType::SlideLeft:
        case SectorDoorMotionType::SlideRight:
            return resolved.width;
    }

    return resolved.height;
}

float DoorAnchorYawRadians(const SectorResolvedDoorAnchor& resolved)
{
    return std::atan2(resolved.tangent.y, resolved.tangent.x);
}

Vector2 NormalizedOrFallback(Vector2 value, Vector2 fallback)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (lengthSquared <= 0.000001f || !std::isfinite(lengthSquared)) {
        return fallback;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return Vector2{value.x * invLength, value.y * invLength};
}

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

float Abs(float value)
{
    return value < 0.0f ? -value : value;
}

float SignForAxis(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

float Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vector2 Add(Vector2 a, Vector2 b)
{
    return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 Subtract(Vector2 a, Vector2 b)
{
    return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 Scale(Vector2 value, float scale)
{
    return Vector2{value.x * scale, value.y * scale};
}

float SmootherStep01(float t)
{
    t = std::isfinite(t) ? Clamp(t, 0.0f, 1.0f) : 0.0f;
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

bool PlayerVerticalIntervalOverlapsDoor(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveConfig& config,
        const SectorDynamicDoorCollider& collider)
{
    const float playerBottom = moveState.feetY;
    const float playerTop = moveState.feetY + config.playerHeight;
    return playerTop > collider.bottom + DoorDynamicCollisionEpsilon
            && playerBottom < collider.top - DoorDynamicCollisionEpsilon;
}

bool ResolveCircleAgainstDoorObb(
        Vector2& position,
        float radius,
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 tangent = NormalizedOrFallback(collider.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(collider.normal, Vector2{0.0f, -1.0f});
    const Vector2 relative = Subtract(position, collider.center);
    const Vector2 local{
            Dot(relative, tangent),
            Dot(relative, normal)};
    const Vector2 closest{
            Clamp(local.x, -collider.halfExtents.x, collider.halfExtents.x),
            Clamp(local.y, -collider.halfExtents.y, collider.halfExtents.y)};
    const Vector2 delta{
            local.x - closest.x,
            local.y - closest.y};
    const float distanceSq = delta.x * delta.x + delta.y * delta.y;
    const float radiusSq = radius * radius;

    Vector2 pushLocal = {};
    if (distanceSq > DoorDynamicCollisionEpsilon) {
        if (distanceSq >= radiusSq - DoorDynamicCollisionEpsilon) {
            return false;
        }
        const float distance = std::sqrt(distanceSq);
        const float penetration = radius - distance + DoorDynamicCollisionEpsilon;
        pushLocal = Vector2{delta.x / distance * penetration, delta.y / distance * penetration};
    } else {
        const float overlapTangent = collider.halfExtents.x + radius - Abs(local.x);
        const float overlapNormal = collider.halfExtents.y + radius - Abs(local.y);
        if (overlapTangent <= 0.0f || overlapNormal <= 0.0f) {
            return false;
        }
        if (overlapTangent < overlapNormal) {
            pushLocal = Vector2{SignForAxis(local.x) * (overlapTangent + DoorDynamicCollisionEpsilon), 0.0f};
        } else {
            pushLocal = Vector2{0.0f, SignForAxis(local.y) * (overlapNormal + DoorDynamicCollisionEpsilon)};
        }
    }

    position = Add(
            position,
            Add(Scale(tangent, pushLocal.x), Scale(normal, pushLocal.y)));
    return true;
}

bool SegmentIntersectsExpandedDoorObb(
        Vector2 startPosition,
        Vector2 endPosition,
        float radius,
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 tangent = NormalizedOrFallback(collider.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(collider.normal, Vector2{0.0f, -1.0f});
    const Vector2 startRelative = Subtract(startPosition, collider.center);
    const Vector2 endRelative = Subtract(endPosition, collider.center);
    const Vector2 startLocal{
            Dot(startRelative, tangent),
            Dot(startRelative, normal)};
    const Vector2 endLocal{
            Dot(endRelative, tangent),
            Dot(endRelative, normal)};
    const Vector2 expanded{
            collider.halfExtents.x + radius,
            collider.halfExtents.y + radius};

    if (startLocal.x > -expanded.x + DoorDynamicCollisionEpsilon
            && startLocal.x < expanded.x - DoorDynamicCollisionEpsilon
            && startLocal.y > -expanded.y + DoorDynamicCollisionEpsilon
            && startLocal.y < expanded.y - DoorDynamicCollisionEpsilon) {
        return false;
    }

    const Vector2 deltaLocal = Subtract(endLocal, startLocal);
    float tEnter = 0.0f;
    float tExit = 1.0f;
    const float startAxes[2] = {startLocal.x, startLocal.y};
    const float deltaAxes[2] = {deltaLocal.x, deltaLocal.y};
    const float minAxes[2] = {-expanded.x, -expanded.y};
    const float maxAxes[2] = {expanded.x, expanded.y};

    for (int axis = 0; axis < 2; ++axis) {
        if (std::fabs(deltaAxes[axis]) <= DoorDynamicCollisionEpsilon) {
            if (startAxes[axis] < minAxes[axis] || startAxes[axis] > maxAxes[axis]) {
                return false;
            }
            continue;
        }

        const float invDelta = 1.0f / deltaAxes[axis];
        float axisEnter = (minAxes[axis] - startAxes[axis]) * invDelta;
        float axisExit = (maxAxes[axis] - startAxes[axis]) * invDelta;
        if (axisEnter > axisExit) {
            std::swap(axisEnter, axisExit);
        }

        tEnter = std::max(tEnter, axisEnter);
        tExit = std::min(tExit, axisExit);
        if (tEnter > tExit) {
            return false;
        }
    }

    return tExit >= -DoorDynamicCollisionEpsilon
            && tEnter <= 1.0f + DoorDynamicCollisionEpsilon;
}

void ConstrainCircleToStartingSideOfDoorObb(
        Vector2& position,
        Vector2 startPosition,
        float radius,
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 tangent = NormalizedOrFallback(collider.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(collider.normal, Vector2{0.0f, -1.0f});
    const Vector2 startRelative = Subtract(startPosition, collider.center);
    const Vector2 positionRelative = Subtract(position, collider.center);
    const Vector2 startLocal{
            Dot(startRelative, tangent),
            Dot(startRelative, normal)};
    const Vector2 positionLocal{
            Dot(positionRelative, tangent),
            Dot(positionRelative, normal)};
    const Vector2 expanded{
            collider.halfExtents.x + radius,
            collider.halfExtents.y + radius};

    Vector2 constrainedLocal = positionLocal;
    bool constrained = false;
    if (startLocal.x <= -expanded.x
            && positionLocal.x > -expanded.x + DoorDynamicCollisionEpsilon) {
        constrainedLocal.x = -expanded.x - DoorDynamicCollisionEpsilon;
        constrained = true;
    } else if (startLocal.x >= expanded.x
            && positionLocal.x < expanded.x - DoorDynamicCollisionEpsilon) {
        constrainedLocal.x = expanded.x + DoorDynamicCollisionEpsilon;
        constrained = true;
    }

    if (startLocal.y <= -expanded.y
            && positionLocal.y > -expanded.y + DoorDynamicCollisionEpsilon) {
        constrainedLocal.y = -expanded.y - DoorDynamicCollisionEpsilon;
        constrained = true;
    } else if (startLocal.y >= expanded.y
            && positionLocal.y < expanded.y - DoorDynamicCollisionEpsilon) {
        constrainedLocal.y = expanded.y + DoorDynamicCollisionEpsilon;
        constrained = true;
    }

    if (!constrained) {
        return;
    }

    position = Add(
            collider.center,
            Add(Scale(tangent, constrainedLocal.x), Scale(normal, constrainedLocal.y)));
}

Vector3 DoorMotionOffset(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion)
{
    const float openFraction = std::isfinite(motion.openFraction)
            ? Clamp(motion.openFraction, 0.0f, 1.0f)
            : 0.0f;
    const float openDistance = motion.openDistance > 0.0f && std::isfinite(motion.openDistance)
            ? motion.openDistance
            : 0.0f;
    const float effectiveOpenDistance = openDistance > kSectorDoorOpenParkingEpsilonWorld
            ? openDistance - kSectorDoorOpenParkingEpsilonWorld
            : openDistance;
    const float offset = SmootherStep01(openFraction) * effectiveOpenDistance;
    const Vector2 tangent = NormalizedOrFallback(anchor.tangent, Vector2{1.0f, 0.0f});

    switch (motion.motion) {
        case SectorDoorMotionType::SlideVertical:
            return Vector3{0.0f, offset, 0.0f};
        case SectorDoorMotionType::SlideLeft:
            return Vector3{-tangent.x * offset, 0.0f, -tangent.y * offset};
        case SectorDoorMotionType::SlideRight:
            return Vector3{tangent.x * offset, 0.0f, tangent.y * offset};
    }

    return Vector3{};
}

Vector3 DoorClosedCenter(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render)
{
    const Vector2 normal = NormalizedOrFallback(anchor.normal, Vector2{0.0f, -1.0f});
    return Vector3{
            anchor.midpoint.x + normal.x * render.normalOffset,
            anchor.openBottom + render.height * 0.5f,
            anchor.midpoint.y + normal.y * render.normalOffset};
}

SectorDoorResolvedAnchor ToRuntimeDoorAnchor(const SectorResolvedDoorAnchor& resolved)
{
    SectorDoorResolvedAnchor anchor;
    anchor.lineDefId = resolved.lineDefId;
    anchor.frontSectorId = resolved.frontSectorId;
    anchor.backSectorId = resolved.backSectorId;
    anchor.frontSideDefId = resolved.frontSideDefId;
    anchor.backSideDefId = resolved.backSideDefId;
    anchor.endpointA = resolved.endpointA;
    anchor.endpointB = resolved.endpointB;
    anchor.midpoint = resolved.midpoint;
    anchor.tangent = resolved.tangent;
    anchor.normal = resolved.normal;
    anchor.openBottom = resolved.openBottom;
    anchor.openTop = resolved.openTop;
    anchor.portalWidth = resolved.portalWidth;
    anchor.portalHeight = resolved.portalHeight;
    return anchor;
}

Vector2 StableDoorInteractionPointXZ(
        const SectorDoorResolvedAnchor& anchor,
        Vector2 playerPosition)
{
    const Vector2 a = anchor.endpointA;
    const Vector2 b = anchor.endpointB;
    if (!IsFinite(a) || !IsFinite(b)) {
        return IsFinite(anchor.midpoint) ? anchor.midpoint : Vector2{};
    }

    const Vector2 ab = Subtract(b, a);
    const float lengthSq = Dot(ab, ab);
    if (lengthSq <= DoorDynamicCollisionEpsilon || !std::isfinite(lengthSq)) {
        return IsFinite(anchor.midpoint) ? anchor.midpoint : a;
    }

    const float t = Clamp(Dot(Subtract(playerPosition, a), ab) / lengthSq, 0.0f, 1.0f);
    return Add(a, Scale(ab, t));
}

Vector2 StableDoorFacingPointXZ(
        const SectorDoorResolvedAnchor& anchor,
        Vector2 playerPosition,
        Vector2 closestPoint)
{
    if (Vector2LengthSqr(Subtract(closestPoint, playerPosition)) > DoorDynamicCollisionEpsilon) {
        return closestPoint;
    }
    if (IsFinite(anchor.midpoint)
            && Vector2LengthSqr(Subtract(anchor.midpoint, playerPosition)) > DoorDynamicCollisionEpsilon) {
        return anchor.midpoint;
    }
    return closestPoint;
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
    RefreshDoorAnchorDiagnostics(state, map);

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

            const SectorDoorResolvedAnchor runtimeAnchor = ToRuntimeDoorAnchor(resolved);
            const SectorDoorMotion runtimeMotion{
                    placedObject.door.motion,
                    placedObject.door.initialOpenFraction,
                    placedObject.door.initialOpenFraction,
                    ResolvedDoorOpenDistance(resolved, placedObject.door),
                    placedObject.door.speed};
            const SectorDoorRender runtimeRender{
                    resolved.width,
                    resolved.height,
                    placedObject.door.thickness,
                    placedObject.door.normalOffset,
                    placedObject.door.textureId,
                    WHITE,
                    true};
            const Vector3 worldPosition = Vector3Add(
                    DoorClosedCenter(runtimeAnchor, runtimeRender),
                    DoorMotionOffset(runtimeAnchor, runtimeMotion));
            SectorObject object;
            if (state.objectSectorLookupWorldValid) {
                const int foundSectorId = state.objectSectorLookupWorld.FindSectorContainingPointPreferCurrent(
                        Vector2{worldPosition.x, worldPosition.z},
                        resolved.frontSectorId);
                object.currentSectorId = foundSectorId != 0 ? foundSectorId : resolved.frontSectorId;
            } else {
                object.currentSectorId = resolved.frontSectorId;
            }

            const engine::Entity entity = world.CreateEntity();
            world.Add(entity, SectorObjectTransform{worldPosition, DoorAnchorYawRadians(resolved)});
            world.Add(entity, object);
            world.Add(entity, SectorObjectLighting{SampleBakedObjectLighting(
                    state.objectLightProbes,
                    worldPosition,
                    object.currentSectorId,
                    &map)});
            world.Add(entity, SectorDoor{placedObject.id, true});
            world.Add(entity, runtimeAnchor);
            world.Add(entity, runtimeMotion);
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
            state.placedObjectEntities.push_back(SectorPlacedRuntimeObjectEntity{placedObject.id, entity});
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
    UpdateSectorDoorDerivedStateSystem(world);
    RefreshPlacedRuntimeObjectDiagnostics(world, assets, state);
}

void UpdateSectorRuntimeObjects(
        engine::World& world,
        engine::AssetManager& assets,
        SectorRuntimeObjectState& state,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition)
{
    AdvanceSectorBillboardAnimatorSystem(world, dt);
    if (playerPosition != nullptr) {
        UpdateSectorDoorAutoOpenSystem(world, *playerPosition);
    }
    AdvanceSectorDoorMotionSystem(world, dt);
    UpdateSectorDoorDerivedStateSystem(world);
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

void UpdateSectorDoorAutoOpenSystem(engine::World& world, const Vector3& playerPosition)
{
    if (!std::isfinite(playerPosition.x) || !std::isfinite(playerPosition.y) || !std::isfinite(playerPosition.z)) {
        return;
    }

    world.ForEach<SectorDoor, SectorDoorResolvedAnchor, SectorDoorMotion, SectorDoorInteraction>(
            [&playerPosition](
                    engine::Entity,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorMotion& motion,
                    SectorDoorInteraction& interaction) {
                if (!door.enabled || !interaction.autoOpen) {
                    return;
                }

                const float distance = interaction.autoOpenDistance > 0.0f
                                && std::isfinite(interaction.autoOpenDistance)
                        ? interaction.autoOpenDistance
                        : kSectorDoorAutoOpenFallbackDistance;
                const Vector2 playerXZ{playerPosition.x, playerPosition.z};
                const Vector2 target = StableDoorInteractionPointXZ(anchor, playerXZ);
                const Vector2 toTarget = Subtract(target, playerXZ);
                const float distanceSq = Vector2LengthSqr(toTarget);
                motion.targetOpenFraction = distanceSq <= distance * distance ? 1.0f : 0.0f;
            });
}

bool ToggleTargetedSectorDoorInteractionSystem(
        engine::World& world,
        const Vector3& playerPosition,
        const Vector3& playerForward)
{
    if (!std::isfinite(playerPosition.x) || !std::isfinite(playerPosition.y) || !std::isfinite(playerPosition.z)
            || !std::isfinite(playerForward.x) || !std::isfinite(playerForward.y) || !std::isfinite(playerForward.z)) {
        return false;
    }

    Vector2 forward{playerForward.x, playerForward.z};
    if (Vector2LengthSqr(forward) <= 0.000001f) {
        return false;
    }
    forward = Vector2Normalize(forward);

    engine::Entity bestEntity;
    bool found = false;
    float bestDistanceSq = std::numeric_limits<float>::max();

    world.ForEach<SectorDoor, SectorDoorResolvedAnchor, SectorDoorMotion, SectorDoorInteraction>(
            [&playerPosition, &forward, &bestEntity, &found, &bestDistanceSq](
                    engine::Entity entity,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorMotion&,
                    SectorDoorInteraction& interaction) {
                if (!door.enabled || interaction.autoOpen) {
                    return;
                }

                const float distance = interaction.interactionDistance > 0.0f
                                && std::isfinite(interaction.interactionDistance)
                        ? interaction.interactionDistance
                        : 1.5f;
                const Vector2 playerXZ{playerPosition.x, playerPosition.z};
                const Vector2 target = StableDoorInteractionPointXZ(anchor, playerXZ);
                const Vector2 toDoor = Subtract(target, playerXZ);
                const float distanceSq = Vector2LengthSqr(toDoor);
                if (distanceSq > distance * distance) {
                    return;
                }

                const Vector2 facingPoint = StableDoorFacingPointXZ(anchor, playerXZ, target);
                const Vector2 toFacingPoint = Subtract(facingPoint, playerXZ);
                const float facingDistanceSq = Vector2LengthSqr(toFacingPoint);
                if (facingDistanceSq > DoorDynamicCollisionEpsilon) {
                    const Vector2 directionToDoor = Vector2Scale(toFacingPoint, 1.0f / std::sqrt(facingDistanceSq));
                    if (Vector2DotProduct(forward, directionToDoor) < DoorInteractionFacingDotThreshold) {
                        return;
                    }
                }

                if (distanceSq < bestDistanceSq) {
                    bestDistanceSq = distanceSq;
                    bestEntity = entity;
                    found = true;
                }
            });

    if (!found) {
        return false;
    }

    SectorDoorMotion& motion = world.Get<SectorDoorMotion>(bestEntity);
    const float openFraction = std::isfinite(motion.openFraction)
            ? Clamp(motion.openFraction, 0.0f, 1.0f)
            : 0.0f;
    const float targetOpenFraction = std::isfinite(motion.targetOpenFraction)
            ? Clamp(motion.targetOpenFraction, 0.0f, 1.0f)
            : openFraction;
    motion.targetOpenFraction = targetOpenFraction > 0.5f || openFraction > 0.5f ? 0.0f : 1.0f;
    return true;
}

void AdvanceSectorDoorMotionSystem(engine::World& world, float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    world.ForEach<SectorDoor, SectorDoorMotion>(
            [dt](engine::Entity, SectorDoor& door, SectorDoorMotion& motion) {
                if (!door.enabled) {
                    return;
                }

                if (!std::isfinite(motion.openFraction)) {
                    motion.openFraction = 0.0f;
                }
                if (!std::isfinite(motion.targetOpenFraction)) {
                    motion.targetOpenFraction = motion.openFraction;
                }

                motion.openFraction = Clamp(motion.openFraction, 0.0f, 1.0f);
                motion.targetOpenFraction = Clamp(motion.targetOpenFraction, 0.0f, 1.0f);
                if (motion.speed <= 0.0f || !std::isfinite(motion.speed)) {
                    return;
                }

                const float distance = motion.openDistance > 0.0f && std::isfinite(motion.openDistance)
                        ? motion.openDistance
                        : 1.0f;
                const float fractionStep = (motion.speed * dt) / distance;
                if (!std::isfinite(fractionStep) || fractionStep <= 0.0f) {
                    return;
                }

                if (motion.openFraction < motion.targetOpenFraction) {
                    motion.openFraction = std::min(motion.openFraction + fractionStep, motion.targetOpenFraction);
                } else if (motion.openFraction > motion.targetOpenFraction) {
                    motion.openFraction = std::max(motion.openFraction - fractionStep, motion.targetOpenFraction);
                }
            });
}

void UpdateSectorDoorDerivedStateSystem(engine::World& world)
{
    world.ForEach<
            SectorObjectTransform,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorMotion,
            SectorDoorRender,
            SectorDoorCollider,
            SectorDoorPortalBlocker>(
            [](engine::Entity,
                    SectorObjectTransform& transform,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorMotion& motion,
                    SectorDoorRender& render,
                    SectorDoorCollider& collider,
                    SectorDoorPortalBlocker& blocker) {
                const Vector2 tangent = NormalizedOrFallback(anchor.tangent, Vector2{1.0f, 0.0f});
                const Vector2 normal = NormalizedOrFallback(anchor.normal, Vector2{0.0f, -1.0f});
                const bool validShape = render.width > 0.0f
                        && std::isfinite(render.width)
                        && render.height > 0.0f
                        && std::isfinite(render.height)
                        && render.thickness > 0.0f
                        && std::isfinite(render.thickness);
                const Vector3 center = Vector3Add(
                        DoorClosedCenter(anchor, render),
                        DoorMotionOffset(anchor, motion));

                transform.position = center;
                transform.yawRadians = std::atan2(tangent.y, tangent.x);

                collider.center = Vector2{center.x, center.z};
                collider.tangent = tangent;
                collider.normal = normal;
                collider.halfExtents = Vector2{
                        validShape ? render.width * 0.5f : 0.0f,
                        validShape ? render.thickness * 0.5f : 0.0f};
                collider.bottom = center.y - (validShape ? render.height * 0.5f : 0.0f);
                collider.top = center.y + (validShape ? render.height * 0.5f : 0.0f);
                collider.enabled = door.enabled && validShape;

                const float openFraction = std::isfinite(motion.openFraction)
                        ? Clamp(motion.openFraction, 0.0f, 1.0f)
                        : 0.0f;
                blocker.blocksPortal = door.enabled && openFraction <= kSectorDoorPortalBlockEpsilon;
            });
}

void CollectSectorDoorDynamicColliders(
        engine::World& world,
        std::vector<SectorDynamicDoorCollider>& colliders)
{
    world.ForEach<SectorDoor, SectorDoorCollider>(
            [&colliders](engine::Entity entity, SectorDoor& door, SectorDoorCollider& collider) {
                if (!door.enabled || !collider.enabled) {
                    return;
                }
                if (!IsFinite(collider.center)
                        || !IsFinite(collider.tangent)
                        || !IsFinite(collider.normal)
                        || !IsFinite(collider.halfExtents)
                        || !std::isfinite(collider.bottom)
                        || !std::isfinite(collider.top)
                        || collider.halfExtents.x <= 0.0f
                        || collider.halfExtents.y <= 0.0f
                        || collider.top <= collider.bottom) {
                    return;
                }

                colliders.push_back(SectorDynamicDoorCollider{
                        door.placedObjectId,
                        entity,
                        collider.center,
                        NormalizedOrFallback(collider.tangent, Vector2{1.0f, 0.0f}),
                        NormalizedOrFallback(collider.normal, Vector2{0.0f, -1.0f}),
                        collider.halfExtents,
                        collider.bottom,
                        collider.top});
            });
}

void CollectSectorDoorDynamicPortalBlockers(
        engine::World& world,
        std::vector<RuntimePortalDynamicBlocker>& blockers)
{
    world.ForEach<SectorDoor, SectorDoorPortalBlocker>(
            [&blockers](engine::Entity, SectorDoor& door, SectorDoorPortalBlocker& blocker) {
                if (!door.enabled || !blocker.blocksPortal) {
                    return;
                }
                if (blocker.lineDefId <= 0
                        || blocker.frontSectorId <= 0
                        || blocker.backSectorId <= 0
                        || blocker.frontSideDefId <= 0
                        || blocker.backSideDefId <= 0) {
                    return;
                }

                blockers.push_back(RuntimePortalDynamicBlocker{
                        blocker.lineDefId,
                        blocker.frontSideDefId,
                        blocker.frontSectorId,
                        blocker.backSectorId,
                        true});
                blockers.push_back(RuntimePortalDynamicBlocker{
                        blocker.lineDefId,
                        blocker.backSideDefId,
                        blocker.backSectorId,
                        blocker.frontSectorId,
                        true});
            });
}

SectorCollisionMoveResult ResolveSectorDoorDynamicCollidersForPlayerMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& staticResult,
        const SectorCollisionMoveConfig& moveConfig,
        const std::vector<SectorDynamicDoorCollider>& colliders)
{
    SectorCollisionMoveResult result = staticResult;
    SectorCollisionMoveConfig config = moveConfig;
    if (!std::isfinite(config.radius)) {
        config.radius = 0.25f;
    }
    if (!std::isfinite(config.playerHeight)) {
        config.playerHeight = 1.6f;
    }
    if (!std::isfinite(config.stepHeight)) {
        config.stepHeight = 0.25f;
    }
    config.radius = std::clamp(config.radius, 0.001f, 64.0f);
    config.playerHeight = std::clamp(config.playerHeight, 0.001f, 64.0f);
    config.stepHeight = std::clamp(config.stepHeight, 0.0f, 64.0f);
    config.maxIterations = std::clamp(config.maxIterations, 1, 16);

    if (!IsFinite(result.positionXZ) || !IsFinite(moveState.positionXZ) || colliders.empty()) {
        return result;
    }

    bool hitDynamicDoor = false;
    for (const SectorDynamicDoorCollider& collider : colliders) {
        if (!IsFinite(collider.center)
                || !IsFinite(collider.tangent)
                || !IsFinite(collider.normal)
                || !IsFinite(collider.halfExtents)
                || !std::isfinite(collider.bottom)
                || !std::isfinite(collider.top)
                || collider.halfExtents.x <= 0.0f
                || collider.halfExtents.y <= 0.0f
                || collider.top <= collider.bottom
                || !PlayerVerticalIntervalOverlapsDoor(moveState, config, collider)) {
            continue;
        }

        if (SegmentIntersectsExpandedDoorObb(
                    moveState.positionXZ,
                    staticResult.positionXZ,
                    config.radius,
                    collider)) {
            result.positionXZ = moveState.positionXZ;
            result.currentSectorId = moveState.currentSectorId;
            result.hitWall = true;
            hitDynamicDoor = true;
            break;
        }
    }

    for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
        bool changed = false;
        for (const SectorDynamicDoorCollider& collider : colliders) {
            if (!IsFinite(collider.center)
                    || !IsFinite(collider.tangent)
                    || !IsFinite(collider.normal)
                    || !IsFinite(collider.halfExtents)
                    || !std::isfinite(collider.bottom)
                    || !std::isfinite(collider.top)
                    || collider.halfExtents.x <= 0.0f
                    || collider.halfExtents.y <= 0.0f
                    || collider.top <= collider.bottom
                    || !PlayerVerticalIntervalOverlapsDoor(moveState, config, collider)) {
                continue;
            }

            if (ResolveCircleAgainstDoorObb(result.positionXZ, config.radius, collider)) {
                if (staticResult.currentSectorId != moveState.currentSectorId) {
                    ConstrainCircleToStartingSideOfDoorObb(
                            result.positionXZ,
                            moveState.positionXZ,
                            config.radius,
                            collider);
                }
                result.hitWall = true;
                hitDynamicDoor = true;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }

    if (hitDynamicDoor && result.currentSectorId != moveState.currentSectorId) {
        result.currentSectorId = moveState.currentSectorId;
    }
    return result;
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
