#include "sector_demo/renderer/SectorAtmosphereCulling.h"

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
    TestDynamicLightMasks();
    return 0;
}
