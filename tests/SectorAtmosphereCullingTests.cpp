#include "sector_demo/renderer/SectorAtmosphereCulling.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace {

Camera3D TestCamera()
{
    Camera3D camera{};
    camera.position = Vector3{0.0f, 0.0f, 0.0f};
    camera.target = Vector3{0.0f, 0.0f, 1.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

void TestProjectedScissors()
{
    const Camera3D camera = TestCamera();
    const game::SectorAtmosphereScissorRect visible =
            game::ProjectSectorAtmosphereBoundsToScissor(
                    camera,
                    1.0f,
                    0.1f,
                    Vector3{-1.0f, -1.0f, 4.0f},
                    Vector3{1.0f, 1.0f, 6.0f},
                    100,
                    100);
    assert(!visible.Empty());
    assert(visible.width < 100 && visible.height < 100);
    assert(game::SectorAtmosphereScissorCoverage(visible, 100, 100) > 0.0f);
    assert(game::SectorAtmosphereScissorCoverage(visible, 100, 100) < 1.0f);

    const game::SectorAtmosphereScissorRect crossingEdge =
            game::ProjectSectorAtmosphereBoundsToScissor(
                    camera,
                    1.0f,
                    0.1f,
                    Vector3{3.9f, -1.0f, 4.0f},
                    Vector3{5.0f, 1.0f, 6.0f},
                    100,
                    100);
    assert(!crossingEdge.Empty());
    assert(crossingEdge.x == 0
            || crossingEdge.x + crossingEdge.width == 100);

    const game::SectorAtmosphereScissorRect offscreen =
            game::ProjectSectorAtmosphereBoundsToScissor(
                    camera,
                    1.0f,
                    0.1f,
                    Vector3{100.0f, -1.0f, 4.0f},
                    Vector3{102.0f, 1.0f, 6.0f},
                    100,
                    100);
    assert(offscreen.Empty());

    const game::SectorAtmosphereScissorRect nearPlane =
            game::ProjectSectorAtmosphereBoundsToScissor(
                    camera,
                    1.0f,
                    0.1f,
                    Vector3{-1.0f, -1.0f, 0.05f},
                    Vector3{1.0f, 1.0f, 1.0f},
                    100,
                    100);
    assert(nearPlane.x == 0 && nearPlane.y == 0);
    assert(nearPlane.width == 100 && nearPlane.height == 100);

    const game::SectorAtmosphereScissorRect invalidProjection =
            game::ProjectSectorAtmosphereBoundsToScissor(
                    camera,
                    0.0f,
                    0.1f,
                    Vector3{-1.0f, -1.0f, 4.0f},
                    Vector3{1.0f, 1.0f, 6.0f},
                    100,
                    100);
    assert(invalidProjection.width == 100
            && invalidProjection.height == 100);

    const game::SectorAtmosphereScissorRect combined =
            game::UnionSectorAtmosphereScissors(
                    visible,
                    game::SectorAtmosphereScissorRect{0, 0, 5, 5},
                    100,
                    100);
    assert(combined.x == 0 && combined.y == 0);
    assert(combined.width >= visible.x + visible.width);
    assert(combined.height >= visible.y + visible.height);
}

void TestYawedBounds()
{
    const Vector3 axisAligned = game::ComputeSectorAtmosphereYawedHalfExtents(
            Vector3{2.0f, 0.5f, 1.0f}, 0.0f);
    assert(std::fabs(axisAligned.x - 2.0f) < 0.0001f);
    assert(std::fabs(axisAligned.y - 0.5f) < 0.0001f);
    assert(std::fabs(axisAligned.z - 1.0f) < 0.0001f);

    const Vector3 quarterTurn = game::ComputeSectorAtmosphereYawedHalfExtents(
            Vector3{2.0f, 0.5f, 1.0f}, PI * 0.5f);
    assert(std::fabs(quarterTurn.x - 1.0f) < 0.0001f);
    assert(std::fabs(quarterTurn.y - 0.5f) < 0.0001f);
    assert(std::fabs(quarterTurn.z - 2.0f) < 0.0001f);

    const Vector3 diagonal = game::ComputeSectorAtmosphereYawedHalfExtents(
            Vector3{2.0f, 0.5f, 1.0f}, PI * 0.25f);
    const float expected = 3.0f / std::sqrt(2.0f);
    assert(std::fabs(diagonal.x - expected) < 0.0001f);
    assert(std::fabs(diagonal.z - expected) < 0.0001f);
}

void TestAnalyticFogEdgeBounds()
{
    const Vector3 radii{2.0f, 0.5f, 1.0f};
    const float cloudyWidth = game::ComputeSectorAnalyticFogEdgeWidth(
            radii, 1.0f, false);
    const float roomWidth = game::ComputeSectorAnalyticFogEdgeWidth(
            radii, 1.0f, true);
    assert(std::fabs(cloudyWidth - 0.225f) < 0.0001f);
    assert(std::fabs(roomWidth - 0.1f) < 0.0001f);

    const float noNoiseExpansion =
            game::ComputeSectorAnalyticFogCloudyEdgeExpansion(
                    radii, 1.0f, 0.0f);
    const float fullNoiseExpansion =
            game::ComputeSectorAnalyticFogCloudyEdgeExpansion(
                    radii, 1.0f, 1.0f);
    const float quarterNoiseExpansion =
            game::ComputeSectorAnalyticFogCloudyEdgeExpansion(
                    radii, 1.0f, 0.25f);
    assert(std::fabs(noNoiseExpansion) < 0.0001f);
    assert(std::fabs(fullNoiseExpansion - cloudyWidth * 0.60f) < 0.0001f);
    assert(std::fabs(quarterNoiseExpansion - fullNoiseExpansion * 0.5f)
            < 0.0001f);

    const Vector3 expanded{
            radii.x + fullNoiseExpansion,
            radii.y + fullNoiseExpansion,
            radii.z + fullNoiseExpansion};
    const Vector3 yawed = game::ComputeSectorAtmosphereYawedHalfExtents(
            expanded, PI * 0.25f);
    const Vector3 unexpandedYawed = game::ComputeSectorAtmosphereYawedHalfExtents(
            radii, PI * 0.25f);
    assert(yawed.x > unexpandedYawed.x);
    assert(yawed.y > unexpandedYawed.y);
    assert(yawed.z > unexpandedYawed.z);
}

void TestFogVolumeCullingAcrossSectors()
{
    game::SectorCompiledLocalFogVolume volume;
    volume.enabled = true;
    volume.maxOpacity = 0.8f;
    volume.shape = game::SectorLocalFogShape::Box;
    volume.analyticStyle = game::SectorAnalyticFogStyle::Room;
    volume.topologySectorId = 1;
    volume.centerWorld = {0, 0, 10};
    volume.radiiWorld = {12, 3, 12};
    game::RuntimePortalVisibilityResult visibility;
    visibility.validStartSector = true;
    visibility.visibleSectorIds = {2, 3};
    assert(!game::ShouldDrawRuntimeSectorForVisibility(volume.topologySectorId, visibility));
    // Different visible rooms inside one volume must still receive fog even
    // though the center's sector is absent from portal visibility.
    for (float x : {-6.0f, 6.0f}) {
        Camera3D camera = TestCamera();
        camera.position = {x, 0, 8};
        camera.target = {x, 0, 9};
        const auto scissor = game::ComputeSectorAnalyticFogVolumeScissor(
                volume, camera, 1, 0.1f, 100, 100);
        assert(scissor.width == 100 && scissor.height == 100);
    }
    const auto project = [&volume]() {
        return game::ComputeSectorAnalyticFogVolumeScissor(
                volume, TestCamera(), 1, 0.1f, 100, 100);
    };
    volume.enabled = false;
    assert(project().Empty());
    volume.enabled = true;
    volume.maxOpacity = 0;
    assert(project().Empty());
    volume.maxOpacity = 1;
    volume.centerWorld = {100, 0, 10};
    volume.radiiWorld = {1, 1, 1};
    assert(project().Empty());
    volume.centerWorld = {0, 0, -10};
    assert(project().Empty());
    volume.centerWorld = {0, 0, 10};
    volume.radiiWorld = {4, 1, 1};
    const auto horizontal = project();
    volume.yawRadians = PI * 0.5f;
    assert(project().width < horizontal.width);
    volume.yawRadians = PI * 0.25f;
    assert(!project().Empty());
    volume.centerWorld = {0, 0, 1};
    assert(project().width == 100); // Conservative near-plane fallback.
    volume.centerWorld = {0, 0, 10};
    volume.radiiWorld = {2, 2, 2};
    volume.yawRadians = 0;
    const auto room = project();
    volume.analyticStyle = game::SectorAnalyticFogStyle::Cloudy;
    volume.edgeSoftness = 1;
    volume.noiseAmount = 1;
    const auto cloudy = project();
    assert(cloudy.width > room.width && cloudy.height > room.height);
    volume.shape = game::SectorLocalFogShape::Ellipsoid;
    assert(!project().Empty());
}

void TestDynamicLightMasks()
{
    game::SectorBillboardDynamicLightContext lights;
    lights.dynamicLightCount = 4;
    lights.dynamicLightPositions[0] = Vector3{0.0f, 0.0f, 0.0f};
    lights.dynamicLightRadii[0] = 1.0f;
    lights.dynamicLightPositions[1] = Vector3{10.0f, 0.0f, 0.0f};
    lights.dynamicLightRadii[1] = 1.0f;
    lights.dynamicLightPositions[2] = Vector3{2.0f, 0.0f, 0.0f};
    lights.dynamicLightRadii[2] = 1.0f;
    lights.dynamicLightPositions[3] = Vector3{0.0f, 0.0f, 0.0f};
    lights.dynamicLightRadii[3] = 0.0f;

    const Vector3 minimum{-1.0f, -1.0f, -1.0f};
    const Vector3 maximum{1.0f, 1.0f, 1.0f};
    const std::uint32_t mask = game::BuildSectorAtmosphereDynamicLightMask(
            lights, minimum, maximum);
    assert((mask & (1u << 0u)) != 0u);
    assert((mask & (1u << 1u)) == 0u);
    assert((mask & (1u << 2u)) != 0u);
    assert((mask & (1u << 3u)) == 0u);

    assert(!game::SectorAtmosphereDynamicLightIntersectsBounds(
            lights,
            0,
            Vector3{-1.0f, 2.0f, -1.0f},
            Vector3{1.0f, 3.0f, 1.0f}));
    assert(game::SectorAtmosphereDynamicLightIntersectsBounds(
            lights,
            0,
            Vector3{-1.0f, 0.25f, -1.0f},
            Vector3{1.0f, 1.25f, 1.0f}));

    const float nan = std::nanf("");
    const std::uint32_t invalidBoundsMask =
            game::BuildSectorAtmosphereDynamicLightMask(
                    lights,
                    Vector3{nan, 0.0f, 0.0f},
                    maximum);
    assert(invalidBoundsMask == 0x0fu);
}

} // namespace

int main()
{
    TestProjectedScissors();
    TestYawedBounds();
    TestAnalyticFogEdgeBounds();
    TestDynamicLightMasks();
    TestFogVolumeCullingAcrossSectors();
    return 0;
}
