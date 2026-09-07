#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorBillboardRenderer.h"


#include "engine/assets/AssetManager.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/ecs/World.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorBillboardRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

namespace game {

namespace {

int GetShaderLocationArrayBase(Shader shader, const char* name)
{
    const int location = GetShaderLocation(shader, name);
    if (location >= 0) {
        return location;
    }

    const std::string indexedName = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexedName.c_str());
}

int GetShaderLocationArrayElement(Shader shader, const char* name, std::size_t index)
{
    const std::string indexedName = std::string(name) + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

void AppendBillboardRenderDebugText(std::string& renderDebugText, const std::string& billboardText)
{
    const size_t existing = renderDebugText.find(" | billboards:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!billboardText.empty() && !renderDebugText.empty()) {
        renderDebugText += " | " + billboardText;
    }
}

uint32_t BillboardPlaybackFrameCount(const engine::SpriteClip& clip)
{
    if (clip.playback == engine::SpritePlaybackMode::PingPong && clip.frameCount > 1) {
        return clip.frameCount * 2 - 2;
    }

    return clip.frameCount;
}

uint32_t BillboardPlaybackFrameOffset(const engine::SpriteClip& clip, uint32_t frameInClip)
{
    if (clip.frameCount == 0) {
        return 0;
    }

    switch (clip.playback) {
        case engine::SpritePlaybackMode::Reverse:
            return clip.frameCount - 1 - std::min(frameInClip, clip.frameCount - 1);
        case engine::SpritePlaybackMode::PingPong: {
            if (clip.frameCount == 1) {
                return 0;
            }

            const uint32_t period = BillboardPlaybackFrameCount(clip);
            const uint32_t sequenceFrame = frameInClip % period;
            if (sequenceFrame < clip.frameCount) {
                return sequenceFrame;
            }
            return period - sequenceFrame;
        }
        case engine::SpritePlaybackMode::Once:
        case engine::SpritePlaybackMode::Loop:
            return std::min(frameInClip, clip.frameCount - 1);
    }

    return 0;
}

uint32_t ResolveBillboardFrameIndexAtTime(
        const engine::SpriteAnimationAsset& asset,
        const engine::SpriteClip& clip,
        float timeSeconds,
        bool loop)
{
    if (clip.frameCount == 0 || clip.firstFrame >= asset.frames.size()) {
        return UINT32_MAX;
    }

    const uint32_t sequenceLength = BillboardPlaybackFrameCount(clip);
    if (sequenceLength == 0) {
        return UINT32_MAX;
    }

    float sequenceDuration = 0.0f;
    for (uint32_t sequenceFrame = 0; sequenceFrame < sequenceLength; ++sequenceFrame) {
        const uint32_t frameOffset = BillboardPlaybackFrameOffset(clip, sequenceFrame);
        const uint32_t frameIndex = clip.firstFrame + frameOffset;
        if (frameIndex >= asset.frames.size()) {
            return UINT32_MAX;
        }
        sequenceDuration += std::max(asset.frames[frameIndex].durationSeconds, 0.001f);
    }

    float localTime = std::max(timeSeconds, 0.0f);
    const bool shouldLoop = loop && clip.playback != engine::SpritePlaybackMode::Once;
    if (shouldLoop && sequenceDuration > 0.0f) {
        localTime = std::fmod(localTime, sequenceDuration);
    } else {
        localTime = std::min(localTime, std::max(sequenceDuration - 0.001f, 0.0f));
    }

    float elapsed = 0.0f;
    for (uint32_t sequenceFrame = 0; sequenceFrame < sequenceLength; ++sequenceFrame) {
        const uint32_t frameOffset = BillboardPlaybackFrameOffset(clip, sequenceFrame);
        const uint32_t frameIndex = clip.firstFrame + frameOffset;
        const float duration = std::max(asset.frames[frameIndex].durationSeconds, 0.001f);
        if (localTime < elapsed + duration || sequenceFrame + 1 == sequenceLength) {
            return frameIndex;
        }
        elapsed += duration;
    }

    return UINT32_MAX;
}

bool ResolveBillboardFrameForDraw(
        engine::AssetManager& assets,
        const SectorBillboardSprite& sprite,
        uint32_t clipIndex,
        const SectorBillboardAnimator& animator,
        const engine::SpriteFrame*& frame,
        const Texture2D*& texture)
{
    frame = nullptr;
    texture = nullptr;
    const engine::SpriteAnimationAsset* asset = assets.GetSpriteAnimation(sprite.animation);
    if (asset == nullptr) {
        return false;
    }

    if (clipIndex == engine::InvalidSpriteClipIndex && !asset->clips.empty()) {
        clipIndex = 0;
    }
    if (clipIndex >= asset->clips.size()) {
        return false;
    }

    const uint32_t frameIndex = ResolveBillboardFrameIndexAtTime(
            *asset,
            asset->clips[clipIndex],
            animator.timeSeconds,
            animator.loop);
    if (frameIndex >= asset->frames.size()) {
        return false;
    }

    frame = &asset->frames[frameIndex];
    texture = assets.GetTexture(asset->atlasTexture);
    return texture != nullptr;
}

Vector3 BakedBillboardLighting(const SectorObjectLighting* lighting)
{
    if (lighting == nullptr) {
        return Vector3{1.0f, 1.0f, 1.0f};
    }

    // Stable upper-hemisphere average. This intentionally ignores the camera-facing quad normal.
    return Vector3Scale(
            Vector3Add(
                    Vector3Add(
                            Vector3Add(lighting->baked.ambientCube[0], lighting->baked.ambientCube[1]),
                            lighting->baked.ambientCube[2]),
                    Vector3Add(lighting->baked.ambientCube[4], lighting->baked.ambientCube[5])),
            1.0f / 5.0f);
}

} // namespace

bool SectorBillboardRenderer::Load()
{
    cutoutShader = LoadGameShader(GameShader::BillboardCutout);
    if (cutoutShader.id == 0) {
        cutoutShader = Shader{};
        textureLoc = -1;
        alphaCutoffLoc = -1;
        bakedLightingLoc = -1;
        dynamicLightCountLoc = -1;
        dynamicLightPositionsLoc = -1;
        dynamicLightColorsLoc = -1;
        dynamicLightRadiiLoc = -1;
        dynamicLightIntensitiesLoc = -1;
        dynamicLightTypesLoc = -1;
        dynamicLightDirectionsLoc = -1;
        dynamicLightInnerConeCosLoc = -1;
        dynamicLightOuterConeCosLoc = -1;
        dynamicLightSpotShadowRightLoc = -1;
        dynamicLightSpotShadowProjectionLoc = -1;
        dynamicLightProfilesLoc = -1;
        dynamicLightProfileParametersLoc = -1;
        flashlightCookieLoc = -1;
        hasPointShadowsLoc = -1;
        dynamicLightShadowSlotsLoc = -1;
        shadowLightMatrixLocs.fill(-1);
        shadowBiasLoc = -1;
        shadowStrengthLoc = -1;
        shadowSoftnessLoc = -1;
        shadowMap0Loc = -1;
        shadowMap1Loc = -1;
        fogShaderLocations = SectorFogShaderLocations{};
        shaderLoaded = false;
        return false;
    }

    cutoutShader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(cutoutShader, "vertexPosition");
    cutoutShader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(cutoutShader, "vertexTexCoord");
    cutoutShader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(cutoutShader, "vertexColor");
    cutoutShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(cutoutShader, "mvp");
    cutoutShader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(cutoutShader, "texture0");
    textureLoc = cutoutShader.locs[SHADER_LOC_MAP_DIFFUSE];
    alphaCutoffLoc = GetShaderLocation(cutoutShader, "alphaCutoff");
    bakedLightingLoc = GetShaderLocation(cutoutShader, "bakedBillboardLighting");
    dynamicLightCountLoc = GetShaderLocation(cutoutShader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightOuterConeCos");
    dynamicLightSpotShadowRightLoc = GetShaderLocationArrayBase(
            cutoutShader, "dynamicLightSpotShadowRight");
    dynamicLightSpotShadowProjectionLoc = GetShaderLocationArrayBase(
            cutoutShader, "dynamicLightSpotShadowProjection");
    dynamicLightProfilesLoc = GetShaderLocationArrayBase(
            cutoutShader, "dynamicLightProfiles");
    dynamicLightProfileParametersLoc = GetShaderLocationArrayBase(
            cutoutShader, "dynamicLightProfileParameters");
    flashlightCookieLoc = GetShaderLocation(cutoutShader, "flashlightCookie");
    hasPointShadowsLoc = GetShaderLocation(cutoutShader, "hasPointShadows");
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(cutoutShader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(cutoutShader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(cutoutShader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(cutoutShader, "shadowSoftness");
    shadowAtlasTilesPerRowLoc = GetShaderLocation(cutoutShader, "shadowAtlasTilesPerRow");
    shadowMap0Loc = GetShaderLocation(cutoutShader, "shadowMap0");
    shadowMap1Loc = GetShaderLocation(cutoutShader, "shadowMap1");
    fogShaderLocations = GetSectorFogShaderLocations(cutoutShader);
    shaderLoaded = true;
    return true;
}

void SectorBillboardRenderer::Shutdown()
{
    if (shaderLoaded) {
        UnloadShader(cutoutShader);
    }
    cutoutShader = Shader{};
    textureLoc = -1;
    alphaCutoffLoc = -1;
    bakedLightingLoc = -1;
    dynamicLightCountLoc = -1;
    dynamicLightPositionsLoc = -1;
    dynamicLightColorsLoc = -1;
    dynamicLightRadiiLoc = -1;
    dynamicLightIntensitiesLoc = -1;
    dynamicLightTypesLoc = -1;
    dynamicLightDirectionsLoc = -1;
    dynamicLightInnerConeCosLoc = -1;
    dynamicLightOuterConeCosLoc = -1;
    dynamicLightSpotShadowRightLoc = -1;
    dynamicLightSpotShadowProjectionLoc = -1;
    dynamicLightProfilesLoc = -1;
    dynamicLightProfileParametersLoc = -1;
    flashlightCookieLoc = -1;
    hasPointShadowsLoc = -1;
    dynamicLightShadowSlotsLoc = -1;
    shadowLightMatrixLocs.fill(-1);
    shadowBiasLoc = -1;
    shadowStrengthLoc = -1;
    shadowSoftnessLoc = -1;
    shadowMap0Loc = -1;
    shadowMap1Loc = -1;
    fogShaderLocations = SectorFogShaderLocations{};
    shaderLoaded = false;
}

void SectorBillboardRenderer::ResetDebugState()
{
    renderDebugText.clear();
    warningPrinted = false;
    consideredCount = 0;
    drawnCount = 0;
    skippedCount = 0;
}

void SectorBillboardRenderer::Draw(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorFogRenderContext& fogContext,
        std::string& debugText)
{
    if (!shaderLoaded || cutoutShader.id == 0) {
        consideredCount = 0;
        drawnCount = 0;
        skippedCount = 0;
        renderDebugText = "billboards: shader unavailable";
        AppendBillboardRenderDebugText(debugText, renderDebugText);
        return;
    }

    size_t frameConsideredCount = 0;
    size_t frameDrawnCount = 0;
    size_t frameSkippedCount = 0;

    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    BeginShaderMode(cutoutShader);
    UploadSectorFogShaderValues(cutoutShader, fogShaderLocations, fogContext);

    SectorDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightSpotShadowRight = dynamicLightSpotShadowRightLoc;
    dynamicLightLocations.dynamicLightSpotShadowProjection =
            dynamicLightSpotShadowProjectionLoc;
    dynamicLightLocations.dynamicLightProfiles = dynamicLightProfilesLoc;
    dynamicLightLocations.dynamicLightProfileParameters =
            dynamicLightProfileParametersLoc;
    dynamicLightLocations.flashlightCookie = flashlightCookieLoc;
    dynamicLightLocations.hasPointShadows = hasPointShadowsLoc;
    UploadSectorRendererDynamicPointLights(cutoutShader, dynamicLightLocations, dynamicLightContext);
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    shadowLocations.shadowAtlasTilesPerRow = shadowAtlasTilesPerRowLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            cutoutShader,
            shadowLocations,
            dynamicLightContext.shadowUniforms);
    if (shadowMap0Loc >= 0
            && dynamicLightContext.shadowMaps.shadowMap0 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap0->id != 0) {
        SetShaderValueTexture(cutoutShader, shadowMap0Loc, *dynamicLightContext.shadowMaps.shadowMap0);
    }
    if (shadowMap1Loc >= 0
            && dynamicLightContext.shadowMaps.shadowMap1 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap1->id != 0) {
        SetShaderValueTexture(cutoutShader, shadowMap1Loc, *dynamicLightContext.shadowMaps.shadowMap1);
    }

    runtimeObjectWorld.ForEach<SectorObjectTransform, SectorObject, SectorBillboardSprite, SectorBillboardAnimator>(
            [this, &assets, &runtimeObjectWorld, &camera, &frameConsideredCount, &frameDrawnCount, &frameSkippedCount](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorBillboardSprite& sprite,
                    SectorBillboardAnimator& animator) {
                ++frameConsideredCount;
                if (!object.visible || !sprite.visible) {
                    ++frameSkippedCount;
                    return;
                }

                uint32_t drawClipIndex = sprite.clipIndex;
                if (runtimeObjectWorld.Has<SectorBillboardDirectionalClips>(entity)) {
                    const SectorBillboardDirectionalClips& directionalClips =
                            runtimeObjectWorld.Get<SectorBillboardDirectionalClips>(entity);
                    const uint32_t selectedClip = SelectSectorBillboardDirectionalClip(
                            transform,
                            camera.position,
                            directionalClips);
                    if (selectedClip != engine::InvalidSpriteClipIndex) {
                        drawClipIndex = selectedClip;
                    }
                }

                const engine::SpriteFrame* frame = nullptr;
                const Texture2D* texture = nullptr;
                if (!ResolveBillboardFrameForDraw(assets, sprite, drawClipIndex, animator, frame, texture)) {
                    ++frameSkippedCount;
                    if (!warningPrinted && (engine::IsNull(sprite.animation) || assets.HasFailed(sprite.animation))) {
                        std::fprintf(stderr,
                                "[SectorMeshRenderer WARNING] Skipping billboard sprite with missing or failed animation asset\n");
                        warningPrinted = true;
                    }
                    return;
                }

                Vector2 size = sprite.sizeWorld;
                if (size.x <= 0.0f || size.y <= 0.0f) {
                    Vector2 frameSize = frame->sourceSize;
                    if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
                        frameSize = Vector2{std::abs(frame->source.width), std::abs(frame->source.height)};
                    }
                    const float height = 1.0f;
                    size = Vector2{height * frameSize.x / std::max(frameSize.y, 1.0f), height};
                }

                const SectorObjectLighting* objectLighting = runtimeObjectWorld.Has<SectorObjectLighting>(entity)
                        ? &runtimeObjectWorld.Get<SectorObjectLighting>(entity)
                        : nullptr;
                const Vector3 bakedLighting = BakedBillboardLighting(objectLighting);
                const SectorBillboardFrameUvs uvs = BuildSectorBillboardFrameUvs(
                        frame->source,
                        texture->width,
                        texture->height);

                const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
                Vector3 right = Vector3{view.m0, view.m4, view.m8};
                if (Vector3LengthSqr(right) <= 0.000001f) {
                    right = Vector3{1.0f, 0.0f, 0.0f};
                }
                const SectorBillboardQuad quad = BuildSectorBillboardQuad(
                        transform.position,
                        size,
                        sprite.originNormalized,
                        right);

                if (alphaCutoffLoc >= 0) {
                    SetShaderValue(cutoutShader, alphaCutoffLoc, &sprite.alphaCutoff, SHADER_UNIFORM_FLOAT);
                }
                if (bakedLightingLoc >= 0) {
                    SetShaderValue(cutoutShader, bakedLightingLoc, &bakedLighting, SHADER_UNIFORM_VEC3);
                }
                if (textureLoc >= 0) {
                    SetShaderValueTexture(cutoutShader, textureLoc, *texture);
                }

                // Cutout-only path: alpha is discarded in the shader, blending stays disabled, and surviving pixels write depth.
                rlCheckRenderBatchLimit(4);
                rlSetTexture(texture->id);
                rlBegin(RL_QUADS);
                    const Color tint = engine::SrgbColorBytesToLinearSceneUnorm(
                            sprite.tint);
                    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                    rlTexCoord2f(uvs.bottomLeft.x, uvs.bottomLeft.y);
                    rlVertex3f(quad.bottomLeft.x, quad.bottomLeft.y, quad.bottomLeft.z);
                    rlTexCoord2f(uvs.bottomRight.x, uvs.bottomRight.y);
                    rlVertex3f(quad.bottomRight.x, quad.bottomRight.y, quad.bottomRight.z);
                    rlTexCoord2f(uvs.topRight.x, uvs.topRight.y);
                    rlVertex3f(quad.topRight.x, quad.topRight.y, quad.topRight.z);
                    rlTexCoord2f(uvs.topLeft.x, uvs.topLeft.y);
                    rlVertex3f(quad.topLeft.x, quad.topLeft.y, quad.topLeft.z);
                rlEnd();
                rlSetTexture(0);
                ++frameDrawnCount;
            });

    EndShaderMode();
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    consideredCount = frameConsideredCount;
    drawnCount = frameDrawnCount;
    skippedCount = frameSkippedCount;
    renderDebugText = "billboards: "
            + std::to_string(frameDrawnCount)
            + " drawn / "
            + std::to_string(frameConsideredCount)
            + " considered, "
            + std::to_string(frameSkippedCount)
            + " skipped";
    AppendBillboardRenderDebugText(debugText, renderDebugText);
}

} // namespace game
