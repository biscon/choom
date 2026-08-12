#include "sector_demo/SectorDoorRuntime.h"

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMath.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <raymath.h>

namespace game {

const char* SectorDoorFaceName(SectorDoorFace face)
{
    switch (face) {
        case SectorDoorFace::Front:
            return "Front";
        case SectorDoorFace::Back:
            return "Back";
        case SectorDoorFace::Left:
            return "Left";
        case SectorDoorFace::Right:
            return "Right";
        case SectorDoorFace::Top:
            return "Top";
        case SectorDoorFace::Bottom:
            return "Bottom";
        case SectorDoorFace::Count:
            break;
    }
    return "Unknown";
}

int SectorDoorFaceIndex(SectorDoorFace face)
{
    const int index = static_cast<int>(face);
    return index >= 0 && index < SectorDoorFaceCount ? index : 0;
}

SectorDoorFace SectorDoorFaceFromIndex(int index)
{
    if (index < 0 || index >= SectorDoorFaceCount) {
        return SectorDoorFace::Front;
    }
    return static_cast<SectorDoorFace>(index);
}

SectorDoorFaceUv& DoorFaceUv(SectorDoorFaceUvSet& uvs, SectorDoorFace face)
{
    return uvs.faces[SectorDoorFaceIndex(face)];
}

const SectorDoorFaceUv& DoorFaceUv(const SectorDoorFaceUvSet& uvs, SectorDoorFace face)
{
    return uvs.faces[SectorDoorFaceIndex(face)];
}

bool IsDefaultSectorDoorFaceUv(const SectorDoorFaceUv& uv)
{
    return uv.scale.x == 1.0f
            && uv.scale.y == 1.0f
            && uv.offset.x == 0.0f
            && uv.offset.y == 0.0f;
}

bool IsDefaultSectorDoorFaceUvSet(const SectorDoorFaceUvSet& uvs)
{
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        if (!IsDefaultSectorDoorFaceUv(uvs.faces[i])) {
            return false;
        }
    }
    return true;
}

bool SameSectorDoorFaceUv(const SectorDoorFaceUv& a, const SectorDoorFaceUv& b)
{
    return a.scale.x == b.scale.x
            && a.scale.y == b.scale.y
            && a.offset.x == b.offset.x
            && a.offset.y == b.offset.y;
}

bool SameSectorDoorFaceUvSet(const SectorDoorFaceUvSet& a, const SectorDoorFaceUvSet& b)
{
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        if (!SameSectorDoorFaceUv(a.faces[i], b.faces[i])) {
            return false;
        }
    }
    return true;
}

bool IsValidSectorDoorUvScale(float scale)
{
    return std::isfinite(scale)
            && scale >= TopologyUvScaleMin
            && scale <= TopologyUvScaleMax;
}

bool IsValidSectorDoorUvOffset(float offset)
{
    return std::isfinite(offset);
}

Vector2 SectorDoorFaceDimensions(const SectorDoorRender& render, SectorDoorFace face)
{
    switch (face) {
        case SectorDoorFace::Front:
        case SectorDoorFace::Back:
            return Vector2{render.width, render.height};
        case SectorDoorFace::Left:
        case SectorDoorFace::Right:
            return Vector2{render.thickness, render.height};
        case SectorDoorFace::Top:
        case SectorDoorFace::Bottom:
            return Vector2{render.width, render.thickness};
        case SectorDoorFace::Count:
            break;
    }
    return Vector2{};
}

Vector2 SectorDoorFaceBaseUvSpan(const SectorDoorRender& render, SectorDoorFace face)
{
    return SectorDoorFaceDimensions(render, face);
}

bool FitSectorDoorFaceUv(
        SectorDoorFaceUvSet& uvs,
        SectorDoorFace face,
        SectorDoorUvFitMode mode,
        const SectorDoorRender& render,
        std::string* outError)
{
    const Vector2 baseUvSpan = SectorDoorFaceBaseUvSpan(render, face);
    if ((mode == SectorDoorUvFitMode::Width || mode == SectorDoorUvFitMode::Both)
            && (!(baseUvSpan.x > 0.0f) || !std::isfinite(baseUvSpan.x))) {
        if (outError != nullptr) {
            *outError = "Fit width needs a positive finite base U span.";
        }
        return false;
    }
    if ((mode == SectorDoorUvFitMode::Height || mode == SectorDoorUvFitMode::Both)
            && (!(baseUvSpan.y > 0.0f) || !std::isfinite(baseUvSpan.y))) {
        if (outError != nullptr) {
            *outError = "Fit height needs a positive finite base V span.";
        }
        return false;
    }

    SectorDoorFaceUv fitted = DoorFaceUv(uvs, face);
    if (mode == SectorDoorUvFitMode::Width || mode == SectorDoorUvFitMode::Both) {
        const float scale = 1.0f / baseUvSpan.x;
        if (!IsValidSectorDoorUvScale(scale)) {
            if (outError != nullptr) {
                *outError = "Fit width requires a UV scale outside the editable range.";
            }
            return false;
        }
        fitted.scale.x = scale;
        fitted.offset.x = 0.0f;
    }
    if (mode == SectorDoorUvFitMode::Height || mode == SectorDoorUvFitMode::Both) {
        const float scale = 1.0f / baseUvSpan.y;
        if (!IsValidSectorDoorUvScale(scale)) {
            if (outError != nullptr) {
                *outError = "Fit height requires a UV scale outside the editable range.";
            }
            return false;
        }
        fitted.scale.y = scale;
        fitted.offset.y = 0.0f;
    }

    DoorFaceUv(uvs, face) = fitted;
    if (outError != nullptr) {
        outError->clear();
    }
    return true;
}

bool ResetSectorDoorFaceUv(SectorDoorFaceUvSet& uvs, SectorDoorFace face)
{
    SectorDoorFaceUv& uv = DoorFaceUv(uvs, face);
    if (IsDefaultSectorDoorFaceUv(uv)) {
        return false;
    }
    uv = SectorDoorFaceUv{};
    return true;
}

bool CopySectorDoorFaceUv(
        SectorDoorFaceUvSet& uvs,
        SectorDoorFace source,
        SectorDoorFace target)
{
    if (SectorDoorFaceIndex(source) == SectorDoorFaceIndex(target)) {
        return false;
    }
    const SectorDoorFaceUv sourceUv = DoorFaceUv(uvs, source);
    SectorDoorFaceUv& targetUv = DoorFaceUv(uvs, target);
    if (SameSectorDoorFaceUv(sourceUv, targetUv)) {
        return false;
    }
    targetUv = sourceUv;
    return true;
}

bool ApplySectorDoorFaceUvToAll(SectorDoorFaceUvSet& uvs, SectorDoorFace source)
{
    const SectorDoorFaceUv sourceUv = DoorFaceUv(uvs, source);
    bool changed = false;
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        if (!SameSectorDoorFaceUv(uvs.faces[i], sourceUv)) {
            uvs.faces[i] = sourceUv;
            changed = true;
        }
    }
    return changed;
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
            Vector2 baseUvSpan,
            const SectorDoorFaceUv& uv) {
        const uint16_t base = static_cast<uint16_t>(data.vertices.size());
        const auto transformUv = [&uv](float u, float v) {
            return Vector2{u * uv.scale.x + uv.offset.x, v * uv.scale.y + uv.offset.y};
        };
        data.vertices.push_back(SectorDoorSlabMeshVertex{a, normal, transformUv(0.0f, baseUvSpan.y), WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{b, normal, transformUv(baseUvSpan.x, baseUvSpan.y), WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{c, normal, transformUv(baseUvSpan.x, 0.0f), WHITE});
        data.vertices.push_back(SectorDoorSlabMeshVertex{d, normal, transformUv(0.0f, 0.0f), WHITE});
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
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Front),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Front));
    appendFace(
            Vector3{0.0f, 0.0f, -1.0f},
            bottomBackRight,
            bottomBackLeft,
            topBackLeft,
            topBackRight,
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Back),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Back));
    appendFace(
            Vector3{1.0f, 0.0f, 0.0f},
            bottomFrontRight,
            bottomBackRight,
            topBackRight,
            topFrontRight,
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Right),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Right));
    appendFace(
            Vector3{-1.0f, 0.0f, 0.0f},
            bottomBackLeft,
            bottomFrontLeft,
            topFrontLeft,
            topBackLeft,
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Left),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Left));
    appendFace(
            Vector3{0.0f, 1.0f, 0.0f},
            topFrontLeft,
            topFrontRight,
            topBackRight,
            topBackLeft,
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Top),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Top));
    appendFace(
            Vector3{0.0f, -1.0f, 0.0f},
            bottomBackLeft,
            bottomBackRight,
            bottomFrontRight,
            bottomFrontLeft,
            SectorDoorFaceBaseUvSpan(render, SectorDoorFace::Bottom),
            DoorFaceUv(render.faceUvs, SectorDoorFace::Bottom));

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

Matrix BuildSectorDoorShadowCasterModelMatrix(
        const SectorDoorShadowCaster& caster,
        float visualWidth,
        float visualHeight)
{
    Matrix model = caster.model;
    if (visualWidth <= 0.0f
            || visualHeight <= 0.0f
            || caster.width <= 0.0f
            || caster.height <= 0.0f
            || !std::isfinite(visualWidth)
            || !std::isfinite(visualHeight)
            || !std::isfinite(caster.width)
            || !std::isfinite(caster.height)) {
        return model;
    }

    const float scaleX = caster.width / visualWidth;
    const float scaleY = caster.height / visualHeight;
    model.m0 *= scaleX;
    model.m1 *= scaleX;
    model.m2 *= scaleX;
    model.m4 *= scaleY;
    model.m5 *= scaleY;
    model.m6 *= scaleY;
    return model;
}

namespace {

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
        std::vector<Vector3>& outLighting)
{
    outLighting.clear();
    if (meshData.vertices.empty()) {
        return false;
    }

    const Matrix model = BuildSectorDoorSlabModelMatrix(transform, anchor);
    outLighting.reserve(meshData.vertices.size());
    for (const SectorDoorSlabMeshVertex& vertex : meshData.vertices) {
        const Vector3 worldPosition = Vector3Transform(vertex.position, model);
        const Vector3 worldNormal = TransformDoorLocalDirection(model, vertex.normal);
        const BakedObjectLightingSample sample = SampleBakedObjectLighting(
                objectLightProbes,
                worldPosition,
                object.currentSectorId,
                mapForFallback);
        outLighting.push_back(EvaluateBakedObjectAmbientCubeLighting(
                sample, worldNormal));
    }
    return outLighting.size() == meshData.vertices.size();
}

bool AppendSectorDoorReceiverBounds(
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorReceiverBounds>& outBounds)
{
    if (!object.visible || !door.enabled || !render.visible) {
        return false;
    }
    if (render.width <= 0.0f
            || render.height <= 0.0f
            || render.thickness <= 0.0f
            || !std::isfinite(render.width)
            || !std::isfinite(render.height)
            || !std::isfinite(render.thickness)
            || !IsFiniteVector3(transform.position)) {
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

    SectorAabb3 bounds = SectorAabb3FromPoint(corners[0]);
    for (const Vector3& corner : corners) {
        if (!IsFiniteVector3(corner)) {
            return false;
        }
        ExpandSectorAabb3(bounds, corner);
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
        outBounds.push_back(SectorReceiverBounds{sectorId, bounds.min, bounds.max});
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
            render.width + 2.0f * kSectorDoorShadowCasterHorizontalSealMarginWorld,
            render.height + 2.0f * kSectorDoorShadowCasterVerticalSealMarginWorld,
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

constexpr float DoorInteractionFacingDotThreshold = 0.5f;
constexpr float DoorDynamicCollisionEpsilon = 0.0001f;
// Park fully-open sliding slabs just inside the exact open distance to avoid coplanar z-fighting.
constexpr float kSectorDoorOpenParkingEpsilonWorld = 0.01f;

} // namespace

float SectorDoorResolvedOpenDistance(const SectorResolvedDoorAnchor& resolved, const SectorPlacedDoor& door)
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
        case SectorDoorMotionType::Swing:
            return 0.0f;
    }

    return resolved.height;
}

float SectorDoorAnchorYawRadians(const SectorResolvedDoorAnchor& resolved)
{
    return std::atan2(resolved.tangent.y, resolved.tangent.x);
}

namespace {

Vector2 NormalizedOrFallback(Vector2 value, Vector2 fallback)
{
    return NormalizeVector2OrFallback(value, fallback, 0.000001f);
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

bool CircleOverlapsDoorObb(
        Vector2 position,
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
    const Vector2 delta = Subtract(local, closest);
    return Dot(delta, delta)
            <= radius * radius + DoorDynamicCollisionEpsilon;
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

} // namespace

Vector3 SectorDoorMotionOffset(
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
        case SectorDoorMotionType::Swing:
            return Vector3{};
    }

    return Vector3{};
}

Vector3 SectorDoorClosedCenter(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render)
{
    const Vector2 normal = NormalizedOrFallback(anchor.normal, Vector2{0.0f, -1.0f});
    return Vector3{
            anchor.midpoint.x + normal.x * render.normalOffset,
            anchor.openBottom + render.height * 0.5f,
            anchor.midpoint.y + normal.y * render.normalOffset};
}

SectorDoorResolvedAnchor ToSectorRuntimeDoorAnchor(const SectorResolvedDoorAnchor& resolved)
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

namespace {

Vector2 StableDoorInteractionPointXZ(
        const SectorDoorResolvedAnchor& anchor,
        Vector2 playerPosition)
{
    const Vector2 a = anchor.endpointA;
    const Vector2 b = anchor.endpointB;
    if (!IsFiniteVector2(a) || !IsFiniteVector2(b)) {
        return IsFiniteVector2(anchor.midpoint) ? anchor.midpoint : Vector2{};
    }

    const Vector2 ab = Subtract(b, a);
    const float lengthSq = Dot(ab, ab);
    if (lengthSq <= DoorDynamicCollisionEpsilon || !std::isfinite(lengthSq)) {
        return IsFiniteVector2(anchor.midpoint) ? anchor.midpoint : a;
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
    if (IsFiniteVector2(anchor.midpoint)
            && Vector2LengthSqr(Subtract(anchor.midpoint, playerPosition)) > DoorDynamicCollisionEpsilon) {
        return anchor.midpoint;
    }
    return closestPoint;
}

} // namespace

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

bool AdvanceSectorDoorMotionSystem(engine::World& world, float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return false;
    }

    bool changed = false;
    world.ForEach<SectorDoor, SectorDoorMotion>(
            [dt, &changed](engine::Entity, SectorDoor& door, SectorDoorMotion& motion) {
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

                const float previousOpenFraction = motion.openFraction;
                if (motion.openFraction < motion.targetOpenFraction) {
                    motion.openFraction = std::min(motion.openFraction + fractionStep, motion.targetOpenFraction);
                } else if (motion.openFraction > motion.targetOpenFraction) {
                    motion.openFraction = std::max(motion.openFraction - fractionStep, motion.targetOpenFraction);
                }
                changed = changed || motion.openFraction != previousOpenFraction;
            });
    return changed;
}

SectorDoorAudioEvent UpdateSectorDoorAudioTransition(
        SectorDoorAudio& audio,
        const SectorDoorMotion& motion)
{
    const bool targetOpen = std::isfinite(motion.targetOpenFraction)
            && Clamp(motion.targetOpenFraction, 0.0f, 1.0f) > 0.5f;
    if (targetOpen == audio.targetWasOpen) {
        return SectorDoorAudioEvent::None;
    }
    audio.targetWasOpen = targetOpen;
    audio.pendingEvent = targetOpen
            ? SectorDoorAudioEvent::Open
            : SectorDoorAudioEvent::Close;
    return audio.pendingEvent;
}

void UpdateSectorDoorAudioSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audioSystem)
{
    world.ForEach<SectorObjectTransform, SectorDoor, SectorDoorMotion, SectorDoorAudio>(
            [&assets, &audioSystem](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorDoor& door,
                    SectorDoorMotion& motion,
                    SectorDoorAudio& audio) {
                if (!door.enabled) {
                    return;
                }
                UpdateSectorDoorAudioTransition(audio, motion);
                if (audio.pendingEvent == SectorDoorAudioEvent::None) {
                    return;
                }

                const engine::SoundHandle sound = audio.pendingEvent == SectorDoorAudioEvent::Open
                        ? audio.openSound
                        : audio.closeSound;
                if (engine::IsNull(sound) || assets.HasFailed(sound)) {
                    audio.pendingEvent = SectorDoorAudioEvent::None;
                    return;
                }
                if (!assets.IsReady(sound)) {
                    return;
                }

                engine::PositionalSoundSettings positional;
                positional.position = transform.position;
                audioSystem.PlaySoundAt(assets, sound, positional);
                audio.pendingEvent = SectorDoorAudioEvent::None;
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
                        SectorDoorClosedCenter(anchor, render),
                        SectorDoorMotionOffset(anchor, motion));

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
                if (!IsFiniteVector2(collider.center)
                        || !IsFiniteVector2(collider.tangent)
                        || !IsFiniteVector2(collider.normal)
                        || !IsFiniteVector2(collider.halfExtents)
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

bool SectorDoorDynamicCollidersAllowPlayerHeight(
        Vector2 positionXZ,
        float feetY,
        float radius,
        float playerHeight,
        const std::vector<SectorDynamicDoorCollider>& colliders)
{
    if (!IsFiniteVector2(positionXZ)
            || !std::isfinite(feetY)
            || !std::isfinite(radius)
            || !std::isfinite(playerHeight)
            || radius <= 0.0f
            || playerHeight <= 0.0f) {
        return false;
    }

    const float playerTop = feetY + playerHeight;
    for (const SectorDynamicDoorCollider& collider : colliders) {
        if (!IsFiniteVector2(collider.center)
                || !IsFiniteVector2(collider.tangent)
                || !IsFiniteVector2(collider.normal)
                || !IsFiniteVector2(collider.halfExtents)
                || !std::isfinite(collider.bottom)
                || !std::isfinite(collider.top)
                || collider.halfExtents.x <= 0.0f
                || collider.halfExtents.y <= 0.0f
                || collider.top <= collider.bottom
                || !CircleOverlapsDoorObb(positionXZ, radius, collider)) {
            continue;
        }
        if (collider.bottom > feetY + DoorDynamicCollisionEpsilon
                && playerTop > collider.bottom + DoorDynamicCollisionEpsilon
                && feetY < collider.top - DoorDynamicCollisionEpsilon) {
            return false;
        }
    }
    return true;
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

    if (!IsFiniteVector2(result.positionXZ) || !IsFiniteVector2(moveState.positionXZ) || colliders.empty()) {
        return result;
    }

    bool hitDynamicDoor = false;
    for (const SectorDynamicDoorCollider& collider : colliders) {
        if (!IsFiniteVector2(collider.center)
                || !IsFiniteVector2(collider.tangent)
                || !IsFiniteVector2(collider.normal)
                || !IsFiniteVector2(collider.halfExtents)
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
            if (!IsFiniteVector2(collider.center)
                    || !IsFiniteVector2(collider.tangent)
                    || !IsFiniteVector2(collider.normal)
                    || !IsFiniteVector2(collider.halfExtents)
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

} // namespace game
