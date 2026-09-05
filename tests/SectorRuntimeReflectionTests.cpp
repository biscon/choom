#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include "sector_demo/SectorReflectionProbes.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <raymath.h>

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
    TestLightChanges();
    TestFaceOrientation();
    std::cout << "Runtime reflection policy tests passed\n";
}
