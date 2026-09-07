#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include "sector_demo/SectorReflectionProbes.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <raymath.h>
#include <limits>

namespace
{
void Check(bool result, const char *message)
{
    if (!result)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}
game::SectorPbrEnvironment::LocalProbe Probe(int id, int sector, float x)
{
    game::SectorPbrEnvironment::LocalProbe p;
    p.definition.sourceAuthoringProbeId = id;
    p.definition.topologySectorId = sector;
    p.definition.capturePositionWorld = {x, 1, 0};
    p.definition.influenceCenterWorld = {x, 1, 0};
    p.definition.halfExtentsWorld = {3, 2, 3};
    p.definition.enabled = true;
    p.ready = true;
    p.cubemap = {static_cast<unsigned int>(id), 1};
    p.inactive = {static_cast<unsigned int>(id + 100), 1};
    p.mipCount = 7;
    return p;
}

void TestReceiverSelection()
{
    game::SectorPbrEnvironment e;
    e.localProbes = {Probe(10, 1, -1), Probe(20, 2, 1)};
    auto select = [&](Vector3 position, int sector, const BoundingBox *bounds = nullptr)
    { return game::SelectSectorPbrEnvironmentBlend(e, position, sector, true, bounds); };
    Check(select({-0.1f, 1, 0}, 1).first.probeId == 10,
          "receiver uses its room, independent of camera or overlapping neighbor box");
    Check(engine::IsNull(select({-0.1f, 1, 0}, 1).second.cubemap),
          "overlapping boxes across a solid wall do not blend");
    Check(engine::IsNull(select({0, 1, 0}, 3).first.cubemap),
          "a room without a ready probe does not borrow through a wall");
    game::RuntimePortalEdge portal;
    portal.lineDefId = 7;
    portal.fromSectorId = 1;
    portal.toSectorId = 2;
    portal.a = {0, -1};
    portal.b = {0, 1};
    portal.openBottom = 0;
    portal.openTop = 2;
    portal.open = true;
    e.portals.push_back(portal);
    portal.fromSectorId = 2;
    portal.toSectorId = 1;
    e.portals.push_back(portal);
    auto left = select({-0.25f, 1, 0}, 1), right = select({0.25f, 1, 0}, 2);
    Check(left.portal && right.portal, "open doorway provides a two-probe pair on both sides");
    Check(Near(game::SectorReflectionBlendWeight(left, {-0.5f, 1, 0}), 0),
          "blend starts half a meter inside own room");
    Check(Near(game::SectorReflectionBlendWeight(left, {0, 1, 0}), 0.5f),
          "doorway center has equal weights");
    Check(Near(game::SectorReflectionBlendWeight(left, {0.5f, 1, 0}), 1),
          "blend finishes half a meter into neighbor");
    Check(Near(game::SectorReflectionBlendWeight(left, {0, 1, 0}),
               1 - game::SectorReflectionBlendWeight(right, {0, 1, 0})),
          "crossing sectors preserves the same mixture");
    Check(!select({-0.1f, 1, 2}, 1).portal, "no transition beside the doorway");
    BoundingBox room{{-3, 0, -3}, {0, 3, 3}};
    const auto large = select({-1.5f, 1.5f, 0}, 1, &room);
    Check(large.portal,
          "large receiver bounds retain a doorway candidate for per-fragment evaluation");
    Check(Near(game::SectorReflectionBlendWeight(large, {0, 1, 2}), 0),
          "fragment outside horizontal aperture keeps own room");
    Check(Near(game::SectorReflectionBlendWeight(large, {0, 2.1f, 0}), 0),
          "fragment above lintel keeps own room");
    game::RuntimePortalDynamicBlocker blocker;
    blocker.lineDefId = 7;
    blocker.blocksPortal = true;
    e.blockers.push_back(blocker);
    Check(!select({-0.1f, 1, 0}, 1).portal, "closed doors suppress neighbor blending");
    e.blockers.clear();
    e.localProbes[1].ready = false;
    Check(!select({-0.1f, 1, 0}, 1).portal, "unpublished cubemaps never enter a blend");
    e.localProbes[1] = Probe(20, 1, 1);
    e.portals.clear();
    const auto overlap = select({0, 1, 0}, 1);
    Check(!engine::IsNull(overlap.second.cubemap) && !overlap.portal,
          "same-sector overlapping boxes blend");
    Check(Near(game::SectorReflectionBlendWeight(overlap, {0, 1, 0}), 0.5f),
          "equal overlap weights are normalized");
    e.localProbes[0].hasPrevious = true;
    e.localProbes[0].publishedAt = 10;
    e.seconds = 10.05;
    const auto temporal = select({-1, 1, 0}, 1);
    Check(Near(temporal.first.transition, 0.5f) && !engine::IsNull(temporal.first.previous),
          "published probe crossfades from its previous complete cube for 100ms");
    Check(game::SectorReflectionProbeMipCount(64) == 7 &&
              game::SectorReflectionProbeMipCount(256) == 9,
          "all supported capture resolutions have complete roughness mip chains");
}

void TestScheduling()
{
    auto p = Probe(10, 1, 0);
    p.ready = false;
    p.required = true;
    p.dirty = false;
    game::MarkSectorReflectionProbeDirty(p, 1, false);
    const auto version = p.revision, discontinuity = p.discontinuity;
    p.lastStarted = 1;
    Check(!game::CanStartSectorReflectionProbe(p, 1.05, false),
          "continuous updates respect minimum start interval");
    Check(game::CanStartSectorReflectionProbe(p, 1.2, true),
          "required dirty probe can run during preparation");
    p.required = false;
    Check(!game::CanStartSectorReflectionProbe(p, 1.2, true),
          "distant probe is deferred until gameplay");
    game::MarkSectorReflectionProbeDirty(p, 1.3, false);
    Check(p.discontinuity == discontinuity && p.dirtySince == 1,
          "continuous edits coalesce without aborting or losing queue age");
    const auto original = p.cubemap;
    game::PublishSectorReflectionProbe(p, 1.4, version);
    Check(p.ready && !p.hasPrevious && p.dirty && p.inactive.index == original.index,
          "initial publish is atomic and preserves edits newer than its snapshot");
    game::PublishSectorReflectionProbe(p, 2, p.revision);
    Check(!p.dirty && p.hasPrevious, "second complete cube becomes a temporal replacement");
    game::MarkSectorReflectionProbeDirty(p, 2.01, true);
    Check(p.discontinuity != discontinuity, "discrete change invalidates obsolete active snapshot");
    Check(!game::CanStartSectorReflectionProbe(p, 2.05, false),
          "old cube cannot be overwritten during crossfade");
    p.failed = true;
    Check(!game::CanStartSectorReflectionProbe(p, 3, false),
          "failed GPU job does not retry every frame");
    game::MarkSectorReflectionProbeDirty(p, 3, true);
    Check(game::CanStartSectorReflectionProbe(p, 3, false),
          "explicit refresh or light change permits retry");
}

void TestCaptureRetries()
{
    using namespace game;
    SectorPbrEnvironment e;
    e.localProbes = {Probe(12, 1, 0)};
    auto& p = e.localProbes[0];
    const auto published = p.cubemap, inactive = p.inactive;
    p.required = true;
    SectorReflectionDemand demand;
    demand.requested = {1};
    const auto select = [&](bool preparing = false, int active = -1, bool paused = false) {
        return SelectSectorReflectionProbeUpdate(e, demand, preparing, {}, active, paused);
    };
    p.lastStarted = 10;
    FailSectorReflectionProbeCapture(p, 10);
    Check(!p.failed && p.captureFailures == 1 && p.retryAt == 10.25,
          "first failure queues a retry after 250ms");
    Check(p.ready && p.cubemap.index == published.index && p.inactive.index == inactive.index,
          "failure preserves the published cube and does not swap partial output");
    e.seconds = 10.249;
    Check(select() == -1 && select(true) == -1, "both gameplay and loading respect retry delay");
    e.seconds = 10.25;
    Check(select() == 0, "demanded retry starts at its deadline");
    Check(select(false, -1, true) == -1, "pause prevents starting an eligible retry");
    Check(select(false, 0, true) == 0, "pause preserves a demanded in-flight job");
    demand.requested[0] = 0;
    Check(select(false, 0) == -1 && select(false, 0, true) == -1,
          "lost demand cancels even a paused retry");
    Check(p.captureFailures == 1 && p.retryAt == 10.25 && p.cubemap.index == published.index,
          "demand cancellation preserves retry budget and published cube");
    Check(select(true) == 0, "required loading probe retries without main-view demand");
    demand.requested[0] = 1;
    Check(select() == 0 && p.captureFailures == 1, "demand re-entry keeps the same retry budget");
    p.lastStarted = e.seconds;
    FailSectorReflectionProbeCapture(p, 11);
    Check(!p.failed && p.captureFailures == 2 && p.retryAt == 12,
          "second failure queues the final retry after one second");
    MarkSectorReflectionProbeDirty(p, 11.1, false);
    Check(p.captureFailures == 2 && p.retryAt == 12,
          "continuous changes cannot erase backoff or replenish retries");
    e.seconds = 11.999;
    Check(select() == -1, "final retry cannot start early");
    e.seconds = 12;
    Check(select() == 0, "final retry becomes eligible after one second");
    p.ready = false;
    Check(!IsSectorReflectionProbePrepared(p), "preparation waits through recoverable failures");
    FailSectorReflectionProbeCapture(p, 13);
    e.seconds = 1000;
    Check(p.failed && p.captureFailures == SectorReflectionMaxCaptureAttempts
                  && select() == -1 && select(true) == -1,
          "three failed attempts are terminal until explicit invalidation");
    Check(IsSectorReflectionProbePrepared(p), "exhausted probe releases loading to fallback");
    MarkSectorReflectionProbeDirty(p, 1000, false);
    Check(p.failed && select() == -1, "continuous changes cannot revive an exhausted probe");
    const auto discontinuity = p.discontinuity;
    MarkSectorReflectionProbeDirty(p, 1000, true);
    Check(!p.failed && p.captureFailures == 0 && p.retryAt == 0 && select() == 0
                  && p.discontinuity == discontinuity + 1,
          "refresh or discrete door/light change resets retries and invalidates the old snapshot");
    FailSectorReflectionProbeCapture(p, 1001);
    const auto revision = p.revision;
    MarkSectorReflectionProbeDirty(p, 1001.1, false);
    PublishSectorReflectionProbe(p, 1002, revision);
    Check(p.ready && p.dirty && !p.failed && p.captureFailures == 0 && p.retryAt == 0
                  && p.cubemap.index == inactive.index,
          "successful retry resets budget and atomically publishes while preserving newer edits");
    PublishSectorReflectionProbe(p, 1003, p.revision);
    MarkSectorReflectionProbeDirty(p, 1003.01, true);
    e.seconds = 1003.05;
    Check(select() == -1, "retry reset still protects the previous cube during crossfade");
    p.resourceFailed = p.failed = true;
    MarkSectorReflectionProbeDirty(p, 1004, true);
    e.seconds = 1005;
    Check(p.failed && select() == -1 && IsSectorReflectionProbePrepared(p),
          "refresh cannot repair failed GPU initialization or block preparation on it");
}

void TestCaptureErrorAttribution()
{
    using namespace game;
    SectorReflectionCaptureFailure failure;
    const unsigned int errors[] = {0x0500, 0x0502, 0};
    std::size_t cursor = 0;
    const auto readError = [&] {
        Check(cursor < 3, "error drain must stop at GL_NO_ERROR");
        return errors[cursor++];
    };
    DrainSectorReflectionGlErrors(failure, SectorReflectionFailureStage::BeforeCapture, readError);
    Check(cursor == 3 && failure.inheritedGlError == 0x0500 && failure.inheritedGlErrorCount == 2
                  && failure.reason == SectorReflectionFailureReason::None && failure.glErrorCount == 0,
          "all inherited GL errors are drained and counted without failing capture");
    for (auto stage : {SectorReflectionFailureStage::Setup, SectorReflectionFailureStage::Shadows,
            SectorReflectionFailureStage::Scene, SectorReflectionFailureStage::Copy,
            SectorReflectionFailureStage::Filter, SectorReflectionFailureStage::Restore}) {
        failure = {};
        cursor = 0;
        DrainSectorReflectionGlErrors(failure, stage, readError);
        Check(cursor == 3 && failure.reason == SectorReflectionFailureReason::GlError
                      && failure.stage == stage && failure.glError == 0x0500 && failure.glErrorCount == 2,
              "capture-local GL errors are fully drained and attributed to the producing stage");
        RecordSectorReflectionGlError(failure, SectorReflectionFailureStage::Restore, 0x0505);
        Check(failure.stage == stage && failure.glError == 0x0500 && failure.glErrorCount == 3,
              "cleanup errors are counted without overwriting the original failure");
    }
    failure = {};
    failure.reason = SectorReflectionFailureReason::IncompleteFramebuffer;
    failure.stage = SectorReflectionFailureStage::Copy;
    failure.framebufferStatus = 0x8cd6;
    RecordSectorReflectionGlError(failure, SectorReflectionFailureStage::Copy, 0x0506);
    Check(failure.reason == SectorReflectionFailureReason::IncompleteFramebuffer
                  && failure.framebufferStatus == 0x8cd6 && failure.glError == 0x0506,
          "framebuffer failure keeps its status alongside GL diagnostics");
}

void TestLightChanges()
{
    game::SectorPreviewDynamicPointLightUniform a;
    a.intensity = 1;
    a.radius = 2;
    a.position = {0, 1, 0};
    auto b = a;
    b.selectionFadeMultiplier = 0.1f;
    Check(game::SectorReflectionLightsMatch(a, b), "camera-selection fades do not dirty captures");
    b.intensity = 0.8f;
    Check(!game::SectorReflectionLightsMatch(a, b) &&
              !game::SectorReflectionLightDiscontinuity(&a, &b),
          "dimming and sampled flicker queue continuous refreshes");
    b = a;
    b.position.x += 0.2f;
    Check(!game::SectorReflectionLightsMatch(a, b),
          "moving light invalidates old and new influence");
    b = a;
    b.color.x += 0.2f;
    Check(!game::SectorReflectionLightsMatch(a, b), "color edit refreshes reflected radiance");
    b.intensity = 0;
    Check(game::SectorReflectionLightDiscontinuity(&a, &b) &&
              game::SectorReflectionLightDiscontinuity(&a, nullptr) &&
              game::SectorReflectionLightDiscontinuity(nullptr, &a),
          "switches, removals and additions invalidate snapshots");
    const auto p = Probe(10, 1, 0);
    Check(game::SectorReflectionLightAffectsProbe(a, p.definition), "nearby light queues probe");
    a.position = {100, 100, 100};
    Check(!game::SectorReflectionLightAffectsProbe(a, p.definition),
          "distant changes leave probe clean");
}

void TestDemandScheduling()
{
    game::SectorPbrEnvironment e;
    e.localProbes = {Probe(3, 1, 0), Probe(9, 2, 5)};
    e.seconds = 1000;
    game::SectorReflectionDemand demand;
    demand.collecting.assign(2, 0);
    demand.requested.assign(2, 0);
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, false, {}, -1) == -1,
          "queue age never starts an unused dirty probe");
    game::SelectSectorPbrEnvironmentBlend(e, {0, 1, 0}, 1, true, nullptr, &demand);
    Check(demand.collecting[0] && !demand.collecting[1], "receiver requests only its probe");
    game::AdvanceSectorReflectionDemandFrame(demand);
    Check(demand.requested[0] && !demand.collecting[0], "demand crosses one frame boundary");
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, false, {}, -1) == 0,
          "demanded dirty probe starts");
    // Repeated invalidation does not interrupt a demanded continuous snapshot.
    game::MarkSectorReflectionProbeDirty(e.localProbes[0], e.seconds, false);
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, false, {}, 0) == 0,
          "demanded active probe continues");
    game::SelectSectorPbrEnvironmentBlend(e, {5, 1, 0}, 2, true, nullptr, &demand);
    game::AdvanceSectorReflectionDemandFrame(demand);
    const auto original = e.localProbes[0].cubemap;
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, false, {}, 0) == 1,
          "lost demand replaces incomplete job with newly demanded probe");
    Check(e.localProbes[0].dirty && e.localProbes[0].cubemap.index == original.index,
          "cancellation preserves dirty state and published cubemap");
    game::AdvanceSectorReflectionDemandFrame(demand);
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, false, {}, 1) == -1,
          "no receiver demand cancels the last active capture");
    e.localProbes[0].required = true;
    Check(game::SelectSectorReflectionProbeUpdate(e, demand, true, {}, -1) == 0,
          "loading required probes bypass normal receiver demand");
    e.localProbes[0].ready = false;
    e.localProbes[0].cubemap = engine::NullTextureHandle();
    const auto unready = game::SelectSectorPbrEnvironmentBlend(e, {0, 1, 0}, 1, true, nullptr, &demand);
    Check(demand.collecting[0] && !unready.first.localProbe,
          "unready probe is demanded without sampling unpublished textures");
    game::AdvanceSectorReflectionDemandFrame(demand);
    game::SelectSectorPbrEnvironmentBlend(e, {5, 1, 0}, 2);
    Check(!demand.collecting[1], "diagnostic selection without collector never requests work");

    e.localProbes = {Probe(3, 1, -1), Probe(9, 2, 1)};
    game::RuntimePortalEdge portal;
    portal.lineDefId = 7; portal.fromSectorId = 1; portal.toSectorId = 2;
    portal.a = {0, -1}; portal.b = {0, 1}; portal.openBottom = 0; portal.openTop = 2;
    portal.open = true;
    e.portals = {portal};
    e.localProbes[1].ready = false;
    game::SelectSectorPbrEnvironmentBlend(e, {-0.1f, 1, 0}, 1, true, nullptr, &demand);
    Check(demand.collecting[0] && demand.collecting[1],
          "doorway receivers request both blend sources including unready neighbor");
    game::AdvanceSectorReflectionDemandFrame(demand);
    game::RuntimePortalDynamicBlocker blocker;
    blocker.lineDefId = 7; blocker.blocksPortal = true;
    e.blockers = {blocker};
    game::SelectSectorPbrEnvironmentBlend(e, {-0.1f, 1, 0}, 1, true, nullptr, &demand);
    Check(demand.collecting[0] && !demand.collecting[1], "closed doorway stops neighbor demand");
    game::AdvanceSectorReflectionDemandFrame(demand);
    e.localProbes[1] = Probe(9, 1, 1);
    game::SelectSectorPbrEnvironmentBlend(e, {0, 1, 0}, 1, true, nullptr, &demand);
    Check(demand.collecting[0] && demand.collecting[1], "overlapping same-room blend requests both sources");
}

void TestSteadyReflectedFlicker()
{
    game::SectorPreviewDynamicPointLightUniform light;
    light.intensity = 3;
    light.flicker = true;
    light.selectionFadeEnabled = true;
    light.selectionFadeMultiplier = 0.2f;
    const auto capture = game::NormalizeSectorReflectionLight(light);
    Check(capture.intensity == 3 && !capture.flicker && !capture.selectionFadeEnabled
                  && capture.selectionFadeMultiplier == 1,
          "capture preserves base intensity and omits direct-light flicker and selection fade");
    light.flickerAmount = 0.99f;
    light.flickerSpeed = 8;
    Check(game::SectorReflectionLightsMatch(capture, game::NormalizeSectorReflectionLight(light)),
          "flicker settings do not dirty steady reflection snapshots");
    Check(light.flicker && light.selectionFadeMultiplier == 0.2f,
          "normalization does not alter the direct-light source");
    light.intensity = 2;
    Check(!game::SectorReflectionLightsMatch(capture, game::NormalizeSectorReflectionLight(light)),
          "scripted base-intensity edits still refresh reflections");
    light.intensity = 0;
    const auto off = game::NormalizeSectorReflectionLight(light);
    Check(game::SectorReflectionLightDiscontinuity(&capture, &off), "off is never normalized to on");
}

void TestFaceBoundsCulling()
{
    for (int face = 0; face < 6; ++face) {
        const auto camera = game::SectorReflectionFaceCamera({5, 2, -7}, face);
        const Vector3 forward = Vector3Subtract(camera.target, camera.position);
        const Vector3 center = Vector3Add(camera.position, Vector3Scale(forward, 4));
        const Vector3 half{0.25f, 0.25f, 0.25f};
        const BoundingBox visible{Vector3Subtract(center, half), Vector3Add(center, half)};
        Check(game::SectorReflectionBoundsInView(camera, 1, 0.01f, 100, visible),
              "each cube face includes geometry in front of its own camera");
        const Vector3 behind = Vector3Subtract(camera.position, Vector3Scale(forward, 4));
        Check(!game::SectorReflectionBoundsInView(camera, 1, 0.01f, 100,
                {Vector3Subtract(behind, half), Vector3Add(behind, half)}),
              "each cube face rejects geometry behind its own camera");
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
        const Vector3 edge = Vector3Add(center, Vector3Scale(right, 4.1f));
        Check(game::SectorReflectionBoundsInView(camera, 1, 0.01f, 100,
                {Vector3Subtract(edge, half), Vector3Add(edge, half)}),
              "bounds crossing a face edge remain visible even if their center is outside");
        Check(game::SectorReflectionBoundsInView(camera, 1, 0.01f, 100,
                {Vector3Subtract(camera.position, half), Vector3Add(camera.position, half)}),
              "bounds crossing the near plane remain visible");
        game::SectorReflectionCaptureCulling culling{camera, 0.01f, 100};
        Check(game::AcceptSectorReflectionObject(&culling, visible), "visible object is submitted");
        Check(!game::AcceptSectorReflectionObject(&culling,
                {Vector3Subtract(behind, half), Vector3Add(behind, half)}), "hidden object is culled");
        Check(game::AcceptSectorReflectionObject(&culling, {}, false), "missing bounds keep object visible");
        Check(culling.objectsDrawn == 2 && culling.objectsCulled == 1, "object diagnostics count submissions");
    }
    const auto camera = game::SectorReflectionFaceCamera({}, 0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Check(game::SectorReflectionBoundsInView(camera, 1, 0.01f, 100, {{nan, 0, 0}, {1, 1, 1}}),
          "invalid bounds fall back conservatively");
    game::SectorPbrEnvironment e;
    e.localProbes = {Probe(1, 1, -5)};
    game::SectorReflectionDemand demand;
    demand.collecting.assign(1, 0); demand.requested.assign(1, 0); demand.camera = camera;
    const BoundingBox behind{{-6, 0, -1}, {-4, 2, 1}};
    game::SelectSectorPbrEnvironmentBlend(e, {-5, 1, 0}, 1, true, &behind, &demand);
    Check(!demand.collecting[0], "off-frustum receiver does not demand a probe");
}
} // namespace

void TestFaceOrientation()
{
    const Vector3 position{5, 2, -7};
    const Vector3 expected[] = {{1, -0.3f, -0.2f}, {-1, -0.3f, 0.2f}, {0.2f, 1, 0.3f},
                                {0.2f, -1, -0.3f}, {0.2f, -0.3f, 1},  {-0.2f, -0.3f, -1}};
    for (int face = 0; face < 6; ++face)
    {
        const auto camera = game::SectorReflectionFaceCamera(position, face);
        const auto forward = Vector3Subtract(camera.target, camera.position);
        const auto right = Vector3CrossProduct(forward, camera.up);
        // Copy pass mirrors both axes to OpenGL cubemap face coordinates.
        const auto ray = Vector3Add(
            forward, Vector3Add(Vector3Scale(right, -0.2f), Vector3Scale(camera.up, -0.3f)));
        Check(Vector3Equals(ray, expected[face]),
              "capture camera and copy pass agree with OpenGL cube face orientation");
    }
}

int main()
{
    TestReceiverSelection();
    TestScheduling();
    TestCaptureRetries();
    TestCaptureErrorAttribution();
    TestLightChanges();
    TestFaceOrientation();
    TestDemandScheduling();
    TestSteadyReflectedFlicker();
    TestFaceBoundsCulling();
    std::cout << "Runtime reflection policy tests passed\n";
}
