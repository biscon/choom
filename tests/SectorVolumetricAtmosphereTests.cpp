#include "engine/render/HdrEffectPolicy.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include <raymath.h>

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
    const auto globalOnly = game::CombineSectorVolumetricMediumSamples(first, {});
    const auto localOnly = game::CombineSectorVolumetricMediumSamples({}, second);
    Check(Near(globalOnly.extinction, 0.25f)
                    && Near(localOnly.extinction, 0.75f),
          "global-only and local-only medium density remain independently valid");
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
            settings,
            camera,
            Vector3{0.0f, 0.0f, 48.0f},
            settings.volumetricMaxDistanceWorld);
    Check(tail > 0.0f && tail < settings.maxOpacity,
          "analytic fog continues beyond the finite volumetric range");
    const float fullOpticalDepth = game::ComputeSectorFogOpticalDepth(
            settings, camera, Vector3{0.0f, 0.0f, 48.0f}, 48.0f);
    Check(Near(
                  game::ComputeSectorAnalyticFogTailOpacity(
                          settings,
                          camera,
                          Vector3{0.0f, 0.0f, 48.0f},
                          0.0f),
                  1.0f - std::exp(-fullOpticalDepth)),
          "quality Off or unavailable resources restore full analytic fog");
}

void TestFinalAtmosphereSettingNormalization()
{
    game::SectorTopologyFogSettings settings;
    settings.anisotropy = -5.0f;
    settings.volumetricMaxDistanceWorld = 999.0f;
    settings = game::NormalizeSectorTopologyFogSettings(settings);
    Check(Near(settings.anisotropy, -0.90f)
                    && Near(settings.volumetricMaxDistanceWorld, 256.0f),
          "authored volumetric settings clamp to final limits");
    settings.anisotropy = std::numeric_limits<float>::quiet_NaN();
    settings.volumetricMaxDistanceWorld =
            std::numeric_limits<float>::infinity();
    settings = game::NormalizeSectorTopologyFogSettings(settings);
    Check(Near(settings.anisotropy, 0.20f)
                    && Near(settings.volumetricMaxDistanceWorld, 32.0f),
          "non-finite volumetric settings restore final defaults");

    game::SectorLightAtmosphereSettings light;
    light.volumetricScatteringIntensity = -1.0f;
    Check(Near(
                  game::NormalizeSectorLightAtmosphereSettings(light)
                          .volumetricScatteringIntensity,
                  0.0f),
          "zero is the normalized lower bound for volumetric light scattering");
    light.volumetricScatteringIntensity = 20.0f;
    Check(Near(
                  game::NormalizeSectorLightAtmosphereSettings(light)
                          .volumetricScatteringIntensity,
                  8.0f),
          "high volumetric light scattering clamps to eight");
}

game::SectorVolumetricHistoryFrameState MakeHistoryFrame()
{
    game::SectorVolumetricHistoryFrameState frame;
    frame.valid = true;
    frame.targetWidth = 1920;
    frame.targetHeight = 1080;
    frame.quality = game::SectorTopologyFogSettings::VolumetricQuality::High;
    frame.atlas = game::ComputeSectorVolumetricAtlasLayout(
            game::SectorVolumetricGridSize{240, 135, 64});
    frame.cameraForward = Vector3{0.0f, 0.0f, 1.0f};
    frame.cameraUp = Vector3{0.0f, 1.0f, 0.0f};
    frame.verticalFovDegrees = 75.0f;
    frame.aspectRatio = 16.0f / 9.0f;
    frame.nearPlane = 0.05f;
    frame.farPlane = 1000.0f;
    frame.renderSeconds = 1.0f;
    frame.fogSignature = 10;
    frame.sourceRevision = 20;
    return frame;
}

void TestTemporalPoliciesAndJitter()
{
    using Quality = game::SectorTopologyFogSettings::VolumetricQuality;
    const auto low = game::GetSectorVolumetricTemporalPolicy(Quality::Low);
    const auto medium = game::GetSectorVolumetricTemporalPolicy(Quality::Medium);
    const auto high = game::GetSectorVolumetricTemporalPolicy(Quality::High);
    Check(!low.enabled && low.jitterPeriod == 1
                    && medium.enabled && medium.jitterPeriod == 8
                    && Near(medium.baseCurrentFrameWeight, 0.20f)
                    && Near(medium.responsiveCurrentFrameWeight, 0.65f)
                    && high.enabled && high.jitterPeriod == 16
                    && Near(high.baseCurrentFrameWeight, 0.10f)
                    && Near(high.responsiveCurrentFrameWeight, 0.50f),
          "temporal quality policies use fixed internal jitter and blend weights");
    const Vector3 centered = game::ComputeSectorVolumetricJitter(Quality::Low, 12);
    const Vector3 first = game::ComputeSectorVolumetricJitter(Quality::Medium, 0);
    const Vector3 wrapped = game::ComputeSectorVolumetricJitter(Quality::Medium, 8);
    Check(Near(centered.x, 0.0f) && Near(centered.y, 0.0f)
                    && Near(centered.z, 0.5f)
                    && Near(first.x, wrapped.x) && Near(first.y, wrapped.y)
                    && Near(first.z, wrapped.z)
                    && first.x >= -0.5f && first.x <= 0.5f
                    && first.y >= -0.5f && first.y <= 0.5f
                    && first.z >= 0.0f && first.z <= 1.0f,
          "jitter is deterministic, bounded, and wraps at the quality period");
}

void TestTemporalReprojectionAndDepthPolicy()
{
    Vector2 historyUv;
    float previousDepth = 0.0f;
    Check(game::ReprojectSectorVolumetricHistoryUv(
                    Vector2{0.25f, 0.75f}, 0.5f,
                    MatrixIdentity(), MatrixIdentity(),
                    Vector2{0.01f, -0.02f}, Vector2{0.01f, -0.02f},
                    historyUv, previousDepth)
                    && Near(historyUv.x, 0.25f)
                    && Near(historyUv.y, 0.75f)
                    && Near(previousDepth, 0.5f),
          "identity reprojection preserves UV and depth after jitter compensation");
    Check(!game::ReprojectSectorVolumetricHistoryUv(
                    Vector2{0.9f, 0.5f}, 0.5f,
                    MatrixIdentity(), MatrixTranslate(2.0f, 0.0f, 0.0f),
                    Vector2{}, Vector2{}, historyUv, previousDepth),
          "reprojection rejects history outside the previous viewport");
    Check(game::AcceptSectorVolumetricHistoryDepth(10.0f, 10.15f)
                    && !game::AcceptSectorVolumetricHistoryDepth(10.0f, 10.25f)
                    && !game::AcceptSectorVolumetricHistoryDepth(
                            std::numeric_limits<float>::quiet_NaN(), 10.0f),
          "history depth accepts nearby surfaces and rejects disocclusion or invalid data");
    Check(game::ComputeSectorVolumetricBilateralDepthWeight(2.0f, 2.0f) > 0.99f
                    && game::ComputeSectorVolumetricBilateralDepthWeight(2.0f, 4.0f)
                            < 0.001f,
          "bilateral reconstruction strongly rejects a mismatched foreground depth");
}

void TestHistoryClampAndResetTriggers()
{
    const Vector4 clamped = game::ClampSectorVolumetricHistorySample(
            Vector4{10.0f, -2.0f, std::numeric_limits<float>::infinity(), 2.0f},
            Vector4{1.0f, 2.0f, 3.0f, 0.2f},
            Vector4{4.0f, 5.0f, 6.0f, 0.8f});
    Check(Near(clamped.x, 4.0f) && Near(clamped.y, 2.0f)
                    && Near(clamped.z, 3.0f) && Near(clamped.w, 0.8f),
          "history clamp sanitizes finite HDR values and clamps to the current neighborhood");

    const auto base = MakeHistoryFrame();
    Check(game::EvaluateSectorVolumetricHistoryReset({}, base)
                    == game::SectorVolumetricHistoryResetReason::FirstFrame,
          "first temporal frame resets history");
    auto changed = base;
    changed.targetWidth = 1280;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::TargetChanged,
          "target changes reset history");
    changed = base;
    changed.quality = game::SectorTopologyFogSettings::VolumetricQuality::Medium;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::QualityChanged,
          "quality changes reset history");
    changed = base;
    ++changed.atlas.tileColumns;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::AtlasLayoutChanged,
          "atlas layout changes reset history");
    changed = base;
    ++changed.fogSignature;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::FogSettingsChanged,
          "fog changes reset history");
    changed = base;
    ++changed.sourceRevision;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::SourceRevisionChanged,
          "static source revisions reset history");
    changed = base;
    changed.verticalFovDegrees += 1.0f;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::ProjectionChanged,
          "projection changes reset history");
    changed = base;
    changed.cameraPosition.x = 2.01f;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::CameraTranslation,
          "camera teleports reset history");
    changed = base;
    changed.cameraForward = Vector3{std::sin(31.0f * DEG2RAD), 0.0f,
            std::cos(31.0f * DEG2RAD)};
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::CameraRotation,
          "large visual-camera rotations reset history");
    changed = base;
    changed.renderSeconds += 0.251f;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::RenderGap,
          "long render gaps reset history");
    changed = base;
    changed.cameraPosition.x = 0.25f;
    changed.renderSeconds += 0.016f;
    Check(game::EvaluateSectorVolumetricHistoryReset(base, changed)
                    == game::SectorVolumetricHistoryResetReason::None,
          "ordinary motion retains temporal history");
}

} // namespace

int main()
{
    TestCombinedDensityAndStaticRadiance();
    TestBeerLambertIntegrationAndPremultipliedComposition();
    TestPhaseAndDepthClipping();
    TestAnalyticFogHandoff();
    TestFinalAtmosphereSettingNormalization();
    TestTemporalPoliciesAndJitter();
    TestTemporalReprojectionAndDepthPolicy();
    TestHistoryClampAndResetTriggers();
    if (failures != 0) {
        std::cerr << failures << " SectorVolumetricAtmosphereTests failure(s)\n";
        return 1;
    }
    return 0;
}
