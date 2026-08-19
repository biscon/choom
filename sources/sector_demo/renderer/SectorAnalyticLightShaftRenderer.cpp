#include "sector_demo/renderer/SectorAnalyticLightShaftRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorMeshTypes.h"

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

const char* AnalyticShaftFs = R"(
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
uniform vec3 coneApex;
uniform vec3 coneDirection;
uniform float coneLength;
uniform float coneBaseRadius;
uniform vec3 shaftRadiance;
uniform vec2 shaftParams; // edge softness, maximum opacity
uniform vec4 fogParamsA; // mode, start, end/density, maximum opacity
uniform vec4 fogParamsB; // exponent, reference height, height falloff, unused

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.00000001 ? value * inversesqrt(lengthSquared) : fallback;
}

void includeConeHit(float t, inout float enterT, inout float exitT, inout int hitCount) {
    if (isnan(t) || isinf(t)) return;
    enterT = min(enterT, t);
    exitT = max(exitT, t);
    hitCount++;
}

bool intersectFiniteCone(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    float height = max(coneLength, 0.0001);
    float radius = max(coneBaseRadius, 0.0001);
    float slopeSquared = radius * radius / (height * height);
    vec3 originOffset = rayOrigin - coneApex;
    float originAxial = dot(originOffset, axis);
    float directionAxial = dot(rayDirection, axis);
    vec3 originRadial = originOffset - axis * originAxial;
    vec3 directionRadial = rayDirection - axis * directionAxial;
    float a = dot(directionRadial, directionRadial)
            - slopeSquared * directionAxial * directionAxial;
    float b = 2.0 * (dot(originRadial, directionRadial)
            - slopeSquared * originAxial * directionAxial);
    float c = dot(originRadial, originRadial) - slopeSquared * originAxial * originAxial;
    enterT = 1e30;
    exitT = -1e30;
    int hitCount = 0;
    const float epsilon = 0.000001;
    if (abs(a) <= epsilon) {
        if (abs(b) > epsilon) {
            float t = -c / b;
            float axial = originAxial + t * directionAxial;
            if (axial >= -epsilon && axial <= height + epsilon) {
                includeConeHit(t, enterT, exitT, hitCount);
            }
        }
    } else {
        float discriminant = b * b - 4.0 * a * c;
        if (discriminant >= 0.0) {
            float root = sqrt(max(discriminant, 0.0));
            float inverse = 0.5 / a;
            float t0 = (-b - root) * inverse;
            float axial0 = originAxial + t0 * directionAxial;
            if (axial0 >= -epsilon && axial0 <= height + epsilon) {
                includeConeHit(t0, enterT, exitT, hitCount);
            }
            float t1 = (-b + root) * inverse;
            float axial1 = originAxial + t1 * directionAxial;
            if (axial1 >= -epsilon && axial1 <= height + epsilon) {
                includeConeHit(t1, enterT, exitT, hitCount);
            }
        }
    }
    if (abs(directionAxial) > epsilon) {
        float baseT = (height - originAxial) / directionAxial;
        vec3 baseOffset = originOffset + rayDirection * baseT - axis * height;
        if (dot(baseOffset, baseOffset) <= radius * radius + epsilon) {
            includeConeHit(baseT, enterT, exitT, hitCount);
        }
    }
    return hitCount >= 2 && exitT - enterT > epsilon;
}

float fogTransmittance(vec3 position) {
    if (fogParamsA.x < 0.5) return 1.0;
    float amount = 0.0;
    if (fogParamsA.x < 1.5) {
        float distance = max(length(position - cameraPosition) - fogParamsA.y, 0.0);
        float midpointHeight = (cameraPosition.y + position.y) * 0.5;
        float height = max(midpointHeight - fogParamsB.y, 0.0);
        amount = min(1.0 - exp(-fogParamsA.z * distance
                * exp(-height * fogParamsB.z)), fogParamsA.w);
    } else {
        amount = pow(clamp((dot(position - cameraPosition, cameraForward) - fogParamsA.y)
                / max(fogParamsA.z - fogParamsA.y, 0.0001), 0.0, 1.0),
                max(fogParamsB.x, 0.0001)) * fogParamsA.w;
    }
    return 1.0 - clamp(amount, 0.0, 1.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    if (!intersectFiniteCone(cameraPosition, rayDirection, enterT, exitT)) discard;
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    enterT = max(enterT, 0.0);
    exitT = min(exitT, sceneDistance);
    float chord = max(exitT - enterT, 0.0);
    if (chord <= 0.00001) discard;
    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    float axial01 = clamp(dot(midpoint - coneApex, axis) / max(coneLength, 0.0001), 0.0, 1.0);
    float localDiameter = max(2.0 * coneBaseRadius * max(axial01, 0.01), 0.0001);
    float coverage = clamp(chord / localDiameter, 0.0, 1.0);
    // The authored midpoint now matches the old maximum softness. The upper
    // half adds a gentler tail without expanding the finite cone.
    float mappedSoftness = clamp(shaftParams.x * 2.0, 0.02, 2.0);
    float baseSoftness = min(mappedSoftness, 1.0);
    float extraSoftness = max(mappedSoftness - 1.0, 0.0);
    float lateralExponent = mix(0.2, 2.5, baseSoftness)
            + 2.0 * extraSoftness;
    float lateral = pow(coverage, lateralExponent);
    float startFadeWidth = min(mix(0.02, 0.18, baseSoftness)
            + 0.12 * extraSoftness, 0.30);
    float endFadeWidth = min(mix(0.04, 0.35, baseSoftness)
            + 0.20 * extraSoftness, 0.55);
    float longitudinal = smoothstep(0.0, startFadeWidth, axial01)
            * (1.0 - smoothstep(1.0 - endFadeWidth, 1.0, axial01));
    float opacity = clamp(shaftParams.y, 0.0, 1.0) * lateral * longitudinal
            * fogTransmittance(midpoint);
    if (opacity <= 0.00001) discard;
    finalColor = vec4(min(max(shaftRadiance, vec3(0.0)), vec3(65504.0)), opacity);
}
)";

int FindDynamicIndex(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& lights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return -1;
    const int type = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1 : 0;
    for (int index = 0; index < lights.dynamicLightCount; ++index) {
        if (lights.dynamicLightIds[static_cast<std::size_t>(index)] == source.lightId
                && lights.dynamicLightTypes[static_cast<std::size_t>(index)] == type) return index;
    }
    return -1;
}

Vector3 Multiply(Vector3 left, Vector3 right)
{
    return Vector3{left.x * right.x, left.y * right.y, left.z * right.z};
}

void ConeBounds(
        Vector3 apex,
        Vector3 axis,
        float length,
        float radius,
        Vector3& minimum,
        Vector3& maximum)
{
    const Vector3 base = Vector3Add(apex, Vector3Scale(axis, length));
    const Vector3 diskExtent{
            radius * std::sqrt(std::max(1.0f - axis.x * axis.x, 0.0f)),
            radius * std::sqrt(std::max(1.0f - axis.y * axis.y, 0.0f)),
            radius * std::sqrt(std::max(1.0f - axis.z * axis.z, 0.0f))};
    minimum = Vector3{
            std::min(apex.x, base.x - diskExtent.x),
            std::min(apex.y, base.y - diskExtent.y),
            std::min(apex.z, base.z - diskExtent.z)};
    maximum = Vector3{
            std::max(apex.x, base.x + diskExtent.x),
            std::max(apex.y, base.y + diskExtent.y),
            std::max(apex.z, base.z + diskExtent.z)};
}

} // namespace

void SectorAnalyticLightShaftRenderer::Reserve(std::size_t sourceCount)
{
    visibleShafts.reserve(sourceCount);
}

bool SectorAnalyticLightShaftRenderer::EnsureShader()
{
    if (shader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(ScreenVs, AnalyticShaftFs);
    if (shader.id == 0) { shaderFailed = true; return false; }
#define LOC(field, name) field = GetShaderLocation(shader, name)
    LOC(sceneDepthLoc, "sceneDepth"); LOC(viewportSizeLoc, "viewportSize");
    LOC(cameraPositionLoc, "cameraPosition"); LOC(cameraForwardLoc, "cameraForward");
    LOC(cameraRightLoc, "cameraRight"); LOC(cameraUpLoc, "cameraUp");
    LOC(tanHalfFovLoc, "tanHalfFov"); LOC(aspectRatioLoc, "aspectRatio");
    LOC(nearPlaneLoc, "nearPlane"); LOC(farPlaneLoc, "farPlane");
    LOC(coneApexLoc, "coneApex"); LOC(coneDirectionLoc, "coneDirection");
    LOC(coneLengthLoc, "coneLength"); LOC(coneBaseRadiusLoc, "coneBaseRadius");
    LOC(shaftRadianceLoc, "shaftRadiance"); LOC(shaftParamsLoc, "shaftParams");
    LOC(fogParamsALoc, "fogParamsA"); LOC(fogParamsBLoc, "fogParamsB");
#undef LOC
    return true;
}

bool SectorAnalyticLightShaftRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyFogSettings& sourceFogSettings,
        SectorVolumetricQuality quality,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount = activeCount = drawCallCount = 0;
    scissorCoverage = 0.0f;
    visibleShafts.clear();
    if (quality == SectorVolumetricQuality::Off || sources.empty()
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0 || !EnsureShader()) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) return false;
    const int width = sceneTarget.texture.width;
    const int height = sceneTarget.texture.height;
    const float aspect = static_cast<float>(width) / std::max(height, 1);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    SectorAtmosphereScissorRect unionScissor{};
    for (const SectorLightAtmosphereSource& source : sources) {
        const SectorLightProxyShaftSettings& settings = source.atmosphere.proxy.shaft;
        if (source.shape != SectorLightAtmosphereShape::Cone || !settings.enabled
                || settings.brightness <= 0.0f || settings.maxOpacity <= 0.0f
                || !IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) continue;
        SectorLightAtmosphereVolume volume;
        if (!MakeSectorLightAtmosphereVolume(source, settings.lengthScale, 0.0f, volume)) continue;
        // The shared volume helper has a haze-oriented 0.05 lower scale bound;
        // shafts deliberately support the authored 0.01 minimum.
        const float authoredExtent = source.rangeWorld * settings.lengthScale;
        if (!std::isfinite(authoredExtent) || authoredExtent <= 0.0f) continue;
        volume.coneRadiusWorld *= authoredExtent / volume.extentWorld;
        volume.extentWorld = authoredExtent;
        volume.coneRadiusWorld *= settings.widthScale;
        volume.boundsCenterWorld = Vector3Add(
                volume.originWorld,
                Vector3Scale(volume.directionWorld, volume.extentWorld * 0.5f));
        volume.boundsRadiusWorld = std::sqrt(
                volume.extentWorld * volume.extentWorld * 0.25f
                + volume.coneRadiusWorld * volume.coneRadiusWorld);
        if (!IsSectorLightAtmosphereVolumeVisible(volume, visibility, receiverBounds,
                        camera, aspect, nearPlane, farPlane)) continue;
        const float baseRadius = volume.coneRadiusWorld;
        Vector3 minimum;
        Vector3 maximum;
        ConeBounds(volume.originWorld, volume.directionWorld, volume.extentWorld,
                baseRadius, minimum, maximum);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        if (scissor.Empty()) continue;
        Vector3 lightColor = engine::SrgbColorBytesToLinearSceneRgb(source.color);
        float intensity = source.intensity;
        const int dynamicIndex = FindDynamicIndex(source, dynamicLights);
        if (dynamicIndex >= 0) {
            lightColor = dynamicLights.dynamicLightColors[static_cast<std::size_t>(dynamicIndex)];
            intensity = dynamicLights.dynamicLightIntensities[static_cast<std::size_t>(dynamicIndex)];
        }
        const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(source.atmosphere.proxy.tint);
        visibleShafts.push_back(VisibleShaft{
                &source,
                volume,
                scissor,
                Vector3Scale(Multiply(lightColor, tint), intensity * settings.brightness),
                Vector3DistanceSqr(camera.position, volume.boundsCenterWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
    }
    std::sort(visibleShafts.begin(), visibleShafts.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) {
            return left.distanceSquared > right.distanceSquared;
        }
        if (left.source->lightId != right.source->lightId) {
            return left.source->lightId < right.source->lightId;
        }
        return static_cast<int>(left.source->kind) < static_cast<int>(right.source->kind);
    });
    eligibleCount = activeCount = static_cast<int>(visibleShafts.size());
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleShafts.empty()) return false;

    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
    const SectorTopologyFogSettings fog = NormalizeSectorTopologyFogSettings(sourceFogSettings);
    const float fogMode = !fog.enabled ? 0.0f
            : (fog.mode == SectorTopologyFogMode::Distance ? 2.0f : 1.0f);
    const Vector4 fogA{fogMode, fog.startDistanceWorld,
            fog.mode == SectorTopologyFogMode::Distance ? fog.endDistanceWorld : fog.density,
            fog.maxOpacity};
    const Vector4 fogB{fog.falloffExponent, fog.referenceHeightWorld, fog.heightFalloff, 0.0f};
    rlDrawRenderBatchActive();
    BeginTextureMode(colorOnlyTarget);
    BeginShaderMode(shader);
    SetShaderValue(shader, viewportSizeLoc, &viewport, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraForwardLoc, &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraRightLoc, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraUpLoc, &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, tanHalfFovLoc, &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, aspectRatioLoc, &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, nearPlaneLoc, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, farPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, fogParamsALoc, &fogA, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, fogParamsBLoc, &fogB, SHADER_UNIFORM_VEC4);
    BeginBlendMode(BLEND_ALPHA);
    rlColorMask(true, true, true, false);
    rlEnableScissorTest();
    for (const VisibleShaft& visible : visibleShafts) {
        const SectorLightProxyShaftSettings& settings = visible.source->atmosphere.proxy.shaft;
        const float baseRadius = visible.volume.coneRadiusWorld;
        const Vector2 shaftParams{settings.edgeSoftness, settings.maxOpacity};
        // rlDrawRenderBatchActive() clears raylib's auxiliary sampler slots.
        // Register sceneDepth after the flush so it remains bound for this draw.
        rlDrawRenderBatchActive();
        SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);
        SetShaderValue(shader, coneApexLoc, &visible.volume.originWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneDirectionLoc, &visible.volume.directionWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneLengthLoc, &visible.volume.extentWorld, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, coneBaseRadiusLoc, &baseRadius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, shaftRadianceLoc, &visible.radiance, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, shaftParamsLoc, &shaftParams, SHADER_UNIFORM_VEC2);
        rlScissor(visible.scissor.x, visible.scissor.y,
                visible.scissor.width, visible.scissor.height);
        DrawRectangle(0, 0, width, height, WHITE);
        ++drawCallCount;
    }
    rlDrawRenderBatchActive();
    rlDisableScissorTest();
    rlColorMask(true, true, true, true);
    EndBlendMode();
    EndShaderMode();
    EndTextureMode();
    return true;
}

void SectorAnalyticLightShaftRenderer::Shutdown()
{
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
    visibleShafts.clear();
    visibleShafts.shrink_to_fit();
}

} // namespace game
