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
    TestColorHelpers();

    if (failures != 0) {
        std::fprintf(stderr, "%d SectorUtilityTests failure(s)\n", failures);
        return 1;
    }
    return 0;
}
