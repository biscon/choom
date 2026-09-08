#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorDistanceFogRenderer.h"

#include "engine/render/ColorTransfer.h"

#include <raymath.h>
#include <rlgl.h>

#include <cmath>

namespace game {
namespace {

Rectangle Source(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

Rectangle Destination(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

} // namespace

bool SectorDistanceFogRenderer::Initialize()
{
    if (shader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadGameShader(GameShader::DistanceFog);
    if (shader.id == 0) {
        shaderFailed = true;
        return false;
    }
    sceneColorLoc = GetShaderLocation(shader, "sceneColor");
    sceneDepthLoc = GetShaderLocation(shader, "sceneDepth");
    nearPlaneLoc = GetShaderLocation(shader, "nearPlane");
    farPlaneLoc = GetShaderLocation(shader, "farPlane");
    startDistanceLoc = GetShaderLocation(shader, "startDistance");
    endDistanceLoc = GetShaderLocation(shader, "endDistance");
    falloffExponentLoc = GetShaderLocation(shader, "falloffExponent");
    maxOpacityLoc = GetShaderLocation(shader, "maxOpacity");
    fogColorLoc = GetShaderLocation(shader, "fogColor");
    return true;
}

bool SectorDistanceFogRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch,
        const SectorTopologyFogSettings& sourceSettings,
        const Camera3D&)
{
    const SectorTopologyFogSettings settings = NormalizeSectorTopologyFogSettings(sourceSettings);
    if (!settings.enabled || settings.mode != SectorTopologyFogMode::Distance
            || settings.maxOpacity <= 0.0f || sceneTarget.texture.id == 0
            || sceneTarget.depth.id == 0 || !(shader.id != 0)) {
        return false;
    }
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) return false;
    const Vector3 fogColor = Vector3Scale(
            engine::SrgbColorBytesToLinearSceneRgb(settings.color), settings.brightness);
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch);
    ClearBackground(BLANK);
    BeginShaderMode(shader);
    SetShaderValueTexture(shader, sceneColorLoc, sceneTarget.texture);
    SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);
    SetShaderValue(shader, nearPlaneLoc, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, farPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, startDistanceLoc, &settings.startDistanceWorld, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, endDistanceLoc, &settings.endDistanceWorld, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, falloffExponentLoc, &settings.falloffExponent, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, maxOpacityLoc, &settings.maxOpacity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, fogColorLoc, &fogColor, SHADER_UNIFORM_VEC3);
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, Source(sceneTarget.texture),
            Destination(sceneScratch.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return true;
}

void SectorDistanceFogRenderer::Shutdown()
{
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
}

} // namespace game
