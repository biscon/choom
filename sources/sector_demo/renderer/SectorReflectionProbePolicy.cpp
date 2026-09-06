#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include <raymath.h>
#include <utility>
#include <algorithm>
#include <cmath>

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
                                   bool preparing, bool demanded)
{
    return probe.definition.enabled && probe.dirty && !probe.failed
           && (preparing ? probe.required : demanded) &&
           seconds - probe.lastStarted >= SectorReflectionUpdateInterval &&
           (!probe.hasPrevious || seconds - probe.publishedAt >= SectorReflectionTransitionSeconds);
}

void AdvanceSectorReflectionDemandFrame(SectorReflectionDemand& demand)
{
    demand.requested.swap(demand.collecting);
    std::fill(demand.collecting.begin(), demand.collecting.end(), 0);
}

bool IsSectorReflectionProbeDemanded(const SectorPbrEnvironment& environment,
        const SectorReflectionDemand& demand, std::size_t index, bool preparing)
{
    return index < environment.localProbes.size()
            && environment.localProbes[index].definition.enabled
            && (preparing ? environment.localProbes[index].required
                : index < demand.requested.size() && demand.requested[index] != 0);
}

int SelectSectorReflectionProbeUpdate(const SectorPbrEnvironment& environment,
        const SectorReflectionDemand& demand, bool preparing, Vector3 viewerPosition,
        int activeProbeIndex)
{
    if (activeProbeIndex >= 0 && IsSectorReflectionProbeDemanded(
            environment, demand, static_cast<std::size_t>(activeProbeIndex), preparing))
        return activeProbeIndex;
    int best = -1;
    double bestScore = -1e30;
    for (std::size_t i = 0; i < environment.localProbes.size(); ++i) {
        const auto& probe = environment.localProbes[i];
        if (!CanStartSectorReflectionProbe(probe, environment.seconds, preparing,
                IsSectorReflectionProbeDemanded(environment, demand, i, preparing))) continue;
        const double score = (!probe.ready ? 100 : 0)
                + (environment.seconds - probe.dirtySince) * 10
                - Vector3Distance(viewerPosition, probe.definition.capturePositionWorld) * 0.01;
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool AcceptSectorReflectionObject(SectorReflectionCaptureCulling* culling,
        BoundingBox bounds, bool validBounds)
{
    if (!culling) return true;
    if (validBounds && !SectorReflectionBoundsInView(culling->camera, 1.0f,
            culling->nearPlane, culling->farPlane, bounds)) {
        ++culling->objectsCulled;
        return false;
    }
    ++culling->objectsDrawn;
    return true;
}

SectorReflectionDemand* SectorReflectionDemandForBounds(SectorReflectionDemand* demand,
        BoundingBox bounds)
{
    return demand && SectorReflectionBoundsInView(demand->camera, demand->aspect,
            demand->nearPlane, demand->farPlane, bounds) ? demand : nullptr;
}

SectorPreviewDynamicPointLightUniform NormalizeSectorReflectionLight(
        SectorPreviewDynamicPointLightUniform light)
{
    // Reflections represent the base light, not its sampled flicker envelope.
    light.flicker = false;
    light.selectionFadeEnabled = false;
    light.selectionFadeMultiplier = 1.0f;
    return light;
}

bool SectorReflectionBoundsInView(const Camera3D& camera, float aspect, float nearPlane,
        float farPlane, BoundingBox bounds)
{
    const auto finite = [](Vector3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite(bounds.min) || !finite(bounds.max)
            || bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y
            || bounds.min.z > bounds.max.z || !finite(camera.position)
            || !finite(camera.target) || !finite(camera.up)
            || !std::isfinite(aspect) || aspect <= 0 || !std::isfinite(camera.fovy)
            || camera.fovy <= 0 || camera.fovy >= 180) return true;
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3LengthSqr(forward) < 0.000001f) return true;
    forward = Vector3Normalize(forward);
    Vector3 right = Vector3CrossProduct(forward, camera.up);
    if (Vector3LengthSqr(right) < 0.000001f) return true;
    right = Vector3Normalize(right);
    const Vector3 up = Vector3CrossProduct(right, forward);
    const float vertical = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const float horizontal = vertical * aspect;
    const Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
    const Vector3 half = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), 0.5f);
    const Vector3 relative = Vector3Subtract(center, camera.position);
    const auto outside = [&](Vector3 normal, float offset) {
        const float radius = std::fabs(normal.x) * half.x
                + std::fabs(normal.y) * half.y + std::fabs(normal.z) * half.z;
        return Vector3DotProduct(normal, relative) + radius < offset - 0.0001f;
    };
    return !outside(forward, nearPlane) && !outside(Vector3Negate(forward), -farPlane)
            && !outside(Vector3Add(Vector3Scale(forward, horizontal), right), 0)
            && !outside(Vector3Subtract(Vector3Scale(forward, horizontal), right), 0)
            && !outside(Vector3Add(Vector3Scale(forward, vertical), up), 0)
            && !outside(Vector3Subtract(Vector3Scale(forward, vertical), up), 0);
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
