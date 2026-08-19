#include "sector_demo/renderer/SectorAnalyticFogRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorAtmosphereCulling.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

const char* ScreenVs = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";

const char* AnalyticFogFs = R"(
#version 330
out vec4 finalColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 fogCenter;
uniform vec3 fogRadii;
uniform vec3 fogColor;
uniform vec4 fogParams; // start, end, exponent, maximum opacity

bool intersectEllipsoid(vec3 origin, vec3 direction, out float enterT, out float exitT) {
    vec3 inverseRadii = 1.0 / max(fogRadii, vec3(0.0001));
    vec3 o = (origin - fogCenter) * inverseRadii;
    vec3 d = direction * inverseRadii;
    float a = dot(d, d);
    float b = 2.0 * dot(o, d);
    float c = dot(o, o) - 1.0;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0 || a <= 0.0) return false;
    float root = sqrt(discriminant);
    enterT = (-b - root) / (2.0 * a);
    exitT = (-b + root) / (2.0 * a);
    return exitT > max(enterT, 0.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    if (!intersectEllipsoid(cameraPosition, rayDirection, enterT, exitT)) discard;
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    enterT = max(enterT, 0.0);
    exitT = min(exitT, sceneDistance);
    float chord = max(exitT - enterT, 0.0);
    if (chord <= 0.0) discard;
    float range = max(fogParams.y - fogParams.x, 0.0001);
    float opacity = clamp(fogParams.w, 0.0, 1.0)
            * pow(clamp((chord - fogParams.x) / range, 0.0, 1.0),
                    max(fogParams.z, 0.0001));
    if (opacity <= 0.00001) discard;
    finalColor = vec4(max(fogColor, vec3(0.0)), opacity);
}
)";

} // namespace

void SectorAnalyticFogRenderer::Reserve(std::size_t volumeCount)
{
    visibleVolumes.reserve(volumeCount);
}

bool SectorAnalyticFogRenderer::EnsureShader()
{
    if (shader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(ScreenVs, AnalyticFogFs);
    if (shader.id == 0) { shaderFailed = true; return false; }
#define LOC(field, name) field = GetShaderLocation(shader, name)
    LOC(sceneDepthLoc, "sceneDepth"); LOC(viewportSizeLoc, "viewportSize");
    LOC(cameraPositionLoc, "cameraPosition"); LOC(cameraForwardLoc, "cameraForward");
    LOC(cameraRightLoc, "cameraRight"); LOC(cameraUpLoc, "cameraUp");
    LOC(tanHalfFovLoc, "tanHalfFov"); LOC(aspectRatioLoc, "aspectRatio");
    LOC(nearPlaneLoc, "nearPlane"); LOC(farPlaneLoc, "farPlane");
    LOC(centerLoc, "fogCenter"); LOC(radiiLoc, "fogRadii");
    LOC(colorLoc, "fogColor"); LOC(fogParamsLoc, "fogParams");
#undef LOC
    return true;
}

bool SectorAnalyticFogRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyMap& map,
        SectorVolumetricQuality quality,
        const Camera3D& camera)
{
    eligibleCount = 0;
    activeCount = 0;
    scissorCoverage = 0.0f;
    visibleVolumes.clear();
    if (quality == SectorVolumetricQuality::Off || sceneTarget.texture.id == 0
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane || !EnsureShader()) return false;
    const int width = sceneTarget.texture.width;
    const int height = sceneTarget.texture.height;
    const float aspect = static_cast<float>(width) / std::max(height, 1);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    SectorAtmosphereScissorRect unionScissor{};
    for (const SectorCompiledLocalFogVolume& volume : map.compiledLocalFogVolumes) {
        if (!volume.enabled || volume.renderMode != SectorLocalFogRenderMode::Analytic
                || volume.maxOpacity <= 0.0f) continue;
        const Vector3 minimum = Vector3Subtract(volume.centerWorld, volume.radiiWorld);
        const Vector3 maximum = Vector3Add(volume.centerWorld, volume.radiiWorld);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        if (scissor.Empty()) continue;
        ++eligibleCount;
        visibleVolumes.push_back(VisibleVolume{
                &volume, Vector3DistanceSqr(camera.position, volume.centerWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
    }
    std::sort(visibleVolumes.begin(), visibleVolumes.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) return left.distanceSquared > right.distanceSquared;
        return left.volume->sourceAuthoringFogVolumeId < right.volume->sourceAuthoringFogVolumeId;
    });
    activeCount = static_cast<int>(visibleVolumes.size());
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleVolumes.empty()) return false;
    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
    rlDrawRenderBatchActive();
    BeginTextureMode(colorOnlyTarget);
    BeginShaderMode(shader);
    SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);
    SetShaderValue(shader, viewportSizeLoc, &viewport, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraForwardLoc, &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraRightLoc, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraUpLoc, &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, tanHalfFovLoc, &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, aspectRatioLoc, &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, nearPlaneLoc, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, farPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ALPHA);
    rlEnableScissorTest();
    for (const VisibleVolume& entry : visibleVolumes) {
        const SectorCompiledLocalFogVolume& volume = *entry.volume;
        const Vector3 minimum = Vector3Subtract(volume.centerWorld, volume.radiiWorld);
        const Vector3 maximum = Vector3Add(volume.centerWorld, volume.radiiWorld);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        const Vector3 color = engine::SrgbColorBytesToLinearSceneRgb(volume.color);
        const Vector4 params{volume.analyticStartDistanceWorld,
                volume.analyticEndDistanceWorld, volume.analyticFalloffExponent,
                volume.maxOpacity};
        rlDrawRenderBatchActive();
        SetShaderValue(shader, centerLoc, &volume.centerWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, radiiLoc, &volume.radiiWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, colorLoc, &color, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, fogParamsLoc, &params, SHADER_UNIFORM_VEC4);
        rlScissor(scissor.x, scissor.y, scissor.width, scissor.height);
        DrawRectangle(0, 0, width, height, WHITE);
    }
    rlDrawRenderBatchActive();
    rlDisableScissorTest();
    EndBlendMode();
    EndShaderMode();
    EndTextureMode();
    return true;
}

void SectorAnalyticFogRenderer::Shutdown()
{
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
    visibleVolumes.clear();
    visibleVolumes.shrink_to_fit();
}

} // namespace game
