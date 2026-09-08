#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include "sector_demo/renderer/SectorOpaqueDrawPolicy.h"
#include <raymath.h>
#include <utility>
#include <algorithm>
#include <cmath>

namespace game
{

const char* SectorReflectionFailureStageName(SectorReflectionFailureStage stage)
{
    switch (stage) {
    case SectorReflectionFailureStage::BeforeCapture: return "before capture";
    case SectorReflectionFailureStage::Setup: return "setup";
    case SectorReflectionFailureStage::Shadows: return "shadows";
    case SectorReflectionFailureStage::Scene: return "scene";
    case SectorReflectionFailureStage::Copy: return "copy";
    case SectorReflectionFailureStage::Filter: return "filter";
    case SectorReflectionFailureStage::Restore: return "restore";
    }
    return "unknown";
}

const char* SectorReflectionFailureReasonName(SectorReflectionFailureReason reason)
{
    switch (reason) {
    case SectorReflectionFailureReason::None: return "none";
    case SectorReflectionFailureReason::GlError: return "OpenGL error";
    case SectorReflectionFailureReason::MissingCubemap: return "missing cubemap";
    case SectorReflectionFailureReason::IncompleteFramebuffer: return "incomplete framebuffer";
    case SectorReflectionFailureReason::ShadowTimeout: return "shadow preparation timeout";
    }
    return "unknown";
}

void RecordSectorReflectionGlError(SectorReflectionCaptureFailure& failure,
        SectorReflectionFailureStage stage, unsigned int error)
{
    if (!error) return;
    if (stage == SectorReflectionFailureStage::BeforeCapture) {
        if (!failure.inheritedGlError) failure.inheritedGlError = error;
        ++failure.inheritedGlErrorCount;
        return;
    }
    if (failure.reason == SectorReflectionFailureReason::None) {
        failure.reason = SectorReflectionFailureReason::GlError;
        failure.stage = stage;
    }
    if (!failure.glError) failure.glError = error;
    ++failure.glErrorCount;
}

void FailSectorReflectionProbeCapture(SectorPbrEnvironment::LocalProbe& probe, double seconds)
{
    probe.captureFailures = std::min(probe.captureFailures + 1, SectorReflectionMaxCaptureAttempts);
    probe.failed = probe.resourceFailed || probe.captureFailures >= SectorReflectionMaxCaptureAttempts;
    probe.retryAt = seconds + (probe.captureFailures == 1 ? 0.25 : 1.0);
    probe.dirty = true;
}

bool IsSectorReflectionProbePrepared(const SectorPbrEnvironment::LocalProbe& probe)
{
    return !probe.required || probe.ready || probe.failed;
}

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
    ++probe.revision;
    if (discontinuity) {
        probe.failed = probe.resourceFailed;
        probe.captureFailures = 0;
        probe.retryAt = 0;
        ++probe.discontinuity;
    }
}

bool CanStartSectorReflectionProbe(const SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                   bool preparing, bool demanded)
{
    return probe.definition.enabled && probe.dirty && !probe.failed
           && !probe.resourceFailed && seconds >= probe.retryAt
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
        int activeProbeIndex, bool paused)
{
    if (activeProbeIndex >= 0 && IsSectorReflectionProbeDemanded(
            environment, demand, static_cast<std::size_t>(activeProbeIndex), preparing))
        return activeProbeIndex;
    if (paused) return -1;
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
    return SectorBoundsInView(camera, aspect, nearPlane, farPlane, bounds);
}
void PublishSectorReflectionProbe(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                  std::uint64_t capturedRevision)
{
    probe.hasPrevious = probe.ready;
    probe.ready = true;
    probe.failed = false;
    probe.captureFailures = 0;
    probe.retryAt = 0;
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
