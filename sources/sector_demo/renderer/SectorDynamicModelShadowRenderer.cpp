#include "sector_demo/renderer/SectorDynamicModelShadowRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <raymath.h>
#include <rlgl.h>

namespace game {
namespace {

constexpr float ProjectedShadowPadding = 1.2f;
constexpr float ProjectedShadowMaximumDistance = 3.0f;
constexpr float ContactShadowOpacity = 0.20f;
constexpr float ContactFullOpacityGap = 0.05f;
constexpr float ContactFadeDistance = 1.0f;
constexpr float MinimumContactHalfExtent = 0.12f;
constexpr float DegreesToRadians = PI / 180.0f;

const char* CasterVs = R"(
#version 330
in vec3 vertexPosition;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
uniform mat4 matModel;
uniform mat4 lightViewProjection;
uniform int useSkinning;
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];
void main()
{
    vec4 position = vec4(vertexPosition, 1.0);
    if (useSkinning != 0) {
        int b0 = int(vertexBoneIndices.x);
        int b1 = int(vertexBoneIndices.y);
        int b2 = int(vertexBoneIndices.z);
        int b3 = int(vertexBoneIndices.w);
        position = vertexBoneWeights.x * (boneMatrices[b0] * position)
                + vertexBoneWeights.y * (boneMatrices[b1] * position)
                + vertexBoneWeights.z * (boneMatrices[b2] * position)
                + vertexBoneWeights.w * (boneMatrices[b3] * position);
    }
    gl_Position = lightViewProjection * matModel * position;
}
)";

const char* CasterFs = R"(
#version 330
void main() {}
)";

const char* ReceiverVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 texCoord;
void main()
{
    vec4 world = matModel * vec4(vertexPosition, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    texCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* ReceiverFs = R"(
#version 330
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texCoord;
uniform sampler2D receiverTexture;
uniform sampler2D shadowTexture;
uniform mat4 lightViewProjection;
uniform vec3 lightPosition;
uniform vec3 lightDirection;
uniform vec3 casterCenter;
uniform int directional;
uniform float maximumDistance;
uniform int alphaTest;
uniform float alphaCutoff;
out vec4 finalColor;
void main()
{
    if (alphaTest != 0 && texture(receiverTexture, texCoord).a < alphaCutoff) discard;
    vec4 clip = lightViewProjection * vec4(worldPosition, 1.0);
    if (clip.w <= 0.0) discard;
    vec3 projected = clip.xyz / clip.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0
            || projected.y <= 0.0 || projected.y >= 1.0
            || projected.z <= 0.0 || projected.z >= 1.0) discard;
    vec3 toLight = directional != 0
            ? normalize(lightDirection)
            : normalize(lightPosition - worldPosition);
    float facing = smoothstep(0.0, 0.18, dot(normalize(worldNormal), toLight));
    if (facing <= 0.0) discard;
    vec2 texel = 1.0 / vec2(textureSize(shadowTexture, 0));
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float depth = texture(shadowTexture, projected.xy + vec2(x, y) * texel).r;
            visible += projected.z - 0.0015 > depth ? 1.0 : 0.0;
        }
    }
    visible /= 9.0;
    float distanceFromCaster = length(worldPosition - casterCenter);
    float fadeStart = maximumDistance * 0.75;
    float distanceFade = 1.0 - smoothstep(fadeStart, maximumDistance, distanceFromCaster);
    float alpha = visible * facing * distanceFade * 0.30;
    if (alpha <= 0.002) discard;
    finalColor = vec4(0.0, 0.0, 0.0, alpha);
}
)";

const char* ContactVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 mvp;
out vec2 texCoord;
void main()
{
    texCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* ContactFs = R"(
#version 330
in vec2 texCoord;
uniform float opacity;
out vec4 finalColor;
void main()
{
    vec2 p = texCoord * 2.0 - 1.0;
    float radius = dot(p, p);
    float alpha = (1.0 - smoothstep(0.35, 1.0, radius)) * opacity;
    if (alpha <= 0.002) discard;
    finalColor = vec4(0.0, 0.0, 0.0, alpha);
}
)";

RenderTexture2D LoadDepthTarget(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;
    if (target.id == 0) return target;

    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(
            target.id, target.depth.id,
            RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(target.id)) {
        rlDisableFramebuffer();
        rlUnloadFramebuffer(target.id);
        return RenderTexture2D{};
    }
    rlDisableFramebuffer();
    SetTextureFilter(target.depth, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(target.depth, TEXTURE_WRAP_CLAMP);
    return target;
}

void UnloadDepthTarget(RenderTexture2D& target)
{
    if (target.id != 0) rlUnloadFramebuffer(target.id);
    target = {};
}

Vector3 NormalizeOr(Vector3 value, Vector3 fallback)
{
    const float lengthSquared = Vector3LengthSqr(value);
    return lengthSquared > 0.000001f
            ? Vector3Scale(value, 1.0f / std::sqrt(lengthSquared))
            : fallback;
}

Vector3 SafeUp(Vector3 direction)
{
    return std::fabs(Vector3DotProduct(direction, Vector3{0.0f, 1.0f, 0.0f})) > 0.95f
            ? Vector3{1.0f, 0.0f, 0.0f}
            : Vector3{0.0f, 1.0f, 0.0f};
}

struct WorldBounds {
    Vector3 min = {};
    Vector3 max = {};
    Vector3 center = {};
    float radius = 0.0f;
};

WorldBounds TransformBounds(BoundingBox bounds, Matrix transform)
{
    WorldBounds result;
    result.min = Vector3{INFINITY, INFINITY, INFINITY};
    result.max = Vector3{-INFINITY, -INFINITY, -INFINITY};
    for (float x : {bounds.min.x, bounds.max.x}) {
        for (float y : {bounds.min.y, bounds.max.y}) {
            for (float z : {bounds.min.z, bounds.max.z}) {
                const Vector3 point = Vector3Transform(Vector3{x, y, z}, transform);
                result.min = Vector3Min(result.min, point);
                result.max = Vector3Max(result.max, point);
            }
        }
    }
    result.center = Vector3Scale(Vector3Add(result.min, result.max), 0.5f);
    result.radius = 0.5f * Vector3Length(Vector3Subtract(result.max, result.min));
    return result;
}

bool SameOrAdjacentSector(
        const SectorCollisionWorld* collisionWorld,
        int a,
        int b)
{
    if (a <= 0 || b <= 0 || a == b) return a == b || a <= 0 || b <= 0;
    if (collisionWorld == nullptr) return true;
    const std::vector<int>* neighbors = collisionWorld->GetPortalNeighbors(a);
    return neighbors != nullptr
            && std::find(neighbors->begin(), neighbors->end(), b) != neighbors->end();
}

float ColorMaximum(Vector3 color)
{
    return std::max(color.x, std::max(color.y, color.z));
}

} // namespace

bool SectorDynamicModelShadowRenderer::Load()
{
    Shutdown();
    candidates.reserve(kSectorRuntimeObjectInitialCapacity);
    lightSources.reserve(64);

    casterMaterial = LoadMaterialDefault();
    casterMaterial.shader = LoadShaderFromMemory(CasterVs, CasterFs);
    receiverMaterial = LoadMaterialDefault();
    receiverDefaultTexture = receiverMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    receiverMaterial.shader = LoadShaderFromMemory(ReceiverVs, ReceiverFs);
    contactMaterial = LoadMaterialDefault();
    contactMaterial.shader = LoadShaderFromMemory(ContactVs, ContactFs);
    if (contactMaterial.shader.id == 0) {
        Shutdown();
        return false;
    }

    loaded = true;
    contactMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(contactMaterial.shader, "vertexPosition");
    contactMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(contactMaterial.shader, "vertexTexCoord");
    contactMaterial.shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(contactMaterial.shader, "mvp");
    contactOpacityLoc = GetShaderLocation(contactMaterial.shader, "opacity");
    contactMesh = GenMeshPlane(2.0f, 2.0f, 1, 1);
    if (contactMesh.vertexCount <= 0) {
        Shutdown();
        return false;
    }

    if (casterMaterial.shader.id == 0 || receiverMaterial.shader.id == 0) {
        if (casterMaterial.maps != nullptr) UnloadMaterial(casterMaterial);
        if (receiverMaterial.maps != nullptr) UnloadMaterial(receiverMaterial);
        casterMaterial = {};
        receiverMaterial = {};
        std::fprintf(stderr,
                "[SectorDynamicModelShadowRenderer WARNING] projected shadow shaders unavailable; using contact shadows\n");
        return true;
    }

    casterMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(casterMaterial.shader, "vertexPosition");
    casterMaterial.shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
            GetShaderLocationAttrib(casterMaterial.shader, "vertexBoneIndices");
    casterMaterial.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
            GetShaderLocationAttrib(casterMaterial.shader, "vertexBoneWeights");
    casterMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(casterMaterial.shader, "matModel");
    casterMaterial.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
            GetShaderLocation(casterMaterial.shader, "boneMatrices");
    casterUseSkinningLoc = GetShaderLocation(casterMaterial.shader, "useSkinning");
    casterLightViewProjectionLoc =
            GetShaderLocation(casterMaterial.shader, "lightViewProjection");

    receiverMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(receiverMaterial.shader, "vertexPosition");
    receiverMaterial.shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(receiverMaterial.shader, "vertexNormal");
    receiverMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(receiverMaterial.shader, "vertexTexCoord");
    receiverMaterial.shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(receiverMaterial.shader, "mvp");
    receiverMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(receiverMaterial.shader, "matModel");
    receiverMaterial.shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(receiverMaterial.shader, "matNormal");
    receiverMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(receiverMaterial.shader, "receiverTexture");
    receiverMaterial.shader.locs[SHADER_LOC_MAP_SPECULAR] =
            GetShaderLocation(receiverMaterial.shader, "shadowTexture");
    receiverLightViewProjectionLoc =
            GetShaderLocation(receiverMaterial.shader, "lightViewProjection");
    receiverLightPositionLoc = GetShaderLocation(receiverMaterial.shader, "lightPosition");
    receiverLightDirectionLoc = GetShaderLocation(receiverMaterial.shader, "lightDirection");
    receiverCasterCenterLoc = GetShaderLocation(receiverMaterial.shader, "casterCenter");
    receiverDirectionalLoc = GetShaderLocation(receiverMaterial.shader, "directional");
    receiverMaximumDistanceLoc = GetShaderLocation(receiverMaterial.shader, "maximumDistance");
    receiverAlphaTestLoc = GetShaderLocation(receiverMaterial.shader, "alphaTest");
    receiverAlphaCutoffLoc = GetShaderLocation(receiverMaterial.shader, "alphaCutoff");

    for (Slot& slot : slots) {
        slot.target = LoadDepthTarget(
                DynamicModelProjectedShadowResolution,
                DynamicModelProjectedShadowResolution);
        if (slot.target.id == 0 || slot.target.depth.id == 0) {
            for (Slot& cleanup : slots) UnloadDepthTarget(cleanup.target);
            if (casterMaterial.maps != nullptr) UnloadMaterial(casterMaterial);
            if (receiverMaterial.maps != nullptr) UnloadMaterial(receiverMaterial);
            casterMaterial = {};
            receiverMaterial = {};
            std::fprintf(stderr,
                    "[SectorDynamicModelShadowRenderer WARNING] projected shadow targets unavailable; using contact shadows\n");
            return true;
        }
    }
    projectedLoaded = true;
    return true;
}

void SectorDynamicModelShadowRenderer::Shutdown()
{
    ResetBorrowedReceiverTextures();
    for (Slot& slot : slots) UnloadDepthTarget(slot.target);
    if (contactMesh.vaoId != 0 || contactMesh.vertexCount != 0) UnloadMesh(contactMesh);
    contactMesh = {};
    if (casterMaterial.maps != nullptr) UnloadMaterial(casterMaterial);
    if (receiverMaterial.maps != nullptr) UnloadMaterial(receiverMaterial);
    if (contactMaterial.maps != nullptr) UnloadMaterial(contactMaterial);
    casterMaterial = {};
    receiverMaterial = {};
    contactMaterial = {};
    receiverDefaultTexture = {};
    lightSources.clear();
    candidates.clear();
    activeSlotCount = 0;
    loaded = false;
    projectedLoaded = false;
}

void SectorDynamicModelShadowRenderer::ResetBorrowedReceiverTextures()
{
    if (receiverMaterial.maps == nullptr) return;

    // DrawMesh material maps are owning from UnloadMaterial's perspective. The
    // receiver texture belongs to AssetManager and the depth texture belongs to
    // its framebuffer, so neither borrowed binding may survive the draw pass.
    receiverMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = receiverDefaultTexture;
    receiverMaterial.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
}

void SectorDynamicModelShadowRenderer::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld)
{
    lightSources.clear();
    fallbackDirectionToLight = NormalizeOr(
            map.directionalLight.directionToLight,
            Vector3{-0.35f, 0.80f, -0.25f});

    const auto sectorFor = [collisionWorld](Vector3 position) {
        return collisionWorld != nullptr
                ? collisionWorld->FindSectorContainingPoint(Vector2{position.x, position.z})
                : -1;
    };
    const auto color = [](Color value) {
        return engine::SrgbColorBytesToLinearSceneRgb(value);
    };
    for (const SectorTopologyStaticPointLight& source : map.staticLights) {
        const Vector3 position = SectorAuthoringToWorldPosition(source.position);
        lightSources.push_back(LightSource{source.id, sectorFor(position), LightKind::Point,
                position, {}, color(source.color), source.intensity,
                SectorAuthoringToWorldDistance(source.radius), 1.0f, -1.0f});
    }
    for (const SectorTopologyDynamicPointLight& source : map.dynamicPointLights) {
        if (!source.enabled) continue;
        const Vector3 position = SectorAuthoringToWorldPosition(source.position);
        lightSources.push_back(LightSource{source.id, sectorFor(position), LightKind::Point,
                position, {}, color(source.color), source.intensity,
                SectorAuthoringToWorldDistance(source.radius), 1.0f, -1.0f});
    }
    const auto addSpot = [&](int id, Vector3 authoredPosition, Vector3 authoredTarget,
                             Color sourceColor, float intensity, float authoredRange,
                             float innerDegrees, float outerDegrees) {
        const Vector3 position = SectorAuthoringToWorldPosition(authoredPosition);
        const Vector3 target = SectorAuthoringToWorldPosition(authoredTarget);
        lightSources.push_back(LightSource{id, sectorFor(position), LightKind::Spot,
                position, NormalizeOr(Vector3Subtract(target, position), Vector3{0.0f, -1.0f, 0.0f}),
                color(sourceColor), intensity, SectorAuthoringToWorldDistance(authoredRange),
                std::cos(innerDegrees * DegreesToRadians),
                std::cos(outerDegrees * DegreesToRadians)});
    };
    for (const SectorTopologyStaticSpotLight& source : map.staticSpotLights) {
        addSpot(source.id, source.position, source.target, source.color, source.intensity,
                source.range, source.innerConeDegrees, source.outerConeDegrees);
    }
    for (const SectorTopologyDynamicSpotLight& source : map.dynamicSpotLights) {
        if (source.enabled) addSpot(source.id, source.position, source.target, source.color,
                source.intensity, source.range, source.innerConeDegrees, source.outerConeDegrees);
    }
}

void SectorDynamicModelShadowRenderer::RenderShadowMaps(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (!loaded || context.assets == nullptr || context.world == nullptr || context.camera == nullptr) return;
    previousPlacedObjectIds.fill(0);
    previousLightSourceIds.fill(-1);
    for (std::size_t i = 0; i < activeSlotCount; ++i) {
        previousPlacedObjectIds[i] = slots[i].placedObjectId;
        previousLightSourceIds[i] = slots[i].lightSourceId;
        previousLightSourceKinds[i] = slots[i].lightSourceKind;
    }
    for (Slot& slot : slots) slot.active = false;
    activeSlotCount = 0;
    candidates.clear();
    if (!projectedLoaded) return;

    context.world->ForEach<SectorObjectTransform, SectorObject, SectorDynamicModel, engine::AnimatedModelInstance>(
            [&](engine::Entity entity, SectorObjectTransform& transform, SectorObject& object,
                    SectorDynamicModel& dynamicModel, engine::AnimatedModelInstance& instance) {
                if (!object.visible
                        || dynamicModel.shadowMode != SectorDynamicModelShadowMode::ProjectedSilhouette
                        || !instance.poseReady || instance.poseFailed) return;
                if (context.visibility != nullptr
                        && !ShouldDrawRuntimeSectorForVisibility(object.currentSectorId, *context.visibility)) return;
                const engine::ModelAsset* asset = context.assets->GetModelAsset(instance.model);
                if (asset == nullptr) return;
                const Matrix transformMatrix = BuildSectorStaticModelAuthoredTransform(
                        transform.position, transform.rotationXRadians, transform.yawRadians,
                        transform.rotationZRadians, dynamicModel.scale);
                const WorldBounds bounds = TransformBounds(asset->localBounds, transformMatrix);
                const Vector3 cameraForward = NormalizeOr(
                        Vector3Subtract(context.camera->target, context.camera->position),
                        Vector3{0.0f, 0.0f, -1.0f});
                const float forwardDistance = Vector3DotProduct(
                        Vector3Subtract(bounds.center, context.camera->position),
                        cameraForward);
                if (forwardDistance + bounds.radius <= 0.0f) return;
                const float distanceSquared = std::max(
                        Vector3DistanceSqr(context.camera->position, bounds.center), 0.01f);
                float priority = bounds.radius * bounds.radius / distanceSquared;
                if (std::find(previousPlacedObjectIds.begin(), previousPlacedObjectIds.end(),
                              dynamicModel.placedObjectId) != previousPlacedObjectIds.end()) {
                    priority *= 1.2f;
                }
                if (candidates.size() < candidates.capacity()) {
                    candidates.push_back(Candidate{
                            entity, dynamicModel.placedObjectId, object.currentSectorId, priority});
                }
            });

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.placedObjectId < b.placedObjectId;
    });

    const std::size_t selectedCount = std::min(candidates.size(), slots.size());
    for (std::size_t i = 0; i < selectedCount; ++i) {
        Candidate& candidate = candidates[i];
        if (!context.world->IsAlive(candidate.entity)) continue;
        auto& transform = context.world->Get<SectorObjectTransform>(candidate.entity);
        auto& dynamicModel = context.world->Get<SectorDynamicModel>(candidate.entity);
        auto& instance = context.world->Get<engine::AnimatedModelInstance>(candidate.entity);
        const engine::ModelAsset* asset = context.assets->GetModelAsset(instance.model);
        if (asset == nullptr) continue;
        bool hasReceiver = false;
        if (context.sectorDrawRecords != nullptr && context.visibility != nullptr) {
            for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
                if (ShouldDrawSectorMeshRecordForVisibility(batch, *context.visibility)
                        && SameOrAdjacentSector(
                                context.collisionWorld, candidate.sectorId, batch.sectorId)) {
                    hasReceiver = true;
                    break;
                }
            }
        }
        if (!hasReceiver) continue;

        const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
                transform.position, transform.rotationXRadians, transform.yawRadians,
                transform.rotationZRadians, dynamicModel.scale);
        const WorldBounds bounds = TransformBounds(asset->localBounds, authoredTransform);
        const float paddedRadius = std::max(bounds.radius * ProjectedShadowPadding, 0.15f);

        const LightSource* best = nullptr;
        float bestScore = 0.0f;
        const LightSource* previousSource = nullptr;
        float previousScore = 0.0f;
        int previousSourceId = -1;
        LightKind previousSourceKind = LightKind::Point;
        for (std::size_t previousIndex = 0;
                previousIndex < previousPlacedObjectIds.size();
                ++previousIndex) {
            if (previousPlacedObjectIds[previousIndex] == candidate.placedObjectId) {
                previousSourceId = previousLightSourceIds[previousIndex];
                previousSourceKind = previousLightSourceKinds[previousIndex];
                break;
            }
        }
        for (const LightSource& source : lightSources) {
            if (!SameOrAdjacentSector(context.collisionWorld, candidate.sectorId, source.sectorId)) continue;
            const Vector3 lightToCaster = Vector3Subtract(bounds.center, source.position);
            const float distance = Vector3Length(lightToCaster);
            if (distance <= paddedRadius + 0.05f || distance >= source.range || source.range <= 0.0f) continue;
            const float attenuation = 1.0f - distance / source.range;
            float cone = 1.0f;
            if (source.kind == LightKind::Spot) {
                const float cosine = Vector3DotProduct(
                        NormalizeOr(lightToCaster, source.direction), source.direction);
                if (cosine <= source.outerConeCos) continue;
                const float divisor = std::max(source.innerConeCos - source.outerConeCos, 0.0001f);
                cone = std::clamp((cosine - source.outerConeCos) / divisor, 0.0f, 1.0f);
                cone = cone * cone * (3.0f - 2.0f * cone);
            }
            const float score = source.intensity * ColorMaximum(source.color)
                    * attenuation * attenuation * cone;
            if (source.id == previousSourceId && source.kind == previousSourceKind) {
                previousSource = &source;
                previousScore = score;
            }
            if (score > bestScore || (score == bestScore && best != nullptr && source.id < best->id)) {
                best = &source;
                bestScore = score;
            }
        }
        if (previousSource != nullptr
                && best != previousSource
                && bestScore <= previousScore * 1.2f) {
            best = previousSource;
            bestScore = previousScore;
        }

        Slot& slot = slots[activeSlotCount];
        Matrix view = MatrixIdentity();
        Matrix projection = MatrixIdentity();
        if (best != nullptr) {
            const Vector3 toCaster = Vector3Subtract(bounds.center, best->position);
            const float distance = Vector3Length(toCaster);
            const Vector3 direction = NormalizeOr(toCaster, Vector3{0.0f, -1.0f, 0.0f});
            const float ratio = std::clamp(paddedRadius / distance, 0.01f, 0.86f);
            const float fov = std::clamp(2.0f * std::asin(ratio), 5.0f * DegreesToRadians, 120.0f * DegreesToRadians);
            view = MatrixLookAt(best->position, bounds.center, SafeUp(direction));
            projection = MatrixPerspective(fov, 1.0,
                    std::max(0.05f, distance - paddedRadius),
                    distance + paddedRadius + ProjectedShadowMaximumDistance);
            slot.lightPosition = best->position;
            slot.lightDirection = direction;
            slot.directional = false;
            slot.lightSourceId = best->id;
            slot.lightSourceKind = best->kind;
        } else {
            const Vector3 directionToLight = NormalizeOr(
                    fallbackDirectionToLight, Vector3{-0.35f, 0.80f, -0.25f});
            const float extent = paddedRadius + ProjectedShadowMaximumDistance;
            const Vector3 eye = Vector3Add(bounds.center, Vector3Scale(directionToLight, extent));
            view = MatrixLookAt(eye, bounds.center, SafeUp(Vector3Negate(directionToLight)));
            projection = MatrixOrtho(-extent, extent, -extent, extent, 0.05f, extent * 3.0f);
            slot.lightPosition = eye;
            slot.lightDirection = directionToLight;
            slot.directional = true;
            slot.lightSourceId = -1;
        }
        slot.lightViewProjection = MatrixMultiply(view, projection);
        slot.entity = candidate.entity;
        slot.placedObjectId = candidate.placedObjectId;
        slot.sectorId = candidate.sectorId;
        slot.casterCenter = bounds.center;
        slot.maximumDistance = ProjectedShadowMaximumDistance;

        Model posedModel = engine::BuildAnimatedModelPoseView(*asset, instance);
        if (posedModel.meshCount <= 0 || posedModel.meshes == nullptr) continue;
        const bool canSkin = posedModel.skeleton.boneCount > 0
                && posedModel.skeleton.boneCount <= engine::MaxAnimatedModelBones
                && posedModel.boneMatrices != nullptr
                && casterMaterial.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0;
        const int useSkinning = canSkin ? 1 : 0;
        SetShaderValue(casterMaterial.shader, casterUseSkinningLoc, &useSkinning, SHADER_UNIFORM_INT);
        SetShaderValueMatrix(casterMaterial.shader, casterLightViewProjectionLoc, slot.lightViewProjection);
        if (canSkin) {
            rlEnableShader(casterMaterial.shader.id);
            rlSetUniformMatrices(
                    casterMaterial.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                    posedModel.boneMatrices,
                    posedModel.skeleton.boneCount);
        }

        BeginTextureMode(slot.target);
        ClearBackground(WHITE);
        rlDisableColorBlend();
        rlEnableDepthTest();
        rlEnableDepthMask();
        const Matrix modelTransform = MatrixMultiply(posedModel.transform, authoredTransform);
        for (int meshIndex = 0; meshIndex < posedModel.meshCount; ++meshIndex) {
            DrawMesh(posedModel.meshes[meshIndex], casterMaterial, modelTransform);
        }
        rlEnableColorBlend();
        EndTextureMode();
        slot.active = true;
        ++activeSlotCount;
    }
}

bool SectorDynamicModelShadowRenderer::IsObjectAssigned(engine::Entity entity) const
{
    for (std::size_t i = 0; i < activeSlotCount; ++i) {
        if (slots[i].active && slots[i].entity == entity) return true;
    }
    return false;
}

void SectorDynamicModelShadowRenderer::Draw(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (!loaded || context.assets == nullptr || context.world == nullptr) return;
    DrawProjectedShadows(context);
    DrawContactShadows(context);
}

void SectorDynamicModelShadowRenderer::DrawProjectedShadows(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (!projectedLoaded || context.sectorDrawRecords == nullptr || context.visibility == nullptr) return;
    rlDisableDepthMask();
    rlSetBlendMode(BLEND_ALPHA);
    for (std::size_t i = 0; i < activeSlotCount; ++i) {
        const Slot& slot = slots[i];
        if (!slot.active) continue;
        receiverMaterial.maps[MATERIAL_MAP_SPECULAR].texture = slot.target.depth;
        SetShaderValueMatrix(receiverMaterial.shader, receiverLightViewProjectionLoc, slot.lightViewProjection);
        SetShaderValue(receiverMaterial.shader, receiverLightPositionLoc, &slot.lightPosition, SHADER_UNIFORM_VEC3);
        SetShaderValue(receiverMaterial.shader, receiverLightDirectionLoc, &slot.lightDirection, SHADER_UNIFORM_VEC3);
        SetShaderValue(receiverMaterial.shader, receiverCasterCenterLoc, &slot.casterCenter, SHADER_UNIFORM_VEC3);
        const int directional = slot.directional ? 1 : 0;
        SetShaderValue(receiverMaterial.shader, receiverDirectionalLoc, &directional, SHADER_UNIFORM_INT);
        SetShaderValue(receiverMaterial.shader, receiverMaximumDistanceLoc, &slot.maximumDistance, SHADER_UNIFORM_FLOAT);
        for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
            if (!ShouldDrawSectorMeshRecordForVisibility(batch, *context.visibility)
                    || !SameOrAdjacentSector(context.collisionWorld, slot.sectorId, batch.sectorId)) continue;
            const Texture2D* texture = context.textureResolver != nullptr
                    ? context.textureResolver(context.textureResolverUserData, *context.assets, batch.textureId)
                    : nullptr;
            receiverMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                    texture != nullptr ? *texture : receiverDefaultTexture;
            const int alphaTest = batch.alphaTest ? 1 : 0;
            SetShaderValue(receiverMaterial.shader, receiverAlphaTestLoc, &alphaTest, SHADER_UNIFORM_INT);
            SetShaderValue(receiverMaterial.shader, receiverAlphaCutoffLoc, &batch.alphaCutoff, SHADER_UNIFORM_FLOAT);
            DrawMesh(batch.mesh, receiverMaterial, MatrixIdentity());
        }
    }
    ResetBorrowedReceiverTextures();
    rlEnableDepthMask();
}

void SectorDynamicModelShadowRenderer::DrawContactShadows(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (context.collisionWorld == nullptr || context.visibility == nullptr) return;
    rlDisableDepthMask();
    rlSetBlendMode(BLEND_ALPHA);
    context.world->ForEach<SectorObjectTransform, SectorObject, SectorDynamicModel, engine::AnimatedModelInstance>(
            [&](engine::Entity entity, SectorObjectTransform& transform, SectorObject& object,
                    SectorDynamicModel& dynamicModel, engine::AnimatedModelInstance& instance) {
                const bool projectedFallback = dynamicModel.shadowMode
                                == SectorDynamicModelShadowMode::ProjectedSilhouette
                        && !IsObjectAssigned(entity);
                if (!object.visible
                        || (dynamicModel.shadowMode != SectorDynamicModelShadowMode::Contact
                                && !projectedFallback)
                        || !ShouldDrawRuntimeSectorForVisibility(object.currentSectorId, *context.visibility)) return;
                const engine::ModelAsset* asset = context.assets->GetModelAsset(instance.model);
                if (asset == nullptr) return;
                SectorCollisionHeights heights;
                if (!context.collisionWorld->GetSectorFloorCeiling(object.currentSectorId, &heights)) return;
                const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
                        transform.position, transform.rotationXRadians, transform.yawRadians,
                        transform.rotationZRadians, dynamicModel.scale);
                const WorldBounds bounds = TransformBounds(asset->localBounds, authoredTransform);
                const Vector3 localCenter = Vector3Scale(
                        Vector3Add(asset->localBounds.min, asset->localBounds.max), 0.5f);
                const Vector3 footprintCenter = Vector3Transform(localCenter, authoredTransform);
                const Vector3 footprintAxisX = NormalizeOr(
                        RotateSectorStaticModelDirection(
                                Vector3{1.0f, 0.0f, 0.0f},
                                0.0f, transform.yawRadians, 0.0f),
                        Vector3{1.0f, 0.0f, 0.0f});
                const Vector3 footprintAxisZ = NormalizeOr(
                        RotateSectorStaticModelDirection(
                                Vector3{0.0f, 0.0f, 1.0f},
                                0.0f, transform.yawRadians, 0.0f),
                        Vector3{0.0f, 0.0f, 1.0f});
                float authoredHalfX = 0.0f;
                float authoredHalfZ = 0.0f;
                for (float x : {asset->localBounds.min.x, asset->localBounds.max.x}) {
                    for (float y : {asset->localBounds.min.y, asset->localBounds.max.y}) {
                        for (float z : {asset->localBounds.min.z, asset->localBounds.max.z}) {
                            const Vector3 corner = Vector3Transform(Vector3{x, y, z}, authoredTransform);
                            const Vector3 offset = Vector3Subtract(corner, footprintCenter);
                            authoredHalfX = std::max(
                                    authoredHalfX,
                                    std::fabs(Vector3DotProduct(offset, footprintAxisX)));
                            authoredHalfZ = std::max(
                                    authoredHalfZ,
                                    std::fabs(Vector3DotProduct(offset, footprintAxisZ)));
                        }
                    }
                }
                const float gap = std::max(0.0f, bounds.min.y - heights.floorZ);
                if (gap >= ContactFadeDistance) return;
                const float fade = gap <= ContactFullOpacityGap
                        ? 1.0f
                        : 1.0f - (gap - ContactFullOpacityGap)
                                / (ContactFadeDistance - ContactFullOpacityGap);
                const float growth = 1.0f + 0.15f * std::clamp(gap / ContactFadeDistance, 0.0f, 1.0f);
                const float halfX = std::max(authoredHalfX, MinimumContactHalfExtent) * growth;
                const float halfZ = std::max(authoredHalfZ, MinimumContactHalfExtent) * growth;
                const float opacity = ContactShadowOpacity * fade;
                SetShaderValue(contactMaterial.shader, contactOpacityLoc, &opacity, SHADER_UNIFORM_FLOAT);
                const Matrix model = MatrixMultiply(
                        MatrixScale(halfX, 1.0f, halfZ),
                        MatrixMultiply(
                                MatrixRotateY(transform.yawRadians),
                                MatrixTranslate(
                                        footprintCenter.x,
                                        heights.floorZ + 0.006f,
                                        footprintCenter.z)));
                DrawMesh(contactMesh, contactMaterial, model);
            });
    rlEnableDepthMask();
}

} // namespace game
