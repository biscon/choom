#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include <raymath.h>
#include <utility>

namespace game
{

Camera3D SectorReflectionFaceCamera(Vector3 position, int face)
{
    const Vector3 directions[] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                  {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    const Vector3 ups[] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    if (face < 0 || face >= 6)
        face = 0;
    return {position, Vector3Add(position, directions[face]), ups[face], 90, CAMERA_PERSPECTIVE};
}

void MarkSectorReflectionProbeDirty(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                    bool discontinuity)
{
    if (!probe.dirty)
        probe.dirtySince = seconds;
    probe.dirty = true;
    probe.failed = false;
    ++probe.revision;
    if (discontinuity)
        ++probe.discontinuity;
}

bool CanStartSectorReflectionProbe(const SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                   bool preparing)
{
    return probe.dirty && !probe.failed && (!preparing || probe.required) &&
           seconds - probe.lastStarted >= SectorReflectionUpdateInterval &&
           (!probe.hasPrevious || seconds - probe.publishedAt >= SectorReflectionTransitionSeconds);
}

void PublishSectorReflectionProbe(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                  std::uint64_t capturedRevision)
{
    probe.hasPrevious = probe.ready;
    probe.ready = true;
    probe.failed = false;
    probe.publishedAt = seconds;
    std::swap(probe.cubemap, probe.inactive);
    probe.dirty = probe.revision != capturedRevision;
    if (probe.dirty)
        probe.dirtySince = seconds; // A completed job goes to the back of the aging queue.
}

bool SectorReflectionLightDiscontinuity(const SectorPreviewDynamicPointLightUniform *old,
                                        const SectorPreviewDynamicPointLightUniform *current)
{
    return !old || !current || ((old->intensity > 0) != (current->intensity > 0));
}

bool SectorReflectionLightsMatch(const SectorPreviewDynamicPointLightUniform &a,
                                 const SectorPreviewDynamicPointLightUniform &b)
{
    // Compare values, never padding or camera selection fades.
    return a.kind == b.kind && a.lightId == b.lightId && Vector3Equals(a.position, b.position) &&
           Vector3Equals(a.direction, b.direction) && Vector3Equals(a.rectRight, b.rectRight) &&
           Vector3Equals(a.color, b.color) && a.radius == b.radius && a.intensity == b.intensity &&
           a.innerConeCos == b.innerConeCos && a.outerConeCos == b.outerConeCos &&
           a.castsShadow == b.castsShadow && a.shadowBias == b.shadowBias &&
           a.shadowPriority == b.shadowPriority && a.shadowStrength == b.shadowStrength &&
           a.shadowSoftness == b.shadowSoftness && a.profile == b.profile &&
           Vector3Equals(a.profileParameters, b.profileParameters);
}

bool SectorReflectionLightAffectsProbe(const SectorPreviewDynamicPointLightUniform &light,
                                       const SectorCompiledReflectionProbe &probe)
{
    return Vector3Distance(light.position, probe.influenceCenterWorld) <=
           light.radius + Vector3Length(probe.halfExtentsWorld);
}

} // namespace game
