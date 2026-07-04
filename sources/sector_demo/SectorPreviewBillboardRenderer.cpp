#include "sector_demo/SectorPreviewBillboardRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/ecs/World.h"
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

const char* SectorBillboardCutoutVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorldPosition = vertexPosition;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* SectorBillboardCutoutFs = R"(
#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float alphaCutoff;
uniform vec3 bakedBillboardLighting;

#define MAX_DYNAMIC_LIGHTS 8
#define MAX_DYNAMIC_SHADOW_CASTERS 2
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform mat4 shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform float dynamicLightingClamp;

out vec4 finalColor;

const vec2 kPoissonDisk[12] = vec2[12](
    vec2(-0.326, -0.406),
    vec2(-0.840, -0.074),
    vec2(-0.696,  0.457),
    vec2(-0.203,  0.621),
    vec2( 0.962, -0.195),
    vec2( 0.473, -0.480),
    vec2( 0.519,  0.767),
    vec2( 0.185, -0.893),
    vec2( 0.507,  0.064),
    vec2( 0.896,  0.412),
    vec2(-0.322, -0.933),
    vec2(-0.792, -0.598)
);

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

float SampleShadowMap(int shadowSlot, vec2 uv)
{
    return shadowSlot == 0 ? texture(shadowMap0, uv).r : texture(shadowMap1, uv).r;
}

float DynamicSpotLightShadowVisibility(
        int shadowSlot,
        vec3 worldPosition,
        vec3 shadowBiasNormal,
        vec3 surfaceToLightDirection)
{
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) {
        return 1.0;
    }

    vec4 lightClip = shadowLightMatrices[shadowSlot] * vec4(worldPosition, 1.0);
    if (lightClip.w <= 0.0) {
        return 1.0;
    }

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec3 shadowCoord = lightNdc * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap0, 0));
    float normalLightDot = max(dot(
            SafeNormalize(shadowBiasNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(shadowBias[shadowSlot] * (1.0 + (1.0 - normalLightDot) * 2.0), 0.02);
    float compareDepth = shadowCoord.z - effectiveBias;
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        float shadowDepth = SampleShadowMap(shadowSlot, shadowCoord.xy);
        return compareDepth <= shadowDepth ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) * texelSize;
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 sampleUv = clamp(shadowCoord.xy + kPoissonDisk[i] * radius, vec2(0.0), vec2(1.0));
        float shadowDepth = SampleShadowMap(shadowSlot, sampleUv);
        visible += compareDepth <= shadowDepth ? 1.0 : 0.0;
    }
    return visible / 12.0;
}

void main()
{
    vec4 sampled = texture(texture0, fragTexCoord);
    if (sampled.a < alphaCutoff) {
        discard;
    }

    vec3 dynamicDirect = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        float radius = dynamicLightRadii[i];
        vec3 toLight = dynamicLightPositions[i] - fragWorldPosition;
        float distanceSq = dot(toLight, toLight);
        if (radius > 0.0 && distanceSq < radius * radius) {
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : vec3(0.0, 1.0, 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            float coneAtten = 1.0;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                float innerConeCos = dynamicLightInnerConeCos[i];
                float outerConeCos = dynamicLightOuterConeCos[i];
                coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                        ? smoothstep(outerConeCos, innerConeCos, coneDot)
                        : step(innerConeCos, coneDot);
                int shadowSlot = dynamicLightShadowSlots[i];
                if (shadowSlot >= 0) {
                    // Stable world-up approximation used only for bias. Baked probe lighting never uses a camera-facing normal.
                    float visibility = DynamicSpotLightShadowVisibility(
                            shadowSlot,
                            fragWorldPosition,
                            vec3(0.0, 1.0, 0.0),
                            lightDirection);
                    coneAtten *= mix(1.0, visibility, clamp(shadowStrength[shadowSlot], 0.0, 1.0));
                }
            }
            dynamicDirect += dynamicLightColors[i] * dynamicLightIntensities[i] * atten * coneAtten;
        }
    }

    vec3 surfaceRgb = sampled.rgb * fragColor.rgb;
    vec3 lighting = clamp(bakedBillboardLighting + dynamicDirect, 0.0, dynamicLightingClamp);
    finalColor = vec4(surfaceRgb * lighting, 1.0);
}
)";

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

bool SectorPreviewBillboardRenderer::Load()
{
    cutoutShader = LoadShaderFromMemory(SectorBillboardCutoutVs, SectorBillboardCutoutFs);
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
        dynamicLightShadowSlotsLoc = -1;
        shadowLightMatrixLocs.fill(-1);
        shadowBiasLoc = -1;
        shadowStrengthLoc = -1;
        shadowSoftnessLoc = -1;
        shadowMap0Loc = -1;
        shadowMap1Loc = -1;
        dynamicLightingClampLoc = -1;
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
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(cutoutShader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(cutoutShader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(cutoutShader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(cutoutShader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(cutoutShader, "shadowSoftness");
    shadowMap0Loc = GetShaderLocation(cutoutShader, "shadowMap0");
    shadowMap1Loc = GetShaderLocation(cutoutShader, "shadowMap1");
    dynamicLightingClampLoc = GetShaderLocation(cutoutShader, "dynamicLightingClamp");
    shaderLoaded = true;
    return true;
}

void SectorPreviewBillboardRenderer::Shutdown()
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
    dynamicLightShadowSlotsLoc = -1;
    shadowLightMatrixLocs.fill(-1);
    shadowBiasLoc = -1;
    shadowStrengthLoc = -1;
    shadowSoftnessLoc = -1;
    shadowMap0Loc = -1;
    shadowMap1Loc = -1;
    dynamicLightingClampLoc = -1;
    shaderLoaded = false;
}

void SectorPreviewBillboardRenderer::ResetDebugState()
{
    renderDebugText.clear();
    warningPrinted = false;
    consideredCount = 0;
    drawnCount = 0;
    skippedCount = 0;
}

void SectorPreviewBillboardRenderer::Draw(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld,
        const Camera3D& camera,
        const SectorPreviewBillboardDynamicLightContext& dynamicLightContext,
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

    SectorPreviewDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightingClamp = dynamicLightingClampLoc;
    UploadSectorPreviewDynamicPointLights(cutoutShader, dynamicLightLocations, dynamicLightContext);
    SectorPreviewDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    UploadSectorPreviewDynamicSpotLightShadowUniforms(
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
                                "[SectorMeshPreview WARNING] Skipping billboard sprite with missing or failed animation asset\n");
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
                    rlColor4ub(sprite.tint.r, sprite.tint.g, sprite.tint.b, sprite.tint.a);
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
