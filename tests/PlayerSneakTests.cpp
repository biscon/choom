#include "game/FpsWeaponRegistry.h"
#include "game/PlayerSneak.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>

namespace {

bool Near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

void TestDefaultsAndSettingsParsing()
{
    game::FpsApplicationSettings settings;
    std::string error;
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1})", settings, &error));
    assert(Near(settings.playerSneak.fullVisibilityLightLevel, 1.0f));
    assert(Near(
            settings.playerSneak.lightHalfResponseRangeNormalized, 0.05f));
    assert(Near(settings.playerSneak.darknessProximityRangeWorld, 4.0f));
    assert(Near(settings.playerSneak.crouchVisualDetectionMultiplier, 0.85f));
    assert(Near(settings.playerSneak.crouchMovementNoiseMultiplier, 0.25f));
    assert(Near(settings.playerFlashlight.intensity, 4.0f));
    assert(Near(settings.playerFlashlight.reachWorld, 18.0f));
    assert(settings.playerFlashlight.castsShadows);
    assert(Near(settings.playerFlashlight.shadowStrength, 1.0f));
    assert(Near(settings.playerFlashlight.shadowContactOffsetWorld, 0.05f));
    assert(Near(settings.playerFlashlight.lateralOffsetWorld, 0.10f));
    assert(Near(
            settings.playerFlashlight.aimConvergenceDistanceWorld, 10.0f));

    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSneak":{"fullVisibilityLightLevel":2.5,"darknessCutoffNormalized":0.1,"lightHalfResponseRangeNormalized":0.2,"visualDetectionBuildSeconds":1.5,"visualDetectionDecaySeconds":2.0,"darknessProximityRangeWorld":0.75,"crouchVisualDetectionMultiplier":0.7,"crouchMovementNoiseMultiplier":0.2}})",
            settings,
            &error));
    assert(Near(settings.playerSneak.fullVisibilityLightLevel, 2.5f));
    assert(Near(settings.playerSneak.darknessCutoffNormalized, 0.1f));
    assert(Near(
            settings.playerSneak.lightHalfResponseRangeNormalized, 0.2f));
    assert(Near(settings.playerSneak.visualDetectionBuildSeconds, 1.5f));
    assert(Near(settings.playerSneak.visualDetectionDecaySeconds, 2.0f));
    assert(Near(settings.playerSneak.darknessProximityRangeWorld, 0.75f));
    assert(Near(settings.playerSneak.crouchVisualDetectionMultiplier, 0.7f));
    assert(Near(settings.playerSneak.crouchMovementNoiseMultiplier, 0.2f));

    const std::filesystem::path path =
            std::filesystem::temp_directory_path()
            / "player_sneak_settings_test.json";
    assert(game::SaveFpsApplicationSettings(
            path.string(), settings, &error));
    game::FpsApplicationSettings loaded;
    assert(game::LoadFpsApplicationSettings(
            path.string(), loaded, &error));
    assert(Near(loaded.playerSneak.fullVisibilityLightLevel, 2.5f));
    assert(Near(loaded.playerSneak.darknessCutoffNormalized, 0.1f));
    assert(Near(
            loaded.playerSneak.lightHalfResponseRangeNormalized, 0.2f));
    assert(Near(loaded.playerSneak.visualDetectionBuildSeconds, 1.5f));
    assert(Near(loaded.playerSneak.crouchMovementNoiseMultiplier, 0.2f));
    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSneak":{"darknessCutoffNormalized":1.0}})",
            settings,
            &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSneak":{"crouchMovementNoiseMultiplier":-0.1}})",
            settings,
            &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSneak":{"darknessCutoffNormalized":0.9,"lightHalfResponseRangeNormalized":0.11}})",
            settings,
            &error));

    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSneak":{"darknessCutoffNormalized":0.99}})",
            settings,
            &error));
    assert(settings.playerSneak.lightHalfResponseRangeNormalized > 0.0f);
    assert(settings.playerSneak.lightHalfResponseRangeNormalized
            < 1.0f - settings.playerSneak.darknessCutoffNormalized);

    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"playerFlashlight":{"intensity":7.5,"reachWorld":24.0,"coneRadiusWorld":6.0,"tint":{"r":200,"g":220,"b":255,"a":255},"hotspotRadiusRatio":0.4,"spillBrightness":0.3,"edgeSoftness":0.2,"beamHaze":0.1,"castsShadows":false,"shadowStrength":0.4,"shadowSoftness":2.0,"shadowContactOffsetWorld":0.007,"heightAboveEyeWorld":0.2,"lateralOffsetWorld":0.14,"aimConvergenceDistanceWorld":12.0,"aimResponseSeconds":0.08}})",
            settings,
            &error));
    assert(Near(settings.playerFlashlight.intensity, 7.5f));
    assert(Near(settings.playerFlashlight.reachWorld, 24.0f));
    assert(!settings.playerFlashlight.castsShadows);
    assert(Near(settings.playerFlashlight.shadowStrength, 0.4f));
    assert(Near(settings.playerFlashlight.shadowContactOffsetWorld, 0.007f));
    assert(Near(settings.playerFlashlight.lateralOffsetWorld, 0.14f));
    assert(Near(
            settings.playerFlashlight.aimConvergenceDistanceWorld, 12.0f));
    assert(settings.playerFlashlight.tint.b == 255);
    const std::filesystem::path flashlightPath =
            std::filesystem::temp_directory_path()
            / "player_flashlight_settings_test.json";
    assert(game::SaveFpsApplicationSettings(
            flashlightPath.string(), settings, &error));
    game::FpsApplicationSettings loadedFlashlight;
    assert(game::LoadFpsApplicationSettings(
            flashlightPath.string(), loadedFlashlight, &error));
    assert(Near(loadedFlashlight.playerFlashlight.shadowContactOffsetWorld,
            0.007f));
    assert(!loadedFlashlight.playerFlashlight.castsShadows);
    assert(Near(loadedFlashlight.playerFlashlight.shadowStrength, 0.4f));
    assert(Near(loadedFlashlight.playerFlashlight.lateralOffsetWorld, 0.14f));
    assert(Near(
            loadedFlashlight.playerFlashlight.aimConvergenceDistanceWorld,
            12.0f));
    std::filesystem::remove(flashlightPath, removeError);
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerFlashlight":{"edgeSoftness":0.0}})",
            settings,
            &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerFlashlight":{"shadowContactOffsetWorld":0.1}})",
            settings,
            &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerFlashlight":{"shadowStrength":1.1}})",
            settings,
            &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerFlashlight":{"castsShadows":1}})",
            settings,
            &error));
}

void TestLowLightDetectionResponseCurve()
{
    const game::PlayerSneakApplicationSettings settings;
    const float cutoff = settings.darknessCutoffNormalized;
    const float halfRange = settings.lightHalfResponseRangeNormalized;

    assert(Near(game::PlayerSneakLightDetectionFactor(
                        cutoff - 0.01f, cutoff, halfRange), 0.0f));
    assert(Near(game::PlayerSneakLightDetectionFactor(
                        cutoff, cutoff, halfRange), 0.0f));
    assert(Near(game::PlayerSneakLightDetectionFactor(
                        cutoff + halfRange, cutoff, halfRange), 0.5f));
    const float mediumResponse = game::PlayerSneakLightDetectionFactor(
            0.5f, cutoff, halfRange);
    const float fullResponse = game::PlayerSneakLightDetectionFactor(
            1.0f, cutoff, halfRange);
    assert(mediumResponse > 0.999f);
    assert(Near(fullResponse, 1.0f));
    assert(fullResponse >= mediumResponse);

    const game::PlayerVisualDetectionStep screenshotStep =
            game::AdvancePlayerVisualDetection(
                    0.0f,
                    true,
                    10.0f,
                    0.105f,
                    1.0f,
                    settings,
                    0.0f);
    assert(screenshotStep.building);
    assert(screenshotStep.lightFactor > 0.53f
            && screenshotStep.lightFactor < 0.54f);
    const float detectionSeconds = settings.visualDetectionBuildSeconds
            / screenshotStep.rateFactor;
    assert(detectionSeconds > 1.6f && detectionSeconds < 1.7f);
}

void TestDarknessProximityDetectionResponse()
{
    const game::PlayerSneakApplicationSettings settings;
    assert(Near(game::PlayerSneakProximityDetectionFactor(
                        0.0f, settings.darknessProximityRangeWorld), 1.0f));
    assert(Near(game::PlayerSneakProximityDetectionFactor(
                        2.0f, settings.darknessProximityRangeWorld), 0.5f));
    assert(Near(game::PlayerSneakProximityDetectionFactor(
                        4.0f, settings.darknessProximityRangeWorld), 0.0f));
    assert(Near(game::PlayerSneakProximityDetectionFactor(
                        5.0f, settings.darknessProximityRangeWorld), 0.0f));

    const game::PlayerVisualDetectionStep screenshotStep =
            game::AdvancePlayerVisualDetection(
                    0.0f,
                    true,
                    2.54f,
                    0.027f,
                    1.0f,
                    settings,
                    0.0f);
    assert(Near(screenshotStep.lightFactor, 0.0f));
    assert(screenshotStep.proximityFactor > 0.364f
            && screenshotStep.proximityFactor < 0.366f);
    assert(Near(
            screenshotStep.visibilityFactor,
            screenshotStep.proximityFactor));
    const float detectionSeconds = settings.visualDetectionBuildSeconds
            / screenshotStep.rateFactor;
    assert(detectionSeconds > 2.4f && detectionSeconds < 2.5f);

    const game::PlayerVisualDetectionStep strongerLightStep =
            game::AdvancePlayerVisualDetection(
                    0.0f,
                    true,
                    3.0f,
                    0.105f,
                    0.0f,
                    settings,
                    0.0f);
    assert(strongerLightStep.lightFactor
            > strongerLightStep.proximityFactor);
    assert(Near(
            strongerLightStep.visibilityFactor,
            strongerLightStep.lightFactor));
}

void TestDetectionBuildAndDecay()
{
    const game::PlayerSneakApplicationSettings settings;

    game::PlayerVisualDetectionStep step =
            game::AdvancePlayerVisualDetection(
                    0.0f, true, 4.0f, 0.0f, 0.0f,
                    settings, settings.visualDetectionBuildSeconds);
    assert(!step.building && Near(step.progress, 0.0f));

    step = game::AdvancePlayerVisualDetection(
            0.0f, true, 0.0f, 0.0f, 0.0f,
            settings, settings.visualDetectionBuildSeconds);
    assert(step.detected && Near(step.progress, 1.0f));

    step = game::AdvancePlayerVisualDetection(
            0.0f, true, 10.0f, 1.0f, 1.0f,
            settings, settings.visualDetectionBuildSeconds);
    assert(step.building && !step.detected);
    assert(Near(step.progress, settings.crouchVisualDetectionMultiplier));

    step = game::AdvancePlayerVisualDetection(
            1.0f, false, 10.0f, 1.0f, 0.0f,
            settings, settings.visualDetectionDecaySeconds * 0.5f);
    assert(!step.building && Near(step.progress, 0.5f));
}

void TestCrouchModifiersBlend()
{
    const game::PlayerSneakApplicationSettings settings;
    assert(Near(game::PlayerSneakCrouchVisualMultiplier(
                        settings, 0.0f), 1.0f));
    assert(Near(game::PlayerSneakCrouchVisualMultiplier(
                        settings, 1.0f), 0.85f));
    assert(Near(game::PlayerSneakMovementNoiseMultiplier(
                        settings, 0.0f), 1.0f));
    assert(Near(game::PlayerSneakMovementNoiseMultiplier(
                        settings, 0.5f), 0.625f));
    assert(Near(game::PlayerSneakMovementNoiseMultiplier(
                        settings, 1.0f), 0.25f));
}

void TestFlashlightForcesStandingFullLightVisibility()
{
    const game::PlayerSneakApplicationSettings settings;
    const float light = game::PlayerSneakVisualLightLevel(0.0f, true);
    const float crouch = game::PlayerSneakVisualCrouchBlend(1.0f, true);
    assert(Near(light, 1.0f));
    assert(Near(crouch, 0.0f));
    const game::PlayerVisualDetectionStep visible =
            game::AdvancePlayerVisualDetection(
                    0.0f,
                    true,
                    20.0f,
                    light,
                    crouch,
                    settings,
                    settings.visualDetectionBuildSeconds);
    assert(visible.detected && Near(visible.rateFactor, 1.0f));
    const game::PlayerVisualDetectionStep occluded =
            game::AdvancePlayerVisualDetection(
                    0.0f,
                    false,
                    20.0f,
                    light,
                    crouch,
                    settings,
                    settings.visualDetectionBuildSeconds);
    assert(!occluded.building && !occluded.detected);
}

} // namespace

int main()
{
    TestDefaultsAndSettingsParsing();
    TestLowLightDetectionResponseCurve();
    TestDarknessProximityDetectionResponse();
    TestDetectionBuildAndDecay();
    TestCrouchModifiersBlend();
    TestFlashlightForcesStandingFullLightVisibility();
    return 0;
}
