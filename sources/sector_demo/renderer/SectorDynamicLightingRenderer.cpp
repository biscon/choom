#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorDynamicModelShadowCasters.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorStaticModelShadow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

namespace game {

namespace {

bool ShadowMatrixMatches(
        const SectorPreviewDynamicSpotLightShadowMatrix& left,
        const SectorPreviewDynamicSpotLightShadowMatrix& right)
{
    return left.lightId == right.lightId
            && left.shadowSlot == right.shadowSlot
            && left.kind == right.kind
            && left.cubeFace == right.cubeFace
            && std::memcmp(&left.lightPosition,
                    &right.lightPosition, sizeof(Vector3)) == 0
            && left.lightRadius == right.lightRadius
            && std::memcmp(&left.lightViewProjection,
                    &right.lightViewProjection, sizeof(Matrix)) == 0;
}

bool DynamicPortalBlockerMatches(
        const RuntimePortalDynamicBlocker& left,
        const RuntimePortalDynamicBlocker& right)
{
    return left.lineDefId == right.lineDefId
            && left.sideDefId == right.sideDefId
            && left.fromSectorId == right.fromSectorId
            && left.toSectorId == right.toSectorId
            && left.blocksPortal == right.blocksPortal;
}

bool DynamicPortalBlockersMatch(
        const std::vector<RuntimePortalDynamicBlocker>& cached,
        const std::vector<RuntimePortalDynamicBlocker>* current)
{
    const std::size_t currentSize = current != nullptr ? current->size() : 0;
    if (cached.size() != currentSize) return false;
    for (const RuntimePortalDynamicBlocker& cachedBlocker : cached) {
        const auto found = std::find_if(
                current->begin(),
                current->end(),
                [&cachedBlocker](const RuntimePortalDynamicBlocker& currentBlocker) {
                    return DynamicPortalBlockerMatches(
                            cachedBlocker,
                            currentBlocker);
                });
        if (found == current->end()) return false;
    }
    return true;
}

bool LightingStartSectorsMatch(
        const std::vector<int>& cached,
        int lightingStartSectorId)
{
    return lightingStartSectorId > 0
            ? cached.size() == 1 && cached[0] == lightingStartSectorId
            : cached.empty();
}

const SectorReceiverBounds* FindSectorReceiverBounds(
        const std::vector<SectorReceiverBounds>* bounds,
        int sectorId)
{
    if (bounds == nullptr || sectorId <= 0) return nullptr;
    for (const SectorReceiverBounds& candidate : *bounds) {
        if (candidate.sectorId == sectorId) return &candidate;
    }
    return nullptr;
}

bool SphereIntersectsAabb(
        Vector3 center,
        float radius,
        const SectorAabb3& bounds)
{
    if (!IsValidSectorAabb3(bounds)
            || !IsFiniteVector3(center)
            || !std::isfinite(radius)
            || radius <= 0.0f) {
        return true;
    }
    const Vector3 closest = ClosestPointOnSectorAabb3(bounds, center);
    return Vector3DistanceSqr(center, closest) <= radius * radius;
}

SectorAabb3 ToSectorAabb3(const SectorReceiverBounds& bounds)
{
    return SectorAabb3{bounds.min, bounds.max};
}

SectorAabb3 ToSectorAabb3(BoundingBox bounds)
{
    return SectorAabb3{bounds.min, bounds.max};
}

SectorAabb3 DoorCasterBounds(const SectorDoorShadowCaster& caster)
{
    const float radius = 0.5f * std::sqrt(
            caster.width * caster.width
            + caster.height * caster.height
            + caster.thickness * caster.thickness);
    return SectorAabb3{
            Vector3{
                    caster.position.x - radius,
                    caster.position.y - radius,
                    caster.position.z - radius},
            Vector3{
                    caster.position.x + radius,
                    caster.position.y + radius,
                    caster.position.z + radius}};
}

bool ShadowLightIntersectsBounds(
        const SectorPreviewDynamicPointLightUniform& light,
        const SectorPreviewDynamicSpotLightShadowMatrix& matrix,
        const SectorAabb3& bounds)
{
    const float influenceRadius = light.kind == SectorPreviewDynamicLightKind::Rect
            ? light.radius + std::sqrt(light.innerConeCos * light.innerConeCos
                    + light.outerConeCos * light.outerConeCos)
            : light.radius;
    if (!SphereIntersectsAabb(light.position, influenceRadius, bounds)) {
        return false;
    }
    if (light.kind == SectorPreviewDynamicLightKind::Point
            || light.kind == SectorPreviewDynamicLightKind::Rect) {
        constexpr std::array<Vector3, DynamicPointLightShadowFaceCount>
                PointFaceDirections = {{
                        {1.0f, 0.0f, 0.0f},
                        {-1.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f},
                        {0.0f, -1.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f},
                        {0.0f, 0.0f, -1.0f}}};
        const int faceCount = SectorDynamicShadowFaceCount(light.kind);
        if (matrix.cubeFace < 0 || matrix.cubeFace >= faceCount) {
            return true;
        }
        Vector3 faceDirection = PointFaceDirections[
                static_cast<std::size_t>(matrix.cubeFace)];
        if (light.kind == SectorPreviewDynamicLightKind::Rect) {
            const Vector3 forward = Vector3LengthSqr(light.direction)
                            > 0.00000001f
                    ? Vector3Normalize(light.direction)
                    : Vector3{0.0f, -1.0f, 0.0f};
            const Vector3 right = Vector3LengthSqr(light.rectRight)
                            > 0.00000001f
                    ? Vector3Normalize(light.rectRight)
                    : Vector3{1.0f, 0.0f, 0.0f};
            const Vector3 emitterUp = Vector3Normalize(
                    Vector3CrossProduct(right, forward));
            const Vector3 cubeUp = Vector3Negate(emitterUp);
            const std::array<Vector3, DynamicRectLightShadowFaceCount>
                    rectFaceDirections = {{
                            right,
                            Vector3Negate(right),
                            cubeUp,
                            emitterUp,
                            forward}};
            faceDirection = rectFaceDirections[
                    static_cast<std::size_t>(matrix.cubeFace)];
        }
        const Vector3 center = SectorAabb3Center(bounds);
        const Vector3 halfExtents = Vector3Scale(
                SectorAabb3Extents(bounds), 0.5f);
        const float boundsRadius = Vector3Length(halfExtents);
        const Vector3 fromLight = Vector3Subtract(center, light.position);
        const float distanceSquared = Vector3LengthSqr(fromLight);
        if (distanceSquared <= boundsRadius * boundsRadius) return true;
        const float axialDistance = Vector3DotProduct(
                fromLight,
                faceDirection);
        if (axialDistance < -boundsRadius
                || axialDistance > matrix.lightRadius + boundsRadius) {
            return false;
        }
        const float radialDistance = std::sqrt(std::max(
                distanceSquared - axialDistance * axialDistance, 0.0f));
        constexpr float CubeFaceCornerTangent = 1.41421356237f;
        constexpr float CubeFaceCornerCosine = 0.57735026919f;
        const float coneRadius = std::max(axialDistance, 0.0f)
                * CubeFaceCornerTangent;
        return radialDistance <= coneRadius
                + boundsRadius / CubeFaceCornerCosine;
    }

    if (light.kind == SectorPreviewDynamicLightKind::Rect) return true;
    const Vector3 center = SectorAabb3Center(bounds);
    const Vector3 halfExtents = Vector3Scale(SectorAabb3Extents(bounds), 0.5f);
    const float boundsRadius = Vector3Length(halfExtents);
    const Vector3 fromLight = Vector3Subtract(center, light.position);
    const float distanceSquared = Vector3LengthSqr(fromLight);
    if (distanceSquared <= boundsRadius * boundsRadius) return true;
    const Vector3 direction = Vector3Normalize(light.direction);
    const float axialDistance = Vector3DotProduct(fromLight, direction);
    if (axialDistance < -boundsRadius
            || axialDistance > light.radius + boundsRadius) {
        return false;
    }
    const float radialDistance = std::sqrt(std::max(
            distanceSquared - axialDistance * axialDistance, 0.0f));
    const float halfAngle = std::acos(std::clamp(
            light.outerConeCos, -0.999f, 0.999f));
    const float clampedHalfAngle = std::min(halfAngle, 1.553343f);
    const float coneRadius = std::max(axialDistance, 0.0f)
            * std::tan(clampedHalfAngle);
    const float sphereAllowance = boundsRadius
            / std::max(std::cos(clampedHalfAngle), 0.017452f);
    return radialDistance <= coneRadius + sphereAllowance;
}

bool DynamicLightIntersectsBounds(
        const SectorPreviewDynamicPointLightUniform& light,
        const SectorAabb3& bounds)
{
    const float influenceRadius = light.kind == SectorPreviewDynamicLightKind::Rect
            ? light.radius + std::sqrt(light.innerConeCos * light.innerConeCos
                    + light.outerConeCos * light.outerConeCos)
            : light.radius;
    if (!SphereIntersectsAabb(light.position, influenceRadius, bounds)) {
        return false;
    }
    if (light.kind == SectorPreviewDynamicLightKind::Point) {
        return true;
    }

    if (light.kind == SectorPreviewDynamicLightKind::Rect) return true;
    const Vector3 center = SectorAabb3Center(bounds);
    const Vector3 halfExtents = Vector3Scale(SectorAabb3Extents(bounds), 0.5f);
    const float boundsRadius = Vector3Length(halfExtents);
    const Vector3 fromLight = Vector3Subtract(center, light.position);
    const float distanceSquared = Vector3LengthSqr(fromLight);
    if (distanceSquared <= boundsRadius * boundsRadius) return true;
    const Vector3 direction = Vector3Normalize(light.direction);
    const float axialDistance = Vector3DotProduct(fromLight, direction);
    if (axialDistance < -boundsRadius
            || axialDistance > light.radius + boundsRadius) {
        return false;
    }
    const float radialDistance = std::sqrt(std::max(
            distanceSquared - axialDistance * axialDistance, 0.0f));
    const float halfAngle = std::acos(std::clamp(
            light.outerConeCos, -0.999f, 0.999f));
    const float clampedHalfAngle = std::min(halfAngle, 1.553343f);
    const float coneRadius = std::max(axialDistance, 0.0f)
            * std::tan(clampedHalfAngle);
    const float sphereAllowance = boundsRadius
            / std::max(std::cos(clampedHalfAngle), 0.017452f);
    return radialDistance <= coneRadius + sphereAllowance;
}

const char* SectorSpotLightShadowVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 lightViewProjection;
uniform mat4 matModel;
uniform int useSkinning;
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec2 fragTexCoord;

void main()
{
    vec4 localPosition = vec4(vertexPosition, 1.0);
    if (useSkinning != 0) {
        int bone0 = int(vertexBoneIndices.x);
        int bone1 = int(vertexBoneIndices.y);
        int bone2 = int(vertexBoneIndices.z);
        int bone3 = int(vertexBoneIndices.w);
        localPosition = vertexBoneWeights.x * (boneMatrices[bone0] * localPosition)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localPosition)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localPosition)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localPosition);
    }
    fragTexCoord = vertexTexCoord;
    gl_Position = lightViewProjection * matModel * localPosition;
}
)";

const char* SectorSpotLightShadowOpaqueFs = R"(
#version 330
void main() {}
)";

const char* SectorSpotLightShadowCutoutFs = R"(
#version 330
in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform int alphaTest;
uniform float alphaCutoff;

void main()
{
    if (alphaTest != 0 && texture(texture0, fragTexCoord).a < alphaCutoff) {
        discard;
    }
}
)";

RenderTexture2D LoadDepthOnlyRenderTexture(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;

    if (target.id <= 0) {
        return target;
    }

    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    if (!rlFramebufferComplete(target.id)) {
        rlDisableFramebuffer();
        if (target.depth.id > 0) {
            rlUnloadTexture(target.depth.id);
        }
        rlUnloadFramebuffer(target.id);
        return RenderTexture2D{};
    }

    rlDisableFramebuffer();
    return target;
}

void UnloadDepthOnlyRenderTexture(RenderTexture2D& target)
{
    if (target.depth.id > 0) {
        rlUnloadTexture(target.depth.id);
    }
    if (target.id > 0) {
        rlUnloadFramebuffer(target.id);
    }
    target = RenderTexture2D{};
}

} // namespace

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights)
{
    const int lightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(lights.size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    SectorBillboardDynamicLightContext context;
    context.dynamicLightCount = lightCount;
    if (lightCount <= 0) {
        UploadSectorRendererDynamicPointLights(shader, locations, context);
        return;
    }

    std::array<Vector3, MaxDynamicLights> positions{};
    std::array<Vector3, MaxDynamicLights> colors{};
    std::array<float, MaxDynamicLights> radii{};
    std::array<float, MaxDynamicLights> intensities{};
    std::array<int, MaxDynamicLights> types{};
    std::array<Vector3, MaxDynamicLights> directions{};
    std::array<float, MaxDynamicLights> innerConeCos{};
    std::array<float, MaxDynamicLights> outerConeCos{};
    std::array<Vector3, MaxDynamicLights> spotShadowRight{};
    std::array<Vector2, MaxDynamicLights> spotShadowProjection{};
    for (int i = 0; i < lightCount; ++i) {
        positions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].position;
        colors[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].color;
        radii[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].radius;
        intensities[static_cast<size_t>(i)] = DynamicLightEffectiveUploadIntensity(
                lights[static_cast<size_t>(i)],
                runtimeSeconds);
        types[static_cast<size_t>(i)] = static_cast<int>(lights[static_cast<size_t>(i)].kind);
        directions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].kind
                        != SectorPreviewDynamicLightKind::Point
                ? (Vector3LengthSqr(lights[static_cast<size_t>(i)].direction)
                                > 0.00000001f
                        ? Vector3Normalize(lights[static_cast<size_t>(i)].direction)
                        : Vector3{0.0f, -1.0f, 0.0f})
                : lights[static_cast<size_t>(i)].direction;
        innerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].innerConeCos;
        outerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].outerConeCos;
        if (lights[static_cast<size_t>(i)].kind
                != SectorPreviewDynamicLightKind::Point) {
            BuildSectorDynamicSpotShadowProjectionUpload(
                    lights[static_cast<size_t>(i)],
                    spotShadowRight[static_cast<size_t>(i)],
                    spotShadowProjection[static_cast<size_t>(i)]);
        } else if (lights[static_cast<size_t>(i)].castsShadow) {
            context.hasPointShadows = 1;
        }
    }

    context.dynamicLightPositions = positions;
    context.dynamicLightColors = colors;
    context.dynamicLightRadii = radii;
    context.dynamicLightIntensities = intensities;
    context.dynamicLightTypes = types;
    context.dynamicLightDirections = directions;
    context.dynamicLightInnerConeCos = innerConeCos;
    context.dynamicLightOuterConeCos = outerConeCos;
    context.dynamicLightSpotShadowRight = spotShadowRight;
    context.dynamicLightSpotShadowProjection = spotShadowProjection;
    UploadSectorRendererDynamicPointLights(shader, locations, context);
}

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        const SectorBillboardDynamicLightContext& context)
{
    if (locations.dynamicLightCount >= 0) {
        SetShaderValue(shader, locations.dynamicLightCount, &context.dynamicLightCount, SHADER_UNIFORM_INT);
    }
    if (locations.hasPointShadows >= 0) {
        SetShaderValue(
                shader,
                locations.hasPointShadows,
                &context.hasPointShadows,
                SHADER_UNIFORM_INT);
    }
    if (context.dynamicLightCount <= 0) {
        return;
    }

    if (locations.dynamicLightPositions >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightPositions,
                context.dynamicLightPositions.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightColors >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightColors,
                context.dynamicLightColors.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightRadii >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightRadii,
                context.dynamicLightRadii.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightIntensities >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightIntensities,
                context.dynamicLightIntensities.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightTypes >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightTypes,
                context.dynamicLightTypes.data(),
                SHADER_UNIFORM_INT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightDirections >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightDirections,
                context.dynamicLightDirections.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightInnerConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightInnerConeCos,
                context.dynamicLightInnerConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightOuterConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightOuterConeCos,
                context.dynamicLightOuterConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightSpotShadowRight >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightSpotShadowRight,
                context.dynamicLightSpotShadowRight.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightSpotShadowProjection >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightSpotShadowProjection,
                context.dynamicLightSpotShadowProjection.data(),
                SHADER_UNIFORM_VEC2,
                context.dynamicLightCount);
    }
}

void UploadSectorRendererDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorDynamicSpotLightShadowShaderLocations& locations,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms)
{
    UploadSectorRendererDynamicShadowSlots(
            shader, locations.dynamicLightShadowSlots, uniforms);
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        if (locations.shadowLightMatrices[i] >= 0) {
            SetShaderValueMatrix(shader, locations.shadowLightMatrices[i], uniforms.shadowLightMatrices[i]);
        }
    }
    if (locations.shadowBias >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowBias,
                uniforms.shadowBias.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowStrength >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowStrength,
                uniforms.shadowStrength.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowSoftness >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowSoftness,
                uniforms.shadowSoftness.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowAtlasTilesPerRow >= 0) {
        SetShaderValue(shader, locations.shadowAtlasTilesPerRow,
                &uniforms.shadowAtlasTilesPerRow, SHADER_UNIFORM_INT);
    }
}

void UploadSectorRendererDynamicShadowSlots(
        Shader shader,
        int dynamicLightShadowSlotsLocation,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms)
{
    if (dynamicLightShadowSlotsLocation < 0) return;
    SetShaderValueV(
            shader,
            dynamicLightShadowSlotsLocation,
            uniforms.dynamicLightShadowSlots.data(),
            SHADER_UNIFORM_INT,
            static_cast<int>(MaxDynamicLights));
}

void SectorDynamicLightingRenderer::Reset()
{
    sources.clear();
    selectionSources.clear();
    runtimePointLight = {};
    runtimePointLightActive = false;
    candidates.clear();
    selectedLights.clear();
    selectedLightKeys.clear();
    ResetSectorDynamicLightFadeTracker(selectionFadeTracker);
    lightingVisibility = {};
    cachedLightingStartSectorIds.clear();
    cachedLightingPortalBlockers.clear();
    cachedLightingVisibilityGraph = nullptr;
    lightingVisibilityCacheValid = false;
    selectionStats = {};
    receiverBounds.clear();
    sectorLightContexts.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
        state = {};
    }
    for (SectorDynamicShadowSlotOwner& owner : shadowAtlasSlotOwners) {
        owner = {};
    }
    pendingShadowLightUpdates.clear();
    previousDoorShadowCasterBounds.clear();
    currentDoorShadowCasterBounds.clear();
    previousStaticShadowCasterBounds.clear();
    currentStaticShadowCasterBounds.clear();
    previousDynamicShadowCasterBounds.clear();
    currentDynamicShadowCasterBounds.clear();
    changedShadowCasterBounds.clear();
    nextShadowDirtySerial = 1;
    shadowAtlasNeedsFullClear = true;
    doorShadowCasterBoundsInitialized = false;
    staticShadowCasterBoundsInitialized = false;
    dynamicShadowCasterBoundsInitialized = false;
    cachedDoorShadowCasterRevision = 0;
    cachedStaticModelShadowCasterRevision = 0;
    cachedDynamicModelShadowCasterRevision = 0;
    shadowRenderStats = {};
}

void SectorDynamicLightingRenderer::BeginShadowFrame(bool enabled)
{
    shadowRenderStats = {};
    shadowRenderStats.enabled = enabled;
}

void SectorDynamicLightingRenderer::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld)
{
    BuildSectorPreviewDynamicPointLightSources(map, sectorLookupWorld, sources);
    lightingVisibilityCacheValid = false;
    selectionSources.reserve(sources.size() + 1);
    ReserveSelectionBuffers();
}

void SectorDynamicLightingRenderer::ReserveReceiverBoundsCapacity(
        size_t sectorCapacity,
        size_t runtimeObjectCapacity)
{
    receiverBounds.clear();
    receiverBounds.reserve(sectorCapacity + runtimeObjectCapacity * 2);
    sectorLightContexts.clear();
    sectorLightContexts.reserve(sectorCapacity);
    previousDoorShadowCasterBounds.reserve(runtimeObjectCapacity);
    currentDoorShadowCasterBounds.reserve(runtimeObjectCapacity);
    previousStaticShadowCasterBounds.reserve(runtimeObjectCapacity);
    currentStaticShadowCasterBounds.reserve(runtimeObjectCapacity);
    previousDynamicShadowCasterBounds.reserve(runtimeObjectCapacity);
    currentDynamicShadowCasterBounds.reserve(runtimeObjectCapacity);
    changedShadowCasterBounds.reserve(runtimeObjectCapacity * 6);
    cachedLightingPortalBlockers.reserve(runtimeObjectCapacity * 2);
}

void SectorDynamicLightingRenderer::SetRuntimePointLight(
        const SectorPreviewDynamicPointLightSource* light)
{
    runtimePointLightActive = light != nullptr;
    runtimePointLight = light != nullptr
            ? *light
            : SectorPreviewDynamicPointLightSource{};
    if (runtimePointLightActive) {
        runtimePointLight.light.selectionFadeMultiplier = 1.0f;
        runtimePointLight.light.selectionFadeEnabled = false;
    }
}

void SectorDynamicLightingRenderer::SetSelectionFadeInSeconds(float seconds)
{
    selectionFadeInSeconds = std::isfinite(seconds)
            ? std::clamp(seconds, 0.0f, DynamicLightMaximumFadeInSeconds)
            : DynamicLightDefaultFadeInSeconds;
}

void SectorDynamicLightingRenderer::UpdateSelection(
        const RuntimePortalVisibilityResult& visibility,
        int lightingStartSectorId,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    UpdateLightingReachability(
            visibility,
            lightingStartSectorId,
            visibilityGraph,
            dynamicPortalBlockers);
    selectionSources.assign(sources.begin(), sources.end());
    if (runtimePointLightActive) selectionSources.push_back(runtimePointLight);
    CollectSectorPreviewDynamicPointLightCandidates(
            selectionSources,
            lightingVisibility,
            receiverBounds,
            candidates);
    SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            lightingVisibility,
            receiverBounds,
            maxDynamicLights,
            selectedLights,
            &selectedLightKeys,
            &selectedLightKeys);
    SynchronizeSectorDynamicLightFadeTracker(
            selectedLights,
            selectionFadeTracker);
    SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selectedLights,
            lightingVisibility,
            receiverBounds,
            ShadowSlotBudget(),
            shadowCasters);
    AssignPersistentSectorDynamicShadowSlots(
            selectedLights,
            ShadowSlotBudget(),
            shadowCasters,
            shadowAtlasSlotOwners);
    BuildSectorPreviewDynamicSpotLightShadowMatrices(
            selectedLights,
            shadowCasters,
            shadowMatrices);
    RefreshShadowTileRequirements();
    UpdateSelectionStats(visibility);
}

void SectorDynamicLightingRenderer::UpdateLightingReachability(
        const RuntimePortalVisibilityResult& visibility,
        int lightingStartSectorId,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    selectionStats.reachabilityCacheHit = false;
    selectionStats.cameraVisibilityFallback = visibility.fallbackDrawAll;
    selectionStats.dynamicPortalBlockerCount = dynamicPortalBlockers != nullptr
            ? dynamicPortalBlockers->size()
            : 0;

    int resolvedStartSectorId = lightingStartSectorId;
    if (visibilityGraph != nullptr
            && FindRuntimeSectorVisibilityNode(
                    *visibilityGraph,
                    resolvedStartSectorId) == nullptr) {
        resolvedStartSectorId = visibility.validStartSector
                ? visibility.startSectorId
                : -1;
    }
    selectionStats.lightingStartSectorId = resolvedStartSectorId;

    if (visibilityGraph == nullptr
            || FindRuntimeSectorVisibilityNode(
                    *visibilityGraph,
                    resolvedStartSectorId) == nullptr) {
        lightingVisibility = {};
        lightingVisibility.totalSectorCount = visibilityGraph != nullptr
                ? visibilityGraph->sectors.size()
                : visibility.totalSectorCount;
        lightingVisibility.fallbackDrawAll = true;
        lightingVisibility.status = "lighting reachability unavailable; fallback all";
        lightingVisibilityCacheValid = false;
        return;
    }

    if (lightingVisibilityCacheValid
            && cachedLightingVisibilityGraph == visibilityGraph
            && LightingStartSectorsMatch(
                    cachedLightingStartSectorIds,
                    resolvedStartSectorId)
            && DynamicPortalBlockersMatch(
                    cachedLightingPortalBlockers,
                    dynamicPortalBlockers)) {
        selectionStats.reachabilityCacheHit = true;
        return;
    }

    if (resolvedStartSectorId > 0) {
        cachedLightingStartSectorIds.assign(1, resolvedStartSectorId);
    } else {
        cachedLightingStartSectorIds.clear();
    }
    if (dynamicPortalBlockers != nullptr) {
        cachedLightingPortalBlockers = *dynamicPortalBlockers;
    } else {
        cachedLightingPortalBlockers.clear();
    }
    cachedLightingVisibilityGraph = visibilityGraph;
    lightingVisibility = TraverseRuntimeSectorVisibilityFromSeeds(
            *visibilityGraph,
            cachedLightingStartSectorIds,
            resolvedStartSectorId,
            dynamicPortalBlockers);
    lightingVisibilityCacheValid = true;
}

void SectorDynamicLightingRenderer::UpdateSelectionStats(
        const RuntimePortalVisibilityResult& cameraVisibility)
{
    selectionStats.reachableSectorCount = lightingVisibility.fallbackDrawAll
            ? lightingVisibility.totalSectorCount
            : lightingVisibility.visibleSectorIds.size();
    selectionStats.visibleReceiverCount = 0;
    selectionStats.visibleReceiverLightReferences = 0;
    selectionStats.maxVisibleReceiverLights = 0;

    for (const SectorReceiverBounds& bounds : receiverBounds) {
        if (!IsValidSectorAabb3(SectorAabb3{bounds.min, bounds.max})) continue;
        if (!ShouldDrawRuntimeSectorForVisibility(bounds.sectorId, cameraVisibility)) {
            continue;
        }

        ++selectionStats.visibleReceiverCount;
        std::size_t lightCount = 0;
        const SectorAabb3 receiverAabb = ToSectorAabb3(bounds);
        for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
            if (DynamicLightIntersectsBounds(light, receiverAabb)) ++lightCount;
        }
        selectionStats.visibleReceiverLightReferences += lightCount;
        selectionStats.maxVisibleReceiverLights = std::max(
                selectionStats.maxVisibleReceiverLights,
                lightCount);
    }
}

void SectorDynamicLightingRenderer::RefreshShadowTileRequirements()
{
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
        state.assigned = false;
    }

    for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
        if (matrix.shadowSlot < 0
                || static_cast<std::size_t>(matrix.shadowSlot)
                        >= shadowAtlasTileStates.size()) {
            continue;
        }
        ShadowAtlasTileState& state =
                shadowAtlasTileStates[static_cast<std::size_t>(matrix.shadowSlot)];
        const bool compatible = state.valid
                && ShadowMatrixMatches(state.matrix, matrix);
        state.assigned = true;
        state.matrix = matrix;
        if (!compatible) {
            state.valid = false;
            if (!state.dirty) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
            state.dirty = true;
            if (state.dirtySerial == 0) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
        }
    }

    // Multi-face point and rect shadows form one cache entry and must always be
    // rebuilt together. Give every face the oldest serial in its span.
    for (const SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0) continue;
        uint64_t serial = 0;
        bool dirty = false;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            const ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            dirty = dirty || state.dirty;
            if (state.dirtySerial != 0
                    && (serial == 0 || state.dirtySerial < serial)) {
                serial = state.dirtySerial;
            }
        }
        if (!dirty) continue;
        if (serial == 0) serial = nextShadowDirtySerial++;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            shadowAtlasTileStates[slot].dirty = true;
            shadowAtlasTileStates[slot].dirtySerial = serial;
        }
    }
}

SectorBillboardDynamicLightContext SectorDynamicLightingRenderer::BuildLightContext(
        const SectorReceiverBounds* bounds,
        bool dynamicLightingEnabled,
        bool shadowMapsEnabled,
        float runtimeSeconds) const
{
    SectorBillboardDynamicLightContext context;
    context.shadowUniforms = PackShadowUniforms(shadowMapsEnabled);
    const std::array<int, MaxDynamicLights> globalShadowSlots =
            context.shadowUniforms.dynamicLightShadowSlots;
    context.shadowUniforms.dynamicLightShadowSlots.fill(-1);
    context.shadowMaps = BuildShadowMapTextures(shadowMapsEnabled);
    if (!dynamicLightingEnabled) return context;

    const SectorAabb3 receiverAabb = bounds != nullptr
            ? ToSectorAabb3(*bounds)
            : SectorAabb3{};
    for (std::size_t selectedIndex = 0;
            selectedIndex < selectedLights.size()
                    && context.dynamicLightCount
                            < static_cast<int>(MaxDynamicLights);
            ++selectedIndex) {
        const SectorPreviewDynamicPointLightUniform& light =
                selectedLights[selectedIndex];
        if (bounds != nullptr
                && !DynamicLightIntersectsBounds(light, receiverAabb)) {
            continue;
        }

        const std::size_t localIndex =
                static_cast<std::size_t>(context.dynamicLightCount++);
        context.dynamicLightIds[localIndex] = light.lightId;
        context.dynamicLightPositions[localIndex] = light.position;
        context.dynamicLightColors[localIndex] = light.color;
        context.dynamicLightRadii[localIndex] = light.radius;
        context.dynamicLightIntensities[localIndex] =
                DynamicLightEffectiveUploadIntensity(light, runtimeSeconds);
        context.dynamicLightTypes[localIndex] = static_cast<int>(light.kind);
        context.dynamicLightDirections[localIndex] = light.kind
                        != SectorPreviewDynamicLightKind::Point
                ? (Vector3LengthSqr(light.direction) > 0.00000001f
                        ? Vector3Normalize(light.direction)
                        : Vector3{0.0f, -1.0f, 0.0f})
                : light.direction;
        context.dynamicLightInnerConeCos[localIndex] = light.innerConeCos;
        context.dynamicLightOuterConeCos[localIndex] = light.outerConeCos;
        if (light.kind != SectorPreviewDynamicLightKind::Point) {
            BuildSectorDynamicSpotShadowProjectionUpload(
                    light,
                    context.dynamicLightSpotShadowRight[localIndex],
                    context.dynamicLightSpotShadowProjection[localIndex]);
        }
        const int shadowSlot = globalShadowSlots[selectedIndex];
        context.shadowUniforms.dynamicLightShadowSlots[localIndex] = shadowSlot;
        if ((light.kind == SectorPreviewDynamicLightKind::Point
                    || light.kind == SectorPreviewDynamicLightKind::Rect)
                && shadowSlot >= 0
                && static_cast<std::size_t>(shadowSlot)
                        < context.shadowUniforms.shadowStrength.size()
                && context.shadowUniforms.shadowStrength[
                        static_cast<std::size_t>(shadowSlot)] > 0.0f) {
            context.hasPointShadows = 1;
        }
    }
    return context;
}

void SectorDynamicLightingRenderer::BuildSectorLightContexts(
        const std::vector<SectorReceiverBounds>& sectorBounds,
        bool dynamicLightingEnabled,
        bool shadowMapsEnabled,
        float runtimeSeconds)
{
    UpdateSelectionFadeMultipliers(shadowMapsEnabled, runtimeSeconds);
    sectorLightContexts.clear();
    for (const SectorReceiverBounds& bounds : sectorBounds) {
        sectorLightContexts.push_back(SectorDynamicLightSectorContext{
                bounds.sectorId,
                BuildLightContext(
                        &bounds,
                        dynamicLightingEnabled,
                        shadowMapsEnabled,
                        runtimeSeconds)});
    }
}

bool SectorDynamicLightingRenderer::IsSelectionFadeShadowReady(
        std::size_t selectedLightIndex,
        bool shadowMapsEnabled) const
{
    if (!shadowMapsEnabled
            || !HasShadowMapResources()
            || !shadowMaterialLoaded
            || selectedLightIndex >= selectedLights.size()
            || !selectedLights[selectedLightIndex].castsShadow) {
        return true;
    }

    const auto caster = std::find_if(
            shadowCasters.begin(),
            shadowCasters.end(),
            [selectedLightIndex](
                    const SectorPreviewDynamicSpotLightShadowCaster& candidate) {
                return candidate.dynamicLightIndex
                        == static_cast<int>(selectedLightIndex);
            });
    if (caster == shadowCasters.end() || caster->shadowSlot < 0) {
        return true;
    }

    for (int offset = 0; offset < caster->shadowSlotCount; ++offset) {
        const int slot = caster->shadowSlot + offset;
        if (slot < 0
                || static_cast<std::size_t>(slot)
                        >= shadowAtlasTileStates.size()) {
            return true;
        }
        const ShadowAtlasTileState& state =
                shadowAtlasTileStates[static_cast<std::size_t>(slot)];
        if (!state.assigned || !state.valid) return false;
    }
    return true;
}

void SectorDynamicLightingRenderer::UpdateSelectionFadeMultipliers(
        bool shadowMapsEnabled,
        float runtimeSeconds)
{
    for (std::size_t selectedIndex = 0;
            selectedIndex < selectedLights.size();
            ++selectedIndex) {
        SectorPreviewDynamicPointLightUniform& light =
                selectedLights[selectedIndex];
        light.selectionFadeMultiplier = !light.selectionFadeEnabled
                ? 1.0f
                : EvaluateSectorDynamicLightFadeMultiplier(
                        selectionFadeTracker,
                        MakeSectorPreviewDynamicLightKey(light),
                        runtimeSeconds,
                        selectionFadeInSeconds,
                        IsSelectionFadeShadowReady(
                                selectedIndex,
                                shadowMapsEnabled));
    }
}

const SectorBillboardDynamicLightContext*
SectorDynamicLightingRenderer::FindSectorLightContext(int sectorId) const
{
    for (const SectorDynamicLightSectorContext& context : sectorLightContexts) {
        if (context.sectorId == sectorId) return &context.lighting;
    }
    return nullptr;
}

SectorPreviewDynamicSpotLightShadowUniforms SectorDynamicLightingRenderer::PackShadowUniforms(
        bool enabled) const
{
    if (!enabled) {
        SectorPreviewDynamicSpotLightShadowUniforms result;
        result.dynamicLightShadowSlots.fill(-1);
        result.shadowAtlasTilesPerRow = DynamicShadowAtlasTilesPerRow;
        return result;
    }
    SectorPreviewDynamicSpotLightShadowUniforms result =
            PackSectorPreviewDynamicSpotLightShadowUniforms(
                    selectedLights, shadowCasters, shadowMatrices);
    for (std::size_t lightIndex = 0;
            lightIndex < selectedLights.size()
                    && lightIndex < result.dynamicLightShadowSlots.size();
            ++lightIndex) {
        const int slot = result.dynamicLightShadowSlots[lightIndex];
        if (slot < 0
                || static_cast<std::size_t>(slot) >= shadowAtlasTileStates.size()) {
            continue;
        }
        const ShadowAtlasTileState& state =
                shadowAtlasTileStates[static_cast<std::size_t>(slot)];
        const int requiredSlots = SectorDynamicShadowFaceCount(
                selectedLights[lightIndex].kind);
        bool valid = state.assigned && state.valid;
        for (int offset = 1; valid && offset < requiredSlots; ++offset) {
            const std::size_t adjacent = static_cast<std::size_t>(slot + offset);
            valid = adjacent < shadowAtlasTileStates.size()
                    && shadowAtlasTileStates[adjacent].assigned
                    && shadowAtlasTileStates[adjacent].valid;
        }
        if (!valid) result.dynamicLightShadowSlots[lightIndex] = -1;
    }
    result.shadowAtlasTilesPerRow = DynamicShadowAtlasTilesPerRow;
    return result;
}

bool SectorDynamicLightingRenderer::EnsureShadowMapResources()
{
    if (shadowAtlas.id != 0 && shadowAtlas.depth.id != 0) {
        return true;
    }
    const int requestedFaceResolution = shadowMapResolution <= 512
            ? 512 : DynamicSpotLightShadowMapResolution;
    const int requestedAtlasResolution = requestedFaceResolution
            * DynamicShadowAtlasTilesPerRow;
    int maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    const bool requiresFallback = requestedAtlasResolution
            > maximumTextureSize;
    if (!requiresFallback) {
        shadowAtlas = LoadDepthOnlyRenderTexture(
                requestedAtlasResolution, requestedAtlasResolution);
    }
    effectiveShadowMapResolution = requestedFaceResolution;
    shadowAtlasResolution = requestedAtlasResolution;
    if ((shadowAtlas.id == 0 || shadowAtlas.depth.id == 0)
            && requestedAtlasResolution > DynamicShadowAtlasLowResolution) {
        effectiveShadowMapResolution = 512;
        shadowAtlasResolution = DynamicShadowAtlasLowResolution;
        shadowAtlas = LoadDepthOnlyRenderTexture(
                shadowAtlasResolution, shadowAtlasResolution);
        if (!shadowAtlasFallbackWarningLogged) {
            TraceLog(
                    LOG_WARNING,
                    "Dynamic shadow atlas: requested %dx%d with %dpx faces, "
                    "falling back to %dx%d with %dpx faces (GL max %d)",
                    requestedAtlasResolution,
                    requestedAtlasResolution,
                    requestedFaceResolution,
                    shadowAtlasResolution,
                    shadowAtlasResolution,
                    effectiveShadowMapResolution,
                    maximumTextureSize);
            shadowAtlasFallbackWarningLogged = true;
        }
    }
    if (shadowAtlas.id == 0 || shadowAtlas.depth.id == 0) {
        TraceLog(
                LOG_ERROR,
                "Dynamic shadow atlas allocation failed at %dx%d",
                shadowAtlasResolution,
                shadowAtlasResolution);
        UnloadShadowMapResources();
        return false;
    }
    SetTextureFilter(shadowAtlas.depth, TEXTURE_FILTER_POINT);
    SetTextureWrap(shadowAtlas.depth, TEXTURE_WRAP_CLAMP);
    shadowAtlasNeedsFullClear = true;
    return true;
}

void SectorDynamicLightingRenderer::SetShadowMapResolution(int resolution)
{
    resolution = resolution <= 512 ? 512 : DynamicSpotLightShadowMapResolution;
    if (shadowMapResolution == resolution) {
        return;
    }
    shadowMapResolution = resolution;
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) state = {};
    for (SectorDynamicShadowSlotOwner& owner : shadowAtlasSlotOwners) owner = {};
    shadowAtlasNeedsFullClear = true;
    UnloadShadowMapResources();
    EnsureShadowMapResources();
}

void SectorDynamicLightingRenderer::UnloadShadowMapResources()
{
    UnloadDepthOnlyRenderTexture(shadowAtlas);
}

bool SectorDynamicLightingRenderer::HasShadowMapResources() const
{
    return shadowAtlas.id != 0 || shadowAtlas.depth.id != 0;
}

bool SectorDynamicLightingRenderer::LoadShadowMaterial()
{
    shadowMaterial = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(
            SectorSpotLightShadowVs, SectorSpotLightShadowOpaqueFs);
    if (shader.id == 0) {
        UnloadMaterial(shadowMaterial);
        shadowMaterial = Material{};
        return false;
    }
    spotShadowCutoutMaterial = LoadMaterialDefault();
    Shader spotCutoutShader = LoadShaderFromMemory(
            SectorSpotLightShadowVs, SectorSpotLightShadowCutoutFs);
    if (spotCutoutShader.id == 0) {
        UnloadShader(shader);
        UnloadMaterial(shadowMaterial);
        UnloadMaterial(spotShadowCutoutMaterial);
        shadowMaterial = Material{};
        spotShadowCutoutMaterial = Material{};
        return false;
    }
    shadowMaterial.shader = shader;
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexPosition");
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexTexCoord");
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexBoneIndices");
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexBoneWeights");
    shadowMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shadowMaterial.shader, "matModel");
    shadowMaterial.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
            GetShaderLocation(shadowMaterial.shader, "boneMatrices");
    shadowMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shadowMaterial.shader, "texture0");
    shadowLightViewProjectionLoc = GetShaderLocation(shadowMaterial.shader, "lightViewProjection");
    shadowUseSkinningLoc = GetShaderLocation(shadowMaterial.shader, "useSkinning");
    shadowDefaultTexture = shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    spotShadowCutoutMaterial.shader = spotCutoutShader;
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexPosition");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexTexCoord");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexBoneIndices");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexBoneWeights");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(spotCutoutShader, "matModel");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
            GetShaderLocation(spotCutoutShader, "boneMatrices");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(spotCutoutShader, "texture0");
    spotShadowCutoutLightViewProjectionLoc =
            GetShaderLocation(spotCutoutShader, "lightViewProjection");
    spotShadowCutoutUseSkinningLoc =
            GetShaderLocation(spotCutoutShader, "useSkinning");
    shadowAlphaTestLoc = GetShaderLocation(spotCutoutShader, "alphaTest");
    shadowAlphaCutoffLoc = GetShaderLocation(spotCutoutShader, "alphaCutoff");
    spotShadowCutoutDefaultTexture =
            spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    shadowMaterialLoaded = true;
    return true;
}

void SectorDynamicLightingRenderer::UnloadShadowMaterial()
{
    if (!shadowMaterialLoaded) {
        return;
    }

    shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = shadowDefaultTexture;
    spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
            spotShadowCutoutDefaultTexture;
    UnloadMaterial(shadowMaterial);
    UnloadMaterial(spotShadowCutoutMaterial);
    shadowMaterial = Material{};
    spotShadowCutoutMaterial = Material{};
    shadowDefaultTexture = Texture2D{};
    spotShadowCutoutDefaultTexture = Texture2D{};
    shadowMaterialLoaded = false;
    shadowLightViewProjectionLoc = -1;
    shadowUseSkinningLoc = -1;
    spotShadowCutoutLightViewProjectionLoc = -1;
    spotShadowCutoutUseSkinningLoc = -1;
    shadowAlphaTestLoc = -1;
    shadowAlphaCutoffLoc = -1;
}

bool SectorDynamicLightingRenderer::IsShadowRenderReady() const
{
    return shadowMaterialLoaded
            && shadowMaterial.shader.id != 0
            && spotShadowCutoutMaterial.shader.id != 0
            && shadowLightViewProjectionLoc >= 0
            && spotShadowCutoutLightViewProjectionLoc >= 0
            && !shadowMatrices.empty();
}

RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index)
{
    if (index != 0) {
        return nullptr;
    }
    return &shadowAtlas;
}

const RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index) const
{
    if (index != 0) {
        return nullptr;
    }
    return &shadowAtlas;
}

const Texture2D* SectorDynamicLightingRenderer::ShadowMapDepthTexture(std::size_t index) const
{
    const RenderTexture2D* shadowMap = ShadowMap(index);
    if (shadowMap == nullptr || shadowMap->depth.id == 0) {
        return nullptr;
    }
    return &shadowMap->depth;
}

SectorDynamicShadowMapTextures SectorDynamicLightingRenderer::BuildShadowMapTextures(
        bool enabled) const
{
    SectorDynamicShadowMapTextures textures;
    if (!enabled) {
        return textures;
    }
    textures.shadowAtlas = ShadowMapDepthTexture(0);
    textures.shadowMap0 = textures.shadowAtlas;
    textures.shadowMap1 = textures.shadowAtlas;
    return textures;
}

void SectorDynamicLightingRenderer::RenderShadowMaps(
        const SectorDynamicSpotLightShadowRenderContext& context)
{
    const auto cpuStart = std::chrono::steady_clock::now();
    const auto finishCpuTiming = [this, cpuStart]() {
        shadowRenderStats.cpuMilliseconds =
                std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - cpuStart).count();
    };
    shadowRenderStats.doorCasterRevision = context.doorShadowCasterRevision;
    shadowRenderStats.staticModelCasterRevision =
            context.staticModelShadowCasterRevision;
    shadowRenderStats.dynamicModelCasterRevision =
            context.dynamicModelShadowCasterRevision;
    if (context.assets == nullptr
            || !IsShadowRenderReady()
            || context.sectorDrawRecords == nullptr
            || shadowMatrices.empty()) {
        return;
    }

    RenderTexture2D* shadowMap = ShadowMap(0);
    if (shadowMap == nullptr || shadowMap->id == 0 || shadowMap->depth.id == 0) {
        return;
    }

    if (shadowAtlasNeedsFullClear) {
        for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
            state.valid = false;
            if (!state.assigned) continue;
            state.dirty = true;
            if (state.dirtySerial == 0) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
        }
    }

    const bool doorRevisionChanged = context.doorShadowCasterRevision
            != cachedDoorShadowCasterRevision;
    const bool staticRevisionChanged = context.staticModelShadowCasterRevision
            != cachedStaticModelShadowCasterRevision;
    const bool dynamicRevisionChanged = context.dynamicModelShadowCasterRevision
            != cachedDynamicModelShadowCasterRevision;
    const bool refreshDoorBounds = doorRevisionChanged
            || !doorShadowCasterBoundsInitialized;
    const bool refreshStaticBounds = staticRevisionChanged
            || !staticShadowCasterBoundsInitialized;
    const bool refreshDynamicBounds = dynamicRevisionChanged
            || !dynamicShadowCasterBoundsInitialized;
    changedShadowCasterBounds.clear();
    bool preciseCasterChanges = true;
    const auto appendChangedBounds = [this](
            const std::vector<ShadowCasterBoundsRecord>& current,
            const std::vector<ShadowCasterBoundsRecord>& previous) {
        for (const ShadowCasterBoundsRecord& currentRecord : current) {
            const auto previousIt = std::find_if(
                    previous.begin(), previous.end(),
                    [&currentRecord](const ShadowCasterBoundsRecord& candidate) {
                        return candidate.key == currentRecord.key;
                    });
            if (previousIt == previous.end()) {
                changedShadowCasterBounds.push_back(currentRecord.bounds);
                continue;
            }
            if (std::memcmp(&previousIt->bounds,
                        &currentRecord.bounds, sizeof(BoundingBox)) != 0
                    || previousIt->contentFingerprint
                            != currentRecord.contentFingerprint) {
                changedShadowCasterBounds.push_back(previousIt->bounds);
                changedShadowCasterBounds.push_back(currentRecord.bounds);
            }
        }
        for (const ShadowCasterBoundsRecord& previousRecord : previous) {
            const auto currentIt = std::find_if(
                    current.begin(), current.end(),
                    [&previousRecord](const ShadowCasterBoundsRecord& candidate) {
                        return candidate.key == previousRecord.key;
                    });
            if (currentIt == current.end()) {
                changedShadowCasterBounds.push_back(previousRecord.bounds);
            }
        }
    };
    if (refreshDoorBounds) {
        currentDoorShadowCasterBounds.clear();
        if (context.doorShadowCasters != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                const SectorAabb3 bounds = DoorCasterBounds(caster);
                currentDoorShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{1} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        BoundingBox{bounds.min, bounds.max}});
            }
        }
        if (context.doorModelShadowCasters != nullptr) {
            for (const SectorDoorModelShadowCaster& caster
                    : *context.doorModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    preciseCasterChanges = false;
                    continue;
                }
                const Matrix transform = MatrixMultiply(
                        asset->model.transform, caster.transform);
                currentDoorShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{2} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        TransformSectorDoorModelBounds(
                                asset->localBounds, transform)});
            }
        }
        appendChangedBounds(
                currentDoorShadowCasterBounds,
                previousDoorShadowCasterBounds);
        previousDoorShadowCasterBounds.swap(currentDoorShadowCasterBounds);
        doorShadowCasterBoundsInitialized = true;
    }
    if (refreshStaticBounds) {
        currentStaticShadowCasterBounds.clear();
        if (context.staticModelShadowCasters != nullptr) {
            for (const SectorStaticModelShadowCaster& caster
                    : *context.staticModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    preciseCasterChanges = false;
                    continue;
                }
                const Matrix transform = MatrixMultiply(
                        asset->model.transform, caster.transform);
                currentStaticShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{3} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        TransformSectorDoorModelBounds(
                                asset->localBounds, transform)});
            }
        }
        appendChangedBounds(
                currentStaticShadowCasterBounds,
                previousStaticShadowCasterBounds);
        previousStaticShadowCasterBounds.swap(currentStaticShadowCasterBounds);
        staticShadowCasterBoundsInitialized = true;
    }
    if (refreshDynamicBounds) {
        currentDynamicShadowCasterBounds.clear();
        if (context.dynamicModelShadowCasters != nullptr) {
            for (const SectorDynamicModelShadowCaster& caster
                    : *context.dynamicModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    preciseCasterChanges = false;
                    continue;
                }
                const Matrix transform = MatrixMultiply(
                        asset->model.transform,
                        caster.transform);
                const BoundingBox localBounds = asset->hasAnimatedLocalBounds
                        ? asset->animatedLocalBounds
                        : asset->localBounds;
                currentDynamicShadowCasterBounds.push_back(
                        ShadowCasterBoundsRecord{
                                (uint64_t{4} << 32)
                                        | static_cast<uint32_t>(
                                                caster.placedObjectId),
                                TransformSectorDoorModelBounds(
                                        localBounds,
                                        transform),
                                caster.contentFingerprint});
            }
        }
        appendChangedBounds(
                currentDynamicShadowCasterBounds,
                previousDynamicShadowCasterBounds);
        previousDynamicShadowCasterBounds.swap(
                currentDynamicShadowCasterBounds);
        dynamicShadowCasterBoundsInitialized = true;
    }

    const auto markCasterDirty = [this](
            const SectorPreviewDynamicSpotLightShadowCaster& caster) {
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            if (!state.dirty) state.dirtySerial = nextShadowDirtySerial++;
            state.dirty = true;
        }
    };
    if (refreshDoorBounds || refreshStaticBounds || refreshDynamicBounds) {
        for (const SectorPreviewDynamicSpotLightShadowCaster& caster
                : shadowCasters) {
            bool affected = !preciseCasterChanges;
            if (!affected && caster.dynamicLightIndex >= 0
                    && static_cast<std::size_t>(caster.dynamicLightIndex)
                            < selectedLights.size()) {
                const SectorPreviewDynamicPointLightUniform& light =
                        selectedLights[static_cast<std::size_t>(
                                caster.dynamicLightIndex)];
                for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix
                        : shadowMatrices) {
                    if (matrix.lightId != caster.lightId
                            || matrix.dynamicLightIndex != caster.dynamicLightIndex) {
                        continue;
                    }
                    for (const BoundingBox& changedBounds
                            : changedShadowCasterBounds) {
                        if (ShadowLightIntersectsBounds(
                                    light, matrix, ToSectorAabb3(changedBounds))) {
                            affected = true;
                            break;
                        }
                    }
                    if (affected) break;
                }
            }
            if (affected) markCasterDirty(caster);
        }
    }
    cachedDoorShadowCasterRevision = context.doorShadowCasterRevision;
    cachedStaticModelShadowCasterRevision =
            context.staticModelShadowCasterRevision;
    cachedDynamicModelShadowCasterRevision =
            context.dynamicModelShadowCasterRevision;

    pendingShadowLightUpdates.clear();
    for (std::size_t casterIndex = 0;
            casterIndex < shadowCasters.size(); ++casterIndex) {
        const SectorPreviewDynamicSpotLightShadowCaster& caster =
                shadowCasters[casterIndex];
        if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0) continue;
        bool dirty = false;
        bool invalid = false;
        bool valid = true;
        uint64_t serial = 0;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            const ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            dirty = dirty || state.dirty;
            invalid = invalid || !state.valid;
            valid = valid && state.valid;
            if (state.dirtySerial != 0
                    && (serial == 0 || state.dirtySerial < serial)) {
                serial = state.dirtySerial;
            }
        }
        if (valid) ++shadowRenderStats.validLights;
        shadowRenderStats.occupiedTiles +=
                static_cast<std::size_t>(caster.shadowSlotCount);
        const SectorPreviewDynamicLightKind lightKind =
                caster.dynamicLightIndex >= 0
                        && static_cast<std::size_t>(caster.dynamicLightIndex)
                                < selectedLights.size()
                ? selectedLights[static_cast<std::size_t>(
                        caster.dynamicLightIndex)].kind
                : SectorPreviewDynamicLightKind::Spot;
        if (lightKind == SectorPreviewDynamicLightKind::Point) {
            ++shadowRenderStats.pointLights;
        } else if (lightKind == SectorPreviewDynamicLightKind::Rect) {
            ++shadowRenderStats.rectLights;
        } else {
            ++shadowRenderStats.spotLights;
        }
        if (!dirty) continue;
        ++shadowRenderStats.dirtyLights;
        pendingShadowLightUpdates.push_back(SectorDynamicShadowUpdateRequest{
                casterIndex, invalid, serial, caster.shadowSlotCount});
    }
    SortSectorDynamicShadowUpdateRequests(pendingShadowLightUpdates);
    shadowRenderStats.queuedLights = pendingShadowLightUpdates.size();
    if (pendingShadowLightUpdates.empty()) {
        shadowRenderStats.cacheHit = true;
        finishCpuTiming();
        return;
    }

    const std::size_t updateCount = SectorDynamicShadowUpdateCount(
            pendingShadowLightUpdates.size(), maxShadowLightUpdatesPerFrame);
    BeginTextureMode(*shadowMap);
    const bool fullClear = shadowAtlasNeedsFullClear;
    if (fullClear) {
        ClearBackground(WHITE);
        shadowAtlasNeedsFullClear = false;
    }
    shadowRenderStats.atlasRendered = true;
    rlEnableDepthTest();
    rlEnableScissorTest();

    for (std::size_t updateIndex = 0; updateIndex < updateCount; ++updateIndex) {
        const SectorPreviewDynamicSpotLightShadowCaster& updateCaster =
                shadowCasters[pendingShadowLightUpdates[updateIndex].casterIndex];
        bool lightCacheable = true;
        for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
            if (matrix.lightId != updateCaster.lightId
                    || matrix.dynamicLightIndex != updateCaster.dynamicLightIndex
                    || matrix.shadowSlot < updateCaster.shadowSlot
                    || matrix.shadowSlot >= updateCaster.shadowSlot
                            + updateCaster.shadowSlotCount) {
                continue;
            }
            if (matrix.shadowSlot < 0) {
                continue;
            }

            if (matrix.dynamicLightIndex < 0
                    || static_cast<std::size_t>(matrix.dynamicLightIndex)
                            >= selectedLights.size()) {
                continue;
            }
        const int tilesPerRow = DynamicShadowAtlasTilesPerRow;
        const int tileX = (matrix.shadowSlot % tilesPerRow)
                * effectiveShadowMapResolution;
        const int tileY = (matrix.shadowSlot / tilesPerRow)
                * effectiveShadowMapResolution;
        rlViewport(
                tileX, tileY,
                effectiveShadowMapResolution, effectiveShadowMapResolution);
        rlScissor(
                tileX, tileY,
                effectiveShadowMapResolution, effectiveShadowMapResolution);
        if (!fullClear) glClear(GL_DEPTH_BUFFER_BIT);

        ++shadowRenderStats.renderedTiles;
        const SectorPreviewDynamicPointLightUniform& light =
                selectedLights[static_cast<std::size_t>(matrix.dynamicLightIndex)];
        Material& activeMaterial = shadowMaterial;
        const int activeUseSkinningLoc = shadowUseSkinningLoc;
        const int activeAlphaTestLoc = -1;
        const int activeAlphaCutoffLoc = -1;
        const Texture2D activeDefaultTexture = shadowDefaultTexture;
        SetShaderValueMatrix(
                activeMaterial.shader,
                shadowLightViewProjectionLoc,
                matrix.lightViewProjection);
        SetShaderValueMatrix(
                spotShadowCutoutMaterial.shader,
                spotShadowCutoutLightViewProjectionLoc,
                matrix.lightViewProjection);
        const int noSkinning = 0;
        if (activeUseSkinningLoc >= 0) {
            SetShaderValue(
                    activeMaterial.shader,
                    activeUseSkinningLoc,
                    &noSkinning,
                    SHADER_UNIFORM_INT);
        }
        if (spotShadowCutoutUseSkinningLoc >= 0) {
            SetShaderValue(
                    spotShadowCutoutMaterial.shader,
                    spotShadowCutoutUseSkinningLoc,
                    &noSkinning,
                    SHADER_UNIFORM_INT);
        }
        for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
            const SectorReceiverBounds* bounds = FindSectorReceiverBounds(
                    context.sectorReceiverBounds, batch.sectorId);
            if (bounds != nullptr
                    && !ShadowLightIntersectsBounds(
                            light, matrix, ToSectorAabb3(*bounds))) {
                ++shadowRenderStats.sectorBatchesCulled;
                continue;
            }
            ++shadowRenderStats.sectorBatchesDrawn;
            const int alphaTest = batch.alphaTest ? 1 : 0;
            const float alphaCutoff = batch.alphaCutoff;
            const Texture2D* texture = nullptr;
            if (batch.alphaTest && context.textureResolver != nullptr) {
                texture = context.textureResolver(context.userData, *context.assets, batch.materialId);
                if (texture == nullptr || texture->id == 0) {
                    lightCacheable = false;
                }
            }
            Material& batchMaterial = batch.alphaTest
                    ? spotShadowCutoutMaterial : activeMaterial;
            const int batchAlphaTestLoc = batch.alphaTest
                    ? shadowAlphaTestLoc : activeAlphaTestLoc;
            const int batchAlphaCutoffLoc = batch.alphaTest
                    ? shadowAlphaCutoffLoc : activeAlphaCutoffLoc;
            const Texture2D batchDefaultTexture = batch.alphaTest
                    ? spotShadowCutoutDefaultTexture : activeDefaultTexture;
            batchMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                    ? *texture
                    : batchDefaultTexture;
            if (batchAlphaTestLoc >= 0) {
                SetShaderValue(
                        batchMaterial.shader,
                        batchAlphaTestLoc,
                        &alphaTest,
                        SHADER_UNIFORM_INT);
            }
            if (batchAlphaCutoffLoc >= 0) {
                SetShaderValue(
                        batchMaterial.shader,
                        batchAlphaCutoffLoc,
                        &alphaCutoff,
                        SHADER_UNIFORM_FLOAT);
            }
            DrawMesh(batch.mesh, batchMaterial, MatrixIdentity());
        }
        const int doorAlphaTest = 0;
        const float doorAlphaCutoff = 0.0f;
        activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = activeDefaultTexture;
        spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                spotShadowCutoutDefaultTexture;
        if (activeAlphaTestLoc >= 0) {
            SetShaderValue(
                    activeMaterial.shader,
                    activeAlphaTestLoc,
                    &doorAlphaTest,
                    SHADER_UNIFORM_INT);
        }
        if (activeAlphaCutoffLoc >= 0) {
            SetShaderValue(
                    activeMaterial.shader,
                    activeAlphaCutoffLoc,
                    &doorAlphaCutoff,
                    SHADER_UNIFORM_FLOAT);
        }
        if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                if (!ShadowLightIntersectsBounds(
                            light, matrix, DoorCasterBounds(caster))) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                float doorWidth = 0.0f;
                float doorHeight = 0.0f;
                const Mesh* doorMesh = context.doorMeshResolver(
                        context.doorMeshResolverUserData,
                        caster,
                        doorWidth,
                        doorHeight);
                if (doorMesh == nullptr || doorMesh->vertexCount <= 0) {
                    continue;
                }
                const Matrix shadowModel = BuildSectorDoorShadowCasterModelMatrix(
                        caster,
                        doorWidth,
                        doorHeight);
                DrawMesh(*doorMesh, activeMaterial, shadowModel);
                ++shadowRenderStats.objectCastersDrawn;
            }
        }
        if (context.doorModelShadowCasters != nullptr) {
            for (const SectorDoorModelShadowCaster& caster
                    : *context.doorModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    continue;
                }
                const Model& model = asset->model;
                const Matrix modelTransform = MatrixMultiply(
                        model.transform, caster.transform);
                const SectorAabb3 casterBounds = ToSectorAabb3(
                        TransformSectorDoorModelBounds(
                                asset->localBounds, modelTransform));
                if (!ShadowLightIntersectsBounds(
                            light, matrix, casterBounds)) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                ++shadowRenderStats.objectCastersDrawn;
                for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
                    if (model.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            model.meshes[meshIndex],
                            activeMaterial,
                            modelTransform);
                }
            }
        }
        if (context.staticModelShadowCasters != nullptr) {
            rlDisableBackfaceCulling();
            for (const SectorStaticModelShadowCaster& caster
                    : *context.staticModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    if (!context.assets->HasFailed(caster.model)) {
                        lightCacheable = false;
                    }
                    continue;
                }
                const Model& model = asset->model;
                const Matrix modelTransform = MatrixMultiply(
                        model.transform, caster.transform);
                const SectorAabb3 casterBounds = ToSectorAabb3(
                        TransformSectorDoorModelBounds(
                                asset->localBounds, modelTransform));
                if (!ShadowLightIntersectsBounds(
                            light, matrix, casterBounds)) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                ++shadowRenderStats.objectCastersDrawn;
                for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
                    if (model.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            model.meshes[meshIndex],
                            activeMaterial,
                            modelTransform);
                }
            }
            rlEnableBackfaceCulling();
        }
        if (context.dynamicModelShadowCasters != nullptr
                && context.runtimeObjectWorld != nullptr) {
            rlDisableBackfaceCulling();
            for (const SectorDynamicModelShadowCaster& caster
                    : *context.dynamicModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    if (!context.assets->HasFailed(caster.model)) {
                        lightCacheable = false;
                    }
                    continue;
                }
                const Matrix modelTransform = MatrixMultiply(
                        asset->model.transform,
                        caster.transform);
                const BoundingBox localBounds = asset->hasAnimatedLocalBounds
                        ? asset->animatedLocalBounds
                        : asset->localBounds;
                const SectorAabb3 casterBounds = ToSectorAabb3(
                        TransformSectorDoorModelBounds(
                                localBounds,
                                modelTransform));
                if (!ShadowLightIntersectsBounds(
                            light,
                            matrix,
                            casterBounds)) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                if (!context.runtimeObjectWorld->IsAlive(caster.entity)
                        || !context.runtimeObjectWorld
                                ->Has<engine::AnimatedModelInstance>(
                                        caster.entity)) {
                    lightCacheable = false;
                    continue;
                }
                engine::AnimatedModelInstance& instance =
                        context.runtimeObjectWorld
                                ->Get<engine::AnimatedModelInstance>(
                                        caster.entity);
                if (instance.model != caster.model
                        || !instance.poseReady
                        || instance.poseFailed) {
                    lightCacheable = false;
                    continue;
                }
                Model posedModel = engine::BuildAnimatedModelPoseView(
                        *asset,
                        instance);
                const bool canSkin = posedModel.skeleton.boneCount > 0
                        && posedModel.skeleton.boneCount
                                <= engine::MaxAnimatedModelBones
                        && posedModel.boneMatrices != nullptr
                        && activeMaterial.shader
                                   .locs[SHADER_LOC_MATRIX_BONETRANSFORMS]
                                >= 0;
                const int useSkinning = canSkin ? 1 : 0;
                if (activeUseSkinningLoc >= 0) {
                    SetShaderValue(
                            activeMaterial.shader,
                            activeUseSkinningLoc,
                            &useSkinning,
                            SHADER_UNIFORM_INT);
                }
                if (canSkin) {
                    rlEnableShader(activeMaterial.shader.id);
                    rlSetUniformMatrices(
                            activeMaterial.shader
                                    .locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                            posedModel.boneMatrices,
                            posedModel.skeleton.boneCount);
                }
                ++shadowRenderStats.objectCastersDrawn;
                for (int meshIndex = 0;
                        meshIndex < posedModel.meshCount;
                        ++meshIndex) {
                    if (posedModel.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            posedModel.meshes[meshIndex],
                            activeMaterial,
                            modelTransform);
                }
            }
            if (activeUseSkinningLoc >= 0) {
                SetShaderValue(
                        activeMaterial.shader,
                        activeUseSkinningLoc,
                        &noSkinning,
                        SHADER_UNIFORM_INT);
            }
            rlEnableBackfaceCulling();
        }
        activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = activeDefaultTexture;
        }
        for (int offset = 0; offset < updateCaster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    updateCaster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            state.valid = lightCacheable;
            state.dirty = !lightCacheable;
            state.dirtySerial = lightCacheable ? 0 : state.dirtySerial;
        }
        ++shadowRenderStats.updatedLights;
    }
    rlDisableScissorTest();
    rlDisableDepthTest();
    EndTextureMode();
    shadowRenderStats.validLights = 0;
    shadowRenderStats.dirtyLights = 0;
    for (const SectorPreviewDynamicSpotLightShadowCaster& caster
            : shadowCasters) {
        bool valid = true;
        bool dirty = false;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) {
                valid = false;
                continue;
            }
            valid = valid && shadowAtlasTileStates[slot].valid;
            dirty = dirty || shadowAtlasTileStates[slot].dirty;
        }
        if (valid) ++shadowRenderStats.validLights;
        if (dirty) ++shadowRenderStats.dirtyLights;
    }
    shadowRenderStats.queuedLights = shadowRenderStats.dirtyLights;
    finishCpuTiming();
}

void SectorDynamicLightingRenderer::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size() + 1);
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightKeys.clear();
    selectedLightKeys.reserve(MaxDynamicLights);
    cachedLightingStartSectorIds.reserve(6);
    cachedLightingPortalBlockers.reserve(16);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
    pendingShadowLightUpdates.clear();
    pendingShadowLightUpdates.reserve(MaxDynamicLights);
}

void SectorDynamicLightingRenderer::BuildReceiverBounds(
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    receiverBounds.clear();
    receiverBounds.insert(receiverBounds.end(), sectorReceiverBounds.begin(), sectorReceiverBounds.end());
    if (runtimeObjectWorld != nullptr) {
        CollectSectorDoorReceiverBounds(*runtimeObjectWorld, receiverBounds);
    }
}

} // namespace game
