#include "sector_demo/renderer/LegacyHazeComparisonAdapter.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace game {
namespace {

constexpr float HazeMaximumOpacity = 0.30f;

bool SameVector(Vector3 left, Vector3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

int StableKind(const SectorLightAtmosphereSource& source)
{
    return static_cast<int>(source.kind);
}

} // namespace

int LegacyHazeComparisonAdapter::Build(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        SectorTopologyFogSettings::VolumetricQuality quality,
        const Camera3D& camera,
        float aspectRatio,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::array<SectorVolumetricComparisonVolume, MaximumVolumes>& outVolumes)
{
    outVolumes = {};
    eligibleCount = 0;
    activeCount = 0;
    if (quality == SectorTopologyFogSettings::VolumetricQuality::Off) return 0;

    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) {
        return 0;
    }
    const float aspect = std::max(aspectRatio, 0.001f);
    const int cap = std::min<int>(
            GetSectorLegacyAtmosphereQualityContract(quality).maximumHazeVolumes,
            static_cast<int>(MaximumVolumes));
    if (cap <= 0) return 0;

    std::array<SectorLightAtmosphereVolume, MaximumVolumes> selected{};
    std::array<float, MaximumVolumes> distances{};
    distances.fill(std::numeric_limits<float>::max());
    for (const SectorLightAtmosphereSource& source : sources) {
        const SectorLightHazeSettings haze =
                NormalizeSectorLightHazeSettings(source.atmosphere.haze);
        if (!haze.enabled || haze.density <= 0.0f
                || !IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) {
            continue;
        }
        SectorLightAtmosphereVolume volume;
        if (!MakeSectorLightAtmosphereVolume(source, haze.extentScale, volume)
                || !IsSectorLightAtmosphereVolumeVisible(
                        volume, visibility, receiverBounds, camera,
                        aspect, nearPlane, farPlane)) {
            continue;
        }
        ++eligibleCount;
        const float distance = Vector3DistanceSqr(
                camera.position, volume.boundsCenterWorld);
        int insertAt = std::min(activeCount, cap - 1);
        const auto comesBefore = [&](int index) {
            const SectorLightAtmosphereSource* existing =
                    selected[static_cast<std::size_t>(index)].source;
            return distance < distances[static_cast<std::size_t>(index)]
                    || (distance == distances[static_cast<std::size_t>(index)]
                        && existing != nullptr
                        && (StableKind(source) < StableKind(*existing)
                            || (StableKind(source) == StableKind(*existing)
                                && source.lightId < existing->lightId)));
        };
        if (activeCount >= cap && !comesBefore(cap - 1)) continue;
        if (activeCount < cap) ++activeCount;
        while (insertAt > 0 && comesBefore(insertAt - 1)) {
            selected[static_cast<std::size_t>(insertAt)] =
                    selected[static_cast<std::size_t>(insertAt - 1)];
            distances[static_cast<std::size_t>(insertAt)] =
                    distances[static_cast<std::size_t>(insertAt - 1)];
            --insertAt;
        }
        selected[static_cast<std::size_t>(insertAt)] = volume;
        distances[static_cast<std::size_t>(insertAt)] = distance;
    }

    RefreshProbeIdentity(map, probes);
    for (int index = 0; index < activeCount; ++index) {
        const SectorLightAtmosphereVolume& volume =
                selected[static_cast<std::size_t>(index)];
        const SectorLightHazeSettings haze = NormalizeSectorLightHazeSettings(
                volume.source->atmosphere.haze);
        SectorVolumetricComparisonVolume& record =
                outVolumes[static_cast<std::size_t>(index)];
        record.shape = volume.source->shape == SectorLightAtmosphereShape::Cone
                ? SectorVolumetricComparisonShape::Cone
                : SectorVolumetricComparisonShape::Sphere;
        record.stableId = volume.source->lightId;
        record.centerWorld = volume.originWorld;
        record.directionWorld = volume.directionWorld;
        record.radiiWorld = Vector3{
                volume.extentWorld, volume.extentWorld, volume.extentWorld};
        record.extentWorld = volume.extentWorld;
        record.coneRadiusWorld = volume.coneRadiusWorld;
        record.tint = engine::SrgbColorBytesToLinearSceneRgb(
                haze.scatteringTint);
        record.density = haze.density;
        record.maximumOpacity = HazeMaximumOpacity;
        record.edgeSoftness = haze.edgeSoftness;
        record.noiseAmount = haze.noiseAmount;
        record.noiseScaleWorld = haze.noiseScaleWorld;
        record.flowDirectionRadians = haze.flowDirectionDegrees * DEG2RAD;
        record.flowSpeedWorld = haze.flowSpeedWorld;
        record.staticLightingSampleCount = 8;
        record.staticLighting = LightingFor(map, probes, volume).corners;
    }
    return activeCount;
}

void LegacyHazeComparisonAdapter::RefreshProbeIdentity(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes)
{
    const SectorBakedObjectLightProbe* data = probes.probes.empty()
            ? nullptr : probes.probes.data();
    const std::size_t hash = std::hash<std::string>{}(
            probes.metadata.sourceHash);
    const std::size_t mapHash = std::hash<std::string>{}(
            map.bakedLightmap.objectProbes.sourceHash);
    if (data == cachedProbeData && probes.probes.size() == cachedProbeCount
            && hash == cachedProbeHash && mapHash == cachedMapProbeHash) {
        return;
    }
    probeCache = {};
    cachedProbeData = data;
    cachedProbeCount = probes.probes.size();
    cachedProbeHash = hash;
    cachedMapProbeHash = mapHash;
}

const SectorLightHazeStaticLightingSamples&
LegacyHazeComparisonAdapter::LightingFor(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorLightAtmosphereVolume& volume)
{
    ProbeCacheEntry* available = nullptr;
    for (ProbeCacheEntry& entry : probeCache) {
        if (entry.valid && entry.kind == volume.source->kind
                && entry.lightId == volume.source->lightId) {
            if (entry.ownerSectorId == volume.source->ownerSectorId
                    && SameVector(entry.origin, volume.originWorld)
                    && SameVector(entry.direction, volume.directionWorld)
                    && entry.extent == volume.extentWorld
                    && entry.coneRadius == volume.coneRadiusWorld) {
                return entry.lighting;
            }
            available = &entry;
            break;
        }
        if (!entry.valid && available == nullptr) available = &entry;
    }
    if (available == nullptr) {
        available = &probeCache[static_cast<std::size_t>(
                std::max(volume.source->lightId, 0)) % probeCache.size()];
    }
    available->valid = true;
    available->kind = volume.source->kind;
    available->lightId = volume.source->lightId;
    available->ownerSectorId = volume.source->ownerSectorId;
    available->origin = volume.originWorld;
    available->direction = volume.directionWorld;
    available->extent = volume.extentWorld;
    available->coneRadius = volume.coneRadiusWorld;
    available->lighting = SampleSectorLightHazeStaticLighting(
            map, probes, volume);
    return available->lighting;
}

void LegacyHazeComparisonAdapter::Reset()
{
    probeCache = {};
    cachedProbeData = nullptr;
    cachedProbeCount = 0;
    cachedProbeHash = 0;
    cachedMapProbeHash = 0;
    eligibleCount = 0;
    activeCount = 0;
}

} // namespace game
