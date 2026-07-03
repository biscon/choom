#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorColor.h"
#include "sector_demo/SectorMath.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

bool Near(Vector2 actual, Vector2 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon) && Near(actual.y, expected.y, epsilon);
}

bool Near(Vector3 actual, Vector3 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon)
            && Near(actual.z, expected.z, epsilon);
}

void TestFiniteHelpers()
{
    Check(game::IsFiniteFloat(1.0f), "finite float accepted");
    Check(!game::IsFiniteFloat(std::numeric_limits<float>::infinity()), "infinite float rejected");
    Check(!game::IsFiniteVector2(Vector2{0.0f, std::numeric_limits<float>::quiet_NaN()}), "nan vector2 rejected");
    Check(!game::IsFiniteVector3(Vector3{0.0f, 1.0f, std::numeric_limits<float>::infinity()}), "infinite vector3 rejected");
}

void TestClampFinite()
{
    Check(Near(game::ClampFinite(-2.0f, 0.0f, 1.0f, 0.5f), 0.0f), "ClampFinite clamps below range");
    Check(Near(game::ClampFinite(2.0f, 0.0f, 1.0f, 0.5f), 1.0f), "ClampFinite clamps above range");
    Check(Near(game::ClampFinite(std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f, 0.5f), 0.5f),
            "ClampFinite uses fallback for nan");
}

void TestNormalizeHelpers()
{
    Check(Near(game::NormalizeVector2OrFallback(Vector2{}, Vector2{1.0f, 0.0f}), Vector2{1.0f, 0.0f}),
            "NormalizeVector2OrFallback returns fallback for zero");
    Check(Near(game::NormalizeVector2OrFallback(Vector2{3.0f, 4.0f}, Vector2{}), Vector2{0.6f, 0.8f}),
            "NormalizeVector2OrFallback normalizes valid vectors");
    Check(Near(
                  game::NormalizeVector3OrFallback(
                          Vector3{std::numeric_limits<float>::infinity(), 0.0f, 0.0f},
                          Vector3{0.0f, 1.0f, 0.0f}),
                  Vector3{0.0f, 1.0f, 0.0f}),
            "NormalizeVector3OrFallback returns fallback for non-finite vectors");
    Check(Near(game::NormalizeVector3OrFallback(Vector3{0.0f, 0.0f, 2.0f}, Vector3{}), Vector3{0.0f, 0.0f, 1.0f}),
            "NormalizeVector3OrFallback normalizes valid vectors");
}

void TestStepHelpers()
{
    Check(Near(game::SmoothStep01(-1.0f), 0.0f), "SmoothStep01 clamps below range");
    Check(Near(game::SmoothStep01(0.0f), 0.0f), "SmoothStep01 starts at zero");
    Check(Near(game::SmoothStep01(1.0f), 1.0f), "SmoothStep01 ends at one");
    Check(Near(game::SmoothStep01(2.0f), 1.0f), "SmoothStep01 clamps above range");
    Check(Near(game::SmootherStep01(0.0f), 0.0f), "SmootherStep01 starts at zero");
    Check(Near(game::SmootherStep01(1.0f), 1.0f), "SmootherStep01 ends at one");
}

void TestBoundsHelpers()
{
    const game::SectorAabb3 empty = game::EmptySectorAabb3();
    Check(!game::IsValidSectorAabb3(empty), "empty sector aabb is invalid");
    Check(game::IsFiniteSectorAabb3(empty), "empty sector aabb sentinel is finite");

    game::SectorAabb3 bounds = game::SectorAabb3FromPoint(Vector3{1.0f, 2.0f, 3.0f});
    Check(game::IsValidSectorAabb3(bounds), "point sector aabb is valid");
    Check(Near(bounds.min, Vector3{1.0f, 2.0f, 3.0f}), "point sector aabb min equals point");
    Check(Near(bounds.max, Vector3{1.0f, 2.0f, 3.0f}), "point sector aabb max equals point");

    game::ExpandSectorAabb3(bounds, Vector3{-1.0f, 5.0f, 0.5f});
    Check(Near(bounds.min, Vector3{-1.0f, 2.0f, 0.5f}), "expanded sector aabb min is accumulated");
    Check(Near(bounds.max, Vector3{1.0f, 5.0f, 3.0f}), "expanded sector aabb max is accumulated");
    Check(Near(game::SectorAabb3Center(bounds), Vector3{0.0f, 3.5f, 1.75f}), "sector aabb center is midpoint");
    Check(Near(game::SectorAabb3Extents(bounds), Vector3{2.0f, 3.0f, 2.5f}), "sector aabb extents are max minus min");

    Check(Near(
                  game::ClosestPointOnSectorAabb3(bounds, Vector3{0.25f, 3.0f, 1.0f}),
                  Vector3{0.25f, 3.0f, 1.0f}),
            "closest sector aabb point preserves inside point");
    Check(Near(
                  game::ClosestPointOnSectorAabb3(bounds, Vector3{-10.0f, 10.0f, 2.0f}),
                  Vector3{-1.0f, 5.0f, 2.0f}),
            "closest sector aabb point clamps outside point");

    game::ExpandSectorAabb3(bounds, Vector3{0.0f, std::numeric_limits<float>::infinity(), 0.0f});
    Check(!game::IsFiniteSectorAabb3(bounds), "sector aabb with infinite expansion is not finite");
    Check(!game::IsValidSectorAabb3(bounds), "sector aabb with infinite expansion is invalid");
}

void TestColorHelpers()
{
    Check(Near(game::ColorToUnitRgb(Color{0, 128, 255, 42}), Vector3{0.0f, 128.0f / 255.0f, 1.0f}),
            "ColorToUnitRgb converts byte channels");
    Check(game::ClampColorByte(-2.0f) == 0, "ClampColorByte clamps negative values");
    Check(game::ClampColorByte(300.0f) == 255, "ClampColorByte clamps high values");
    Check(game::ClampColorByte(127.6f) == 128, "ClampColorByte rounds before clamping");
    const Color color = game::UnitRgbToColor(Vector3{0.0f, 0.5f, 1.0f}, 123);
    Check(color.r == 0 && color.g == 128 && color.b == 255 && color.a == 123, "UnitRgbToColor converts unit rgb with alpha");
}

} // namespace

int main()
{
    TestFiniteHelpers();
    TestClampFinite();
    TestNormalizeHelpers();
    TestStepHelpers();
    TestBoundsHelpers();
    TestColorHelpers();

    if (failures != 0) {
        std::fprintf(stderr, "%d SectorUtilityTests failure(s)\n", failures);
        return 1;
    }
    return 0;
}
