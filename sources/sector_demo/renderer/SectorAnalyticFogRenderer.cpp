#include "sector_demo/renderer/SectorAnalyticFogRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorAtmosphereCulling.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

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
uniform int fogShape; // 0 ellipsoid, 1 yaw-oriented box
uniform int fogStyle; // 0 cloudy volume, 1 room fog
uniform float fogYaw;
uniform vec4 fogNoiseParams; // edge softness, scale, amount, runtime seconds
uniform vec2 fogFlow; // direction radians, speed world units/second
uniform vec2 fogEdgeParams; // edge width, maximum cloudy outward jitter
uniform vec3 fogLighting0;
uniform vec3 fogLighting1;
uniform vec3 fogLighting2;
uniform vec3 fogLighting3;

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(cell + vec3(0,0,0));
    float n100 = hash31(cell + vec3(1,0,0));
    float n010 = hash31(cell + vec3(0,1,0));
    float n110 = hash31(cell + vec3(1,1,0));
    float n001 = hash31(cell + vec3(0,0,1));
    float n101 = hash31(cell + vec3(1,0,1));
    float n011 = hash31(cell + vec3(0,1,1));
    float n111 = hash31(cell + vec3(1,1,1));
    return mix(mix(mix(n000,n100,f.x), mix(n010,n110,f.x), f.y),
               mix(mix(n001,n101,f.x), mix(n011,n111,f.x), f.y), f.z);
}

vec3 worldToBox(vec3 value) {
    float cosine = cos(fogYaw);
    float sine = sin(fogYaw);
    return vec3(
            cosine * value.x - sine * value.z,
            value.y,
            sine * value.x + cosine * value.z);
}

bool intersectEllipsoid(
        vec3 origin,
        vec3 direction,
        vec3 radii,
        out float enterT,
        out float exitT) {
    vec3 inverseRadii = 1.0 / max(radii, vec3(0.0001));
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

bool intersectBox(
        vec3 origin,
        vec3 direction,
        vec3 inputRadii,
        out float enterT,
        out float exitT) {
    vec3 radii = max(inputRadii, vec3(0.0001));
    vec3 o = worldToBox(origin - fogCenter);
    vec3 d = worldToBox(direction);
    const float epsilon = 0.000001;
    if ((abs(d.x) <= epsilon && abs(o.x) > radii.x)
            || (abs(d.y) <= epsilon && abs(o.y) > radii.y)
            || (abs(d.z) <= epsilon && abs(o.z) > radii.z)) return false;
    vec3 inverseDirection = vec3(
            abs(d.x) > epsilon ? 1.0 / d.x : 1e20,
            abs(d.y) > epsilon ? 1.0 / d.y : 1e20,
            abs(d.z) > epsilon ? 1.0 / d.z : 1e20);
    vec3 t0 = (-radii - o) * inverseDirection;
    vec3 t1 = (radii - o) * inverseDirection;
    vec3 minimumT = min(t0, t1);
    vec3 maximumT = max(t0, t1);
    enterT = max(max(minimumT.x, minimumT.y), minimumT.z);
    exitT = min(min(maximumT.x, maximumT.y), maximumT.z);
    return exitT > max(enterT, 0.0);
}

vec3 normalizedLocalPosition(vec3 worldPosition) {
    vec3 offset = worldPosition - fogCenter;
    if (fogShape == 1) offset = worldToBox(offset);
    return offset / max(fogRadii, vec3(0.0001));
}

float sampleFogNoiseAtScale(
        vec3 worldPosition,
        vec2 flowWorld,
        float noiseScale) {
    vec3 noisePosition = worldPosition;
    noisePosition.xz -= flowWorld;
    return smoothstep(
            0.15,
            0.85,
            valueNoise(noisePosition / max(noiseScale, 0.05)));
}

float sampleFogNoise(vec3 worldPosition, vec2 flowWorld) {
    return sampleFogNoiseAtScale(
            worldPosition,
            flowWorld,
            fogNoiseParams.y);
}

float roundedBoxDistance(vec3 localPosition, vec3 halfExtents, float radius) {
    vec3 q = abs(localPosition) - max(halfExtents - vec3(radius), vec3(0.0001));
    return length(max(q, vec3(0.0)))
            + min(max(q.x, max(q.y, q.z)), 0.0)
            - radius;
}

float boxDistance(vec3 localPosition, vec3 halfExtents) {
    vec3 q = abs(localPosition) - halfExtents;
    return length(max(q, vec3(0.0)))
            + min(max(q.x, max(q.y, q.z)), 0.0);
}

float ellipsoidDistance(vec3 localPosition) {
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    float normalizedRadius = length(
            localPosition / max(fogRadii, vec3(0.0001)));
    return (normalizedRadius - 1.0) * minimumRadius;
}

float analyticShapeDistance(vec3 localPosition, bool roundedCloudyBox) {
    if (fogShape == 0) return ellipsoidDistance(localPosition);
    if (!roundedCloudyBox) {
        return boxDistance(localPosition, max(fogRadii, vec3(0.0001)));
    }
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    return roundedBoxDistance(
            localPosition,
            max(fogRadii, vec3(0.0001)),
            minimumRadius * 0.15);
}

float cloudyBoundary(vec3 localPosition, float silhouetteNoise) {
    float edgeJitter = (silhouetteNoise - 0.5) * 2.0 * fogEdgeParams.y;
    float distance = analyticShapeDistance(localPosition, true);
    return smoothstep(
            0.0,
            max(fogEdgeParams.x, 0.0001),
            -distance + edgeJitter);
}

float roomBoundary(vec3 localPosition) {
    float distance = analyticShapeDistance(localPosition, false);
    return smoothstep(
            0.0,
            max(fogEdgeParams.x, 0.0001),
            -distance);
}

vec3 interpolateFogLighting(vec2 normalizedLocalXZ) {
    const float footprint = 0.5;
    vec2 uv = clamp(
            normalizedLocalXZ / (2.0 * footprint) + 0.5,
            vec2(0.0),
            vec2(1.0));
    vec3 negativeZ = mix(fogLighting0, fogLighting1, uv.x);
    vec3 positiveZ = mix(fogLighting2, fogLighting3, uv.x);
    return mix(negativeZ, positiveZ, uv.y);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    vec3 intersectionRadii = fogRadii
            + vec3(fogStyle == 0 ? fogEdgeParams.y : 0.0);
    bool intersects = fogShape == 1
            ? intersectBox(
                    cameraPosition,
                    rayDirection,
                    intersectionRadii,
                    enterT,
                    exitT)
            : intersectEllipsoid(
                    cameraPosition,
                    rayDirection,
                    intersectionRadii,
                    enterT,
                    exitT);
    if (!intersects) discard;
    enterT = max(enterT, 0.0);
    if (exitT <= enterT) discard;
    vec2 flowWorld = vec2(cos(fogFlow.x), sin(fogFlow.x))
            * fogFlow.y * fogNoiseParams.w;
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    exitT = min(exitT, sceneDistance);
    float chord = max(exitT - enterT, 0.0);
    if (chord <= 0.0) discard;
    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    float pixelsPerWorld = viewportSize.y / max(
            2.0 * tanHalfFov * max(midpointT, nearPlane),
            0.0001);
    float projectedNoisePixels = max(fogNoiseParams.y, 0.05) * pixelsPerWorld;
    float noiseDetail = smoothstep(4.0, 16.0, projectedNoisePixels);
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    float projectedMinimumDiameterPixels = 2.0 * minimumRadius * pixelsPerWorld;
    float peakAttenuation = mix(
            0.65,
            1.0,
            smoothstep(48.0, 192.0, projectedMinimumDiameterPixels));
    float smallVolumeVisibility = smoothstep(
            12.0,
            24.0,
            projectedMinimumDiameterPixels);
    float noiseModulation = 1.0;
    if (fogNoiseParams.z > 0.0001) {
        if (fogStyle == 0) {
            float nearNoise = sampleFogNoise(
                    cameraPosition + rayDirection * mix(enterT, exitT, 0.20),
                    flowWorld);
            float middleNoise = sampleFogNoise(midpoint, flowWorld);
            float farNoise = sampleFogNoise(
                    cameraPosition + rayDirection * mix(enterT, exitT, 0.80),
                    flowWorld);
            nearNoise = mix(0.5, nearNoise, noiseDetail);
            middleNoise = mix(0.5, middleNoise, noiseDetail);
            farNoise = mix(0.5, farNoise, noiseDetail);
            float integratedNoise = nearNoise * 0.25
                    + middleNoise * 0.50
                    + farNoise * 0.25;
            noiseModulation = mix(
                    1.0,
                    mix(0.30, 1.70, integratedNoise),
                    fogNoiseParams.z);
        } else {
            float filteredNoise = mix(
                    0.5,
                    sampleFogNoise(midpoint, flowWorld),
                    noiseDetail);
            noiseModulation = mix(
                    1.0,
                    mix(0.80, 1.20, filteredNoise),
                    fogNoiseParams.z);
        }
    }
    vec3 midpointLocal = midpoint - fogCenter;
    if (fogShape == 1) midpointLocal = worldToBox(midpointLocal);
    float boundary = 1.0;
    if (fogStyle == 0) {
        float silhouetteScale = max(fogNoiseParams.y, minimumRadius * 0.75);
        float silhouetteNoise = sampleFogNoiseAtScale(
                midpoint,
                flowWorld,
                silhouetteScale);
        boundary = cloudyBoundary(midpointLocal, silhouetteNoise);
    } else {
        boundary = roomBoundary(midpointLocal);
    }
    if (boundary <= 0.00001) discard;
    float shapedPath = chord * boundary * noiseModulation;
    float range = max(fogParams.y - fogParams.x, 0.0001);
    float pathProfile = clamp((shapedPath - fogParams.x) / range, 0.0, 1.0);
    float opacity = clamp(fogParams.w, 0.0, 1.0)
            * pow(pathProfile, max(fogParams.z, 0.0001))
            * peakAttenuation
            * smallVolumeVisibility;
    if (opacity <= 0.00001) discard;
    vec3 midpointNormalized = normalizedLocalPosition(midpoint);
    vec3 staticLighting = interpolateFogLighting(midpointNormalized.xz);
    finalColor = vec4(
            min(max(fogColor * staticLighting, vec3(0.0)), vec3(65504.0)),
            opacity);
}
)";

bool SameVector3(Vector3 left, Vector3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool SameColor(Color left, Color right)
{
    return left.r == right.r && left.g == right.g
            && left.b == right.b && left.a == right.a;
}

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
    LOC(fogShapeLoc, "fogShape"); LOC(fogStyleLoc, "fogStyle");
    LOC(fogYawLoc, "fogYaw");
    LOC(fogNoiseParamsLoc, "fogNoiseParams"); LOC(fogFlowLoc, "fogFlow");
    LOC(fogEdgeParamsLoc, "fogEdgeParams");
    LOC(fogLightingLocs[0], "fogLighting0");
    LOC(fogLightingLocs[1], "fogLighting1");
    LOC(fogLightingLocs[2], "fogLighting2");
    LOC(fogLightingLocs[3], "fogLighting3");
#undef LOC
    return true;
}

void SectorAnalyticFogRenderer::ClearStaticLightingCache()
{
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        entry = StaticLightingCacheEntry{};
    }
}

void SectorAnalyticFogRenderer::RefreshStaticLightingCacheIdentity(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes)
{
    const SectorBakedObjectLightProbe* probeData = objectLightProbes.probes.empty()
            ? nullptr
            : objectLightProbes.probes.data();
    const std::size_t probeSourceHashValue =
            std::hash<std::string>{}(objectLightProbes.metadata.sourceHash);
    const std::size_t mapProbeSourceHashValue =
            std::hash<std::string>{}(map.bakedLightmap.objectProbes.sourceHash);
    if (cachedProbeData == probeData
            && cachedProbeCount == objectLightProbes.probes.size()
            && cachedProbeSourceHashValue == probeSourceHashValue
            && cachedMapProbeSourceHashValue == mapProbeSourceHashValue) {
        return;
    }
    ClearStaticLightingCache();
    cachedProbeData = probeData;
    cachedProbeCount = objectLightProbes.probes.size();
    cachedProbeSourceHashValue = probeSourceHashValue;
    cachedMapProbeSourceHashValue = mapProbeSourceHashValue;
}

const SectorLocalFogStaticLightingSamples& SectorAnalyticFogRenderer::StaticLightingForVolume(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorCompiledLocalFogVolume& volume)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, volume.topologySectorId);
    const Color sectorAmbientColor = sector != nullptr ? sector->ambientColor : Color{};
    const float sectorAmbientIntensity = sector != nullptr ? sector->ambientIntensity : 0.0f;
    const float lightingYaw = volume.shape == SectorLocalFogShape::Box
            ? volume.yawRadians
            : 0.0f;
    StaticLightingCacheEntry* available = nullptr;
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        if (entry.valid && entry.sourceFogVolumeId == volume.sourceAuthoringFogVolumeId) {
            if (entry.topologySectorId == volume.topologySectorId
                    && SameVector3(entry.centerWorld, volume.centerWorld)
                    && SameVector3(entry.radiiWorld, volume.radiiWorld)
                    && entry.yawRadians == lightingYaw
                    && SameColor(entry.sectorAmbientColor, sectorAmbientColor)
                    && entry.sectorAmbientIntensity == sectorAmbientIntensity) {
                return entry.samples;
            }
            available = &entry;
            break;
        }
        if (!entry.valid && available == nullptr) available = &entry;
    }
    if (available == nullptr) {
        const std::size_t slot = static_cast<std::size_t>(
                std::max(volume.sourceAuthoringFogVolumeId, 0)) % staticLightingCache.size();
        available = &staticLightingCache[slot];
    }
    available->valid = true;
    available->sourceFogVolumeId = volume.sourceAuthoringFogVolumeId;
    available->topologySectorId = volume.topologySectorId;
    available->centerWorld = volume.centerWorld;
    available->radiiWorld = volume.radiiWorld;
    available->yawRadians = lightingYaw;
    available->sectorAmbientColor = sectorAmbientColor;
    available->sectorAmbientIntensity = sectorAmbientIntensity;
    available->samples = SampleSectorLocalFogStaticLighting(
            map, objectLightProbes, volume, lightingYaw);
    return available->samples;
}

bool SectorAnalyticFogRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyMap& map,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes)
{
    eligibleCount = 0;
    activeCount = 0;
    scissorCoverage = 0.0f;
    visibleVolumes.clear();
    if (sceneTarget.texture.id == 0
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
        if (!volume.enabled || volume.maxOpacity <= 0.0f) continue;
        const bool roomStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room;
        const float edgeExpansion = roomStyle
                ? 0.0f
                : ComputeSectorAnalyticFogCloudyEdgeExpansion(
                        volume.radiiWorld,
                        volume.edgeSoftness,
                        volume.noiseAmount);
        const Vector3 renderRadii{
                volume.radiiWorld.x + edgeExpansion,
                volume.radiiWorld.y + edgeExpansion,
                volume.radiiWorld.z + edgeExpansion};
        const Vector3 boundsExtents = volume.shape == SectorLocalFogShape::Box
                ? ComputeSectorAtmosphereYawedHalfExtents(
                        renderRadii, volume.yawRadians)
                : renderRadii;
        const Vector3 minimum = Vector3Subtract(volume.centerWorld, boundsExtents);
        const Vector3 maximum = Vector3Add(volume.centerWorld, boundsExtents);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        if (scissor.Empty()) continue;
        ++eligibleCount;
        visibleVolumes.push_back(VisibleVolume{
                &volume,
                scissor,
                Vector3DistanceSqr(camera.position, volume.centerWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
    }
    std::sort(visibleVolumes.begin(), visibleVolumes.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) return left.distanceSquared > right.distanceSquared;
        return left.volume->sourceAuthoringFogVolumeId < right.volume->sourceAuthoringFogVolumeId;
    });
    activeCount = static_cast<int>(visibleVolumes.size());
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleVolumes.empty()) return false;
    RefreshStaticLightingCacheIdentity(map, objectLightProbes);
    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
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
    BeginBlendMode(BLEND_ALPHA);
    rlEnableScissorTest();
    for (const VisibleVolume& entry : visibleVolumes) {
        const SectorCompiledLocalFogVolume& volume = *entry.volume;
        const Vector3 color = engine::SrgbColorBytesToLinearSceneRgb(volume.color);
        const Vector4 params{volume.analyticStartDistanceWorld,
                volume.analyticEndDistanceWorld, volume.analyticFalloffExponent,
                volume.maxOpacity};
        const int shape = volume.shape == SectorLocalFogShape::Box ? 1 : 0;
        const int analyticStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room ? 1 : 0;
        const Vector4 noiseParams{
                volume.edgeSoftness,
                volume.noiseScaleWorld,
                volume.noiseAmount,
                runtimeSeconds};
        const Vector2 flow{
                volume.flowDirectionDegrees * DEG2RAD,
                volume.flowSpeedWorld};
        const bool roomStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room;
        const Vector2 edgeParams{
                ComputeSectorAnalyticFogEdgeWidth(
                        volume.radiiWorld,
                        volume.edgeSoftness,
                        roomStyle),
                roomStyle
                        ? 0.0f
                        : ComputeSectorAnalyticFogCloudyEdgeExpansion(
                                volume.radiiWorld,
                                volume.edgeSoftness,
                                volume.noiseAmount)};
        const SectorLocalFogStaticLightingSamples& lighting =
                StaticLightingForVolume(map, objectLightProbes, volume);
        // rlDrawRenderBatchActive() clears raylib's auxiliary sampler slots.
        // Register sceneDepth after the flush so it remains bound for this draw.
        rlDrawRenderBatchActive();
        SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);
        SetShaderValue(shader, centerLoc, &volume.centerWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, radiiLoc, &volume.radiiWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, colorLoc, &color, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, fogParamsLoc, &params, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, fogShapeLoc, &shape, SHADER_UNIFORM_INT);
        SetShaderValue(shader, fogStyleLoc, &analyticStyle, SHADER_UNIFORM_INT);
        SetShaderValue(shader, fogYawLoc, &volume.yawRadians, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, fogNoiseParamsLoc, &noiseParams, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, fogFlowLoc, &flow, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, fogEdgeParamsLoc, &edgeParams, SHADER_UNIFORM_VEC2);
        for (std::size_t index = 0; index < lighting.corners.size(); ++index) {
            SetShaderValue(
                    shader,
                    fogLightingLocs[index],
                    &lighting.corners[index],
                    SHADER_UNIFORM_VEC3);
        }
        rlScissor(
                entry.scissor.x,
                entry.scissor.y,
                entry.scissor.width,
                entry.scissor.height);
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
    ClearStaticLightingCache();
    cachedProbeData = nullptr;
    cachedProbeCount = 0;
    cachedProbeSourceHashValue = 0;
    cachedMapProbeSourceHashValue = 0;
}

} // namespace game
