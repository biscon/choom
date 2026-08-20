#include "sector_demo/renderer/SectorDistanceFogRenderer.h"

#include "engine/render/ColorTransfer.h"

#include <raymath.h>
#include <rlgl.h>

#include <cmath>

namespace game {
namespace {

const char* FullscreenVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";

const char* DistanceFogFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform float nearPlane;
uniform float farPlane;
uniform float startDistance;
uniform float endDistance;
uniform float falloffExponent;
uniform float maxOpacity;
uniform vec3 fogColor;
void main() {
    vec4 scene = texture(sceneColor, fragUv);
    float depth = texture(sceneDepth, fragUv).r;
    if (depth >= 0.999999) { finalColor = scene; return; }
    float zNdc = depth * 2.0 - 1.0;
    float distance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float fogRange = max(endDistance - startDistance, 0.0001);
    float factor = pow(clamp((distance - startDistance) / fogRange, 0.0, 1.0),
            max(falloffExponent, 0.0001)) * clamp(maxOpacity, 0.0, 1.0);
    vec3 result = max(scene.rgb, vec3(0.0)) * (1.0 - factor)
            + max(fogColor, vec3(0.0)) * factor;
    finalColor = vec4(min(result, vec3(65504.0)), scene.a);
}
)";

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

bool SectorDistanceFogRenderer::EnsureShader()
{
    if (shader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(FullscreenVs, DistanceFogFs);
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
            || sceneTarget.depth.id == 0 || !EnsureShader()) {
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
