#include "sector_demo/renderer/SectorVolumetricAtmosphereClusters.h"

#include "engine/render/ColorTransfer.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {
namespace {

Vector3 SafeNormalize(Vector3 value, Vector3 fallback)
{
    return Vector3LengthSqr(value) > 0.00000001f
            ? Vector3Normalize(value)
            : fallback;
}

bool ContainsSector(
        const RuntimePortalVisibilityResult& visibility,
        int sectorId)
{
    return std::find(
            visibility.visibleSectorIds.begin(),
            visibility.visibleSectorIds.end(),
            sectorId) != visibility.visibleSectorIds.end();
}

float DistanceSquaredToAabb(Vector3 point, Vector3 center, Vector3 radii)
{
    const Vector3 minimum = Vector3Subtract(center, radii);
    const Vector3 maximum = Vector3Add(center, radii);
    const Vector3 closest{
            std::clamp(point.x, minimum.x, maximum.x),
            std::clamp(point.y, minimum.y, maximum.y),
            std::clamp(point.z, minimum.z, maximum.z)};
    return Vector3DistanceSqr(point, closest);
}

float ProjectedViewCoverage(
        const SectorVolumetricProjectedClusterBounds& bounds,
        const SectorVolumetricDepthSliceLayout& depth)
{
    const float xy = std::max(0.0f, bounds.gridMaximumX - bounds.gridMinimumX)
            * std::max(0.0f, bounds.gridMaximumY - bounds.gridMinimumY);
    const float totalDepth = depth.endpoints[
            static_cast<std::size_t>(depth.sliceCount)] - depth.endpoints[0];
    const float depthCoverage = totalDepth > 0.0f
            ? std::clamp((bounds.maximumDepth - bounds.minimumDepth)
                    / totalDepth, 0.0f, 1.0f)
            : 0.0f;
    return xy * std::max(depthCoverage, 1.0f / std::max(depth.sliceCount, 1));
}

bool BetterLightView(
        const SectorVolumetricGpuLightRecord& left,
        const SectorVolumetricGpuLightRecord& right)
{
    if (left.viewImportance != right.viewImportance) {
        return left.viewImportance > right.viewImportance;
    }
    if (left.kind != right.kind) {
        return static_cast<int>(left.kind) < static_cast<int>(right.kind);
    }
    return left.stableId < right.stableId;
}

bool BetterVolumeView(
        const SectorVolumetricGpuVolumeRecord& left,
        const SectorVolumetricGpuVolumeRecord& right)
{
    if (left.viewDistanceSquared != right.viewDistanceSquared) {
        return left.viewDistanceSquared < right.viewDistanceSquared;
    }
    return left.stableId < right.stableId;
}

float IntervalOverlap(float firstMin, float firstMax, float secondMin, float secondMax)
{
    return std::max(0.0f, std::min(firstMax, secondMax)
            - std::max(firstMin, secondMin));
}

int FindShadowSlot(
        const SectorVolumetricGpuLightRecord& light,
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    if (light.kind != SectorLightAtmosphereSourceKind::DynamicSpot) return -1;
    for (int index = 0; index < dynamicLights.dynamicLightCount; ++index) {
        if (dynamicLights.dynamicLightIds[static_cast<std::size_t>(index)]
                            == light.stableId
                && dynamicLights.dynamicLightTypes[
                           static_cast<std::size_t>(index)] == 1) {
            return dynamicLights.shadowUniforms.dynamicLightShadowSlots[
                    static_cast<std::size_t>(index)];
        }
    }
    return -1;
}

} // namespace

bool ProjectSectorVolumetricSphereToClusters(
        const SectorVolumetricClusterCamera& camera,
        Vector3 centerWorld,
        float radiusWorld,
        SectorVolumetricProjectedClusterBounds& outBounds)
{
    outBounds = {};
    if (!std::isfinite(radiusWorld) || radiusWorld <= 0.0f
            || camera.grid.x <= 0 || camera.grid.y <= 0
            || camera.depth.sliceCount <= 0
            || !std::isfinite(camera.aspectRatio)
            || camera.aspectRatio <= 0.0f) {
        return false;
    }
    const Vector3 forward = SafeNormalize(
            Vector3Subtract(camera.camera.target, camera.camera.position),
            Vector3{0.0f, 0.0f, 1.0f});
    const Vector3 right = SafeNormalize(
            Vector3CrossProduct(forward, camera.camera.up),
            Vector3{1.0f, 0.0f, 0.0f});
    const Vector3 up = SafeNormalize(
            Vector3CrossProduct(right, forward),
            Vector3{0.0f, 1.0f, 0.0f});
    const Vector3 relative = Vector3Subtract(centerWorld, camera.camera.position);
    const float centerDepth = Vector3DotProduct(relative, forward);
    const float depthStart = camera.depth.endpoints[0];
    const float depthEnd = camera.depth.endpoints[
            static_cast<std::size_t>(camera.depth.sliceCount)];
    const float minimumDepth = std::max(depthStart, centerDepth - radiusWorld);
    const float maximumDepth = std::min(depthEnd, centerDepth + radiusWorld);
    if (maximumDepth <= minimumDepth || centerDepth + radiusWorld <= 0.0f) {
        return false;
    }
    const float tanHalfFov = std::tan(camera.camera.fovy * DEG2RAD * 0.5f);
    if (!std::isfinite(tanHalfFov) || tanHalfFov <= 0.0f) return false;
    const float projectionDepth = std::max(minimumDepth, depthStart);
    const float centerX = Vector3DotProduct(relative, right);
    const float centerY = Vector3DotProduct(relative, up);
    float ndcMinimumX = (centerX - radiusWorld)
            / (projectionDepth * tanHalfFov * camera.aspectRatio);
    float ndcMaximumX = (centerX + radiusWorld)
            / (projectionDepth * tanHalfFov * camera.aspectRatio);
    float ndcMinimumY = (centerY - radiusWorld)
            / (projectionDepth * tanHalfFov);
    float ndcMaximumY = (centerY + radiusWorld)
            / (projectionDepth * tanHalfFov);
    if (ndcMaximumX <= -1.0f || ndcMinimumX >= 1.0f
            || ndcMaximumY <= -1.0f || ndcMinimumY >= 1.0f) {
        return false;
    }
    ndcMinimumX = std::clamp(ndcMinimumX, -1.0f, 1.0f);
    ndcMaximumX = std::clamp(ndcMaximumX, -1.0f, 1.0f);
    ndcMinimumY = std::clamp(ndcMinimumY, -1.0f, 1.0f);
    ndcMaximumY = std::clamp(ndcMaximumY, -1.0f, 1.0f);
    outBounds.gridMinimumX = (ndcMinimumX * 0.5f + 0.5f) * camera.grid.x;
    outBounds.gridMaximumX = (ndcMaximumX * 0.5f + 0.5f) * camera.grid.x;
    outBounds.gridMinimumY = (ndcMinimumY * 0.5f + 0.5f) * camera.grid.y;
    outBounds.gridMaximumY = (ndcMaximumY * 0.5f + 0.5f) * camera.grid.y;
    outBounds.minimumDepth = minimumDepth;
    outBounds.maximumDepth = maximumDepth;
    outBounds.minimumX = std::clamp(
            static_cast<int>(std::floor(outBounds.gridMinimumX)),
            0, camera.grid.x - 1);
    outBounds.maximumX = std::clamp(
            static_cast<int>(std::ceil(outBounds.gridMaximumX)) - 1,
            0, camera.grid.x - 1);
    outBounds.minimumY = std::clamp(
            static_cast<int>(std::floor(outBounds.gridMinimumY)),
            0, camera.grid.y - 1);
    outBounds.maximumY = std::clamp(
            static_cast<int>(std::ceil(outBounds.gridMaximumY)) - 1,
            0, camera.grid.y - 1);
    const int minimumSlice = FindSectorVolumetricDepthSlice(
            camera.depth, minimumDepth);
    const int maximumSlice = FindSectorVolumetricDepthSlice(
            camera.depth, maximumDepth);
    if (minimumSlice < 0 || maximumSlice < 0) return false;
    outBounds.minimumBand = minimumSlice / 8;
    outBounds.maximumBand = maximumSlice / 8;
    return outBounds.maximumX >= outBounds.minimumX
            && outBounds.maximumY >= outBounds.minimumY
            && outBounds.maximumBand >= outBounds.minimumBand;
}

bool SectorVolumetricClusterBuilder::Configure(
        SectorVolumetricGridSize grid,
        int clusterBandCount)
{
    if (grid.x <= 0 || grid.y <= 0 || grid.z <= 0
            || clusterBandCount <= 0 || grid.z != clusterBandCount * 8) {
        Clear();
        return false;
    }
    if (configuredGrid.x == grid.x && configuredGrid.y == grid.y
            && configuredGrid.z == grid.z
            && configuredBands == clusterBandCount) {
        return true;
    }
    configuredGrid = grid;
    configuredBands = clusterBandCount;
    clusterCount = static_cast<std::size_t>(grid.x)
            * static_cast<std::size_t>(grid.y)
            * static_cast<std::size_t>(clusterBandCount);
    const std::size_t slots = clusterCount
            * static_cast<std::size_t>(SectorVolumetricMaximumClusterLights);
    lightClusterIndices.resize(slots);
    volumeClusterIndices.resize(slots);
    lightClusterImportance.resize(slots);
    volumeClusterDistance.resize(slots);
    clusterCenters.resize(clusterCount);
    ResetFrame();
    return true;
}

void SectorVolumetricClusterBuilder::ResetFrame()
{
    lights = {};
    volumes = {};
    diagnostics = {};
    std::fill(lightClusterIndices.begin(), lightClusterIndices.end(),
            static_cast<std::uint8_t>(SectorVolumetricListTerminator));
    std::fill(volumeClusterIndices.begin(), volumeClusterIndices.end(),
            static_cast<std::uint8_t>(SectorVolumetricListTerminator));
    std::fill(lightClusterImportance.begin(), lightClusterImportance.end(),
            -std::numeric_limits<float>::infinity());
    std::fill(volumeClusterDistance.begin(), volumeClusterDistance.end(),
            std::numeric_limits<float>::infinity());
}

void SectorVolumetricClusterBuilder::Clear()
{
    configuredGrid = {};
    configuredBands = 0;
    clusterCount = 0;
    lights = {};
    volumes = {};
    lightClusterIndices.clear();
    volumeClusterIndices.clear();
    lightClusterImportance.clear();
    volumeClusterDistance.clear();
    clusterCenters.clear();
    diagnostics = {};
}

bool SectorVolumetricClusterBuilder::InsertLightView(
        const SectorVolumetricGpuLightRecord& record)
{
    const int count = diagnostics.retainedLightCount;
    constexpr int capacity = SectorVolumetricMaximumViewLights;
    if (count >= capacity && !BetterLightView(record, lights[capacity - 1])) {
        return false;
    }
    int insertAt = std::min(count, capacity - 1);
    if (count < capacity) ++diagnostics.retainedLightCount;
    while (insertAt > 0 && BetterLightView(record, lights[insertAt - 1])) {
        lights[static_cast<std::size_t>(insertAt)] =
                lights[static_cast<std::size_t>(insertAt - 1)];
        --insertAt;
    }
    lights[static_cast<std::size_t>(insertAt)] = record;
    return true;
}

bool SectorVolumetricClusterBuilder::InsertVolumeView(
        const SectorVolumetricGpuVolumeRecord& record)
{
    const int count = diagnostics.retainedVolumeCount;
    constexpr int capacity = SectorVolumetricMaximumViewVolumes;
    if (count >= capacity && !BetterVolumeView(record, volumes[capacity - 1])) {
        return false;
    }
    int insertAt = std::min(count, capacity - 1);
    if (count < capacity) ++diagnostics.retainedVolumeCount;
    while (insertAt > 0 && BetterVolumeView(record, volumes[insertAt - 1])) {
        volumes[static_cast<std::size_t>(insertAt)] =
                volumes[static_cast<std::size_t>(insertAt - 1)];
        --insertAt;
    }
    volumes[static_cast<std::size_t>(insertAt)] = record;
    return true;
}

bool SectorVolumetricClusterBuilder::Build(
        const SectorTopologyMap& map,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const SectorPreviewDynamicPointLightSource* runtimePointLight,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        const Camera3D& camera,
        float aspectRatio,
        const SectorVolumetricDepthSliceLayout& depth,
        float runtimeSeconds,
        bool dynamicLightingEnabled)
{
    if (clusterCount == 0 || depth.sliceCount != configuredGrid.z
            || depth.clusterBandCount != configuredBands) {
        return false;
    }
    ResetFrame();
    const SectorVolumetricClusterCamera clusterCamera{
            camera, aspectRatio, configuredGrid, depth};

    for (const SectorLightAtmosphereSource& source : sources) {
        const bool dynamic = IsSectorLightAtmosphereSourceDynamic(source);
        const SectorLightAtmosphereSettings atmosphere =
                NormalizeSectorLightAtmosphereSettings(source.atmosphere);
        if ((dynamic && !dynamicLightingEnabled)
                || source.rangeWorld <= 0.0f || source.intensity <= 0.0f
                || atmosphere.volumetricScatteringIntensity <= 0.0f) {
            continue;
        }
        SectorLightAtmosphereVolume bounds;
        if (!MakeSectorLightAtmosphereVolume(source, 1.0f, bounds)
                || !IsSectorLightAtmosphereVolumeVisible(
                        bounds, visibility, receiverBounds, camera,
                        aspectRatio, depth.endpoints[0],
                        depth.endpoints[static_cast<std::size_t>(depth.sliceCount)])) {
            continue;
        }
        SectorVolumetricGpuLightRecord record;
        record.kind = source.kind;
        record.stableId = source.lightId;
        record.positionWorld = source.positionWorld;
        record.directionWorld = source.directionWorld;
        record.linearColor = engine::SrgbColorBytesToLinearSceneRgb(source.color);
        record.rangeWorld = source.rangeWorld;
        record.effectiveIntensity = source.intensity * (source.flicker
                ? EvaluateDynamicLightFlickerMultiplier(
                        source.lightId, runtimeSeconds,
                        source.flickerSpeed, source.flickerAmount)
                : 1.0f);
        record.scatteringIntensity = atmosphere.volumetricScatteringIntensity;
        record.innerConeCos = source.innerConeCos;
        record.outerConeCos = source.outerConeCos;
        if (!ProjectSectorVolumetricSphereToClusters(
                    clusterCamera, bounds.boundsCenterWorld,
                    bounds.boundsRadiusWorld, record.projected)) {
            continue;
        }
        record.viewImportance = ProjectedViewCoverage(
                record.projected, depth)
                * record.effectiveIntensity * record.scatteringIntensity;
        record.shadowSlot = FindShadowSlot(record, dynamicLights);
        ++diagnostics.eligibleLightCount;
        InsertLightView(record);
    }

    if (runtimePointLight != nullptr && dynamicLightingEnabled
            && runtimePointLight->light.radius > 0.0f
            && runtimePointLight->light.intensity > 0.0f) {
        SectorLightAtmosphereSource source;
        source.kind = SectorLightAtmosphereSourceKind::DynamicPoint;
        source.shape = SectorLightAtmosphereShape::Sphere;
        source.lightId = runtimePointLight->lightId;
        source.ownerSectorId = runtimePointLight->ownerSectorId;
        source.positionWorld = runtimePointLight->light.position;
        source.rangeWorld = runtimePointLight->light.radius;
        source.intensity = runtimePointLight->light.intensity;
        SectorLightAtmosphereVolume bounds;
        if (MakeSectorLightAtmosphereVolume(source, 1.0f, bounds)
                && IsSectorLightAtmosphereVolumeVisible(
                        bounds, visibility, receiverBounds, camera,
                        aspectRatio, depth.endpoints[0],
                        depth.endpoints[static_cast<std::size_t>(depth.sliceCount)])) {
            SectorVolumetricGpuLightRecord record;
            record.kind = source.kind;
            record.stableId = source.lightId;
            record.positionWorld = source.positionWorld;
            record.linearColor = runtimePointLight->light.color;
            record.rangeWorld = source.rangeWorld;
            record.effectiveIntensity = source.intensity;
            record.scatteringIntensity = 1.0f;
            if (ProjectSectorVolumetricSphereToClusters(
                        clusterCamera, bounds.boundsCenterWorld,
                        bounds.boundsRadiusWorld, record.projected)) {
                record.viewImportance = ProjectedViewCoverage(
                        record.projected, depth) * record.effectiveIntensity;
                ++diagnostics.eligibleLightCount;
                InsertLightView(record);
            }
        }
    }
    diagnostics.lightViewOverflowCount = std::max(0,
            diagnostics.eligibleLightCount - diagnostics.retainedLightCount);

    for (const SectorCompiledLocalFogVolume& source : map.compiledLocalFogVolumes) {
        if (!source.enabled || source.density <= 0.0f
                || source.maxOpacity <= 0.0f
                || source.radiiWorld.x <= 0.0f
                || source.radiiWorld.y <= 0.0f
                || source.radiiWorld.z <= 0.0f) {
            continue;
        }
        if (visibility.validStartSector && !visibility.fallbackDrawAll
                && source.topologySectorId > 0
                && !ContainsSector(visibility, source.topologySectorId)) {
            continue;
        }
        SectorVolumetricGpuVolumeRecord record;
        record.stableId = source.sourceAuthoringFogVolumeId;
        record.topologySectorId = source.topologySectorId;
        record.centerWorld = source.centerWorld;
        record.radiiWorld = source.radiiWorld;
        record.linearTint = engine::SrgbColorBytesToLinearSceneRgb(source.color);
        record.density = source.density;
        record.maximumOpacity = source.maxOpacity;
        record.edgeSoftness = source.edgeSoftness;
        record.noiseAmount = source.noiseAmount;
        record.noiseScaleWorld = source.noiseScaleWorld;
        record.flowDirectionRadians = source.flowDirectionDegrees * DEG2RAD;
        record.flowSpeedWorld = source.flowSpeedWorld;
        const float radius = std::max({
                source.radiiWorld.x,
                source.radiiWorld.y,
                source.radiiWorld.z});
        if (!ProjectSectorVolumetricSphereToClusters(
                    clusterCamera, source.centerWorld, radius,
                    record.projected)) {
            continue;
        }
        record.viewDistanceSquared = DistanceSquaredToAabb(
                camera.position, source.centerWorld, source.radiiWorld);
        ++diagnostics.eligibleVolumeCount;
        InsertVolumeView(record);
    }
    diagnostics.volumeViewOverflowCount = std::max(0,
            diagnostics.eligibleVolumeCount - diagnostics.retainedVolumeCount);

    BuildClusterCenters(clusterCamera);
    BuildLightLists(clusterCamera);
    BuildVolumeLists(clusterCamera);
    return true;
}

void SectorVolumetricClusterBuilder::BuildClusterCenters(
        const SectorVolumetricClusterCamera& camera)
{
    const Vector3 forward = SafeNormalize(
            Vector3Subtract(camera.camera.target, camera.camera.position),
            Vector3{0.0f, 0.0f, 1.0f});
    const Vector3 right = SafeNormalize(
            Vector3CrossProduct(forward, camera.camera.up),
            Vector3{1.0f, 0.0f, 0.0f});
    const Vector3 up = SafeNormalize(
            Vector3CrossProduct(right, forward),
            Vector3{0.0f, 1.0f, 0.0f});
    const float tanHalfFov = std::tan(camera.camera.fovy * DEG2RAD * 0.5f);
    for (int band = 0; band < configuredBands; ++band) {
        const float bandNear = camera.depth.endpoints[
                static_cast<std::size_t>(band * 8)];
        const float bandFar = camera.depth.endpoints[
                static_cast<std::size_t>((band + 1) * 8)];
        const float depth = std::sqrt(bandNear * bandFar);
        for (int y = 0; y < configuredGrid.y; ++y) {
            const float ndcY = (static_cast<float>(y) + 0.5f)
                    / configuredGrid.y * 2.0f - 1.0f;
            for (int x = 0; x < configuredGrid.x; ++x) {
                const float ndcX = (static_cast<float>(x) + 0.5f)
                        / configuredGrid.x * 2.0f - 1.0f;
                Vector3 ray = Vector3Add(forward,
                        Vector3Add(
                                Vector3Scale(right,
                                        ndcX * tanHalfFov * camera.aspectRatio),
                                Vector3Scale(up, ndcY * tanHalfFov)));
                const std::size_t cluster = (static_cast<std::size_t>(band)
                        * configuredGrid.y + static_cast<std::size_t>(y))
                        * configuredGrid.x + static_cast<std::size_t>(x);
                clusterCenters[cluster] = Vector3Add(
                        camera.camera.position, Vector3Scale(ray, depth));
            }
        }
    }
}

void SectorVolumetricClusterBuilder::BuildLightLists(
        const SectorVolumetricClusterCamera& camera)
{
    for (int lightIndex = 0;
         lightIndex < diagnostics.retainedLightCount;
         ++lightIndex) {
        const auto& light = lights[static_cast<std::size_t>(lightIndex)];
        const auto& bounds = light.projected;
        for (int band = bounds.minimumBand; band <= bounds.maximumBand; ++band) {
            const float bandNear = camera.depth.endpoints[
                    static_cast<std::size_t>(band * 8)];
            const float bandFar = camera.depth.endpoints[
                    static_cast<std::size_t>((band + 1) * 8)];
            const float depthInfluence = IntervalOverlap(
                    bounds.minimumDepth, bounds.maximumDepth,
                    bandNear, bandFar) / std::max(bandFar - bandNear, 0.000001f);
            if (depthInfluence <= 0.0f) continue;
            for (int y = bounds.minimumY; y <= bounds.maximumY; ++y) {
                const float yInfluence = IntervalOverlap(
                        bounds.gridMinimumY, bounds.gridMaximumY,
                        static_cast<float>(y), static_cast<float>(y + 1));
                for (int x = bounds.minimumX; x <= bounds.maximumX; ++x) {
                    const float xInfluence = IntervalOverlap(
                            bounds.gridMinimumX, bounds.gridMaximumX,
                            static_cast<float>(x), static_cast<float>(x + 1));
                    const float importance = xInfluence * yInfluence
                            * depthInfluence * light.effectiveIntensity
                            * light.scatteringIntensity;
                    if (importance <= 0.0f) continue;
                    const std::size_t cluster = (static_cast<std::size_t>(band)
                            * configuredGrid.y + static_cast<std::size_t>(y))
                            * configuredGrid.x + static_cast<std::size_t>(x);
                    const std::size_t base = cluster
                            * SectorVolumetricMaximumClusterLights;
                    int slot = 0;
                    while (slot < SectorVolumetricMaximumClusterLights
                            && lightClusterIndices[base + static_cast<std::size_t>(slot)]
                                    != SectorVolumetricListTerminator) {
                        const int otherIndex = lightClusterIndices[
                                base + static_cast<std::size_t>(slot)];
                        const float otherImportance = lightClusterImportance[
                                base + static_cast<std::size_t>(slot)];
                        const auto& other = lights[static_cast<std::size_t>(otherIndex)];
                        const bool better = importance > otherImportance
                                || (importance == otherImportance
                                    && (static_cast<int>(light.kind)
                                                < static_cast<int>(other.kind)
                                        || (light.kind == other.kind
                                            && light.stableId < other.stableId)));
                        if (better) break;
                        ++slot;
                    }
                    if (slot >= SectorVolumetricMaximumClusterLights) {
                        ++diagnostics.lightClusterOverflowCount;
                        continue;
                    }
                    if (lightClusterIndices[base
                                + SectorVolumetricMaximumClusterLights - 1]
                            != SectorVolumetricListTerminator) {
                        ++diagnostics.lightClusterOverflowCount;
                    }
                    for (int move = SectorVolumetricMaximumClusterLights - 1;
                         move > slot;
                         --move) {
                        lightClusterIndices[base + static_cast<std::size_t>(move)] =
                                lightClusterIndices[base + static_cast<std::size_t>(move - 1)];
                        lightClusterImportance[base + static_cast<std::size_t>(move)] =
                                lightClusterImportance[base + static_cast<std::size_t>(move - 1)];
                    }
                    lightClusterIndices[base + static_cast<std::size_t>(slot)] =
                            static_cast<std::uint8_t>(lightIndex);
                    lightClusterImportance[base + static_cast<std::size_t>(slot)] =
                            importance;
                }
            }
        }
    }
}

void SectorVolumetricClusterBuilder::BuildVolumeLists(
        const SectorVolumetricClusterCamera& camera)
{
    for (int volumeIndex = 0;
         volumeIndex < diagnostics.retainedVolumeCount;
         ++volumeIndex) {
        const auto& volume = volumes[static_cast<std::size_t>(volumeIndex)];
        const auto& bounds = volume.projected;
        for (int band = bounds.minimumBand; band <= bounds.maximumBand; ++band) {
            for (int y = bounds.minimumY; y <= bounds.maximumY; ++y) {
                for (int x = bounds.minimumX; x <= bounds.maximumX; ++x) {
                    const std::size_t cluster = (static_cast<std::size_t>(band)
                            * configuredGrid.y + static_cast<std::size_t>(y))
                            * configuredGrid.x + static_cast<std::size_t>(x);
                    const float distance = DistanceSquaredToAabb(
                            clusterCenters[cluster],
                            volume.centerWorld,
                            volume.radiiWorld);
                    const std::size_t base = cluster
                            * SectorVolumetricMaximumClusterVolumes;
                    int slot = 0;
                    while (slot < SectorVolumetricMaximumClusterVolumes
                            && volumeClusterIndices[base + static_cast<std::size_t>(slot)]
                                    != SectorVolumetricListTerminator) {
                        const int otherIndex = volumeClusterIndices[
                                base + static_cast<std::size_t>(slot)];
                        const float otherDistance = volumeClusterDistance[
                                base + static_cast<std::size_t>(slot)];
                        const auto& other = volumes[static_cast<std::size_t>(otherIndex)];
                        if (distance < otherDistance
                                || (distance == otherDistance
                                    && volume.stableId < other.stableId)) {
                            break;
                        }
                        ++slot;
                    }
                    if (slot >= SectorVolumetricMaximumClusterVolumes) {
                        ++diagnostics.volumeClusterOverflowCount;
                        continue;
                    }
                    if (volumeClusterIndices[base
                                + SectorVolumetricMaximumClusterVolumes - 1]
                            != SectorVolumetricListTerminator) {
                        ++diagnostics.volumeClusterOverflowCount;
                    }
                    for (int move = SectorVolumetricMaximumClusterVolumes - 1;
                         move > slot;
                         --move) {
                        volumeClusterIndices[base + static_cast<std::size_t>(move)] =
                                volumeClusterIndices[base + static_cast<std::size_t>(move - 1)];
                        volumeClusterDistance[base + static_cast<std::size_t>(move)] =
                                volumeClusterDistance[base + static_cast<std::size_t>(move - 1)];
                    }
                    volumeClusterIndices[base + static_cast<std::size_t>(slot)] =
                            static_cast<std::uint8_t>(volumeIndex);
                    volumeClusterDistance[base + static_cast<std::size_t>(slot)] =
                            distance;
                }
            }
        }
    }
}

} // namespace game
