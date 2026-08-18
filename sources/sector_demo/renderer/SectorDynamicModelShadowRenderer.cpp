#include "sector_demo/renderer/SectorDynamicModelShadowRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <rlgl.h>

namespace game {
namespace {

constexpr float ContactShadowOpacity = 0.20f;
constexpr float ContactFullOpacityGap = 0.05f;
constexpr float ContactFadeDistance = 1.0f;
constexpr float MinimumContactHalfExtent = 0.12f;

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

Vector3 NormalizeOr(Vector3 value, Vector3 fallback)
{
    const float lengthSquared = Vector3LengthSqr(value);
    return lengthSquared > 0.000001f
            ? Vector3Scale(value, 1.0f / std::sqrt(lengthSquared))
            : fallback;
}

struct WorldBounds {
    Vector3 min = {};
    Vector3 max = {};
};

WorldBounds TransformBounds(BoundingBox bounds, Matrix transform)
{
    WorldBounds result;
    result.min = Vector3{
            INFINITY,
            INFINITY,
            INFINITY};
    result.max = Vector3{
            -INFINITY,
            -INFINITY,
            -INFINITY};
    for (float x : {bounds.min.x, bounds.max.x}) {
        for (float y : {bounds.min.y, bounds.max.y}) {
            for (float z : {bounds.min.z, bounds.max.z}) {
                const Vector3 corner = Vector3Transform(
                        Vector3{x, y, z}, transform);
                result.min = Vector3Min(result.min, corner);
                result.max = Vector3Max(result.max, corner);
            }
        }
    }
    return result;
}

} // namespace

bool SectorDynamicModelShadowRenderer::Load()
{
    Shutdown();
    contactMaterial = LoadMaterialDefault();
    contactMaterial.shader = LoadShaderFromMemory(ContactVs, ContactFs);
    if (contactMaterial.shader.id == 0) {
        Shutdown();
        return false;
    }

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

    loaded = true;
    return true;
}

void SectorDynamicModelShadowRenderer::Shutdown()
{
    ClearSectorDynamicModelShadowCasters(shadowCasterCollection);
    if (contactMesh.vaoId != 0 || contactMesh.vertexCount != 0) {
        UnloadMesh(contactMesh);
    }
    contactMesh = {};
    if (contactMaterial.maps != nullptr) {
        UnloadMaterial(contactMaterial);
    }
    contactMaterial = {};
    contactOpacityLoc = -1;
    loaded = false;
}

void SectorDynamicModelShadowRenderer::ReserveShadowCasterCapacity(
        std::size_t capacity)
{
    ReserveSectorDynamicModelShadowCasters(
            shadowCasterCollection,
            capacity);
}

void SectorDynamicModelShadowRenderer::PrepareShadowRenderContext(
        SectorDynamicSpotLightShadowRenderContext& context,
        engine::World* runtimeObjectWorld)
{
    UpdateSectorDynamicModelShadowCasters(
            shadowCasterCollection,
            runtimeObjectWorld);
    context.runtimeObjectWorld = runtimeObjectWorld;
    context.dynamicModelShadowCasters = &shadowCasterCollection.casters;
    context.dynamicModelShadowCasterRevision = shadowCasterCollection.revision;
}

void SectorDynamicModelShadowRenderer::ClearPreparedShadowCasters()
{
    ClearSectorDynamicModelShadowCasters(shadowCasterCollection);
}

void SectorDynamicModelShadowRenderer::Draw(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (!loaded || context.assets == nullptr || context.world == nullptr) {
        return;
    }
    rlDrawRenderBatchActive();
    // Contact alpha is a temporary RGB blend factor. Preserve the opaque
    // world target alpha used by later HDR composition.
    rlColorMask(true, true, true, false);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();
    DrawContactShadows(context);
    rlDrawRenderBatchActive();
    rlDisableShader();
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlColorMask(true, true, true, true);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void SectorDynamicModelShadowRenderer::DrawContactShadows(
        const SectorDynamicModelShadowDrawContext& context)
{
    if (context.collisionWorld == nullptr || context.visibility == nullptr) {
        return;
    }
    context.world->ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDynamicModel,
            engine::AnimatedModelInstance>(
            [&](engine::Entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDynamicModel& dynamicModel,
                    engine::AnimatedModelInstance& instance) {
                if (!object.visible
                        || dynamicModel.shadowMode
                                != SectorDynamicModelShadowMode::Contact
                        || !ShouldDrawRuntimeSectorForVisibility(
                                object.currentSectorId,
                                *context.visibility)) {
                    return;
                }
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(instance.model);
                if (asset == nullptr) return;
                SectorCollisionHeights heights;
                if (!context.collisionWorld->GetSectorFloorCeiling(
                            object.currentSectorId,
                            &heights)) {
                    return;
                }
                const Matrix authoredTransform =
                        BuildSectorStaticModelAuthoredTransform(
                                transform.position,
                                transform.rotationXRadians,
                                transform.yawRadians,
                                transform.rotationZRadians,
                                dynamicModel.scale);
                const WorldBounds bounds = TransformBounds(
                        asset->localBounds,
                        authoredTransform);
                const Vector3 localCenter = Vector3Scale(
                        Vector3Add(
                                asset->localBounds.min,
                                asset->localBounds.max),
                        0.5f);
                const Vector3 footprintCenter = Vector3Transform(
                        localCenter,
                        authoredTransform);
                const Vector3 footprintAxisX = NormalizeOr(
                        RotateSectorStaticModelDirection(
                                Vector3{1.0f, 0.0f, 0.0f},
                                0.0f,
                                transform.yawRadians,
                                0.0f),
                        Vector3{1.0f, 0.0f, 0.0f});
                const Vector3 footprintAxisZ = NormalizeOr(
                        RotateSectorStaticModelDirection(
                                Vector3{0.0f, 0.0f, 1.0f},
                                0.0f,
                                transform.yawRadians,
                                0.0f),
                        Vector3{0.0f, 0.0f, 1.0f});
                float authoredHalfX = 0.0f;
                float authoredHalfZ = 0.0f;
                for (float x : {asset->localBounds.min.x, asset->localBounds.max.x}) {
                    for (float y : {asset->localBounds.min.y, asset->localBounds.max.y}) {
                        for (float z : {asset->localBounds.min.z, asset->localBounds.max.z}) {
                            const Vector3 corner = Vector3Transform(
                                    Vector3{x, y, z},
                                    authoredTransform);
                            const Vector3 offset = Vector3Subtract(
                                    corner,
                                    footprintCenter);
                            authoredHalfX = std::max(
                                    authoredHalfX,
                                    std::fabs(Vector3DotProduct(
                                            offset,
                                            footprintAxisX)));
                            authoredHalfZ = std::max(
                                    authoredHalfZ,
                                    std::fabs(Vector3DotProduct(
                                            offset,
                                            footprintAxisZ)));
                        }
                    }
                }
                const float gap = std::max(
                        0.0f,
                        bounds.min.y - heights.floorZ);
                if (gap >= ContactFadeDistance) return;
                const float fade = gap <= ContactFullOpacityGap
                        ? 1.0f
                        : 1.0f - (gap - ContactFullOpacityGap)
                                / (ContactFadeDistance
                                        - ContactFullOpacityGap);
                const float growth = 1.0f + 0.15f * std::clamp(
                        gap / ContactFadeDistance,
                        0.0f,
                        1.0f);
                const float halfX = std::max(
                        authoredHalfX,
                        MinimumContactHalfExtent) * growth;
                const float halfZ = std::max(
                        authoredHalfZ,
                        MinimumContactHalfExtent) * growth;
                const float opacity = ContactShadowOpacity * fade;
                SetShaderValue(
                        contactMaterial.shader,
                        contactOpacityLoc,
                        &opacity,
                        SHADER_UNIFORM_FLOAT);
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
}

} // namespace game
