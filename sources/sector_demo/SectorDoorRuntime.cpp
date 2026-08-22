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
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <raymath.h>

namespace game {

namespace {

uint64_t HashDoorShadowBytes(uint64_t hash, const void* data, size_t size)
{
    constexpr uint64_t prime = 1099511628211ull;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= prime;
    }
    return hash;
}

uint64_t HashDoorShadowMatrix(uint64_t hash, const Matrix& matrix)
{
    const std::array<float, 16> values{
            matrix.m0, matrix.m1, matrix.m2, matrix.m3,
            matrix.m4, matrix.m5, matrix.m6, matrix.m7,
            matrix.m8, matrix.m9, matrix.m10, matrix.m11,
            matrix.m12, matrix.m13, matrix.m14, matrix.m15};
    return HashDoorShadowBytes(
            hash, values.data(), values.size() * sizeof(float));
}

uint64_t FingerprintDoorShadowCasters(
        const std::vector<SectorDoorShadowCaster>& proceduralCasters,
        const std::vector<SectorDoorModelShadowCaster>& modelCasters)
{
    uint64_t hash = 1469598103934665603ull;
    const uint64_t proceduralCount =
            static_cast<uint64_t>(proceduralCasters.size());
    const uint64_t modelCount = static_cast<uint64_t>(modelCasters.size());
    hash = HashDoorShadowBytes(
            hash, &proceduralCount, sizeof(proceduralCount));
    for (const SectorDoorShadowCaster& caster : proceduralCasters) {
        hash = HashDoorShadowBytes(
                hash, &caster.placedObjectId, sizeof(caster.placedObjectId));
        hash = HashDoorShadowMatrix(hash, caster.model);
        hash = HashDoorShadowBytes(hash, &caster.width, sizeof(caster.width));
        hash = HashDoorShadowBytes(hash, &caster.height, sizeof(caster.height));
        hash = HashDoorShadowBytes(
                hash, &caster.thickness, sizeof(caster.thickness));
    }
    hash = HashDoorShadowBytes(hash, &modelCount, sizeof(modelCount));
    for (const SectorDoorModelShadowCaster& caster : modelCasters) {
        hash = HashDoorShadowBytes(
                hash, &caster.placedObjectId, sizeof(caster.placedObjectId));
        hash = HashDoorShadowBytes(
                hash, &caster.model.index, sizeof(caster.model.index));
        hash = HashDoorShadowBytes(
                hash, &caster.model.generation, sizeof(caster.model.generation));
        hash = HashDoorShadowMatrix(hash, caster.transform);
    }
    return hash;
}

} // namespace

void RefreshSectorDoorShadowCasterRevision(
        SectorDoorShadowCasterRevisionState& state,
        const std::vector<SectorDoorShadowCaster>& proceduralCasters,
        const std::vector<SectorDoorModelShadowCaster>& modelCasters)
{
    const uint64_t fingerprint =
            FingerprintDoorShadowCasters(proceduralCasters, modelCasters);
    if (!state.fingerprintInitialized || state.fingerprint != fingerprint) {
        state.fingerprint = fingerprint;
        ++state.revision;
        state.fingerprintInitialized = true;
    }
}

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
    const Vector2 resolvedWidthAxis = IsFiniteVector2(render.widthAxis)
                    && Vector2LengthSqr(render.widthAxis) > 0.000001f
            ? render.widthAxis
            : anchor.tangent;
    const Vector2 resolvedThicknessAxis = IsFiniteVector2(render.thicknessAxis)
                    && Vector2LengthSqr(render.thicknessAxis) > 0.000001f
            ? render.thicknessAxis
            : anchor.normal;
    Vector3 tangent = Vector3{resolvedWidthAxis.x, 0.0f, resolvedWidthAxis.y};
    if (Vector3LengthSqr(tangent) <= 0.000001f) {
        tangent = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        tangent = Vector3Normalize(tangent);
    }

    Vector3 normal = Vector3{resolvedThicknessAxis.x, 0.0f, resolvedThicknessAxis.y};
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
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render)
{
    const Vector2 resolvedWidthAxis = IsFiniteVector2(render.widthAxis)
                    && Vector2LengthSqr(render.widthAxis) > 0.000001f
            ? render.widthAxis
            : anchor.tangent;
    const Vector2 resolvedThicknessAxis = IsFiniteVector2(render.thicknessAxis)
                    && Vector2LengthSqr(render.thicknessAxis) > 0.000001f
            ? render.thicknessAxis
            : anchor.normal;
    Vector3 tangent = Vector3{resolvedWidthAxis.x, 0.0f, resolvedWidthAxis.y};
    if (Vector3LengthSqr(tangent) <= 0.000001f) {
        tangent = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        tangent = Vector3Normalize(tangent);
    }

    Vector3 normal = Vector3{resolvedThicknessAxis.x, 0.0f, resolvedThicknessAxis.y};
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
        const SectorDoorRender& render,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback,
        std::vector<Vector3>& outLighting)
{
    outLighting.clear();
    if (meshData.vertices.empty()) {
        return false;
    }

    const Matrix model = BuildSectorDoorSlabModelMatrix(transform, anchor, render);
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

bool BuildSectorDoorReceiverBounds(
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        int sectorId,
        SectorReceiverBounds& outBounds)
{
    if (!object.visible || !door.enabled || !render.visible || sectorId <= 0) {
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

    outBounds = SectorReceiverBounds{sectorId, bounds.min, bounds.max};
    return true;
}

bool AppendSectorDoorReceiverBounds(
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorReceiverBounds>& outBounds)
{
    SectorReceiverBounds receiverBounds;

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
        if (BuildSectorDoorReceiverBounds(
                    transform,
                    object,
                    door,
                    anchor,
                    render,
                    sectorId,
                    receiverBounds)) {
            outBounds.push_back(receiverBounds);
        }
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
            SectorDoorMotion,
            SectorDoorRender>(
            [&world, &outBounds](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorMotion& motion,
                    SectorDoorRender& render) {
                if (motion.motion == SectorDoorMotionType::Swing
                        && world.Has<SectorDoorModelRender>(entity)) {
                    const BoundingBox bounds = world.Get<SectorDoorModelRender>(entity)
                            .receiverBounds;
                    if (object.visible
                            && door.enabled
                            && render.visible
                            && IsFiniteVector3(bounds.min)
                            && IsFiniteVector3(bounds.max)
                            && bounds.max.x >= bounds.min.x
                            && bounds.max.y >= bounds.min.y
                            && bounds.max.z >= bounds.min.z) {
                        outBounds.push_back(SectorReceiverBounds{
                                anchor.frontSectorId, bounds.min, bounds.max});
                        if (anchor.backSectorId > 0
                                && anchor.backSectorId != anchor.frontSectorId) {
                            outBounds.push_back(SectorReceiverBounds{
                                    anchor.backSectorId, bounds.min, bounds.max});
                        }
                        return;
                    }
                }
                AppendSectorDoorReceiverBounds(transform, object, door, anchor, render, outBounds);
            });
}

SectorDoorModelDrawPolicy ResolveSectorDoorModelDrawPolicy(
        const SectorDoorModelRender& model,
        bool leafAssetAvailable,
        bool frameAssetAvailable)
{
    SectorDoorModelDrawPolicy policy;
    const bool validRequest = model.modelVisualRequested
            && model.catalogResolved;
    policy.drawLeaf = validRequest
            && !engine::IsNull(model.leafModel)
            && model.leafReady
            && !model.leafFailed
            && leafAssetAvailable;
    policy.drawFrame = validRequest
            && model.frameDeclared
            && !engine::IsNull(model.frameModel)
            && model.frameReady
            && !model.frameFailed
            && frameAssetAvailable;
    policy.drawProcedural = !policy.drawLeaf;
    return policy;
}

bool ShouldDrawSectorDoorForVisibility(
        const SectorDoorResolvedAnchor& anchor,
        const RuntimePortalVisibilityResult& visibility)
{
    if (!visibility.validStartSector || visibility.fallbackDrawAll) {
        return true;
    }
    const auto visible = [&visibility](int sectorId) {
        return sectorId > 0
                && std::binary_search(
                        visibility.visibleSectorIds.begin(),
                        visibility.visibleSectorIds.end(),
                        sectorId);
    };
    return visible(anchor.frontSectorId) || visible(anchor.backSectorId);
}

int ResolveSectorDoorAdjacentLightingSector(
        const SectorDoorResolvedAnchor& anchor,
        Vector3 leafCenter,
        int containingSectorId)
{
    if (containingSectorId == anchor.frontSectorId
            || containingSectorId == anchor.backSectorId) {
        return containingSectorId;
    }
    const Vector2 displacement{
            leafCenter.x - anchor.midpoint.x,
            leafCenter.z - anchor.midpoint.y};
    const float side = Vector2DotProduct(displacement, anchor.normal);
    if (side > 0.0f && anchor.backSectorId > 0) {
        return anchor.backSectorId;
    }
    if (anchor.frontSectorId > 0) {
        return anchor.frontSectorId;
    }
    return anchor.backSectorId;
}

BoundingBox TransformSectorDoorModelBounds(
        BoundingBox localBounds,
        Matrix transform)
{
    SectorAabb3 bounds = EmptySectorAabb3();
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float y : {localBounds.min.y, localBounds.max.y}) {
            for (float z : {localBounds.min.z, localBounds.max.z}) {
                const Vector3 point = Vector3Transform(Vector3{x, y, z}, transform);
                if (IsFiniteVector3(point)) {
                    ExpandSectorAabb3(bounds, point);
                }
            }
        }
    }
    return BoundingBox{bounds.min, bounds.max};
}

BoundingBox UnionSectorDoorModelBounds(
        BoundingBox first,
        BoundingBox second)
{
    return BoundingBox{
            Vector3{
                    std::min(first.min.x, second.min.x),
                    std::min(first.min.y, second.min.y),
                    std::min(first.min.z, second.min.z)},
            Vector3{
                    std::max(first.max.x, second.max.x),
                    std::max(first.max.y, second.max.y),
                    std::max(first.max.z, second.max.z)}};
}

bool AppendSectorDoorModelShadowCasters(
        engine::Entity entity,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorRender& render,
        const SectorDoorModelRender& model,
        const SectorDoorModelDrawPolicy& policy,
        std::vector<SectorDoorModelShadowCaster>& outCasters)
{
    if (!object.visible || !door.enabled || !render.visible) {
        return false;
    }
    const std::size_t beginIndex = outCasters.size();
    if (policy.drawLeaf) {
        outCasters.push_back(SectorDoorModelShadowCaster{
                door.placedObjectId, entity, model.leafModel, model.leafMatrix});
    }
    if (policy.drawFrame) {
        outCasters.push_back(SectorDoorModelShadowCaster{
                door.placedObjectId, entity, model.frameModel, model.frameMatrix});
    }
    return outCasters.size() > beginIndex;
}

void CollectSectorDoorModelShadowCasters(
        engine::World& world,
        engine::AssetManager& assets,
        std::vector<SectorDoorModelShadowCaster>& outCasters)
{
    world.ForEach<SectorObject, SectorDoor, SectorDoorRender, SectorDoorModelRender>(
            [&assets, &outCasters](
                    engine::Entity entity,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorRender& render,
                    SectorDoorModelRender& model) {
                if (!object.visible || !door.enabled || !render.visible) {
                    return;
                }
                const engine::ModelAsset* leaf = assets.GetModelAsset(model.leafModel);
                const engine::ModelAsset* frame = assets.GetModelAsset(model.frameModel);
                const SectorDoorModelDrawPolicy policy =
                        ResolveSectorDoorModelDrawPolicy(
                                model, leaf != nullptr, frame != nullptr);
                AppendSectorDoorModelShadowCasters(
                        entity, object, door, render, model, policy, outCasters);
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
            BuildSectorDoorSlabModelMatrix(transform, anchor, render),
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

bool IsValidDynamicDoorCollider(const SectorDynamicDoorCollider& collider)
{
    return IsFiniteVector2(collider.center)
            && IsFiniteVector2(collider.tangent)
            && IsFiniteVector2(collider.normal)
            && IsFiniteVector2(collider.halfExtents)
            && std::isfinite(collider.bottom)
            && std::isfinite(collider.top)
            && collider.halfExtents.x > DoorDynamicCollisionEpsilon
            && collider.halfExtents.y > DoorDynamicCollisionEpsilon
            && collider.top > collider.bottom + DoorDynamicCollisionEpsilon;
}

Vector2 DoorColliderWorldAabbHalfExtents(
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 tangent = NormalizedOrFallback(
            collider.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(
            collider.normal, Vector2{0.0f, -1.0f});
    return Vector2{
            std::fabs(tangent.x) * collider.halfExtents.x
                    + std::fabs(normal.x) * collider.halfExtents.y,
            std::fabs(tangent.y) * collider.halfExtents.x
                    + std::fabs(normal.y) * collider.halfExtents.y};
}

bool CircleMayOverlapDoorAabb(
        Vector2 position,
        float radius,
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 half = DoorColliderWorldAabbHalfExtents(collider);
    return position.x + radius >= collider.center.x - half.x
            && position.x - radius <= collider.center.x + half.x
            && position.y + radius >= collider.center.y - half.y
            && position.y - radius <= collider.center.y + half.y;
}

bool SweepMayOverlapDoorAabb(
        Vector2 start,
        Vector2 delta,
        float radius,
        const SectorDynamicDoorCollider& collider)
{
    const Vector2 end = Add(start, delta);
    const Vector2 half = DoorColliderWorldAabbHalfExtents(collider);
    const float sweepMinX = std::min(start.x, end.x) - radius;
    const float sweepMaxX = std::max(start.x, end.x) + radius;
    const float sweepMinY = std::min(start.y, end.y) - radius;
    const float sweepMaxY = std::max(start.y, end.y) + radius;
    return sweepMaxX >= collider.center.x - half.x
            && sweepMinX <= collider.center.x + half.x
            && sweepMaxY >= collider.center.y - half.y
            && sweepMinY <= collider.center.y + half.y;
}

bool SweepCircleAgainstDoorObb(
        Vector2 start,
        Vector2 delta,
        float radius,
        const SectorDynamicDoorCollider& collider,
        float& outTime,
        Vector2& outNormal)
{
    const Vector2 tangent = NormalizedOrFallback(
            collider.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(
            collider.normal, Vector2{0.0f, -1.0f});
    const Vector2 startRelative = Subtract(start, collider.center);
    const Vector2 startLocal{
            Dot(startRelative, tangent),
            Dot(startRelative, normal)};
    const Vector2 deltaLocal{
            Dot(delta, tangent),
            Dot(delta, normal)};
    const float movementLengthSquared = Dot(deltaLocal, deltaLocal);
    if (!(movementLengthSquared
                    > DoorDynamicCollisionEpsilon * DoorDynamicCollisionEpsilon)
            || !std::isfinite(movementLengthSquared)) {
        return false;
    }

    float earliest = std::numeric_limits<float>::infinity();
    Vector2 earliestNormalLocal{};
    const auto recordCandidate = [&](float time, Vector2 normalLocal) {
        if (!std::isfinite(time)
                || !IsFiniteVector2(normalLocal)
                || time < -DoorDynamicCollisionEpsilon
                || time > 1.0f + DoorDynamicCollisionEpsilon
                || Dot(deltaLocal, normalLocal) >= -DoorDynamicCollisionEpsilon) {
            return;
        }
        const float clampedTime = Clamp(time, 0.0f, 1.0f);
        if (clampedTime < earliest) {
            earliest = clampedTime;
            earliestNormalLocal = normalLocal;
        }
    };

    const float faceTangent = collider.halfExtents.x + radius;
    if (std::fabs(deltaLocal.x) > DoorDynamicCollisionEpsilon) {
        for (float sign : {-1.0f, 1.0f}) {
            const Vector2 normalLocal{sign, 0.0f};
            const float time = (sign * faceTangent - startLocal.x) / deltaLocal.x;
            const float normalAtHit = startLocal.y + deltaLocal.y * time;
            if (normalAtHit >= -collider.halfExtents.y - DoorDynamicCollisionEpsilon
                    && normalAtHit <= collider.halfExtents.y + DoorDynamicCollisionEpsilon) {
                recordCandidate(time, normalLocal);
            }
        }
    }

    const float faceNormal = collider.halfExtents.y + radius;
    if (std::fabs(deltaLocal.y) > DoorDynamicCollisionEpsilon) {
        for (float sign : {-1.0f, 1.0f}) {
            const Vector2 normalLocal{0.0f, sign};
            const float time = (sign * faceNormal - startLocal.y) / deltaLocal.y;
            const float tangentAtHit = startLocal.x + deltaLocal.x * time;
            if (tangentAtHit >= -collider.halfExtents.x - DoorDynamicCollisionEpsilon
                    && tangentAtHit <= collider.halfExtents.x + DoorDynamicCollisionEpsilon) {
                recordCandidate(time, normalLocal);
            }
        }
    }

    const float radiusSquared = radius * radius;
    for (float tangentSign : {-1.0f, 1.0f}) {
        for (float normalSign : {-1.0f, 1.0f}) {
            const Vector2 corner{
                    tangentSign * collider.halfExtents.x,
                    normalSign * collider.halfExtents.y};
            const Vector2 fromCorner = Subtract(startLocal, corner);
            const float projected = Dot(fromCorner, deltaLocal);
            const float constant = Dot(fromCorner, fromCorner) - radiusSquared;
            const float discriminant = projected * projected
                    - movementLengthSquared * constant;
            if (!std::isfinite(discriminant)
                    || discriminant < -DoorDynamicCollisionEpsilon) {
                continue;
            }

            const float time = (-projected
                    - std::sqrt(std::max(0.0f, discriminant)))
                    / movementLengthSquared;
            if (!std::isfinite(time)) {
                continue;
            }
            const Vector2 hitPoint = Add(startLocal, Scale(deltaLocal, time));
            const Vector2 fromHitCorner = Subtract(hitPoint, corner);
            if (tangentSign * fromHitCorner.x < -DoorDynamicCollisionEpsilon
                    || normalSign * fromHitCorner.y < -DoorDynamicCollisionEpsilon) {
                continue;
            }
            recordCandidate(
                    time,
                    NormalizedOrFallback(
                            fromHitCorner,
                            Vector2{tangentSign, normalSign}));
        }
    }

    if (!std::isfinite(earliest)) {
        return false;
    }

    outTime = earliest;
    outNormal = Add(
            Scale(tangent, earliestNormalLocal.x),
            Scale(normal, earliestNormalLocal.y));
    outNormal = NormalizedOrFallback(outNormal, Vector2{1.0f, 0.0f});
    return true;
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

} // namespace

Vector3 SectorDoorMotionOffset(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion)
{
    const float openFraction = std::isfinite(motion.openFraction)
            ? Clamp(motion.openFraction, 0.0f, 1.0f)
            : 0.0f;
    const float openDistance = motion.travelAmount > 0.0f && std::isfinite(motion.travelAmount)
            ? motion.travelAmount
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
            anchor.openBottom + render.heightOffsetWorld + render.height * 0.5f,
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

Vector2 RotateDoorAxis(Vector2 axis, float angleRadians)
{
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    return Vector2{
            axis.x * cosine - axis.y * sine,
            axis.x * sine + axis.y * cosine};
}

Matrix BuildDoorBasisMatrix(
        Vector2 widthAxis,
        Vector2 thicknessAxis,
        Vector3 translation,
        float scale)
{
    widthAxis = NormalizedOrFallback(widthAxis, Vector2{1.0f, 0.0f});
    thicknessAxis = NormalizedOrFallback(thicknessAxis, Vector2{0.0f, -1.0f});
    const float safeScale = std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
    return Matrix{
            widthAxis.x * safeScale, 0.0f, thicknessAxis.x * safeScale, translation.x,
            0.0f, safeScale, 0.0f, translation.y,
            widthAxis.y * safeScale, 0.0f, thicknessAxis.y * safeScale, translation.z,
            0.0f, 0.0f, 0.0f, 1.0f};
}

} // namespace

float SectorDoorSwingSign(
        const SectorDoorResolvedAnchor& anchor,
        SectorDoorHinge hinge,
        SectorDoorSwingSide swingSide)
{
    const Vector2 tangent = NormalizedOrFallback(anchor.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(anchor.normal, Vector2{0.0f, -1.0f});
    const Vector2 closedWidthAxis = hinge == SectorDoorHinge::End
            ? Scale(tangent, -1.0f)
            : tangent;
    constexpr float probeAngle = 1.0f * DEG2RAD;
    const Vector2 positiveDisplacement = Subtract(
            RotateDoorAxis(closedWidthAxis, probeAngle),
            closedWidthAxis);
    const float positiveDot = Dot(positiveDisplacement, normal);
    const bool wantsPositiveNormal = swingSide == SectorDoorSwingSide::Back;
    const bool positiveMatches = wantsPositiveNormal
            ? positiveDot > 0.0f
            : positiveDot < 0.0f;
    return positiveMatches ? 1.0f : -1.0f;
}

namespace {

SectorDoorSwingPose BuildSectorDoorSwingPoseAtAngle(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion,
        const SectorDoorRender& render,
        float effectiveScale,
        float angle)
{
    const Vector2 tangent = NormalizedOrFallback(anchor.tangent, Vector2{1.0f, 0.0f});
    const Vector2 normal = NormalizedOrFallback(anchor.normal, Vector2{0.0f, -1.0f});
    const bool endHinge = motion.hinge == SectorDoorHinge::End;
    const Vector2 closedWidthAxis = endHinge ? Scale(tangent, -1.0f) : tangent;
    const Vector2 closedThicknessAxis = endHinge ? Scale(normal, -1.0f) : normal;
    const Vector2 endpoint = endHinge ? anchor.endpointB : anchor.endpointA;
    const Vector2 hingeBase = render.alignLeafToFrame
            ? Subtract(
                    anchor.midpoint,
                    Scale(closedWidthAxis, render.leafHingeToFrameCenter))
            : endpoint;
    const Vector2 hingeXZ = Add(hingeBase, Scale(normal, render.normalOffset));
    const Vector2 widthAxis = NormalizedOrFallback(
            RotateDoorAxis(closedWidthAxis, angle),
            closedWidthAxis);
    const Vector2 thicknessAxis = NormalizedOrFallback(
            RotateDoorAxis(closedThicknessAxis, angle),
            closedThicknessAxis);
    const float leafBottom = anchor.openBottom + render.heightOffsetWorld
            + (render.alignLeafToFrame ? render.leafBottomOffset : 0.0f);
    const Vector3 hingePosition{hingeXZ.x, leafBottom, hingeXZ.y};
    const Vector3 center{
            hingeXZ.x + widthAxis.x * render.width * 0.5f,
            leafBottom + render.height * 0.5f,
            hingeXZ.y + widthAxis.y * render.width * 0.5f};
    const Vector3 framePosition{
            anchor.midpoint.x + normal.x * render.normalOffset,
            anchor.openBottom + render.heightOffsetWorld,
            anchor.midpoint.y + normal.y * render.normalOffset};

    SectorDoorSwingPose pose;
    pose.hingePosition = hingePosition;
    pose.center = center;
    pose.widthAxis = widthAxis;
    pose.thicknessAxis = thicknessAxis;
    pose.bottom = leafBottom;
    pose.top = leafBottom + render.height;
    pose.angleRadians = angle;
    pose.leafMatrix = BuildDoorBasisMatrix(
            widthAxis, thicknessAxis, hingePosition, effectiveScale);
    pose.frameMatrix = BuildDoorBasisMatrix(
            tangent, normal, framePosition, effectiveScale);
    return pose;
}

} // namespace

SectorDoorSwingPose BuildSectorDoorSwingPose(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion,
        const SectorDoorRender& render,
        float effectiveScale,
        float openFraction)
{
    const float clampedFraction = std::isfinite(openFraction)
            ? Clamp(openFraction, 0.0f, 1.0f)
            : 0.0f;
    const float travel = std::isfinite(motion.travelAmount) && motion.travelAmount > 0.0f
            ? motion.travelAmount
            : 0.0f;
    const float angle = SmootherStep01(clampedFraction)
            * travel
            * SectorDoorSwingSign(anchor, motion.hinge, motion.swingSide);
    return BuildSectorDoorSwingPoseAtAngle(
            anchor, motion, render, effectiveScale, angle);
}

SectorDoorCollider BuildSectorDoorSwingCollider(
        const SectorDoorSwingPose& pose,
        float width,
        float thickness,
        bool enabled)
{
    const bool valid = enabled
            && std::isfinite(width) && width > 0.0f
            && std::isfinite(thickness) && thickness > 0.0f
            && std::isfinite(pose.bottom) && std::isfinite(pose.top)
            && pose.top > pose.bottom;
    return SectorDoorCollider{
            Vector2{pose.center.x, pose.center.z},
            pose.widthAxis,
            pose.thicknessAxis,
            Vector2{valid ? width * 0.5f : 0.0f, valid ? thickness * 0.5f : 0.0f},
            pose.bottom,
            pose.top,
            valid};
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

bool AdvanceSectorDoorMotionSystem(
        engine::World& world,
        float dt,
        const SectorDoorPlayerObstacle* playerObstacle)
{
    return AdvanceSectorDoorMotionSystem(
            world,
            dt,
            playerObstacle,
            playerObstacle != nullptr ? 1u : 0u);
}

bool AdvanceSectorDoorMotionSystem(
        engine::World& world,
        float dt,
        const SectorDoorPlayerObstacle* obstacles,
        size_t obstacleCount)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return false;
    }

    bool changed = false;
    world.ForEach<SectorDoor, SectorDoorMotion>(
            [dt, obstacles, obstacleCount, &world, &changed](
                    engine::Entity entity,
                    SectorDoor& door,
                    SectorDoorMotion& motion) {
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
                const float effectiveTarget = world.Has<SectorDoorOpenControl>(entity)
                                && world.Get<SectorDoorOpenControl>(entity).navigationHolderCount > 0
                        ? 1.0f
                        : motion.targetOpenFraction;
                if (motion.travelSpeed <= 0.0f || !std::isfinite(motion.travelSpeed)) {
                    return;
                }

                const float distance = motion.travelAmount > 0.0f && std::isfinite(motion.travelAmount)
                        ? motion.travelAmount
                        : 1.0f;
                const float fractionStep = (motion.travelSpeed * dt) / distance;
                if (!std::isfinite(fractionStep) || fractionStep <= 0.0f) {
                    return;
                }

                const float previousOpenFraction = motion.openFraction;
                float candidateOpenFraction = motion.openFraction;
                if (candidateOpenFraction < effectiveTarget) {
                    candidateOpenFraction = std::min(
                            candidateOpenFraction + fractionStep,
                            effectiveTarget);
                } else if (candidateOpenFraction > effectiveTarget) {
                    candidateOpenFraction = std::max(
                            candidateOpenFraction - fractionStep,
                            effectiveTarget);
                }

                const bool closingSwing = motion.motion == SectorDoorMotionType::Swing
                        && candidateOpenFraction < previousOpenFraction;
                bool obstructed = false;
                if (closingSwing
                        && obstacles != nullptr
                        && obstacleCount > 0
                        && world.Has<SectorDoorResolvedAnchor>(entity)
                        && world.Has<SectorDoorRender>(entity)
                        ) {
                    const SectorDoorResolvedAnchor& anchor =
                            world.Get<SectorDoorResolvedAnchor>(entity);
                    const SectorDoorRender& render = world.Get<SectorDoorRender>(entity);
                    const SectorDoorSwingPose previousPose = BuildSectorDoorSwingPose(
                            anchor, motion, render, 1.0f, previousOpenFraction);
                    const SectorDoorSwingPose candidatePose = BuildSectorDoorSwingPose(
                            anchor, motion, render, 1.0f, candidateOpenFraction);
                    constexpr float maximumSampleStep = 5.0f * DEG2RAD;
                    constexpr int maximumSegments = 34;
                    const float sweptAngle = std::fabs(
                            candidatePose.angleRadians - previousPose.angleRadians);
                    const int segmentCount = std::clamp(
                            static_cast<int>(std::ceil(sweptAngle / maximumSampleStep)),
                            1,
                            maximumSegments);
                    for (int sampleIndex = 0; sampleIndex <= segmentCount; ++sampleIndex) {
                        const float sampleT = static_cast<float>(sampleIndex)
                                / static_cast<float>(segmentCount);
                        const float sampleAngle = previousPose.angleRadians
                                + (candidatePose.angleRadians
                                        - previousPose.angleRadians) * sampleT;
                        const SectorDoorSwingPose samplePose =
                                BuildSectorDoorSwingPoseAtAngle(
                                        anchor,
                                        motion,
                                        render,
                                        1.0f,
                                        sampleAngle);
                        const SectorDoorCollider sampleCollider = BuildSectorDoorSwingCollider(
                                samplePose, render.width, render.thickness, door.enabled);
                        const SectorDynamicDoorCollider dynamicCollider{
                                door.placedObjectId,
                                entity,
                                sampleCollider.center,
                                sampleCollider.tangent,
                                sampleCollider.normal,
                                sampleCollider.halfExtents,
                                sampleCollider.bottom,
                                sampleCollider.top};
                        for (size_t obstacleIndex = 0;
                                sampleCollider.enabled && obstacleIndex < obstacleCount;
                                ++obstacleIndex) {
                            const SectorDoorPlayerObstacle& obstacle = obstacles[obstacleIndex];
                            if (!IsFiniteVector3(obstacle.feetPosition)
                                    || !std::isfinite(obstacle.radius)
                                    || obstacle.radius <= 0.0f
                                    || !std::isfinite(obstacle.height)
                                    || obstacle.height <= 0.0f) {
                                continue;
                            }
                            const float obstacleBottom = obstacle.feetPosition.y;
                            const float obstacleTop = obstacleBottom + obstacle.height;
                            if (obstacleTop <= samplePose.bottom + DoorDynamicCollisionEpsilon
                                    || obstacleBottom >= samplePose.top - DoorDynamicCollisionEpsilon) {
                                continue;
                            }
                            if (CircleOverlapsDoorObb(
                                        Vector2{obstacle.feetPosition.x,
                                                obstacle.feetPosition.z},
                                        obstacle.radius,
                                        dynamicCollider)) {
                                obstructed = true;
                                break;
                            }
                        }
                        if (obstructed) break;
                    }
                }

                const bool closingSlide = motion.motion != SectorDoorMotionType::Swing
                        && candidateOpenFraction < previousOpenFraction;
                if (!obstructed && closingSlide && obstacles != nullptr
                        && obstacleCount > 0
                        && world.Has<SectorDoorResolvedAnchor>(entity)
                        && world.Has<SectorDoorRender>(entity)
                        && world.Has<SectorDoorCollider>(entity)) {
                    const SectorDoorResolvedAnchor& anchor =
                            world.Get<SectorDoorResolvedAnchor>(entity);
                    const SectorDoorCollider& current = world.Get<SectorDoorCollider>(entity);
                    SectorDoorMotion candidateMotion = motion;
                    candidateMotion.openFraction = candidateOpenFraction;
                    const Vector3 previousOffset = SectorDoorMotionOffset(anchor, motion);
                    const Vector3 candidateOffset = SectorDoorMotionOffset(anchor, candidateMotion);
                    const Vector2 delta{candidateOffset.x - previousOffset.x,
                            candidateOffset.z - previousOffset.z};
                    SectorDynamicDoorCollider swept{
                            door.placedObjectId,
                            entity,
                            Vector2{current.center.x + delta.x * 0.5f,
                                    current.center.y + delta.y * 0.5f},
                            current.tangent,
                            current.normal,
                            Vector2{
                                    current.halfExtents.x
                                            + std::fabs(Vector2DotProduct(delta, current.tangent)) * 0.5f,
                                    current.halfExtents.y
                                            + std::fabs(Vector2DotProduct(delta, current.normal)) * 0.5f},
                            std::min(current.bottom,
                                    current.bottom + candidateOffset.y - previousOffset.y),
                            std::max(current.top,
                                    current.top + candidateOffset.y - previousOffset.y)};
                    for (size_t obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex) {
                        const SectorDoorPlayerObstacle& obstacle = obstacles[obstacleIndex];
                        if (!IsFiniteVector3(obstacle.feetPosition)
                                || obstacle.radius <= 0.0f || obstacle.height <= 0.0f) continue;
                        const float obstacleTop = obstacle.feetPosition.y + obstacle.height;
                        if (obstacleTop <= swept.bottom + DoorDynamicCollisionEpsilon
                                || obstacle.feetPosition.y >= swept.top - DoorDynamicCollisionEpsilon) continue;
                        if (CircleOverlapsDoorObb(
                                    {obstacle.feetPosition.x, obstacle.feetPosition.z},
                                    obstacle.radius,
                                    swept)) {
                            obstructed = true;
                            break;
                        }
                    }
                }

                if (obstructed) {
                    const bool targetChanged = motion.targetOpenFraction != 1.0f;
                    motion.targetOpenFraction = 1.0f;
                    changed = changed || targetChanged;
                    return;
                }

                motion.openFraction = candidateOpenFraction;
                changed = changed || motion.openFraction != previousOpenFraction;
            });
    return changed;
}

bool RefreshSectorDoorModelReadinessSystem(
        engine::World& world,
        engine::AssetManager& assets)
{
    bool changed = false;
    world.ForEach<SectorDoorModelRender>(
            [&assets, &changed](engine::Entity, SectorDoorModelRender& model) {
                const bool leafReady = !engine::IsNull(model.leafModel)
                        && assets.IsReady(model.leafModel);
                const bool leafFailed = !engine::IsNull(model.leafModel)
                        && assets.HasFailed(model.leafModel);
                const bool frameReady = model.frameDeclared
                        && !engine::IsNull(model.frameModel)
                        && assets.IsReady(model.frameModel);
                const bool frameFailed = model.frameDeclared
                        && model.catalogResolved
                        && model.fallbackReason
                                != SectorDoorModelFallbackReason::AssetScopeUnavailable
                        && (engine::IsNull(model.frameModel)
                                || assets.HasFailed(model.frameModel));
                const engine::ModelAsset* leafAsset = leafReady
                        ? assets.GetModelAsset(model.leafModel) : nullptr;
                const engine::ModelAsset* frameAsset = frameReady
                        ? assets.GetModelAsset(model.frameModel) : nullptr;
                const bool leafBoundsReady = leafAsset != nullptr
                        && leafAsset->hasLocalBounds;
                const bool frameBoundsReady = frameAsset != nullptr
                        && frameAsset->hasLocalBounds;
                changed = changed
                        || leafReady != model.leafReady
                        || leafFailed != model.leafFailed
                        || frameReady != model.frameReady
                        || frameFailed != model.frameFailed
                        || leafBoundsReady != model.leafBoundsReady
                        || frameBoundsReady != model.frameBoundsReady;
                model.leafReady = leafReady;
                model.leafFailed = leafFailed;
                model.frameReady = frameReady;
                model.frameFailed = frameFailed;
                model.leafBoundsReady = leafBoundsReady;
                model.frameBoundsReady = frameBoundsReady;
                if (leafBoundsReady) {
                    model.leafLocalBounds = leafAsset->localBounds;
                }
                if (frameBoundsReady) {
                    model.frameLocalBounds = frameAsset->localBounds;
                }
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

bool IsSectorDoorAudioEventReady(
        SectorDoorAudioEvent event,
        const SectorDoorMotion& motion)
{
    if (event == SectorDoorAudioEvent::None) {
        return false;
    }
    if (event != SectorDoorAudioEvent::Close
            || motion.motion != SectorDoorMotionType::Swing) {
        return true;
    }
    return std::isfinite(motion.openFraction)
            && Clamp(motion.openFraction, 0.0f, 1.0f) == 0.0f;
}

void UpdateSectorDoorAudioSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audioSystem)
{
    world.ForEach<SectorObjectTransform, SectorDoor, SectorDoorMotion, SectorDoorAudio>(
            [&world, &assets, &audioSystem](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorDoor& door,
                    SectorDoorMotion& motion,
                    SectorDoorAudio& audio) {
                if (!door.enabled) {
                    return;
                }
                SectorDoorMotion effectiveMotion = motion;
                if (world.Has<SectorDoorOpenControl>(entity)
                        && world.Get<SectorDoorOpenControl>(entity)
                                .navigationHolderCount > 0) {
                    effectiveMotion.targetOpenFraction = 1.0f;
                }
                UpdateSectorDoorAudioTransition(audio, effectiveMotion);
                if (audio.pendingEvent == SectorDoorAudioEvent::None) {
                    return;
                }
                if (!IsSectorDoorAudioEventReady(audio.pendingEvent, motion)) {
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
            [&world](engine::Entity entity,
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
                if (motion.motion == SectorDoorMotionType::Swing) {
                    const float effectiveScale = world.Has<SectorDoorModelRender>(entity)
                            ? world.Get<SectorDoorModelRender>(entity).effectiveScale
                            : 1.0f;
                    const SectorDoorSwingPose pose = BuildSectorDoorSwingPose(
                            anchor,
                            motion,
                            render,
                            effectiveScale,
                            motion.openFraction);
                    transform.position = pose.center;
                    transform.yawRadians = std::atan2(
                            pose.widthAxis.y, pose.widthAxis.x);
                    render.widthAxis = pose.widthAxis;
                    render.thicknessAxis = pose.thicknessAxis;
                    collider = BuildSectorDoorSwingCollider(
                            pose,
                            render.width,
                            render.thickness,
                            door.enabled && validShape);

                    if (world.Has<SectorDoorModelRender>(entity)) {
                        SectorDoorModelRender& model =
                                world.Get<SectorDoorModelRender>(entity);
                        model.leafMatrix = pose.leafMatrix;
                        model.frameMatrix = pose.frameMatrix;

                        const float leafHalfWidth = validShape ? render.width * 0.5f : 0.0f;
                        const float leafHalfThickness = validShape ? render.thickness * 0.5f : 0.0f;
                        const float extentX = std::fabs(pose.widthAxis.x) * leafHalfWidth
                                + std::fabs(pose.thicknessAxis.x) * leafHalfThickness;
                        const float extentZ = std::fabs(pose.widthAxis.y) * leafHalfWidth
                                + std::fabs(pose.thicknessAxis.y) * leafHalfThickness;
                        Vector3 boundsMin{
                                pose.center.x - extentX,
                                pose.bottom,
                                pose.center.z - extentZ};
                        Vector3 boundsMax{
                                pose.center.x + extentX,
                                pose.top,
                                pose.center.z + extentZ};
                        if (model.frameDeclared
                                && std::isfinite(model.frameOuterWidth)
                                && model.frameOuterWidth > 0.0f
                                && std::isfinite(model.frameOuterHeight)
                                && model.frameOuterHeight > 0.0f
                                && std::isfinite(model.effectiveScale)
                                && model.effectiveScale > 0.0f) {
                            const float frameHalfWidth = model.frameOuterWidth
                                    * model.effectiveScale * 0.5f;
                            const float frameHalfDepth = render.thickness * 0.5f;
                            const Vector3 frameCenter = Vector3Transform(
                                    Vector3{
                                            0.0f,
                                            model.frameOuterHeight * 0.5f,
                                            0.0f},
                                    pose.frameMatrix);
                            const float frameExtentX = std::fabs(tangent.x) * frameHalfWidth
                                    + std::fabs(normal.x) * frameHalfDepth;
                            const float frameExtentZ = std::fabs(tangent.y) * frameHalfWidth
                                    + std::fabs(normal.y) * frameHalfDepth;
                            boundsMin.x = std::min(boundsMin.x, frameCenter.x - frameExtentX);
                            const float frameBottom =
                                    anchor.openBottom + render.heightOffsetWorld;
                            boundsMin.y = std::min(boundsMin.y, frameBottom);
                            boundsMin.z = std::min(boundsMin.z, frameCenter.z - frameExtentZ);
                            boundsMax.x = std::max(boundsMax.x, frameCenter.x + frameExtentX);
                            boundsMax.y = std::max(
                                    boundsMax.y,
                                    frameBottom
                                            + model.frameOuterHeight * model.effectiveScale);
                            boundsMax.z = std::max(boundsMax.z, frameCenter.z + frameExtentZ);
                        }
                        model.analyticReceiverBounds = BoundingBox{boundsMin, boundsMax};
                        model.receiverBounds = model.analyticReceiverBounds;
                        if (model.leafBoundsReady) {
                            model.receiverBounds = UnionSectorDoorModelBounds(
                                    model.receiverBounds,
                                    TransformSectorDoorModelBounds(
                                            model.leafLocalBounds,
                                            model.leafMatrix));
                        }
                        if (model.frameBoundsReady) {
                            model.receiverBounds = UnionSectorDoorModelBounds(
                                    model.receiverBounds,
                                    TransformSectorDoorModelBounds(
                                            model.frameLocalBounds,
                                            model.frameMatrix));
                        }
                    }
                } else {
                    const Vector3 center = Vector3Add(
                            SectorDoorClosedCenter(anchor, render),
                            SectorDoorMotionOffset(anchor, motion));
                    transform.position = center;
                    transform.yawRadians = std::atan2(tangent.y, tangent.x);
                    render.widthAxis = tangent;
                    render.thicknessAxis = normal;
                    collider.center = Vector2{center.x, center.z};
                    collider.tangent = tangent;
                    collider.normal = normal;
                    collider.halfExtents = Vector2{
                            validShape ? render.width * 0.5f : 0.0f,
                            validShape ? render.thickness * 0.5f : 0.0f};
                    collider.bottom = center.y - (validShape ? render.height * 0.5f : 0.0f);
                    collider.top = center.y + (validShape ? render.height * 0.5f : 0.0f);
                    collider.enabled = door.enabled && validShape;
                }

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

bool SectorDoorTraversalIsClear(
        int placedObjectId,
        Vector3 staging,
        Vector3 landing,
        float radius,
        float agentHeight,
        const std::vector<SectorDynamicDoorCollider>& colliders)
{
    if (placedObjectId <= 0 || !IsFiniteVector3(staging)
            || !IsFiniteVector3(landing) || radius <= 0.0f
            || agentHeight <= 0.0f) return false;
    for (const SectorDynamicDoorCollider& collider : colliders) {
        if (collider.placedObjectId != placedObjectId) continue;
        const float traversalBottom = std::min(staging.y, landing.y);
        const float traversalTop = std::max(staging.y, landing.y) + agentHeight;
        if (traversalTop <= collider.bottom + DoorDynamicCollisionEpsilon
                || traversalBottom >= collider.top - DoorDynamicCollisionEpsilon) continue;
        const Vector2 start{staging.x, staging.z};
        const Vector2 delta{landing.x - staging.x, landing.z - staging.z};
        float hitTime = 0.0f;
        Vector2 hitNormal{};
        if (CircleOverlapsDoorObb(start, radius, collider)
                || SweepCircleAgainstDoorObb(
                        start, delta, radius, collider, hitTime, hitNormal)) {
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

    Vector2 position = moveState.positionXZ;
    Vector2 remaining = Subtract(staticResult.positionXZ, moveState.positionXZ);
    bool hitDynamicDoor = false;
    const auto resolvePenetrations = [&]() {
        for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
            bool changed = false;
            for (const SectorDynamicDoorCollider& collider : colliders) {
                if (!IsValidDynamicDoorCollider(collider)
                        || !PlayerVerticalIntervalOverlapsDoor(
                                moveState, config, collider)
                        || !CircleMayOverlapDoorAabb(
                                position, config.radius, collider)) {
                    continue;
                }
                if (ResolveCircleAgainstDoorObb(
                            position, config.radius, collider)) {
                    result.hitWall = true;
                    hitDynamicDoor = true;
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
    };

    // Recover first so a frame that begins slightly embedded cannot turn that
    // overlap into a persistent zero-time sweep contact.
    resolvePenetrations();
    for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
        float earliest = std::numeric_limits<float>::infinity();
        Vector2 hitNormal{};
        for (const SectorDynamicDoorCollider& collider : colliders) {
            if (!IsValidDynamicDoorCollider(collider)
                    || !PlayerVerticalIntervalOverlapsDoor(
                            moveState, config, collider)
                    || !SweepMayOverlapDoorAabb(
                            position, remaining, config.radius, collider)) {
                continue;
            }
            float hitTime = 0.0f;
            Vector2 normal{};
            if (SweepCircleAgainstDoorObb(
                        position,
                        remaining,
                        config.radius,
                        collider,
                        hitTime,
                        normal)
                    && hitTime < earliest) {
                earliest = hitTime;
                hitNormal = normal;
            }
        }

        if (!std::isfinite(earliest)) {
            position = Add(position, remaining);
            remaining = Vector2{};
            break;
        }

        const float approachDistance = -Dot(remaining, hitNormal);
        const float skinTime = approachDistance > DoorDynamicCollisionEpsilon
                ? DoorDynamicCollisionEpsilon / approachDistance
                : 0.0f;
        const float safeTime = std::max(0.0f, earliest - skinTime);
        position = Add(position, Scale(remaining, safeTime));
        remaining = Scale(remaining, 1.0f - earliest);
        const float intoDoor = Dot(remaining, hitNormal);
        if (intoDoor < 0.0f) {
            remaining = Subtract(remaining, Scale(hitNormal, intoDoor));
        }
        result.hitWall = true;
        hitDynamicDoor = true;
        if (Dot(remaining, remaining)
                <= DoorDynamicCollisionEpsilon * DoorDynamicCollisionEpsilon) {
            remaining = Vector2{};
            break;
        }
    }

    resolvePenetrations();
    result.positionXZ = position;

    if (hitDynamicDoor && result.currentSectorId != moveState.currentSectorId) {
        result.currentSectorId = moveState.currentSectorId;
    }
    return result;
}

} // namespace game
