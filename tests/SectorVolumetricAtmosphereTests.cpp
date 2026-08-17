#include "engine/render/HdrEffectPolicy.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float left, float right, float tolerance = 0.00001f)
{
    return std::fabs(left - right) <= tolerance;
}

void TestCombinedDensityAndStaticRadiance()
{
    game::SectorVolumetricMediumSample first;
    first.extinction = 0.25f;
    first.extinctionWeightedTint = Vector3{0.25f, 0.0f, 0.0f};
    first.extinctionWeightedStaticRadiance = Vector3{0.5f, 0.0f, 0.0f};
    game::SectorVolumetricMediumSample second;
    second.extinction = 0.75f;
    second.extinctionWeightedTint = Vector3{0.0f, 0.75f, 0.0f};
    second.extinctionWeightedStaticRadiance = Vector3{0.0f, 1.5f, 0.0f};
    const auto combined = game::CombineSectorVolumetricMediumSamples(first, second);
    Check(Near(combined.extinction, 1.0f)
                    && Near(combined.extinctionWeightedTint.x, 0.25f)
                    && Near(combined.extinctionWeightedTint.y, 0.75f)
                    && Near(combined.extinctionWeightedStaticRadiance.x, 0.5f)
                    && Near(combined.extinctionWeightedStaticRadiance.y, 1.5f),
          "overlapping media add extinction and extinction-weighted inputs");
    const auto empty = game::CombineSectorVolumetricMediumSamples({}, {});
    Check(empty.extinction == 0.0f,
          "zero media retain zero extinction and radiance");
}

void TestBeerLambertIntegrationAndPremultipliedComposition()
{
    game::SectorVolumetricIntegrationState state;
    game::IntegrateSectorVolumetricStep(
            state, std::log(2.0f), Vector3{2.0f, 1.0f, 0.5f});
    const Vector4 atmosphere = game::FinishSectorVolumetricIntegration(state);
    Check(Near(atmosphere.w, 0.5f)
                    && Near(atmosphere.x, 1.0f)
                    && Near(atmosphere.y, 0.5f)
                    && Near(atmosphere.z, 0.25f),
          "Beer-Lambert integration stores premultiplied scattering and opacity");
    const Vector4 composed = engine::CompositeHdrPremultipliedAtmosphere(
            Vector4{4.0f, 2.0f, 1.0f, 0.8f}, atmosphere);
    Check(Near(composed.x, 3.0f) && Near(composed.y, 1.5f)
                    && Near(composed.z, 0.75f) && Near(composed.w, 0.8f),
          "premultiplied atmosphere applies transmittance and preserves scene alpha");
}

void TestPhaseAndDepthClipping()
{
    const float isotropic = game::EvaluateSectorHenyeyGreensteinPhase(0.4f, 0.0f);
    Check(Near(isotropic, 1.0f / (4.0f * PI)),
          "zero anisotropy produces the isotropic phase value");
    Check(game::EvaluateSectorHenyeyGreensteinPhase(1.0f, 0.2f)
                    > game::EvaluateSectorHenyeyGreensteinPhase(-1.0f, 0.2f),
          "positive anisotropy favors forward scattering");
    Check(Near(game::ResolveSectorVolumetricSceneDistance(
                       1.0f, 0.1f, 100.0f, 1.0f, 32.0f),
                       32.0f)
                    && Near(game::ResolveSectorVolumetricSceneDistance(
                       std::numeric_limits<float>::quiet_NaN(),
                       0.1f, 100.0f, 1.0f, 32.0f),
                       32.0f),
          "sky and invalid depth safely use the bounded integration distance");
    Check(game::ResolveSectorVolumetricSceneDistance(
                    0.0f, 0.1f, 100.0f, 1.0f, 32.0f) < 0.11f,
          "foreground depth clips the atmosphere ray");
}

void TestAnalyticFogHandoff()
{
    game::SectorTopologyFogSettings settings;
    settings.enabled = true;
    settings.startDistanceWorld = 2.0f;
    settings.density = 0.02f;
    settings.maxOpacity = 0.8f;
    settings.referenceHeightWorld = 0.0f;
    settings.heightFalloff = 0.0f;
    const Vector3 camera{};
    Check(Near(game::ComputeSectorAnalyticFogTailOpacity(
                       settings, camera, Vector3{0.0f, 0.0f, 20.0f}, 32.0f),
                       0.0f),
          "analytic fog contributes no tail inside the volumetric range");
    const float tail = game::ComputeSectorAnalyticFogTailOpacity(
            settings, camera, Vector3{0.0f, 0.0f, 48.0f}, 32.0f);
    Check(tail > 0.0f && tail < settings.maxOpacity,
          "analytic fog continues beyond the finite volumetric range");
}

} // namespace

int main()
{
    TestCombinedDensityAndStaticRadiance();
    TestBeerLambertIntegrationAndPremultipliedComposition();
    TestPhaseAndDepthClipping();
    TestAnalyticFogHandoff();
    if (failures != 0) {
        std::cerr << failures << " SectorVolumetricAtmosphereTests failure(s)\n";
        return 1;
    }
    return 0;
}
