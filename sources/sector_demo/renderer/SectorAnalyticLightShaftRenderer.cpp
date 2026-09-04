#include "sector_demo/renderer/SectorAnalyticLightShaftRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace game {
namespace {

const char* ScreenVs = R"(
#version 330
in vec3 vertexPosition;
void main() { gl_Position = vec4(vertexPosition.xy, 0.0, 1.0); }
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
uniform int shaftShape; // 0 cone, 1 rectangular frustum
uniform vec3 rectRight;
uniform vec3 rectUp;
uniform vec2 rectNearHalfSize;
uniform vec2 rectFarHalfSize;
uniform vec3 shaftRadiance;
uniform vec2 shaftParams; // edge softness, maximum extinction
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

bool clipRectFrustumPlane(
        vec3 localOrigin,
        vec3 localDirection,
        vec3 planeNormal,
        float planeLimit,
        inout float enterT,
        inout float exitT) {
    float originDistance = dot(planeNormal, localOrigin) - planeLimit;
    float directionDistance = dot(planeNormal, localDirection);
    const float epsilon = 0.000001;
    if (abs(directionDistance) <= epsilon) return originDistance <= epsilon;
    float t = -originDistance / directionDistance;
    if (directionDistance < 0.0) enterT = max(enterT, t);
    else exitT = min(exitT, t);
    return enterT <= exitT + epsilon;
}

bool intersectRectFrustum(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    vec3 offset = rayOrigin - coneApex;
    vec3 localOrigin = vec3(dot(offset, rectRight), dot(offset, rectUp), dot(offset, axis));
    vec3 localDirection = vec3(dot(rayDirection, rectRight), dot(rayDirection, rectUp), dot(rayDirection, axis));
    float length = max(coneLength, 0.0001);
    vec2 nearHalfSize = max(rectNearHalfSize, vec2(0.0001));
    vec2 farHalfSize = max(rectFarHalfSize, nearHalfSize);
    vec2 sideSlope = (farHalfSize - nearHalfSize) / length;
    enterT = -1e30;
    exitT = 1e30;
    return clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 0.0, -1.0), 0.0, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 0.0, 1.0), length, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(1.0, 0.0, -sideSlope.x), nearHalfSize.x, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(-1.0, 0.0, -sideSlope.x), nearHalfSize.x, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 1.0, -sideSlope.y), nearHalfSize.y, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, -1.0, -sideSlope.y), nearHalfSize.y, enterT, exitT)
            && exitT - enterT > 0.000001;
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

float shaftOpticalProfileAt(
        float sampleT,
        vec3 rayDirection,
        float visibleChord,
        vec3 axis,
        float lateralExponent,
        float startFadeWidth,
        float endFadeWidth) {
    vec3 samplePosition = cameraPosition + rayDirection * sampleT;
    float axial01 = clamp(
            dot(samplePosition - coneApex, axis) / max(coneLength, 0.0001),
            0.0,
            1.0);
    float localDiameter = max(
            2.0 * coneBaseRadius * max(axial01, 0.01),
            0.0001);
    // The old hard clamp exposed a contour where chord/localDiameter crossed
    // one. Smooth that transition before applying the authored edge profile.
    float rawCoverage = max(visibleChord / localDiameter, 0.0);
    float coverage = smoothstep(0.0, 1.0, rawCoverage);
    float lateral = pow(coverage, lateralExponent);
    float longitudinal = smoothstep(0.0, startFadeWidth, axial01)
            * (1.0 - smoothstep(1.0 - endFadeWidth, 1.0, axial01));
    return (1.0 - exp(-2.0 * coverage)) * lateral * longitudinal;
}

float rectShaftDensityAt(
        float sampleT,
        vec3 rayDirection,
        vec3 axis,
        float edgeFeather,
        float cornerExponent,
        float startFadeWidth,
        float endFadeWidth) {
    vec3 samplePosition = cameraPosition + rayDirection * sampleT;
    vec3 sampleOffset = samplePosition - coneApex;
    float axial01 = clamp(
            dot(sampleOffset, axis) / max(coneLength, 0.0001),
            0.0,
            1.0);
    vec2 localHalfSize = max(
            mix(rectNearHalfSize, rectFarHalfSize, axial01),
            vec2(0.0001));
    vec2 normalizedLateral = abs(vec2(
            dot(sampleOffset, rectRight),
            dot(sampleOffset, rectUp))) / localHalfSize;
    float roundedRectDistance = pow(
            pow(normalizedLateral.x, cornerExponent)
                    + pow(normalizedLateral.y, cornerExponent),
            1.0 / cornerExponent);
    float lateral = 1.0 - smoothstep(
            1.0 - edgeFeather,
            1.0,
            roundedRectDistance);
    float longitudinal = smoothstep(0.0, startFadeWidth, axial01)
            * (1.0 - smoothstep(1.0 - endFadeWidth, 1.0, axial01));
    return lateral * longitudinal;
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    bool hit = shaftShape == 1
            ? intersectRectFrustum(cameraPosition, rayDirection, enterT, exitT)
            : intersectFiniteCone(cameraPosition, rayDirection, enterT, exitT);
    if (!hit) discard;
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
    // The authored midpoint now matches the old maximum softness. The upper
    // half adds a gentler tail without expanding the finite cone.
    float mappedSoftness = clamp(shaftParams.x * 2.0, 0.02, 2.0);
    float baseSoftness = min(mappedSoftness, 1.0);
    float extraSoftness = max(mappedSoftness - 1.0, 0.0);
    float lateralExponent = mix(0.2, 2.5, baseSoftness)
            + 2.0 * extraSoftness;
    float startFadeWidth = min(mix(0.02, 0.18, baseSoftness)
            + 0.12 * extraSoftness, 0.30);
    float endFadeWidth = min(mix(0.04, 0.35, baseSoftness)
            + 0.20 * extraSoftness, 0.55);
    float opticalThickness = 0.0;
    if (shaftShape == 1) {
        float rectSoftness = clamp((shaftParams.x - 0.01) / 0.99, 0.0, 1.0);
        float edgeFeather = mix(0.02, 0.45, rectSoftness);
        float cornerExponent = mix(24.0, 4.0, rectSoftness);
        vec3 midpointOffset = midpoint - coneApex;
        float midpointAxial01 = clamp(
                dot(midpointOffset, axis) / max(coneLength, 0.0001),
                0.0,
                1.0);
        vec2 midpointHalfSize = mix(
                rectNearHalfSize,
                rectFarHalfSize,
                midpointAxial01);
        float localDiameter = max(
                2.0 * max(midpointHalfSize.x, midpointHalfSize.y),
                0.0001);
        // Saturating within half a local diameter prevents changes in the
        // intersected frustum face from drawing corner rays through the fog.
        float pathCoverage = smoothstep(
                0.0,
                0.5,
                max(chord / localDiameter, 0.0));
        // Fixed five-point Gauss-Legendre integration keeps the rounded
        // density stable as the view crosses the frustum's planar edges.
        float integratedDensity =
                0.1184634430 * rectShaftDensityAt(
                        enterT + chord * 0.0469100770,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2393143352 * rectShaftDensityAt(
                        enterT + chord * 0.2307653449,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2844444444 * rectShaftDensityAt(
                        enterT + chord * 0.5,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2393143352 * rectShaftDensityAt(
                        enterT + chord * 0.7692346551,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.1184634430 * rectShaftDensityAt(
                        enterT + chord * 0.9530899230,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth);
        opticalThickness = 0.8646647168 * pathCoverage * integratedDensity;
    } else {
        // Fixed, unrolled samples avoid the single-midpoint profile trough that
        // becomes visible when an authored shaft origin is displaced.
        opticalThickness = (
                shaftOpticalProfileAt(
                        enterT + chord * (1.0 / 6.0),
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)
                + shaftOpticalProfileAt(
                        enterT + chord * 0.5,
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)
                + shaftOpticalProfileAt(
                        enterT + chord * (5.0 / 6.0),
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)) / 3.0;
    }
    float phaseFacing = clamp(dot(axis, -rayDirection) * 0.5 + 0.5, 0.0, 1.0);
    float phase = mix(0.20, 1.0, pow(phaseFacing, 3.0));
    float scatterWeight = opticalThickness * phase * fogTransmittance(midpoint);
    float extinction = clamp(shaftParams.y, 0.0, 1.0) * opticalThickness;
    if (scatterWeight <= 0.00001 && extinction <= 0.00001) discard;
    vec3 inScattering = min(max(shaftRadiance * scatterWeight, vec3(0.0)), vec3(65504.0));
    if (any(isnan(inScattering)) || any(isinf(inScattering))
            || isnan(extinction) || isinf(extinction)) discard;
    finalColor = vec4(inScattering, extinction);
}
)";

bool EnsureScreenTriangle(Mesh& mesh)
{
    if (mesh.vaoId != 0) return true;
    constexpr float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f};
    mesh.vertexCount = 3;
    mesh.triangleCount = 1;
    mesh.vertices = static_cast<float*>(MemAlloc(sizeof(vertices)));
    if (mesh.vertices == nullptr) {
        mesh = {};
        return false;
    }
    std::memcpy(mesh.vertices, vertices, sizeof(vertices));
    UploadMesh(&mesh, false);
    if (mesh.vaoId == 0) {
        UnloadMesh(mesh);
        mesh = {};
        return false;
    }
    return true;
}

int FindDynamicIndex(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& lights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return -1;
    const int type = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1
            : source.kind == SectorLightAtmosphereSourceKind::DynamicRect ? 2 : 0;
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

Vector2 RectShaftFarHalfSize(
        const SectorLightAtmosphereVolume& volume,
        float spreadScale)
{
    constexpr float SpreadDegreesAtScaleOne = 15.0f;
    const float spreadHalfAngleDegrees = std::clamp(
            SpreadDegreesAtScaleOne * spreadScale,
            SpreadDegreesAtScaleOne * 0.01f,
            SpreadDegreesAtScaleOne * 2.0f);
    const float expansion = volume.extentWorld
            * std::tan(spreadHalfAngleDegrees * DEG2RAD);
    return Vector2{
            volume.halfWidthWorld + expansion,
            volume.halfHeightWorld + expansion};
}

} // namespace

void SectorAnalyticLightShaftRenderer::Reserve(std::size_t sourceCount)
{
    visibleShafts.reserve(sourceCount);
}

bool SectorAnalyticLightShaftRenderer::EnsureResources()
{
    if (shader.id != 0 && screenTriangle.vaoId != 0
            && material.maps != nullptr) return true;
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
    LOC(shaftShapeLoc, "shaftShape"); LOC(rectRightLoc, "rectRight");
    LOC(rectUpLoc, "rectUp"); LOC(rectNearHalfSizeLoc, "rectNearHalfSize");
    LOC(rectFarHalfSizeLoc, "rectFarHalfSize");
    LOC(shaftRadianceLoc, "shaftRadiance"); LOC(shaftParamsLoc, "shaftParams");
    LOC(fogParamsALoc, "fogParamsA"); LOC(fogParamsBLoc, "fogParamsB");
#undef LOC
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = sceneDepthLoc;
    if (!EnsureScreenTriangle(screenTriangle)) {
        UnloadShader(shader);
        shader = {};
        shaderFailed = true;
        return false;
    }
    material = LoadMaterialDefault();
    if (material.maps == nullptr) {
        UnloadMesh(screenTriangle);
        screenTriangle = {};
        UnloadShader(shader);
        shader = {};
        shaderFailed = true;
        return false;
    }
    material.shader = shader;
    return true;
}

bool SectorAnalyticLightShaftRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyFogSettings& sourceFogSettings,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount = activeCount = drawCallCount = 0;
    scissorCoverage = 0.0f;
    visibleShafts.clear();
    if (sources.empty()
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0 || !EnsureResources()) return false;
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
        if ((source.shape != SectorLightAtmosphereShape::Cone
                    && source.shape != SectorLightAtmosphereShape::RectPrism) || !settings.enabled
                || settings.brightness <= 0.0f
                || !IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) continue;
        SectorLightAtmosphereVolume volume;
        if (!MakeSectorLightAtmosphereVolume(
                    source,
                    settings.lengthScale,
                    settings.originOffsetWorld,
                    volume)) continue;
        // The shared volume helper has a general 0.05 lower scale bound;
        // shafts deliberately support the authored 0.01 minimum.
        const float authoredExtent = source.rangeWorld * settings.lengthScale;
        if (!std::isfinite(authoredExtent) || authoredExtent <= 0.0f) continue;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            volume.coneRadiusWorld *= authoredExtent / volume.extentWorld;
        }
        volume.extentWorld = authoredExtent;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            volume.coneRadiusWorld *= settings.widthScale;
        }
        const Vector2 rectFarHalfSize = source.shape == SectorLightAtmosphereShape::RectPrism
                ? RectShaftFarHalfSize(volume, settings.widthScale)
                : Vector2{};
        volume.boundsCenterWorld = Vector3Add(
                volume.originWorld,
                Vector3Scale(volume.directionWorld, volume.extentWorld * 0.5f));
        volume.boundsRadiusWorld = source.shape == SectorLightAtmosphereShape::Cone
                ? std::sqrt(volume.extentWorld * volume.extentWorld * 0.25f
                        + volume.coneRadiusWorld * volume.coneRadiusWorld)
                : std::sqrt(volume.extentWorld * volume.extentWorld * 0.25f
                        + rectFarHalfSize.x * rectFarHalfSize.x
                        + rectFarHalfSize.y * rectFarHalfSize.y);
        if (!IsSectorLightAtmosphereVolumeVisible(volume, visibility, receiverBounds,
                        camera, aspect, nearPlane, farPlane)) continue;
        const float baseRadius = volume.coneRadiusWorld;
        Vector3 minimum;
        Vector3 maximum;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            ConeBounds(volume.originWorld, volume.directionWorld, volume.extentWorld,
                    baseRadius, minimum, maximum);
        } else {
            const Vector3 center = volume.boundsCenterWorld;
            const Vector3 extent{
                    std::fabs(volume.directionWorld.x) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.x) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.x) * rectFarHalfSize.y,
                    std::fabs(volume.directionWorld.y) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.y) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.y) * rectFarHalfSize.y,
                    std::fabs(volume.directionWorld.z) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.z) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.z) * rectFarHalfSize.y};
            minimum = Vector3Subtract(center, extent);
            maximum = Vector3Add(center, extent);
        }
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
        const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(
                settings.scatteringTint);
        visibleShafts.push_back(VisibleShaft{
                &source,
                volume,
                rectFarHalfSize,
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
    material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlColorMask(true, true, true, false);
    rlEnableScissorTest();
    for (const VisibleShaft& visible : visibleShafts) {
        const SectorLightProxyShaftSettings& settings = visible.source->atmosphere.proxy.shaft;
        const float baseRadius = visible.volume.coneRadiusWorld;
        const Vector2 shaftParams{settings.edgeSoftness, settings.maxExtinction};
        SetShaderValue(shader, coneApexLoc, &visible.volume.originWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneDirectionLoc, &visible.volume.directionWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneLengthLoc, &visible.volume.extentWorld, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, coneBaseRadiusLoc, &baseRadius, SHADER_UNIFORM_FLOAT);
        const int shaftShape = visible.source->shape == SectorLightAtmosphereShape::RectPrism ? 1 : 0;
        const Vector2 rectNearHalfSize{
                visible.volume.halfWidthWorld,
                visible.volume.halfHeightWorld};
        SetShaderValue(shader, shaftShapeLoc, &shaftShape, SHADER_UNIFORM_INT);
        SetShaderValue(shader, rectRightLoc, &visible.volume.rightWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, rectUpLoc, &visible.volume.upWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, rectNearHalfSizeLoc, &rectNearHalfSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, rectFarHalfSizeLoc, &visible.rectFarHalfSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, shaftRadianceLoc, &visible.radiance, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, shaftParamsLoc, &shaftParams, SHADER_UNIFORM_VEC2);
        rlScissor(visible.scissor.x, visible.scissor.y,
                visible.scissor.width, visible.scissor.height);
        DrawMesh(screenTriangle, material, MatrixIdentity());
        ++drawCallCount;
    }
    rlDisableScissorTest();
    rlColorMask(true, true, true, true);
    EndBlendMode();
    EndTextureMode();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
    return true;
}

void SectorAnalyticLightShaftRenderer::Shutdown()
{
    if (material.maps != nullptr) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
        material.shader = {};
        UnloadMaterial(material);
    }
    material = {};
    if (screenTriangle.vaoId != 0) UnloadMesh(screenTriangle);
    screenTriangle = {};
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
    visibleShafts.clear();
    visibleShafts.shrink_to_fit();
}

} // namespace game
