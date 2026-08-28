#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "game/FpsHudRenderer.h"
#include "game/FpsViewmodelEffectsRenderer.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"

#include <raymath.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {

constexpr const char* ValidRegistry = R"({
 "version":1,"initialWeaponId":"pistol","weapons":[{
 "id":"pistol","crosshair":{"enabled":true,
 "innerColor":{"r":235,"g":235,"b":225,"a":255},
 "outlineColor":{"r":0,"g":0,"b":0,"a":220},
 "centerGapPixels":4,"segmentLengthPixels":6,
 "innerThicknessPixels":2,"outlineThicknessPixels":1},
 "firing":{"shotIntervalSeconds":0.18,"maximumRangeWorld":100,
 "shootSound":"weapons/pistol/shot_01.ogg",
 "recoil":{"translationImpulse":[0,0,-0.03],
 "rotationImpulseDegrees":[-3,0,0],"rollVariationDegrees":0.45,
 "springFrequencyHz":8,"dampingRatio":0.82,
 "maximumTranslation":[0.05,0.05,0.08],
 "maximumRotationDegrees":[8,2,2]},
 "cameraRecoil":{"enabled":true,"pitchKickDegrees":0.4,
 "pitchVariationDegrees":0.08,"yawVariationDegrees":0.15,
 "rollVariationDegrees":0.07,"springFrequencyHz":5.5,
 "springDampingRatio":0.97,"maxPitchDegrees":1.25,
 "maxYawDegrees":0.6,"maxRollDegrees":0.2},
 "muzzleSocket":{"position":[0,0.035,0.105],"rotationDegrees":[0,0,0]},
 "muzzleFlash":{"enabled":true,"lifetimeSeconds":0.033,"sizeWorld":0.1,
 "radianceStrength":12,
 "sizeVariation":0.12,"irregularity":0.65,"forwardStretch":1.8,
 "minimumLobeCount":5,"maximumLobeCount":8,"rearSuppression":0.9,
 "coreColor":{"r":255,"g":255,"b":245,"a":255},
 "hotColor":{"r":255,"g":235,"b":120,"a":255},
 "warmColor":{"r":255,"g":90,"b":15,"a":230},
 "edgeColor":{"r":120,"g":15,"b":5,"a":150},"edgeSoftness":0.35},
 "muzzleLight":{"enabled":true,"color":{"r":255,"g":165,"b":70,"a":255},
 "intensity":6,"radiusWorld":2.5,"lifetimeSeconds":0.07,"decayExponent":2.5}},
 "viewmodel":{"modelPath":"assets/models/weapons/pistol_arms.glb",
 "idleAnimation":"Pistol Idle","sourceFps":30,"firstFrame":1,"lastFrame":41,
 "playbackSpeed":1,"position":[0,-1.4,0.25],"rotationDegrees":[1,2,3],
 "scale":1,"verticalFovDegrees":65,
 "holsterTransition":{"holsterDurationSeconds":0.25,
 "unholsterDurationSeconds":0.34,"hiddenTranslation":[0.45,-1.75,0],
 "hiddenRotationDegrees":[13,0,-10]},"brightnessAdjustment":-0.15,
 "materialOverride":{"metallicFactor":0,
 "roughnessFactor":0.78,"useMetallicRoughnessTexture":false},
 "attachment":{"modelPath":"assets/models/weapons/pistol.glb",
 "boneName":"mixamorig12:RightHand",
 "translation":[0.020654,0.037913,-0.149138],
 "rotationDegrees":[3.058116,175.482727,88.187634],"scale":1,
 "brightnessAdjustment":0,
 "materialOverride":{"metallicFactor":0.35,"roughnessFactor":1,
 "useMetallicRoughnessTexture":true}}}}]})";

bool Near(float a, float b, float tolerance = 0.0002f) { return std::abs(a - b) <= tolerance; }

bool Near(Vector3 a, Vector3 b, float tolerance = 0.0002f)
{
    return Near(a.x, b.x, tolerance)
            && Near(a.y, b.y, tolerance)
            && Near(a.z, b.z, tolerance);
}

bool SameRectangle(Rectangle lhs, Rectangle rhs)
{
    return Near(lhs.x, rhs.x)
            && Near(lhs.y, rhs.y)
            && Near(lhs.width, rhs.width)
            && Near(lhs.height, rhs.height);
}

bool SameTransform(Matrix lhs, Matrix rhs, float tolerance = 0.0003f)
{
    const Vector3 points[] = {
            Vector3{},
            Vector3{1.0f, 0.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, 0.0f, 1.0f}};
    for (Vector3 point : points) {
        const Vector3 a = Vector3Transform(point, lhs);
        const Vector3 b = Vector3Transform(point, rhs);
        if (!Near(a.x, b.x, tolerance)
                || !Near(a.y, b.y, tolerance)
                || !Near(a.z, b.z, tolerance)) {
            return false;
        }
    }
    return true;
}

void RegistrySuccess()
{
    game::FpsWeaponRegistry registry; std::string error;
    assert(game::ParseFpsWeaponRegistry(ValidRegistry, registry, &error));
    const auto* pistol = game::FindFpsWeaponDefinition(registry, "pistol");
    assert(pistol && registry.version == 3 && pistol->weaponSlot == 1);
    assert(pistol->reload.magazineSize == 1);
    assert(Near(pistol->reload.durationSeconds, 1.0f));
    assert(pistol->reload.dryFireSoundPath.empty());
    assert(pistol->reload.reloadSoundPath.empty());
    assert(pistol->crosshair.enabled);
    assert(pistol->crosshair.innerColor.r == 235
            && pistol->crosshair.innerColor.g == 235
            && pistol->crosshair.innerColor.b == 225
            && pistol->crosshair.innerColor.a == 255);
    assert(pistol->crosshair.outlineColor.r == 0
            && pistol->crosshair.outlineColor.a == 220);
    assert(Near(pistol->crosshair.centerGapPixels, 4.0f));
    assert(Near(pistol->crosshair.segmentLengthPixels, 6.0f));
    assert(Near(pistol->crosshair.innerThicknessPixels, 2.0f));
    assert(Near(pistol->crosshair.outlineThicknessPixels, 1.0f));
    assert(Near(pistol->firing.shotIntervalSeconds, 0.18f));
    assert(Near(pistol->firing.maximumRangeWorld, 100.0f));
    assert(Near(pistol->firing.noiseRadiusWorld, 40.0f));
    assert(!pistol->firing.pellets.enabled
            && pistol->firing.pellets.count == 8
            && Near(
                    pistol->firing.pellets.spreadHalfAngleDegrees,
                    6.0f));
    assert(pistol->firing.impact.damage == 0
            && !pistol->firing.impact.blood.enabled
            && !pistol->firing.impact.surfaceDebris.enabled);
    assert(pistol->firing.shootSoundPath == "weapons/pistol/shot_01.ogg");
    assert(Near(pistol->firing.recoil.translationImpulse.z, -0.03f));
    assert(Near(pistol->firing.recoil.rotationImpulseDegrees.x, -3.0f));
    assert(pistol->firing.cameraRecoil.enabled);
    assert(Near(pistol->firing.cameraRecoil.pitchKickDegrees, 0.4f));
    assert(Near(pistol->firing.cameraRecoil.pitchVariationDegrees, 0.08f));
    assert(Near(pistol->firing.cameraRecoil.yawVariationDegrees, 0.15f));
    assert(Near(pistol->firing.cameraRecoil.rollVariationDegrees, 0.07f));
    assert(Near(pistol->firing.cameraRecoil.springFrequencyHz, 5.5f));
    assert(Near(pistol->firing.cameraRecoil.springDampingRatio, 0.97f));
    assert(Near(pistol->firing.cameraRecoil.maxPitchDegrees, 1.25f));
    assert(Near(pistol->firing.cameraRecoil.maxYawDegrees, 0.6f));
    assert(Near(pistol->firing.cameraRecoil.maxRollDegrees, 0.2f));
    assert(Near(pistol->firing.muzzleSocket.position.z, 0.105f));
    assert(Near(pistol->firing.muzzleFlash.lifetimeSeconds, 0.033f));
    assert(Near(pistol->firing.muzzleFlash.radianceStrength, 12.0f));
    assert(Near(pistol->firing.muzzleFlash.sizeVariation, 0.12f));
    assert(Near(pistol->firing.muzzleFlash.irregularity, 0.65f));
    assert(Near(pistol->firing.muzzleFlash.forwardStretch, 1.8f));
    assert(pistol->firing.muzzleFlash.minimumLobeCount == 5);
    assert(pistol->firing.muzzleFlash.maximumLobeCount == 8);
    assert(Near(pistol->firing.muzzleFlash.rearSuppression, 0.9f));
    assert(pistol->firing.muzzleFlash.coreColor.b == 245);
    assert(pistol->firing.muzzleFlash.hotColor.g == 235);
    assert(pistol->firing.muzzleFlash.warmColor.r == 255);
    assert(pistol->firing.muzzleFlash.edgeColor.r == 120);
    assert(Near(pistol->firing.muzzleFlash.edgeSoftness, 0.35f));
    assert(Near(pistol->firing.muzzleLight.radiusWorld, 2.5f));
    assert(pistol->viewmodel.modelPath == "assets/models/weapons/pistol_arms.glb");
    assert(pistol->viewmodel.idleAnimation == "Pistol Idle");
    assert(pistol->viewmodel.firstFrame == 1 && pistol->viewmodel.lastFrame == 41);
    assert(Near(pistol->viewmodel.presentation.position.y, -1.4f));
    assert(Near(pistol->viewmodel.presentation.rotationDegrees.z, 3));
    assert(Near(pistol->viewmodel.presentation.scale, 1));
    assert(Near(pistol->viewmodel.presentation.verticalFovDegrees, 65));
    assert(Near(
            pistol->viewmodel.holsterTransition.holsterDurationSeconds,
            0.25f));
    assert(Near(
            pistol->viewmodel.holsterTransition.unholsterDurationSeconds,
            0.34f));
    assert(Near(
            pistol->viewmodel.holsterTransition.hiddenTranslation.x,
            0.45f));
    assert(Near(
            pistol->viewmodel.holsterTransition.hiddenTranslation.y,
            -1.75f));
    assert(Near(
            pistol->viewmodel.holsterTransition.hiddenRotationDegrees.x,
            13.0f));
    assert(Near(
            pistol->viewmodel.holsterTransition.hiddenRotationDegrees.z,
            -10.0f));
    assert(Near(pistol->viewmodel.brightnessAdjustment, -0.15f));
    assert(pistol->viewmodel.materialOverride.enabled);
    assert(Near(pistol->viewmodel.materialOverride.metallicFactor, 0));
    assert(Near(pistol->viewmodel.materialOverride.roughnessFactor, 0.78f));
    assert(!pistol->viewmodel.materialOverride.useMetallicRoughnessTexture);
    assert(pistol->viewmodel.attachment.modelPath
            == "assets/models/weapons/pistol.glb");
    assert(pistol->viewmodel.attachment.boneName
            == "mixamorig12:RightHand");
    assert(Near(
            pistol->viewmodel.attachment.gripCorrection.translation.z,
            -0.149138f));
    assert(Near(
            pistol->viewmodel.attachment.gripCorrection.rotationDegrees.y,
            175.482727f));
    assert(Near(pistol->viewmodel.attachment.gripCorrection.scale, 1.0f));
    assert(Near(
            pistol->viewmodel.attachment.lighting.brightnessAdjustment,
            0.0f));
    assert(pistol->viewmodel.attachment.lighting.materialOverride.enabled);
    assert(Near(
            pistol->viewmodel.attachment.lighting.materialOverride
                    .metallicFactor,
            0.35f));
    assert(Near(
            pistol->viewmodel.attachment.lighting.materialOverride
                    .roughnessFactor,
            1.0f));
    assert(pistol->viewmodel.attachment.lighting.materialOverride
            .useMetallicRoughnessTexture);
    assert(Near(game::FpsViewmodelBrightnessMultiplier(-0.55f), 0.45f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(
            pistol->viewmodel.attachment.lighting.brightnessAdjustment), 1.0f));

    std::string withImpact = ValidRegistry;
    const std::string impactJson =
            "\"impact\":{\"damage\":25,\"staggerSeconds\":0.18,"
            "\"knockbackImpulseWorldPerSecond\":1.25,"
            "\"blood\":{\"enabled\":true,\"particleCount\":18,"
            "\"sizeScale\":1,\"intensity\":1},"
            "\"surfaceDebris\":{\"enabled\":true,\"particleCount\":14,"
            "\"sizeScale\":0.85,\"intensity\":0.8}},";
    withImpact.insert(withImpact.find("\"shootSound\""), impactJson);
    assert(game::ParseFpsWeaponRegistry(withImpact, registry, &error));
    pistol = game::FindFpsWeaponDefinition(registry, "pistol");
    assert(pistol != nullptr
            && pistol->firing.impact.damage == 25
            && Near(pistol->firing.impact.staggerSeconds, 0.18f)
            && Near(pistol->firing.impact.knockbackImpulseWorldPerSecond, 1.25f)
            && pistol->firing.impact.blood.enabled
            && pistol->firing.impact.blood.particleCount == 18
            && pistol->firing.impact.surfaceDebris.enabled
            && pistol->firing.impact.surfaceDebris.particleCount == 14);

    std::string withoutOverride = ValidRegistry;
    const size_t overrideField = withoutOverride.find("\"materialOverride\"");
    const size_t overrideBegin = withoutOverride.rfind(',', overrideField);
    const size_t overrideEnd = withoutOverride.find('}', overrideField);
    assert(overrideField != std::string::npos
            && overrideBegin != std::string::npos
            && overrideEnd != std::string::npos);
    withoutOverride.erase(overrideBegin, overrideEnd - overrideBegin + 1);
    game::FpsWeaponRegistry defaultRegistry;
    assert(game::ParseFpsWeaponRegistry(withoutOverride, defaultRegistry, &error));
    const auto* defaultPistol = game::FindFpsWeaponDefinition(defaultRegistry, "pistol");
    assert(defaultPistol && !defaultPistol->viewmodel.materialOverride.enabled);

    std::string withoutBrightness = ValidRegistry;
    const size_t brightnessBegin = withoutBrightness.find(
            ",\"brightnessAdjustment\":-0.15");
    assert(brightnessBegin != std::string::npos);
    withoutBrightness.erase(
            brightnessBegin,
            std::string(",\"brightnessAdjustment\":-0.15").size());
    game::FpsWeaponRegistry neutralRegistry;
    assert(game::ParseFpsWeaponRegistry(withoutBrightness, neutralRegistry, &error));
    const auto* neutralPistol = game::FindFpsWeaponDefinition(neutralRegistry, "pistol");
    assert(neutralPistol && Near(neutralPistol->viewmodel.brightnessAdjustment, 0.0f));

    std::string withoutCrosshair = ValidRegistry;
    const size_t crosshairBegin = withoutCrosshair.find(",\"crosshair\":");
    const size_t viewmodelBegin = withoutCrosshair.find(
            "\"firing\":", crosshairBegin);
    assert(crosshairBegin != std::string::npos
            && viewmodelBegin != std::string::npos);
    withoutCrosshair.replace(
            crosshairBegin,
            viewmodelBegin - crosshairBegin,
            ",");
    game::FpsWeaponRegistry noCrosshairRegistry;
    assert(game::ParseFpsWeaponRegistry(
            withoutCrosshair, noCrosshairRegistry, &error));
    const auto* noCrosshairPistol = game::FindFpsWeaponDefinition(
            noCrosshairRegistry, "pistol");
    assert(noCrosshairPistol && !noCrosshairPistol->crosshair.enabled);
    assert(Near(noCrosshairPistol->crosshair.centerGapPixels, 4.0f));

    std::string withoutCameraRecoil = ValidRegistry;
    const size_t cameraRecoilBegin = withoutCameraRecoil.find(
            ",\n \"cameraRecoil\":");
    const size_t muzzleSocketBegin = withoutCameraRecoil.find(
            "\n \"muzzleSocket\":", cameraRecoilBegin);
    assert(cameraRecoilBegin != std::string::npos
            && muzzleSocketBegin != std::string::npos);
    withoutCameraRecoil.replace(
            cameraRecoilBegin,
            muzzleSocketBegin - cameraRecoilBegin,
            ",");
    game::FpsWeaponRegistry legacyCameraRegistry;
    assert(game::ParseFpsWeaponRegistry(
            withoutCameraRecoil, legacyCameraRegistry, &error));
    const auto* legacyCameraPistol = game::FindFpsWeaponDefinition(
            legacyCameraRegistry, "pistol");
    assert(legacyCameraPistol
            && !legacyCameraPistol->firing.cameraRecoil.enabled);

    std::string legacyFlash = ValidRegistry;
    const size_t gradientBegin = legacyFlash.find("\"coreColor\"");
    const std::string edgeSoftness = "\"edgeSoftness\":0.35";
    const size_t gradientEnd = legacyFlash.find(
            edgeSoftness, gradientBegin);
    assert(gradientBegin != std::string::npos
            && gradientEnd != std::string::npos);
    legacyFlash.replace(
            gradientBegin,
            gradientEnd + edgeSoftness.size() - gradientBegin,
            "\"innerColor\":{\"r\":255,\"g\":250,\"b\":220,\"a\":255},"
            "\"outerColor\":{\"r\":255,\"g\":145,\"b\":40,\"a\":200}");
    game::FpsWeaponRegistry legacyRegistry;
    assert(!game::ParseFpsWeaponRegistry(
            legacyFlash, legacyRegistry, &error));
    assert(error.find("coreColor") != std::string::npos);
}

void RegistryRoundTripAndSharedArmsConfiguration()
{
    game::FpsWeaponRegistry registry;
    std::string error;
    assert(game::ParseFpsWeaponRegistry(ValidRegistry, registry, &error));

    game::FpsWeaponDefinition rifle = registry.weapons.front();
    registry.weapons.front().reload.magazineSize = 18;
    registry.weapons.front().reload.durationSeconds = 0.63f;
    registry.weapons.front().reload.dryFireSoundPath =
            "weapons/pistol/dry_fire_01.ogg";
    registry.weapons.front().reload.reloadSoundPath =
            "weapons/pistol/reload_02.ogg";
    rifle.id = "rifle";
    rifle.weaponSlot = 2;
    rifle.reload.magazineSize = 2;
    rifle.reload.durationSeconds = 2.33f;
    rifle.reload.dryFireSoundPath = "weapons/rifle/dry_fire.ogg";
    rifle.reload.reloadSoundPath = "weapons/rifle/reload.ogg";
    rifle.firing.shotIntervalSeconds = 0.09f;
    rifle.firing.noiseRadiusWorld = 65.0f;
    rifle.firing.pellets = {true, 12, 7.5f};
    rifle.viewmodel.attachment.gripCorrection.translation.x = 0.11f;
    registry.weapons.push_back(rifle);

    std::string serialized;
    assert(game::SerializeFpsWeaponRegistryJson(
            registry, serialized, &error));
    game::FpsWeaponRegistry roundTrip;
    assert(game::ParseFpsWeaponRegistry(serialized, roundTrip, &error));
    assert(roundTrip.weapons.size() == 2);
    const auto* pistol = game::FindFpsWeaponDefinition(roundTrip, "pistol");
    const auto* loadedRifle = game::FindFpsWeaponDefinition(roundTrip, "rifle");
    assert(pistol != nullptr && loadedRifle != nullptr);
    assert(pistol->viewmodel.modelPath == loadedRifle->viewmodel.modelPath);
    assert(!Near(
            pistol->viewmodel.attachment.gripCorrection.translation.x,
            loadedRifle->viewmodel.attachment.gripCorrection.translation.x));
    assert(!Near(
            pistol->firing.shotIntervalSeconds,
            loadedRifle->firing.shotIntervalSeconds));
    assert(Near(loadedRifle->firing.noiseRadiusWorld, 65.0f));
    assert(pistol->weaponSlot == 1 && loadedRifle->weaponSlot == 2);
    assert(pistol->reload.magazineSize == 18
            && Near(pistol->reload.durationSeconds, 0.63f)
            && pistol->reload.dryFireSoundPath
                    == "weapons/pistol/dry_fire_01.ogg"
            && pistol->reload.reloadSoundPath
                    == "weapons/pistol/reload_02.ogg");
    assert(loadedRifle->reload.magazineSize == 2
            && Near(loadedRifle->reload.durationSeconds, 2.33f));
    assert(!pistol->firing.pellets.enabled);
    assert(loadedRifle->firing.pellets.enabled
            && loadedRifle->firing.pellets.count == 12
            && Near(
                    loadedRifle->firing.pellets.spreadHalfAngleDegrees,
                    7.5f));
    assert(serialized.find("\"pellets\"") != std::string::npos);
    assert(serialized.find("\"version\": 3") != std::string::npos);
    assert(serialized.find("\"reload\"") != std::string::npos);
    assert(serialized.find("initialWeaponId") == std::string::npos);

    game::FpsApplicationSettings settings;
    game::FpsViewmodelPresentationOverride override;
    override.position = Vector3{0.25f, -1.0f, 0.1f};
    game::SetFpsViewmodelOverride(settings, "rifle", override);
    game::ApplyFpsApplicationWeaponOverrides(roundTrip, settings);
    loadedRifle = game::FindFpsWeaponDefinition(roundTrip, "rifle");
    assert(loadedRifle != nullptr
            && Near(loadedRifle->viewmodel.presentation.position.x, 0.25f));

    roundTrip.weapons.back().weaponSlot = 1;
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("duplicate weapon slot 1") != std::string::npos);
    roundTrip.weapons.back().weaponSlot = 2;
    roundTrip.weapons.back().id = "pistol";
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("duplicate weapon id") != std::string::npos);
    roundTrip.weapons.back().id = "rifle";
    roundTrip.weapons.back().firing.pellets.count =
            game::MaxFpsWeaponPellets + 1;
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("pellets contains an invalid count")
            != std::string::npos);

    roundTrip.weapons.back().firing.pellets.count = 8;
    roundTrip.weapons.back().reload.magazineSize = 0;
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("magazineSize") != std::string::npos);
    roundTrip.weapons.back().reload.magazineSize = 2;
    roundTrip.weapons.back().reload.durationSeconds = 0.0f;
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("durationSeconds") != std::string::npos);
    roundTrip.weapons.back().reload.durationSeconds = 2.33f;
    roundTrip.weapons.back().reload.reloadSoundPath = "../outside.wav";
    assert(!game::ValidateFpsWeaponRegistry(roundTrip, &error));
    assert(error.find("reloadSound") != std::string::npos);
}

void PelletDirectionGeneration()
{
    const Vector3 aim{0.0f, 0.0f, 1.0f};
    game::FpsWeaponPelletDefinition pellets{true, 8, 6.0f};
    assert(Near(
            game::FpsWeaponPelletDirection(aim, pellets, 0, 17),
            aim));

    constexpr float Pi = 3.14159265358979323846f;
    const float minimumDot = std::cos(6.0f * Pi / 180.0f);
    bool sequenceChangedPattern = false;
    for (int pelletIndex = 1; pelletIndex < pellets.count; ++pelletIndex) {
        const Vector3 first = game::FpsWeaponPelletDirection(
                aim, pellets, pelletIndex, 17);
        const Vector3 duplicate = game::FpsWeaponPelletDirection(
                aim, pellets, pelletIndex, 17);
        const Vector3 nextShot = game::FpsWeaponPelletDirection(
                aim, pellets, pelletIndex, 18);
        assert(Near(Vector3Length(first), 1.0f));
        assert(Vector3DotProduct(first, aim) >= minimumDot - 0.0001f);
        assert(Near(first, duplicate));
        sequenceChangedPattern = sequenceChangedPattern
                || !Near(first, nextShot);
    }
    assert(sequenceChangedPattern);

    pellets.enabled = false;
    assert(Near(
            game::FpsWeaponPelletDirection(aim, pellets, 7, 17),
            aim));
}

void ExpectRegistryFailure(std::string text, const char* expected)
{
    game::FpsWeaponRegistry registry; std::string error;
    assert(!game::ParseFpsWeaponRegistry(text, registry, &error));
    if (error.find(expected) == std::string::npos) {
        std::cerr << "Expected registry error containing '" << expected
                  << "', got '" << error << "'\n";
    }
    assert(error.find(expected) != std::string::npos);
}

void RegistryValidation()
{
    std::string value = ValidRegistry;
    const size_t firingField = value.find("\"firing\":");
    const size_t firingBegin = value.rfind(',', firingField);
    const size_t viewmodelAfterFiring = value.find("\"viewmodel\":", firingField);
    assert(firingField != std::string::npos
            && firingBegin != std::string::npos
            && viewmodelAfterFiring != std::string::npos);
    value.replace(firingBegin, viewmodelAfterFiring - firingBegin, ",");
    ExpectRegistryFailure(value, "missing required field 'firing'");

    value = ValidRegistry;
    value.replace(value.find("\"enabled\":true"), 14, "\"enabled\":1");
    ExpectRegistryFailure(value, "crosshair.enabled must be a boolean");

    value = ValidRegistry;
    value.replace(value.find("\"r\":235"), 7, "\"r\":256");
    ExpectRegistryFailure(value, "innerColor.r must be between 0 and 255");

    value = ValidRegistry;
    const std::string innerColor =
            "{\"r\":235,\"g\":235,\"b\":225,\"a\":255}";
    value.replace(value.find(innerColor), innerColor.size(), "0");
    ExpectRegistryFailure(value, "crosshair.innerColor must be an object");

    value = ValidRegistry;
    value.replace(value.find("\"centerGapPixels\":4"), 19,
            "\"centerGapPixels\":0");
    ExpectRegistryFailure(value, "crosshair dimensions");

    value = ValidRegistry;
    value.replace(value.find("\"shotIntervalSeconds\":0.18"), 26,
            "\"shotIntervalSeconds\":0");
    ExpectRegistryFailure(value, "shot interval and maximum range");

    value = ValidRegistry;
    value.replace(
            value.find("weapons/pistol/shot_01.ogg"),
            std::string("weapons/pistol/shot_01.ogg").size(),
            "../shot_01.ogg");
    ExpectRegistryFailure(value, "shootSound");

    value = ValidRegistry;
    value.replace(
            value.find("weapons/pistol/shot_01.ogg"),
            std::string("weapons/pistol/shot_01.ogg").size(),
            "weapons/pistol/shot_01.flac");
    ExpectRegistryFailure(value, "shootSound");

    value = ValidRegistry;
    value.replace(value.find("\"springFrequencyHz\":8"), 21,
            "\"springFrequencyHz\":0");
    ExpectRegistryFailure(value, "recoil contains an invalid response");

    value = ValidRegistry;
    value.replace(value.find("\"springDampingRatio\":0.97"),
            std::string("\"springDampingRatio\":0.97").size(),
            "\"springDampingRatio\":0");
    ExpectRegistryFailure(value, "cameraRecoil contains an invalid kick");

    value = ValidRegistry;
    value.replace(value.find("\"yawVariationDegrees\":0.15"),
            std::string("\"yawVariationDegrees\":0.15").size(),
            "\"yawVariationDegrees\":-1");
    ExpectRegistryFailure(value, "cameraRecoil contains an invalid kick");

    value = ValidRegistry;
    value.replace(value.find("\"maxRollDegrees\":0.2"),
            std::string("\"maxRollDegrees\":0.2").size(),
            "\"maxRollDegrees\":1e999");
    ExpectRegistryFailure(value, "overflow");

    value = ValidRegistry;
    value.replace(value.find("\"lifetimeSeconds\":0.033"), 23,
            "\"lifetimeSeconds\":0");
    ExpectRegistryFailure(value, "muzzleFlash contains an invalid lifetime");

    value = ValidRegistry;
    value.replace(value.find("\"edgeSoftness\":0.35"), 19,
            "\"edgeSoftness\":0");
    ExpectRegistryFailure(value, "invalid lifetime, size, shape, lobe range, or edge softness");

    value = ValidRegistry;
    value.replace(value.find("\"minimumLobeCount\":5"), 20,
            "\"minimumLobeCount\":9");
    ExpectRegistryFailure(value, "invalid lifetime, size, shape, lobe range");

    value = ValidRegistry;
    value.replace(value.find("\"irregularity\":0.65"), 19,
            "\"irregularity\":2");
    ExpectRegistryFailure(value, "invalid lifetime, size, shape, lobe range");

    value = ValidRegistry;
    const std::string hotColor =
            "\"hotColor\":{\"r\":255,\"g\":235,\"b\":120,\"a\":255},";
    value.erase(value.find(hotColor), hotColor.size());
    ExpectRegistryFailure(value, "missing required field 'hotColor'");

    value = ValidRegistry;
    value.replace(value.find("\"segmentLengthPixels\":6"), 23,
            "\"segmentLengthPixels\":-1");
    ExpectRegistryFailure(value, "crosshair dimensions");

    value = ValidRegistry;
    value.replace(value.find("\"innerThicknessPixels\":2"), 24,
            "\"innerThicknessPixels\":\"wide\"");
    ExpectRegistryFailure(value, "innerThicknessPixels must be a number");

    value = ValidRegistry;
    value.replace(value.find("\"outlineThicknessPixels\":1"), 26,
            "\"outlineThicknessPixels\":1e999");
    ExpectRegistryFailure(value, "overflow");

    value = ValidRegistry;
    value.replace(value.find("\"idleAnimation\":"), 16, "\"missingField\":");
    ExpectRegistryFailure(value, "idleAnimation");

    value = ValidRegistry;
    value.replace(
            value.find("\"holsterTransition\""),
            std::string("\"holsterTransition\"").size(),
            "\"missingHolsterTransition\"");
    ExpectRegistryFailure(value, "holsterTransition");

    value = ValidRegistry;
    value.replace(
            value.find("\"holsterDurationSeconds\":0.25"),
            std::string("\"holsterDurationSeconds\":0.25").size(),
            "\"holsterDurationSeconds\":0");
    ExpectRegistryFailure(value, "holsterDurationSeconds must be greater than zero");

    value = ValidRegistry;
    value.replace(
            value.find("\"unholsterDurationSeconds\":0.34"),
            std::string("\"unholsterDurationSeconds\":0.34").size(),
            "\"unholsterDurationSeconds\":\"slow\"");
    ExpectRegistryFailure(value, "unholsterDurationSeconds must be a number");

    value = ValidRegistry;
    value.replace(
            value.find("\"hiddenTranslation\":[0.45,-1.75,0]"),
            std::string("\"hiddenTranslation\":[0.45,-1.75,0]").size(),
            "\"hiddenTranslation\":[0.45,-1.75]");
    ExpectRegistryFailure(value, "hiddenTranslation must contain 3 numbers");

    value = ValidRegistry;
    const size_t close = value.rfind("]");
    const size_t object = value.find("{\n \"id\"");
    value.insert(close, "," + value.substr(object, close - object));
    ExpectRegistryFailure(value, "duplicate weapon id");

    value = ValidRegistry;
    value.replace(value.find("\"pistol\""), 8, "\"missing\"");
    ExpectRegistryFailure(value, "has no definition");

    value = ValidRegistry;
    value.replace(value.find("assets/models/weapons/pistol_arms.glb"), 37, "../pistol.glb");
    ExpectRegistryFailure(value, "modelPath");

    value = ValidRegistry;
    value.replace(value.find("Pistol Idle"), 11, "");
    ExpectRegistryFailure(value, "must not be empty");

    value = ValidRegistry;
    value.replace(value.find("\"attachment\""), 12, "\"missingAttachment\"");
    ExpectRegistryFailure(value, "attachment");

    value = ValidRegistry;
    value.replace(
            value.find("assets/models/weapons/pistol.glb"),
            std::string("assets/models/weapons/pistol.glb").size(),
            "../pistol.glb");
    ExpectRegistryFailure(value, "attachment.modelPath");

    value = ValidRegistry;
    value.replace(value.find("mixamorig12:RightHand"), 21, "");
    ExpectRegistryFailure(value, "boneName must not be empty");

    value = ValidRegistry;
    value.replace(value.find("mixamorig12:RightHand"), 21,
            "bone_name_that_is_longer_than_raylib_allows");
    ExpectRegistryFailure(value, "at most 31 characters");

    value = ValidRegistry;
    value.replace(value.rfind("\"scale\":1"), 9, "\"scale\":0");
    ExpectRegistryFailure(value, "attachment.scale must be greater than zero");

    value = ValidRegistry;
    const size_t attachmentBrightness = value.rfind(
            "\"brightnessAdjustment\":0");
    assert(attachmentBrightness != std::string::npos);
    value.replace(
            attachmentBrightness,
            std::string("\"brightnessAdjustment\":0").size(),
            "\"brightnessAdjustment\":1.1");
    ExpectRegistryFailure(
            value, "attachment.brightnessAdjustment must be between -1 and 1");

    value = ValidRegistry;
    value.replace(
            value.rfind("\"materialOverride\""),
            std::string("\"materialOverride\"").size(),
            "\"missingMaterialOverride\"");
    ExpectRegistryFailure(value, "missing required field 'materialOverride'");

    value = ValidRegistry;
    value.replace(
            value.find("\"translation\":[0.020654,0.037913,-0.149138]"),
            13,
            "\"missingGrip\"");
    ExpectRegistryFailure(value, "missing required field 'translation'");

    value = ValidRegistry;
    value.replace(value.find("0.020654"), 8, "\"invalid\"");
    ExpectRegistryFailure(value, "translation must contain only numbers");

    value = ValidRegistry;
    value.replace(value.find("\"lastFrame\":41"), 14, "\"lastFrame\":1");
    ExpectRegistryFailure(value, "lastFrame > firstFrame");

    value = ValidRegistry;
    value.replace(value.find("\"metallicFactor\":0"), 18, "\"metallicFactor\":2");
    ExpectRegistryFailure(value, "metallicFactor must be between 0 and 1");

    value = ValidRegistry;
    value.replace(value.find("\"roughnessFactor\":0.78"), 22, "\"roughnessFactor\":0");
    ExpectRegistryFailure(value, "roughnessFactor must be between 0.045 and 1");

    value = ValidRegistry;
    value.replace(value.find("\"useMetallicRoughnessTexture\":false"), 35,
            "\"useMetallicRoughnessTexture\":0");
    ExpectRegistryFailure(value, "useMetallicRoughnessTexture must be a boolean");

    value = ValidRegistry;
    value.replace(value.find("\"brightnessAdjustment\":-0.15"), 28,
            "\"brightnessAdjustment\":1.01");
    ExpectRegistryFailure(value, "brightnessAdjustment must be between -1 and 1");

    value = ValidRegistry;
    value.replace(value.find("\"brightnessAdjustment\":-0.15"), 28,
            "\"brightnessAdjustment\":-1.01");
    ExpectRegistryFailure(value, "brightnessAdjustment must be between -1 and 1");

    value = ValidRegistry;
    value.replace(value.find("\"brightnessAdjustment\":-0.15"), 28,
            "\"brightnessAdjustment\":\"dark\"");
    ExpectRegistryFailure(value, "brightnessAdjustment must be a number");

    value = ValidRegistry;
    value.replace(value.find("\"brightnessAdjustment\":-0.15"), 28,
            "\"brightnessAdjustment\":1e999");
    game::FpsWeaponRegistry nonFiniteRegistry;
    std::string nonFiniteError;
    assert(!game::ParseFpsWeaponRegistry(
            value,
            nonFiniteRegistry,
            &nonFiniteError));
    assert(!nonFiniteError.empty());
}

void WeaponSlotSchemaAndKeys()
{
    game::FpsWeaponRegistry registry;
    std::string error;
    assert(game::ParseFpsWeaponRegistry(ValidRegistry, registry, &error));
    assert(registry.version == 3);
    assert(game::FindFpsWeaponDefinitionForSlot(registry, 1)
            == &registry.weapons.front());
    assert(game::FindFpsWeaponDefinitionForSlot(registry, 0) == nullptr);

    registry.weapons.front().weaponSlot = 0;
    std::string serialized;
    assert(game::SerializeFpsWeaponRegistryJson(
            registry, serialized, &error));
    assert(serialized.find("\"initialWeaponId\"") == std::string::npos);
    assert(serialized.find("\"slot\"") == std::string::npos);

    game::FpsWeaponRegistry unassigned;
    assert(game::ParseFpsWeaponRegistry(serialized, unassigned, &error));
    assert(unassigned.version == 3);
    assert(unassigned.weapons.front().weaponSlot == 0);
    assert(game::FindFpsWeaponDefinitionForSlot(unassigned, 1) == nullptr);

    registry.weapons.front().weaponSlot = 7;
    assert(!game::ValidateFpsWeaponRegistry(registry, &error));
    assert(error.find("slot must be between 1 and 6") != std::string::npos);

    assert(game::FpsWeaponSlotFromKey(KEY_ONE) == 1);
    assert(game::FpsWeaponSlotFromKey(KEY_SIX) == 6);
    assert(game::FpsWeaponSlotFromKey(KEY_ZERO) == 0);
    assert(game::FpsWeaponSlotFromKey(KEY_KP_1) == 0);
}

void SettingsResolutionAndPersistence()
{
    game::FpsViewmodelPresentation defaults;
    defaults.position = {1,2,3}; defaults.rotationDegrees = {4,5,6};
    defaults.scale = 1; defaults.verticalFovDegrees = 65;
    game::FpsViewmodelPresentationOverride over;
    over.position = Vector3{20,-20,4}; over.scale = 0.001f; over.verticalFovDegrees = 150;
    const auto effective = game::ResolveFpsViewmodelPresentation(defaults, &over);
    assert(Near(effective.position.x, 10) && Near(effective.position.y, -10));
    assert(Near(effective.rotationDegrees.y, 5));
    assert(Near(effective.scale, 0.01f) && Near(effective.verticalFovDegrees, 120));
    const auto persisted = game::BuildFpsViewmodelOverride(defaults, effective);
    assert(persisted.position && persisted.scale && persisted.verticalFovDegrees && !persisted.rotationDegrees);
    assert(game::FpsViewmodelOverrideEmpty(game::BuildFpsViewmodelOverride(defaults, defaults)));

    game::FpsViewmodelHolsterTransition transitionDefaults;
    transitionDefaults.holsterDurationSeconds = 0.25f;
    transitionDefaults.unholsterDurationSeconds = 0.34f;
    transitionDefaults.hiddenTranslation = {0.45f, -1.75f, 0.0f};
    transitionDefaults.hiddenRotationDegrees = {13.0f, 0.0f, -10.0f};
    game::FpsViewmodelHolsterTransitionOverride transitionOverride;
    transitionOverride.holsterDurationSeconds = 0.01f;
    transitionOverride.unholsterDurationSeconds = 3.0f;
    transitionOverride.hiddenTranslation = Vector3{20.0f, -20.0f, 0.25f};
    const auto effectiveTransition =
            game::ResolveFpsViewmodelHolsterTransition(
                    transitionDefaults,
                    &transitionOverride);
    assert(Near(effectiveTransition.holsterDurationSeconds, 0.05f));
    assert(Near(effectiveTransition.unholsterDurationSeconds, 2.0f));
    assert(Near(effectiveTransition.hiddenTranslation.x, 10.0f));
    assert(Near(effectiveTransition.hiddenTranslation.y, -10.0f));
    assert(Near(effectiveTransition.hiddenTranslation.z, 0.25f));
    assert(Near(effectiveTransition.hiddenRotationDegrees.x, 13.0f));
    const auto persistedTransition =
            game::BuildFpsViewmodelHolsterTransitionOverride(
                    transitionDefaults,
                    effectiveTransition);
    assert(persistedTransition.holsterDurationSeconds
            && persistedTransition.unholsterDurationSeconds
            && persistedTransition.hiddenTranslation
            && !persistedTransition.hiddenRotationDegrees);
    assert(game::FpsViewmodelHolsterTransitionOverrideEmpty(
            game::BuildFpsViewmodelHolsterTransitionOverride(
                    transitionDefaults,
                    transitionDefaults)));

    game::FpsApplicationSettings settings; std::string error;
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"hdrBloom":{"enabled":true,"threshold":1.25,"softKnee":0.4,"intensity":0.35,"radius":1.5},"footsteps":{"defaultSet":"DirtRoad_Mono","volume":0.7,"landingImpactVolumeMultiplier":1.5},"playerSounds":{"events":{"jump":{"set":"Jump","volume":0.8},"land":{"set":"Land"},"wallImpact":{"set":"future/WallImpact","volume":0.6}}},"viewmodelOverrides":{"pistol":{"position":[1,2,3],"scale":2,"holsterTransition":{"holsterDurationSeconds":0.2,"unholsterDurationSeconds":0.4,"hiddenTranslation":[0.5,-2,0.1],"hiddenRotationDegrees":[15,1,-12]},"gripCorrection":{"translation":[0.1,0.2,0.3],"rotationDegrees":[10,20,30],"scale":1.25},"attachmentLighting":{"brightnessAdjustment":0.2,"metallicFactor":0.45,"roughnessFactor":0.8},"firing":{"shotIntervalSeconds":0.2,"recoilTranslationImpulse":[0,0,-0.04],"recoilRotationImpulseDegrees":[-4,0,0],"recoilRollVariationDegrees":0.5,"recoilSpringFrequencyHz":9,"recoilDampingRatio":0.9,"cameraRecoilEnabled":true,"cameraRecoilPitchKickDegrees":0.55,"cameraRecoilPitchVariationDegrees":0.09,"cameraRecoilYawVariationDegrees":0.16,"cameraRecoilRollVariationDegrees":0.08,"cameraRecoilSpringFrequencyHz":6,"cameraRecoilSpringDampingRatio":1,"cameraRecoilMaxPitchDegrees":1.5,"cameraRecoilMaxYawDegrees":0.7,"cameraRecoilMaxRollDegrees":0.25,"muzzlePosition":[0,0.04,0.11],"muzzleRotationDegrees":[1,2,3],"flashLifetimeSeconds":0.06,"flashSizeWorld":0.12,"flashRadianceStrength":14,"flashSizeVariation":0.1,"flashIrregularity":0.7,"flashForwardStretch":2.1,"flashMinimumLobeCount":4,"flashMaximumLobeCount":7,"flashRearSuppression":0.85,"flashEdgeSoftness":0.4,"muzzleLightIntensity":7,"muzzleLightRadiusWorld":3,"muzzleLightLifetimeSeconds":0.08}}}})",
            settings, &error));
    assert(settings.firstLevel == "hub");
    assert(settings.footsteps.defaultSet == "DirtRoad_Mono");
    assert(Near(settings.footsteps.volume, 0.7f));
    assert(Near(settings.hdrBloom.threshold,1.25f)
            && Near(settings.hdrBloom.intensity,0.35f));
    assert(settings.toneMapping.toneMapper
            == engine::ToneMappingOperator::KhronosPbrNeutral);
    assert(Near(settings.toneMapping.exposureCompensationEv, 0.0f));
    game::FpsApplicationSettings parsedToneMapping;
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"toneMapping":{"operator":"acesFilmicFitted","exposureCompensationEv":-1.25}})",
            parsedToneMapping,
            &error));
    assert(parsedToneMapping.toneMapping.toneMapper
            == engine::ToneMappingOperator::AcesFilmicFitted);
    assert(Near(
            parsedToneMapping.toneMapping.exposureCompensationEv,
            -1.25f));
    assert(Near(settings.footsteps.landingImpactVolumeMultiplier, 1.5f));
    assert(Near(settings.footsteps.noiseRadiusWorld, 6.0f));
    assert(Near(settings.footsteps.landingNoiseRadiusWorld, 12.0f));
    assert(settings.playerSounds.events.size() == 3);
    assert(settings.playerSounds.events[0].id == "jump");
    assert(settings.playerSounds.events[0].set == "Jump");
    assert(Near(settings.playerSounds.events[0].volume, 0.8f));
    assert(settings.playerSounds.events[1].id == "land");
    assert(Near(settings.playerSounds.events[1].volume, 1.0f));
    assert(settings.playerSounds.events[2].id == "wallImpact");
    assert(game::FindFpsViewmodelOverride(settings, "pistol") != nullptr);
    const auto* parsedTransition =
            game::FindFpsViewmodelHolsterTransitionOverride(
                    settings,
                    "pistol");
    assert(parsedTransition != nullptr
            && parsedTransition->holsterDurationSeconds
            && parsedTransition->unholsterDurationSeconds
            && parsedTransition->hiddenTranslation
            && parsedTransition->hiddenRotationDegrees);
    const auto* parsedGrip = game::FindFpsViewmodelGripCorrectionOverride(
            settings, "pistol");
    assert(parsedGrip != nullptr && parsedGrip->translation
            && parsedGrip->rotationDegrees && parsedGrip->scale);
    const auto* parsedLighting =
            game::FindFpsViewmodelAttachmentLightingOverride(
                    settings, "pistol");
    assert(parsedLighting != nullptr
            && parsedLighting->brightnessAdjustment
            && parsedLighting->metallicFactor
            && parsedLighting->roughnessFactor);
    const auto* parsedFiring = game::FindFpsWeaponFiringOverride(
            settings, "pistol");
    assert(parsedFiring != nullptr
            && parsedFiring->shotIntervalSeconds
            && parsedFiring->recoilTranslationImpulse
            && parsedFiring->cameraRecoilEnabled
            && *parsedFiring->cameraRecoilEnabled
            && parsedFiring->cameraRecoilPitchKickDegrees
            && parsedFiring->cameraRecoilSpringDampingRatio
            && parsedFiring->cameraRecoilMaxRollDegrees
            && parsedFiring->muzzlePosition
            && parsedFiring->flashSizeVariation
            && parsedFiring->flashRadianceStrength
            && parsedFiring->flashIrregularity
            && parsedFiring->flashForwardStretch
            && parsedFiring->flashMinimumLobeCount
            && parsedFiring->flashMaximumLobeCount
            && parsedFiring->flashRearSuppression
            && parsedFiring->flashEdgeSoftness
            && parsedFiring->muzzleLightLifetimeSeconds);
    game::FpsViewmodelGripCorrection gripDefaults;
    gripDefaults.translation = {0.02f, 0.04f, -0.15f};
    gripDefaults.rotationDegrees = {3.0f, 175.0f, 88.0f};
    game::FpsViewmodelGripCorrectionOverride gripOverride;
    gripOverride.translation = Vector3{2.0f, -2.0f, 0.25f};
    gripOverride.scale = 0.001f;
    const auto effectiveGrip = game::ResolveFpsViewmodelGripCorrection(
            gripDefaults, &gripOverride);
    assert(Near(effectiveGrip.translation.x, 1.0f));
    assert(Near(effectiveGrip.translation.y, -1.0f));
    assert(Near(effectiveGrip.translation.z, 0.25f));
    assert(Near(effectiveGrip.rotationDegrees.y, 175.0f));
    assert(Near(effectiveGrip.scale, 0.01f));
    const auto persistedGrip = game::BuildFpsViewmodelGripCorrectionOverride(
            gripDefaults, effectiveGrip);
    assert(persistedGrip.translation && persistedGrip.scale
            && !persistedGrip.rotationDegrees);
    assert(game::FpsViewmodelGripCorrectionOverrideEmpty(
            game::BuildFpsViewmodelGripCorrectionOverride(
                    gripDefaults, gripDefaults)));
    game::FpsViewmodelAttachmentLighting lightingDefaults;
    lightingDefaults.brightnessAdjustment = 0.0f;
    lightingDefaults.materialOverride.enabled = true;
    lightingDefaults.materialOverride.metallicFactor = 0.35f;
    lightingDefaults.materialOverride.roughnessFactor = 1.0f;
    lightingDefaults.materialOverride.useMetallicRoughnessTexture = true;
    game::FpsViewmodelAttachmentLightingOverride lightingOverride;
    lightingOverride.brightnessAdjustment = 2.0f;
    lightingOverride.metallicFactor = -1.0f;
    lightingOverride.roughnessFactor = 0.0f;
    const auto effectiveLighting =
            game::ResolveFpsViewmodelAttachmentLighting(
                    lightingDefaults, &lightingOverride);
    assert(Near(effectiveLighting.brightnessAdjustment, 1.0f));
    assert(Near(effectiveLighting.materialOverride.metallicFactor, 0.0f));
    assert(Near(effectiveLighting.materialOverride.roughnessFactor, 0.045f));
    assert(effectiveLighting.materialOverride.enabled);
    assert(effectiveLighting.materialOverride.useMetallicRoughnessTexture);
    const auto persistedLighting =
            game::BuildFpsViewmodelAttachmentLightingOverride(
                    lightingDefaults, effectiveLighting);
    assert(persistedLighting.brightnessAdjustment
            && persistedLighting.metallicFactor
            && persistedLighting.roughnessFactor);
    assert(game::FpsViewmodelAttachmentLightingOverrideEmpty(
            game::BuildFpsViewmodelAttachmentLightingOverride(
                    lightingDefaults, lightingDefaults)));
    game::SetFpsViewmodelOverride(settings, "pistol", persisted);
    game::SetFpsViewmodelHolsterTransitionOverride(
            settings, "pistol", persistedTransition);
    game::SetFpsViewmodelGripCorrectionOverride(
            settings, "pistol", persistedGrip);
    game::SetFpsViewmodelAttachmentLightingOverride(
            settings, "pistol", persistedLighting);
    game::FpsWeaponFiringDefinition firingDefaults;
    game::FpsWeaponFiringDefinition effectiveFiring = firingDefaults;
    effectiveFiring.shotIntervalSeconds = 0.21f;
    effectiveFiring.recoil.translationImpulse.z = -0.05f;
    effectiveFiring.cameraRecoil.enabled = true;
    effectiveFiring.cameraRecoil.pitchKickDegrees = 0.45f;
    effectiveFiring.cameraRecoil.maxPitchDegrees = 1.4f;
    effectiveFiring.muzzleSocket.position.y = 0.05f;
    effectiveFiring.muzzleFlash.sizeVariation = 0.2f;
    effectiveFiring.muzzleFlash.radianceStrength = 18.0f;
    effectiveFiring.muzzleFlash.irregularity = 0.8f;
    effectiveFiring.muzzleFlash.forwardStretch = 2.2f;
    effectiveFiring.muzzleFlash.minimumLobeCount = 4;
    effectiveFiring.muzzleFlash.maximumLobeCount = 7;
    effectiveFiring.muzzleFlash.rearSuppression = 0.95f;
    effectiveFiring.muzzleFlash.edgeSoftness = 0.6f;
    const game::FpsWeaponFiringOverride firingOverride =
            game::BuildFpsWeaponFiringOverride(
                    firingDefaults, effectiveFiring);
    assert(!game::FpsWeaponFiringOverrideEmpty(firingOverride));
    game::SetFpsWeaponFiringOverride(settings, "pistol", firingOverride);
    settings.firstLevel = "test4";
    settings.consoleEnabled = false;
    settings.hdrBloom={true,2.0f,0.25f,0.5f,2.0f};
    settings.graphics.renderScale = 1.25f;
    settings.graphics.fxaa = false;
    settings.graphics.shadowQuality = game::FpsShadowQuality::Medium;
    settings.graphics.maxDynamicLights = 17;
    settings.graphics.maxShadowLightUpdatesPerFrame = 7;
    settings.graphics.dynamicLightFadeInSeconds = 0.35f;
    settings.graphics.depthPrepass = false;
    settings.graphics.showFpsCounter = true;
    settings.graphics.performanceOverlay = true;
    settings.graphics.vsync = false;
    settings.graphics.horizontalFovDegrees = 96;
    settings.toneMapping.toneMapper =
            engine::ToneMappingOperator::AcesFilmicFitted;
    settings.toneMapping.exposureCompensationEv = 1.5f;
    settings.playerStamina.maximum = 120.0f;
    settings.playerStamina.sprintDrainPerSecond = 18.0f;
    settings.playerStamina.jumpCost = 24.0f;
    settings.playerStamina.regenerationPerSecond = 14.0f;
    settings.playerStamina.exhaustedRecoveryRatio = 0.25f;
    settings.playerStamina.windedCamera.enabled = false;
    settings.playerStamina.windedCamera.startThresholdRatio = 0.5f;
    settings.playerStamina.windedCamera.verticalAmplitudeWorld = 0.02f;
    settings.playerStamina.windedCamera.pitchAmplitudeDegrees = 0.8f;
    settings.playerStamina.windedCamera.frequencyHz = 0.6f;
    settings.playerStamina.windedCamera.responseSeconds = 0.4f;
    settings.playerStamina.breathingAudio.thresholdRatio = 0.3f;
    settings.playerStamina.breathingAudio.volume = 0.6f;
    settings.playerStamina.breathingAudio.fadeOutSeconds = 3.0f;
    settings.playerHealth.lowHealthVisual.enabled = true;
    settings.playerHealth.lowHealthVisual.thresholdRatio = 0.45f;
    settings.playerHealth.lowHealthVisual.vignetteColor = Color{55, 4, 9, 255};
    settings.playerHealth.lowHealthVisual.vignetteInnerRadius = 0.42f;
    settings.playerHealth.lowHealthVisual.vignetteOuterRadius = 1.15f;
    settings.playerHealth.lowHealthVisual.maximumVignetteOpacity = 0.72f;
    settings.playerHealth.lowHealthVisual.maximumDesaturation = 0.31f;
    settings.playerHealth.heartbeatAudio.enabled = false;
    settings.playerHealth.heartbeatAudio.startThresholdRatio = 0.55f;
    settings.playerHealth.heartbeatAudio.fullEffectRatio = 0.12f;
    settings.playerHealth.heartbeatAudio.maximumVolume = 0.8f;
    settings.playerHealth.heartbeatAudio.startPitch = 0.9f;
    settings.playerHealth.heartbeatAudio.maximumPitch = 1.6f;
    settings.playerHealth.heartbeatAudio.responseSeconds = 0.4f;
    settings.playerHealth.lowHealthMovement.enabled = false;
    settings.playerHealth.lowHealthMovement.startThresholdRatio = 0.6f;
    settings.playerHealth.lowHealthMovement.minimumSpeedScale = 0.3f;
    settings.playerHealth.lowHealthMovement.minimumSprintSpeedScale = 0.8f;
    settings.playerHealth.lowHealthCamera.enabled = false;
    settings.playerHealth.lowHealthCamera.startThresholdRatio = 0.48f;
    settings.playerHealth.lowHealthCamera.fullEffectRatio = 0.08f;
    settings.playerHealth.lowHealthCamera.lateralAmplitudeWorld = 0.03f;
    settings.playerHealth.lowHealthCamera.verticalAmplitudeWorld = 0.02f;
    settings.playerHealth.lowHealthCamera.pitchAmplitudeDegrees = 1.1f;
    settings.playerHealth.lowHealthCamera.yawAmplitudeDegrees = 0.9f;
    settings.playerHealth.lowHealthCamera.rollAmplitudeDegrees = 1.8f;
    settings.playerHealth.lowHealthCamera.frequencyHz = 0.7f;
    settings.playerHealth.lowHealthCamera.responseSeconds = 0.5f;
    const std::filesystem::path path = std::filesystem::temp_directory_path()/"fps_viewmodel_settings_test.json";
    assert(game::SaveFpsApplicationSettings(path.string(), settings, &error));
    std::ifstream savedSettingsInput(path);
    const std::string savedSettings{
            std::istreambuf_iterator<char>(savedSettingsInput),
            std::istreambuf_iterator<char>()};
    game::FpsApplicationSettings loaded;
    assert(game::LoadFpsApplicationSettings(path.string(), loaded, &error));
    assert(loaded.firstLevel == "test4");
    assert(!loaded.consoleEnabled);
    assert(Near(loaded.hdrBloom.threshold,2.0f)
            && Near(loaded.hdrBloom.radius,2.0f));
    assert(Near(loaded.graphics.renderScale, 1.25f));
    assert(!loaded.graphics.fxaa);
    assert(loaded.graphics.shadowQuality == game::FpsShadowQuality::Medium);
    assert(loaded.graphics.maxDynamicLights == 17);
    assert(loaded.graphics.maxShadowLightUpdatesPerFrame == 7);
    assert(Near(loaded.graphics.dynamicLightFadeInSeconds, 0.35f));
    assert(!loaded.graphics.depthPrepass);
    assert(loaded.graphics.showFpsCounter);
    assert(loaded.graphics.performanceOverlay);
    assert(!loaded.graphics.vsync);
    assert(loaded.graphics.horizontalFovDegrees == 96);
    assert(loaded.toneMapping.toneMapper
            == engine::ToneMappingOperator::AcesFilmicFitted);
    assert(Near(loaded.toneMapping.exposureCompensationEv, 1.5f));
    assert(Near(loaded.playerStamina.maximum, 120.0f));
    assert(Near(loaded.playerStamina.sprintDrainPerSecond, 18.0f));
    assert(Near(loaded.playerStamina.jumpCost, 24.0f));
    assert(Near(loaded.playerStamina.regenerationPerSecond, 14.0f));
    assert(Near(loaded.playerStamina.exhaustedRecoveryRatio, 0.25f));
    assert(!loaded.playerStamina.windedCamera.enabled);
    assert(Near(loaded.playerStamina.windedCamera.startThresholdRatio, 0.5f));
    assert(Near(loaded.playerStamina.windedCamera.verticalAmplitudeWorld, 0.02f));
    assert(Near(loaded.playerStamina.windedCamera.pitchAmplitudeDegrees, 0.8f));
    assert(Near(loaded.playerStamina.windedCamera.frequencyHz, 0.6f));
    assert(Near(loaded.playerStamina.windedCamera.responseSeconds, 0.4f));
    assert(Near(loaded.playerStamina.breathingAudio.thresholdRatio, 0.3f));
    assert(Near(loaded.playerStamina.breathingAudio.volume, 0.6f));
    assert(Near(loaded.playerStamina.breathingAudio.fadeOutSeconds, 3.0f));
    assert(loaded.playerHealth.lowHealthVisual.enabled);
    assert(Near(loaded.playerHealth.lowHealthVisual.thresholdRatio, 0.45f));
    assert(loaded.playerHealth.lowHealthVisual.vignetteColor.r == 55);
    assert(loaded.playerHealth.lowHealthVisual.vignetteColor.g == 4);
    assert(loaded.playerHealth.lowHealthVisual.vignetteColor.b == 9);
    assert(Near(loaded.playerHealth.lowHealthVisual.vignetteInnerRadius, 0.42f));
    assert(Near(loaded.playerHealth.lowHealthVisual.vignetteOuterRadius, 1.15f));
    assert(Near(loaded.playerHealth.lowHealthVisual.maximumVignetteOpacity, 0.72f));
    assert(Near(loaded.playerHealth.lowHealthVisual.maximumDesaturation, 0.31f));
    assert(!loaded.playerHealth.heartbeatAudio.enabled);
    assert(Near(loaded.playerHealth.heartbeatAudio.startThresholdRatio, 0.55f));
    assert(Near(loaded.playerHealth.heartbeatAudio.fullEffectRatio, 0.12f));
    assert(Near(loaded.playerHealth.heartbeatAudio.maximumVolume, 0.8f));
    assert(Near(loaded.playerHealth.heartbeatAudio.startPitch, 0.9f));
    assert(Near(loaded.playerHealth.heartbeatAudio.maximumPitch, 1.6f));
    assert(Near(loaded.playerHealth.heartbeatAudio.responseSeconds, 0.4f));
    assert(!loaded.playerHealth.lowHealthMovement.enabled);
    assert(Near(loaded.playerHealth.lowHealthMovement.startThresholdRatio, 0.6f));
    assert(Near(loaded.playerHealth.lowHealthMovement.minimumSpeedScale, 0.3f));
    assert(Near(
            loaded.playerHealth.lowHealthMovement.minimumSprintSpeedScale,
            0.8f));
    assert(!loaded.playerHealth.lowHealthCamera.enabled);
    assert(Near(loaded.playerHealth.lowHealthCamera.startThresholdRatio, 0.48f));
    assert(Near(loaded.playerHealth.lowHealthCamera.fullEffectRatio, 0.08f));
    assert(Near(loaded.playerHealth.lowHealthCamera.lateralAmplitudeWorld, 0.03f));
    assert(Near(loaded.playerHealth.lowHealthCamera.verticalAmplitudeWorld, 0.02f));
    assert(Near(loaded.playerHealth.lowHealthCamera.pitchAmplitudeDegrees, 1.1f));
    assert(Near(loaded.playerHealth.lowHealthCamera.yawAmplitudeDegrees, 0.9f));
    assert(Near(loaded.playerHealth.lowHealthCamera.rollAmplitudeDegrees, 1.8f));
    assert(Near(loaded.playerHealth.lowHealthCamera.frequencyHz, 0.7f));
    assert(Near(loaded.playerHealth.lowHealthCamera.responseSeconds, 0.5f));
    assert(loaded.footsteps.defaultSet == "DirtRoad_Mono");
    assert(Near(loaded.footsteps.volume, 0.7f));
    assert(Near(loaded.footsteps.landingImpactVolumeMultiplier, 1.5f));
    assert(Near(loaded.footsteps.noiseRadiusWorld, 6.0f));
    assert(Near(loaded.footsteps.landingNoiseRadiusWorld, 12.0f));
    assert(loaded.playerSounds.events.size() == 3);
    assert(loaded.playerSounds.events[2].set == "future/WallImpact");
    assert(Near(loaded.playerSounds.events[2].volume, 0.6f));
    assert(game::FindFpsViewmodelOverride(loaded, "pistol") != nullptr);
    assert(game::FindFpsViewmodelHolsterTransitionOverride(
            loaded, "pistol") != nullptr);
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            != nullptr);
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") != nullptr);
    const auto* loadedFiring = game::FindFpsWeaponFiringOverride(
            loaded, "pistol");
    assert(loadedFiring != nullptr
            && loadedFiring->cameraRecoilEnabled
            && *loadedFiring->cameraRecoilEnabled
            && loadedFiring->cameraRecoilPitchKickDegrees
            && Near(*loadedFiring->cameraRecoilPitchKickDegrees, 0.45f)
            && loadedFiring->cameraRecoilMaxPitchDegrees
            && loadedFiring->flashEdgeSoftness
            && loadedFiring->flashSizeVariation
            && loadedFiring->flashRadianceStrength
            && loadedFiring->flashIrregularity
            && loadedFiring->flashForwardStretch
            && loadedFiring->flashMinimumLobeCount
            && loadedFiring->flashMaximumLobeCount
            && loadedFiring->flashRearSuppression
            && *loadedFiring->flashMinimumLobeCount == 4
            && *loadedFiring->flashMaximumLobeCount == 7
            && Near(*loadedFiring->flashEdgeSoftness, 0.6f));
    assert(Near(*loadedFiring->flashRadianceStrength,18.0f));
    std::error_code ignored; std::filesystem::remove(path, ignored);
    game::ClearFpsViewmodelOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelOverride(loaded, "pistol") == nullptr);
    assert(game::FindFpsViewmodelHolsterTransitionOverride(
            loaded, "pistol") != nullptr);
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            != nullptr);
    game::ClearFpsViewmodelHolsterTransitionOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelHolsterTransitionOverride(
            loaded, "pistol") == nullptr);
    game::ClearFpsViewmodelGripCorrectionOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            == nullptr);
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") != nullptr);
    game::ClearFpsViewmodelAttachmentLightingOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") == nullptr);
    assert(game::FindFpsWeaponFiringOverride(loaded, "pistol") != nullptr);
    game::ClearFpsWeaponFiringOverride(loaded, "pistol");
    assert(game::FindFpsWeaponFiringOverride(loaded, "pistol") == nullptr);
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"scale":"bad"}}})", loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"holsterTransition":{"holsterDurationSeconds":"bad"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"gripCorrection":{"scale":"bad"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"attachmentLighting":{"metallicFactor":"bad"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"firstLevel":"../hub"})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"firstLevel":5})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"footsteps":{"defaultSet":"../Tile_Mono"}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"footsteps":{"volume":1.1}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"footsteps":{"volume":"loud"}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"footsteps":{"landingImpactVolumeMultiplier":-0.1}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSounds":{"events":[]}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSounds":{"events":{"../jump":{"set":"Jump"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSounds":{"events":{"jump":{"set":"../Jump"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSounds":{"events":{"jump":{"set":"Jump","volume":1.1}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerSounds":{"events":{"jump":{"volume":0.5}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"maximum":0}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"maximum":10,"jumpCost":11}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"exhaustedRecoveryRatio":1.1}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"windedCamera":{"enabled":"yes"}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"windedCamera":{"responseSeconds":0}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"breathingAudio":{"thresholdRatio":-0.1}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerStamina":{"breathingAudio":{"fadeOutSeconds":0}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"enabled":"yes"}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"thresholdRatio":0}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"vignetteColor":{"r":40,"g":3,"b":7}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"vignetteInnerRadius":0.8,"vignetteOuterRadius":0.7}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"maximumVignetteOpacity":1.1}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthVisual":{"maximumDesaturation":-0.1}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"heartbeatAudio":{"enabled":"yes"}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"heartbeatAudio":{"startThresholdRatio":0.5,"fullEffectRatio":0.5}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"heartbeatAudio":{"maximumPitch":0.5}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthMovement":{"minimumSpeedScale":1.1}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthMovement":{"minimumSprintSpeedScale":1.1}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthCamera":{"startThresholdRatio":0.5,"fullEffectRatio":0.6}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerHealth":{"lowHealthCamera":{"frequencyHz":21.0}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"hdrBloom":{"threshold":-1}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"toneMapping":false})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"toneMapping":{"operator":"aces"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"toneMapping":{"exposureCompensationEv":8.01}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"renderScale":2.1}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"renderScale":"fast"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"shadowQuality":false}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"maxDynamicLights":33}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"maxDynamicLights":2.5}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"maxShadowLightUpdatesPerFrame":33}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"maxShadowLightUpdatesPerFrame":2.5}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"dynamicLightFadeInSeconds":-0.01}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"dynamicLightFadeInSeconds":2.01}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"dynamicLightFadeInSeconds":"slow"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"depthPrepass":"yes"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"showFpsCounter":"yes"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"vsync":"yes"}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"horizontalFovDegrees":69}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"horizontalFovDegrees":121}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"graphics":{"horizontalFovDegrees":90.5}})",loaded,&error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"consoleEnabled":"yes"})",loaded,&error));
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1})",
            loaded,
            &error));
    assert(loaded.consoleEnabled);
    assert(loaded.graphics.maxDynamicLights == game::DefaultFpsDynamicLights);
    assert(loaded.graphics.maxShadowLightUpdatesPerFrame
            == game::DefaultFpsShadowLightUpdatesPerFrame);
    assert(Near(
            loaded.graphics.dynamicLightFadeInSeconds,
            game::DefaultFpsDynamicLightFadeInSeconds));
    assert(!loaded.graphics.depthPrepass);
    assert(!loaded.graphics.showFpsCounter);
    assert(loaded.toneMapping.toneMapper
            == engine::ToneMappingOperator::KhronosPbrNeutral);
    assert(Near(loaded.toneMapping.exposureCompensationEv, 0.0f));
    assert(loaded.footsteps.defaultSet == "Tile_Mono");
    assert(Near(loaded.footsteps.volume, 0.65f));
    assert(Near(loaded.footsteps.landingImpactVolumeMultiplier, 1.35f));
    assert(Near(loaded.footsteps.noiseRadiusWorld, 6.0f));
    assert(Near(loaded.footsteps.landingNoiseRadiusWorld, 12.0f));
    assert(loaded.playerSounds.events.size() == 3);
    assert(loaded.playerSounds.events[0].id == "jump");
    assert(loaded.playerSounds.events[0].set == "Jump");
    assert(loaded.playerSounds.events[1].id == "land");
    assert(loaded.playerSounds.events[2].id == "pain");
    assert(loaded.playerSounds.events[2].set == "human_pain");
    assert(Near(loaded.playerSounds.events[2].volume, 1.0f));
    assert(Near(loaded.playerStamina.maximum, 100.0f));
    assert(Near(loaded.playerStamina.sprintDrainPerSecond, 20.0f));
    assert(Near(loaded.playerStamina.jumpCost, 20.0f));
    assert(Near(loaded.playerStamina.regenerationPerSecond, 12.5f));
    assert(Near(loaded.playerStamina.exhaustedRecoveryRatio, 0.20f));
    assert(loaded.playerStamina.windedCamera.enabled);
    assert(Near(loaded.playerStamina.windedCamera.startThresholdRatio, 0.40f));
    assert(Near(loaded.playerStamina.breathingAudio.thresholdRatio, 0.20f));
    assert(Near(loaded.playerStamina.breathingAudio.fadeOutSeconds, 2.0f));
    assert(loaded.playerHealth.lowHealthVisual.enabled);
    assert(Near(loaded.playerHealth.lowHealthVisual.thresholdRatio, 0.50f));
    assert(loaded.playerHealth.lowHealthVisual.vignetteColor.r == 40);
    assert(Near(loaded.playerHealth.lowHealthVisual.maximumVignetteOpacity, 0.65f));
    assert(Near(loaded.playerHealth.lowHealthVisual.maximumDesaturation, 0.22f));
    assert(loaded.playerHealth.heartbeatAudio.enabled);
    assert(Near(loaded.playerHealth.heartbeatAudio.startThresholdRatio, 0.50f));
    assert(Near(loaded.playerHealth.heartbeatAudio.fullEffectRatio, 0.10f));
    assert(Near(loaded.playerHealth.heartbeatAudio.maximumVolume, 1.0f));
    assert(Near(loaded.playerHealth.heartbeatAudio.maximumPitch, 1.5f));
    assert(loaded.playerHealth.lowHealthMovement.enabled);
    assert(Near(loaded.playerHealth.lowHealthMovement.startThresholdRatio, 0.50f));
    assert(Near(loaded.playerHealth.lowHealthMovement.minimumSpeedScale, 0.20f));
    assert(Near(
            loaded.playerHealth.lowHealthMovement.minimumSprintSpeedScale,
            0.20f));
    assert(loaded.playerHealth.lowHealthCamera.enabled);
    assert(Near(loaded.playerHealth.lowHealthCamera.startThresholdRatio, 0.50f));
    assert(Near(loaded.playerHealth.lowHealthCamera.fullEffectRatio, 0.10f));
    assert(Near(loaded.playerHealth.lowHealthCamera.rollAmplitudeDegrees, 1.50f));
    assert(loaded.graphics.vsync);
    assert(loaded.graphics.horizontalFovDegrees
            == game::DefaultFpsHorizontalFovDegrees);

    game::FpsGraphicsSettings normalized;
    normalized.horizontalFovDegrees = 20;
    assert(game::NormalizeFpsGraphicsSettings(normalized).horizontalFovDegrees
            == game::MinFpsHorizontalFovDegrees);
    normalized.horizontalFovDegrees = 200;
    assert(game::NormalizeFpsGraphicsSettings(normalized).horizontalFovDegrees
            == game::MaxFpsHorizontalFovDegrees);
    normalized.dynamicLightFadeInSeconds =
            std::numeric_limits<float>::quiet_NaN();
    assert(Near(
            game::NormalizeFpsGraphicsSettings(normalized)
                    .dynamicLightFadeInSeconds,
            game::DefaultFpsDynamicLightFadeInSeconds));
    normalized.dynamicLightFadeInSeconds = -1.0f;
    assert(Near(
            game::NormalizeFpsGraphicsSettings(normalized)
                    .dynamicLightFadeInSeconds,
            game::MinFpsDynamicLightFadeInSeconds));
    normalized.dynamicLightFadeInSeconds = 3.0f;
    assert(Near(
            game::NormalizeFpsGraphicsSettings(normalized)
                    .dynamicLightFadeInSeconds,
            game::MaxFpsDynamicLightFadeInSeconds));

    const float defaultVerticalFov = game::FpsVerticalFovDegrees(
            game::DefaultFpsHorizontalFovDegrees, 16.0f / 9.0f);
    assert(Near(defaultVerticalFov, 75.5f, 0.1f));
    assert(game::FpsVerticalFovDegrees(
                    game::MinFpsHorizontalFovDegrees, 16.0f / 9.0f)
            < defaultVerticalFov);
    assert(game::FpsVerticalFovDegrees(
                    game::MaxFpsHorizontalFovDegrees, 16.0f / 9.0f)
            > defaultVerticalFov);
}

void PreviewSettingsOverrideDeltaCoverage()
{
    const auto expectPresentationDelta = [](auto edit) {
        const game::FpsViewmodelPresentation current;
        game::FpsViewmodelPresentation draft = current;
        edit(draft);
        assert(!game::FpsViewmodelOverrideEmpty(
                game::BuildFpsViewmodelOverride(current, draft)));
    };
    expectPresentationDelta([](auto& value) { value.position.x = 0.25f; });
    expectPresentationDelta([](auto& value) { value.position.y = 0.25f; });
    expectPresentationDelta([](auto& value) { value.position.z = 0.25f; });
    expectPresentationDelta([](auto& value) { value.rotationDegrees.x = 1.0f; });
    expectPresentationDelta([](auto& value) { value.rotationDegrees.y = 1.0f; });
    expectPresentationDelta([](auto& value) { value.rotationDegrees.z = 1.0f; });
    expectPresentationDelta([](auto& value) { value.scale = 1.25f; });
    expectPresentationDelta([](auto& value) { value.verticalFovDegrees = 75.0f; });

    const auto expectHolsterDelta = [](auto edit) {
        const game::FpsViewmodelHolsterTransition current;
        game::FpsViewmodelHolsterTransition draft = current;
        edit(draft);
        assert(!game::FpsViewmodelHolsterTransitionOverrideEmpty(
                game::BuildFpsViewmodelHolsterTransitionOverride(current, draft)));
    };
    expectHolsterDelta([](auto& value) { value.holsterDurationSeconds = 0.5f; });
    expectHolsterDelta([](auto& value) { value.unholsterDurationSeconds = 0.5f; });
    expectHolsterDelta([](auto& value) { value.hiddenTranslation.x = 0.25f; });
    expectHolsterDelta([](auto& value) { value.hiddenTranslation.y = 0.25f; });
    expectHolsterDelta([](auto& value) { value.hiddenTranslation.z = 0.25f; });
    expectHolsterDelta([](auto& value) { value.hiddenRotationDegrees.x = 1.0f; });
    expectHolsterDelta([](auto& value) { value.hiddenRotationDegrees.y = 1.0f; });
    expectHolsterDelta([](auto& value) { value.hiddenRotationDegrees.z = 1.0f; });

    const auto expectGripDelta = [](auto edit) {
        const game::FpsViewmodelGripCorrection current;
        game::FpsViewmodelGripCorrection draft = current;
        edit(draft);
        assert(!game::FpsViewmodelGripCorrectionOverrideEmpty(
                game::BuildFpsViewmodelGripCorrectionOverride(current, draft)));
    };
    expectGripDelta([](auto& value) { value.translation.x = 0.25f; });
    expectGripDelta([](auto& value) { value.translation.y = 0.25f; });
    expectGripDelta([](auto& value) { value.translation.z = 0.25f; });
    expectGripDelta([](auto& value) { value.rotationDegrees.x = 1.0f; });
    expectGripDelta([](auto& value) { value.rotationDegrees.y = 1.0f; });
    expectGripDelta([](auto& value) { value.rotationDegrees.z = 1.0f; });
    expectGripDelta([](auto& value) { value.scale = 1.25f; });

    const auto expectLightingDelta = [](auto edit) {
        const game::FpsViewmodelAttachmentLighting current;
        game::FpsViewmodelAttachmentLighting draft = current;
        edit(draft);
        assert(!game::FpsViewmodelAttachmentLightingOverrideEmpty(
                game::BuildFpsViewmodelAttachmentLightingOverride(current, draft)));
    };
    expectLightingDelta([](auto& value) { value.brightnessAdjustment = 0.25f; });
    expectLightingDelta([](auto& value) {
        value.materialOverride.metallicFactor = 0.5f;
    });
    expectLightingDelta([](auto& value) {
        value.materialOverride.roughnessFactor = 0.5f;
    });

    const auto expectFiringDelta = [](auto edit) {
        const game::FpsWeaponFiringDefinition current;
        game::FpsWeaponFiringDefinition draft = current;
        edit(draft);
        assert(!game::FpsWeaponFiringOverrideEmpty(
                game::BuildFpsWeaponFiringOverride(current, draft)));
    };
    expectFiringDelta([](auto& value) { value.shotIntervalSeconds = 0.25f; });
    expectFiringDelta([](auto& value) { value.recoil.translationImpulse.x = 0.1f; });
    expectFiringDelta([](auto& value) { value.recoil.translationImpulse.y = 0.1f; });
    expectFiringDelta([](auto& value) { value.recoil.translationImpulse.z = -0.1f; });
    expectFiringDelta([](auto& value) { value.recoil.rotationImpulseDegrees.x = -4.0f; });
    expectFiringDelta([](auto& value) { value.recoil.rotationImpulseDegrees.y = 1.0f; });
    expectFiringDelta([](auto& value) { value.recoil.rotationImpulseDegrees.z = 1.0f; });
    expectFiringDelta([](auto& value) { value.recoil.rollVariationDegrees = 0.75f; });
    expectFiringDelta([](auto& value) { value.recoil.springFrequencyHz = 9.0f; });
    expectFiringDelta([](auto& value) { value.recoil.dampingRatio = 1.0f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.enabled = true; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.pitchKickDegrees = 0.5f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.pitchVariationDegrees = 0.1f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.yawVariationDegrees = 0.1f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.rollVariationDegrees = 0.1f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.springFrequencyHz = 6.0f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.springDampingRatio = 1.1f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.maxPitchDegrees = 1.0f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.maxYawDegrees = 1.0f; });
    expectFiringDelta([](auto& value) { value.cameraRecoil.maxRollDegrees = 1.0f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.position.x = 0.1f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.position.y = 0.1f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.position.z = 0.2f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.rotationDegrees.x = 1.0f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.rotationDegrees.y = 1.0f; });
    expectFiringDelta([](auto& value) { value.muzzleSocket.rotationDegrees.z = 1.0f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.lifetimeSeconds = 0.05f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.sizeWorld = 0.15f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.radianceStrength = 12.0f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.sizeVariation = 0.2f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.irregularity = 0.8f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.forwardStretch = 2.0f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.minimumLobeCount = 4; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.maximumLobeCount = 9; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.rearSuppression = 0.8f; });
    expectFiringDelta([](auto& value) { value.muzzleFlash.edgeSoftness = 0.5f; });
    expectFiringDelta([](auto& value) { value.muzzleLight.intensity = 8.0f; });
    expectFiringDelta([](auto& value) { value.muzzleLight.radiusWorld = 3.0f; });
    expectFiringDelta([](auto& value) { value.muzzleLight.lifetimeSeconds = 0.1f; });

    const game::FpsWeaponFiringDefinition defaults;
    game::FpsWeaponFiringDefinition radianceOnly = defaults;
    radianceOnly.muzzleFlash.radianceStrength = 12.0f;
    const game::FpsWeaponFiringOverride radianceOverride =
            game::BuildFpsWeaponFiringOverride(defaults, radianceOnly);
    assert(radianceOverride.flashRadianceStrength
            && Near(*radianceOverride.flashRadianceStrength, 12.0f));
    assert(!radianceOverride.flashLifetimeSeconds);

    game::FpsApplicationSettings settings;
    game::SetFpsWeaponFiringOverride(settings, "pistol", radianceOverride);
    const std::filesystem::path path = std::filesystem::temp_directory_path()
            / "fps_preview_radiance_only_settings_test.json";
    std::string error;
    assert(game::SaveFpsApplicationSettings(path.string(), settings, &error));
    game::FpsApplicationSettings loaded;
    assert(game::LoadFpsApplicationSettings(path.string(), loaded, &error));
    const game::FpsWeaponFiringOverride* loadedOverride =
            game::FindFpsWeaponFiringOverride(loaded, "pistol");
    assert(loadedOverride != nullptr
            && loadedOverride->flashRadianceStrength
            && Near(*loadedOverride->flashRadianceStrength, 12.0f)
            && !loadedOverride->flashLifetimeSeconds);
    const game::FpsWeaponFiringDefinition resolved =
            game::ResolveFpsWeaponFiringDefinition(defaults, loadedOverride);
    assert(Near(resolved.muzzleFlash.radianceStrength, 12.0f));
    assert(Near(
            resolved.muzzleFlash.lifetimeSeconds,
            defaults.muzzleFlash.lifetimeSeconds));
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    assert(!game::FpsWeaponFiringOverrideEmpty(
            game::BuildFpsWeaponFiringOverride(radianceOnly, defaults)));
    assert(game::FpsWeaponFiringOverrideEmpty(
            game::BuildFpsWeaponFiringOverride(defaults, defaults)));
}

void CameraMath()
{
    Camera3D camera{}; camera.position={10,20,30}; camera.target={10,20,31}; camera.up={0,1,0};
    Vector3 world = game::TransformFpsViewmodelLocalPosition(camera, {1,2,3});
    assert(Near(world.x,9) && Near(world.y,22) && Near(world.z,33));
    camera.target={11,20,30};
    world = game::TransformFpsViewmodelLocalPosition(camera, {0,0,2});
    assert(Near(world.x,12) && Near(world.y,20) && Near(world.z,30));
    camera.target={10,21,31};
    const auto basis = game::BuildFpsViewmodelCameraBasis(camera);
    assert(Near(Vector3Length(basis.forward),1) && basis.forward.y > 0.7f);
    assert(Near(Vector3DotProduct(basis.right,basis.forward),0));
    game::FpsViewmodelPresentation presentation;
    presentation.position={0,0,2}; presentation.rotationDegrees={10,20,30};
    const Vector3 origin = Vector3Transform(Vector3{}, game::BuildFpsViewmodelTransform(camera,presentation));
    const Vector3 expected = game::TransformFpsViewmodelLocalPosition(camera,presentation.position);
    assert(Near(origin.x,expected.x,0.001f) && Near(origin.y,expected.y,0.001f) && Near(origin.z,expected.z,0.001f));
}

void HolsterTransitionStateAndMath()
{
    game::FpsViewmodelRuntimeState state;
    assert(state.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(Near(state.equipProgress, 0.0f));
    state.equipState = game::FpsViewmodelEquipState::Equipped;
    state.equipProgress = 1.0f;
    state.activeWeaponId = "pistol";
    state.loadState = game::FpsViewmodelLoadState::Pending;
    state.holsterTransition.holsterDurationSeconds = 0.25f;
    state.holsterTransition.unholsterDurationSeconds = 0.34f;
    state.holsterTransition.hiddenTranslation = {0.45f, -1.75f, 0.0f};
    state.holsterTransition.hiddenRotationDegrees = {13.0f, 0.0f, -10.0f};

    assert(state.equipState == game::FpsViewmodelEquipState::Equipped);
    assert(Near(state.equipProgress, 1.0f));
    assert(game::IsFpsViewmodelReadyForUse(state));
    assert(game::IsFpsViewmodelPresentationVisible(state));
    assert(!game::ToggleFpsViewmodelHolster(state, false, false));
    assert(!game::ToggleFpsViewmodelHolster(state, true, true));

    assert(game::ToggleFpsViewmodelHolster(state, true, false));
    assert(state.equipState == game::FpsViewmodelEquipState::Holstering);
    assert(!game::IsFpsViewmodelReadyForUse(state));
    assert(game::IsFpsViewmodelPresentationVisible(state));
    game::AdvanceFpsViewmodelEquipTransition(state, 0.125f);
    assert(Near(state.equipProgress, 0.5f));
    assert(Near(state.holsterPose.hiddenAmount, 0.5f));
    assert(Near(state.holsterPose.translation.x, 0.225f));
    assert(Near(state.holsterPose.translation.y, -0.875f));
    state.holsterTransition.hiddenTranslation.x = 0.8f;
    game::AdvanceFpsViewmodelEquipTransition(state, 0.0f);
    assert(Near(state.equipProgress, 0.5f));
    assert(Near(state.holsterPose.translation.x, 0.4f));
    state.holsterTransition.hiddenTranslation.x = 0.45f;
    game::AdvanceFpsViewmodelEquipTransition(state, 0.0f);
    assert(Near(state.equipProgress, 0.5f));
    assert(Near(state.holsterPose.translation.x, 0.225f));

    Camera3D camera{};
    camera.position = {2.0f, 3.0f, 4.0f};
    camera.target = {2.0f, 3.0f, 5.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    game::FpsViewmodelPresentation presentation;
    presentation.position = {0.1f, -1.5f, 0.25f};
    presentation.rotationDegrees = {-4.0f, 4.0f, 0.0f};
    const Matrix beforeReverse = game::BuildFpsViewmodelAnimatedTransform(
            camera, presentation, state.holsterPose);
    assert(game::ToggleFpsViewmodelHolster(state, true, false));
    assert(state.equipState == game::FpsViewmodelEquipState::Unholstering);
    assert(Near(state.equipProgress, 0.5f));
    game::AdvanceFpsViewmodelEquipTransition(state, 0.0f);
    const Matrix afterReverse = game::BuildFpsViewmodelAnimatedTransform(
            camera, presentation, state.holsterPose);
    assert(SameTransform(beforeReverse, afterReverse));

    for (int toggle = 0; toggle < 20; ++toggle) {
        assert(game::ToggleFpsViewmodelHolster(state, true, false));
        assert(Near(state.equipProgress, 0.5f));
    }
    assert(state.equipState == game::FpsViewmodelEquipState::Unholstering);
    game::AdvanceFpsViewmodelEquipTransition(
            state,
            std::numeric_limits<float>::quiet_NaN());
    assert(Near(state.equipProgress, 0.5f));
    game::AdvanceFpsViewmodelEquipTransition(state, 100.0f);
    assert(state.equipState == game::FpsViewmodelEquipState::Equipped);
    assert(Near(state.equipProgress, 1.0f));
    assert(Near(state.holsterPose.hiddenAmount, 0.0f));
    assert(game::IsFpsViewmodelReadyForUse(state));

    const Matrix ready = game::BuildFpsViewmodelTransform(camera, presentation);
    const Matrix animatedReady = game::BuildFpsViewmodelAnimatedTransform(
            camera, presentation, state.holsterPose);
    assert(SameTransform(ready, animatedReady, 0.00001f));

    assert(game::ToggleFpsViewmodelHolster(state, true, false));
    game::AdvanceFpsViewmodelEquipTransition(state, 100.0f);
    assert(state.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(Near(state.equipProgress, 0.0f));
    assert(Near(state.holsterPose.hiddenAmount, 1.0f));
    assert(!game::IsFpsViewmodelReadyForUse(state));
    assert(!game::IsFpsViewmodelPresentationVisible(state));

    const Matrix hidden = game::BuildFpsViewmodelAnimatedTransform(
            camera, presentation, state.holsterPose);
    const Vector3 readyOrigin = Vector3Transform(Vector3{}, ready);
    const Vector3 hiddenOrigin = Vector3Transform(Vector3{}, hidden);
    const game::FpsViewmodelCameraBasis basis =
            game::BuildFpsViewmodelCameraBasis(camera);
    const Vector3 originDelta = Vector3Subtract(hiddenOrigin, readyOrigin);
    assert(Near(Vector3DotProduct(originDelta, basis.right), 0.45f, 0.001f));
    assert(Near(Vector3DotProduct(originDelta, basis.up), -1.75f, 0.001f));
    assert(Near(Vector3DotProduct(originDelta, basis.forward), 0.0f, 0.001f));
    const Vector3 hiddenForward = Vector3Subtract(
            Vector3Transform({0.0f, 0.0f, 1.0f}, hidden),
            hiddenOrigin);
    assert(Vector3DotProduct(hiddenForward, basis.up) < -0.1f);

    game::FpsViewmodelGripCorrection grip;
    grip.translation = {0.03f, 0.13f, 0.03f};
    const Matrix hand = MatrixTranslate(0.0f, 1.35f, 0.37f);
    const Matrix attached = game::BuildFpsViewmodelAttachmentTransform(
            hidden, hand, grip);
    const Matrix localAttachment = game::BuildFpsViewmodelAttachmentTransform(
            MatrixIdentity(), hand, grip);
    const Matrix expectedAttached = MatrixMultiply(localAttachment, hidden);
    assert(SameTransform(attached, expectedAttached));
    assert(!SameTransform(
            attached,
            MatrixMultiply(expectedAttached, hidden),
            0.01f));

    assert(game::ToggleFpsViewmodelHolster(state, true, false));
    assert(state.equipState == game::FpsViewmodelEquipState::Unholstering);
    assert(game::IsFpsViewmodelPresentationVisible(state));
    assert(!game::IsFpsViewmodelReadyForUse(state));
    game::AdvanceFpsViewmodelEquipTransition(state, 0.17f);
    assert(Near(state.equipProgress, 0.5f));
    game::AdvanceFpsViewmodelEquipTransition(state, 0.17f);
    assert(state.equipState == game::FpsViewmodelEquipState::Equipped);
    assert(Near(state.equipProgress, 1.0f));

    Camera3D pitchedCamera{};
    pitchedCamera.position = {-4.0f, 6.0f, 3.0f};
    pitchedCamera.target = {-3.0f, 7.0f, 4.0f};
    pitchedCamera.up = {0.0f, 1.0f, 0.0f};
    const game::FpsViewmodelHolsterPose fullHidden =
            game::EvaluateFpsViewmodelHolsterPose(
                    state.holsterTransition,
                    0.0f);
    const Matrix pitchedReady = game::BuildFpsViewmodelTransform(
            pitchedCamera,
            presentation);
    const Matrix pitchedHidden = game::BuildFpsViewmodelAnimatedTransform(
            pitchedCamera,
            presentation,
            fullHidden);
    const Vector3 pitchedDelta = Vector3Subtract(
            Vector3Transform(Vector3{}, pitchedHidden),
            Vector3Transform(Vector3{}, pitchedReady));
    const game::FpsViewmodelCameraBasis pitchedBasis =
            game::BuildFpsViewmodelCameraBasis(pitchedCamera);
    assert(Near(Vector3DotProduct(pitchedDelta, pitchedBasis.right), 0.45f, 0.001f));
    assert(Near(Vector3DotProduct(pitchedDelta, pitchedBasis.up), -1.75f, 0.001f));

    game::FpsViewmodelRuntimeState switchState;
    switchState.activeWeaponId = "pistol";
    switchState.equipState = game::FpsViewmodelEquipState::Equipped;
    switchState.equipProgress = 1.0f;
    switchState.holsterTransition = state.holsterTransition;
    int pendingSlot = 0;
    assert(game::QueueFpsWeaponSlotSwitch(switchState, 2, pendingSlot));
    assert(pendingSlot == 2);
    assert(switchState.equipState
            == game::FpsViewmodelEquipState::Holstering);
    assert(!game::QueueFpsWeaponSlotSwitch(switchState, 3, pendingSlot));
    game::AdvanceFpsViewmodelEquipTransition(switchState, 100.0f);
    assert(switchState.equipState
            == game::FpsViewmodelEquipState::Holstered);
    pendingSlot = 0;
    game::BeginFpsWeaponSlotTargetUnholster(switchState);
    assert(switchState.equipState
            == game::FpsViewmodelEquipState::Unholstering);
    assert(Near(switchState.equipProgress, 0.0f));

    switchState.activeWeaponId.clear();
    switchState.equipState = game::FpsViewmodelEquipState::Holstered;
    pendingSlot = 0;
    assert(game::QueueFpsWeaponSlotSwitch(switchState, 1, pendingSlot));
    assert(switchState.equipState
            == game::FpsViewmodelEquipState::Holstered);
    pendingSlot = 0;
    assert(!game::QueueFpsWeaponSlotSwitch(switchState, 0, pendingSlot));
}

void AnimationTiming()
{
    float a=0,b=0,c=0;
    for(int i=0;i<30;++i) a=game::AdvanceFpsViewmodelAnimationCursor(a,1.0f/30,30,1,1,41);
    for(int i=0;i<60;++i) b=game::AdvanceFpsViewmodelAnimationCursor(b,1.0f/60,30,1,1,41);
    for(int i=0;i<120;++i) c=game::AdvanceFpsViewmodelAnimationCursor(c,1.0f/120,30,1,1,41);
    assert(Near(a,30) && Near(b,30) && Near(c,30));
    assert(Near(game::AdvanceFpsViewmodelAnimationCursor(39,2.0f/30,30,1,1,41),1));
    assert(Near(game::FpsViewmodelCursorToSeconds(30,30),1));
    assert(Near(game::FpsViewmodelCursorToRaylibFrame(30,30),60));
}

Transform PoseTransform(Vector3 translation, Quaternion rotation = QuaternionIdentity())
{
    Transform result{};
    result.translation = translation;
    result.rotation = rotation;
    result.scale = {1.0f, 1.0f, 1.0f};
    return result;
}

void AttachmentMathAndBoneResolution()
{
    BoneInfo bones[3]{};
    std::strncpy(bones[0].name, "root", sizeof(bones[0].name) - 1);
    std::strncpy(bones[1].name, "arm", sizeof(bones[1].name) - 1);
    std::strncpy(bones[2].name, "mixamorig12:RightHand",
            sizeof(bones[2].name) - 1);
    bones[0].parent = -1;
    bones[1].parent = 0;
    bones[2].parent = 1;
    assert(game::FindFpsViewmodelBoneIndex(
            bones, 3, "mixamorig12:RightHand") == 2);
    assert(game::FindFpsViewmodelBoneIndex(bones, 3, "RightHand") == -1);
    assert(game::FindFpsViewmodelBoneIndex(nullptr, 3, "root") == -1);

    Transform localPose[3] = {
            PoseTransform({1.0f, 0.0f, 0.0f}),
            PoseTransform({0.0f, 2.0f, 0.0f}),
            PoseTransform({0.0f, 0.0f, 3.0f})};
    Matrix handModel{};
    assert(game::BuildFpsViewmodelBoneModelTransform(
            localPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Local, handModel));
    Vector3 origin = Vector3Transform(Vector3{}, handModel);
    assert(Near(origin.x, 1.0f) && Near(origin.y, 2.0f)
            && Near(origin.z, 3.0f));

    localPose[0] = PoseTransform(
            Vector3{}, QuaternionFromEuler(0.0f, 0.0f, PI * 0.5f));
    localPose[1] = PoseTransform(Vector3{});
    localPose[2] = PoseTransform({1.0f, 0.0f, 0.0f});
    assert(game::BuildFpsViewmodelBoneModelTransform(
            localPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Local, handModel));
    origin = Vector3Transform(Vector3{}, handModel);
    assert(Near(origin.x, 0.0f, 0.001f));
    assert(Near(origin.y, 1.0f, 0.001f));
    assert(Near(origin.z, 0.0f, 0.001f));

    Transform modelPose[3] = {
            PoseTransform({50.0f, 0.0f, 0.0f}),
            PoseTransform({50.0f, 0.0f, 0.0f}),
            PoseTransform({4.0f, 5.0f, 6.0f})};
    assert(game::BuildFpsViewmodelBoneModelTransform(
            modelPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Model, handModel));
    origin = Vector3Transform(Vector3{}, handModel);
    assert(Near(origin.x, 4.0f) && Near(origin.y, 5.0f)
            && Near(origin.z, 6.0f));
    assert(!game::BuildFpsViewmodelBoneModelTransform(
            modelPose, bones, 3, 3,
            game::FpsViewmodelBonePoseSpace::Model, handModel));
    assert(!game::BuildFpsViewmodelBoneModelTransform(
            modelPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Unknown, handModel));

    bones[0].parent = 2;
    assert(!game::BuildFpsViewmodelBoneModelTransform(
            localPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Local, handModel));
    bones[0].parent = -1;

    game::FpsViewmodelGripCorrection grip;
    grip.translation = {0.5f, 0.0f, 0.0f};
    const Matrix attachment = game::BuildFpsViewmodelAttachmentTransform(
            MatrixTranslate(10.0f, 0.0f, 0.0f),
            MatrixTranslate(2.0f, 0.0f, 0.0f),
            grip);
    origin = Vector3Transform(Vector3{}, attachment);
    assert(Near(origin.x, 12.5f));

    modelPose[2] = PoseTransform({7.0f, 5.0f, 6.0f});
    Matrix changedHand{};
    assert(game::BuildFpsViewmodelBoneModelTransform(
            modelPose, bones, 3, 2,
            game::FpsViewmodelBonePoseSpace::Model, changedHand));
    const Matrix changedAttachment = game::BuildFpsViewmodelAttachmentTransform(
            MatrixIdentity(), changedHand, grip);
    assert(!Near(
            Vector3Transform(Vector3{}, changedAttachment).x,
            Vector3Transform(Vector3{}, attachment).x));
}

void PreparedPistolFrameTwentyFit()
{
    const Matrix handModel{
            -0.031528f, 0.998209f, 0.050835f, -0.059613f,
            0.996395f, 0.027382f, 0.080295f, 1.355357f,
            0.078760f, 0.053183f, -0.995474f, 0.372894f,
            0.0f, 0.0f, 0.0f, 1.0f};
    game::FpsViewmodelGripCorrection grip;
    grip.translation = {0.020654f, 0.037913f, -0.149138f};
    grip.rotationDegrees = {3.058116f, 175.482727f, 88.187634f};
    grip.scale = 1.0f;
    const Matrix pistol = game::BuildFpsViewmodelAttachmentTransform(
            MatrixIdentity(), handModel, grip);
    const Vector3 origin = Vector3Transform(Vector3{}, pistol);
    assert(Near(origin.x, -0.030f, 0.001f));
    assert(Near(origin.y, 1.365f, 0.001f));
    assert(Near(origin.z, 0.525f, 0.001f));
    const Vector3 muzzle = Vector3Transform({0.0f, 0.0f, 1.0f}, pistol);
    const Vector3 forward = Vector3Subtract(muzzle, origin);
    assert(Near(forward.x, 0.0f, 0.001f));
    assert(Near(forward.y, 0.0f, 0.001f));
    assert(Near(forward.z, 1.0f, 0.001f));
}

void BrightnessMapping()
{
    assert(Near(game::FpsViewmodelBrightnessMultiplier(-1.0f), 0.0f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(-0.25f), 0.75f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(0.0f), 1.0f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(0.5f), 1.5f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(1.0f), 2.0f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(-2.0f), 0.0f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(2.0f), 2.0f));
    assert(Near(game::FpsViewmodelBrightnessMultiplier(
            std::numeric_limits<float>::quiet_NaN()), 1.0f));
}

void MaterialOverrideApplication()
{
    game::SectorViewmodelLightingContext pistolLighting;
    pistolLighting.materialOverrideEnabled = true;
    pistolLighting.metallicFactor = 0.35f;
    pistolLighting.roughnessFactor = 1.0f;
    pistolLighting.useMetallicRoughnessTexture = true;
    float metallic = 1.0f;
    float roughness = 0.5f;
    bool hasMetallicTexture = true;
    bool hasRoughnessTexture = true;
    game::ApplySectorViewmodelMaterialOverride(
            pistolLighting,
            metallic,
            roughness,
            hasMetallicTexture,
            hasRoughnessTexture);
    assert(Near(metallic, 0.35f));
    assert(Near(roughness, 1.0f));
    assert(hasMetallicTexture && hasRoughnessTexture);

    pistolLighting.useMetallicRoughnessTexture = false;
    game::ApplySectorViewmodelMaterialOverride(
            pistolLighting,
            metallic,
            roughness,
            hasMetallicTexture,
            hasRoughnessTexture);
    assert(!hasMetallicTexture && !hasRoughnessTexture);
}

void CrosshairVisibilityAndLayout()
{
    game::FpsWeaponRegistry registry;
    std::string error;
    assert(game::ParseFpsWeaponRegistry(ValidRegistry, registry, &error));
    game::FpsViewmodelRuntimeState runtime;
    runtime.activeWeaponId = "pistol";
    runtime.equipState = game::FpsViewmodelEquipState::Equipped;
    runtime.equipProgress = 1.0f;

    const auto visible = [&](bool preview3DActive) {
        return game::ShouldDrawFpsCrosshair(
                game::FpsHudContext{
                        preview3DActive,
                        Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f},
                        registry,
                        runtime});
    };

    runtime.loadState = game::FpsViewmodelLoadState::Pending;
    assert(visible(true));
    runtime.loadState = game::FpsViewmodelLoadState::Ready;
    runtime.modelInstance.poseReady = true;
    assert(visible(true));
    runtime.loadState = game::FpsViewmodelLoadState::Failed;
    runtime.modelInstance.poseReady = false;
    assert(visible(true));

    assert(game::ToggleFpsViewmodelHolster(runtime, true, false));
    assert(runtime.equipState == game::FpsViewmodelEquipState::Holstering);
    assert(!visible(true));
    runtime.reload.phase = game::FpsWeaponReloadPhase::Holstering;
    assert(visible(true));
    runtime.reload.phase = game::FpsWeaponReloadPhase::Inactive;
    game::AdvanceFpsViewmodelEquipTransition(runtime, 100.0f);
    assert(runtime.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(!visible(true));
    assert(game::ToggleFpsViewmodelHolster(runtime, true, false));
    assert(runtime.equipState == game::FpsViewmodelEquipState::Unholstering);
    assert(!visible(true));
    game::AdvanceFpsViewmodelEquipTransition(runtime, 100.0f);
    assert(runtime.equipState == game::FpsViewmodelEquipState::Equipped);
    assert(visible(true));
    assert(!visible(false));

    runtime.activeWeaponId.clear();
    assert(!visible(true));
    runtime.activeWeaponId = "missing";
    assert(!visible(true));
    runtime.activeWeaponId = "pistol";
    registry.weapons.front().crosshair.enabled = false;
    assert(!visible(true));
    registry.weapons.front().crosshair.enabled = true;

    const game::FpsWeaponCrosshairDefinition& crosshair =
            registry.weapons.front().crosshair;
    assert(Near(game::FpsHudScale(
            Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f}), 1.0f));
    assert(Near(game::FpsHudScale(
            Rectangle{100.0f, 50.0f, 960.0f, 540.0f}), 0.5f));
    assert(Near(game::FpsHudScale(
            Rectangle{0.0f, 0.0f, 3840.0f, 2160.0f}), 2.0f));

    const auto full = game::BuildFpsCrosshairLayout(
            crosshair,
            Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f},
            1.0f);
    assert(Near(full.center.x, 960.0f) && Near(full.center.y, 540.0f));
    assert(Near(full.segments[0].outline.width, 8.0f));
    assert(Near(full.segments[0].outline.height, 4.0f));
    assert(Near(full.segments[0].inner.width, 6.0f));
    assert(Near(full.segments[0].inner.height, 2.0f));
    assert(Near(
            full.segments[1].outline.x
                    - (full.segments[0].outline.x
                       + full.segments[0].outline.width),
            4.0f));
    assert(Near(
            full.segments[3].outline.y
                    - (full.segments[2].outline.y
                       + full.segments[2].outline.height),
            4.0f));

    const Rectangle oddViewport{13.0f, 17.0f, 1919.0f, 1079.0f};
    const auto odd = game::BuildFpsCrosshairLayout(
            crosshair, oddViewport, 1.0f);
    assert(Near(odd.center.x, 973.0f));
    assert(Near(odd.center.y, 557.0f));

    const auto half = game::BuildFpsCrosshairLayout(
            crosshair,
            Rectangle{100.0f, 50.0f, 960.0f, 540.0f},
            0.5f);
    assert(Near(half.center.x, 580.0f) && Near(half.center.y, 320.0f));
    assert(Near(half.segments[0].inner.width, 3.0f));
    assert(Near(half.segments[0].inner.height, 1.0f));

    const auto twice = game::BuildFpsCrosshairLayout(
            crosshair,
            Rectangle{0.0f, 0.0f, 3840.0f, 2160.0f},
            2.0f);
    assert(Near(twice.center.x, 1920.0f)
            && Near(twice.center.y, 1080.0f));
    assert(Near(twice.segments[0].inner.width, 12.0f));
    assert(Near(twice.segments[0].inner.height, 4.0f));

    const game::FpsReloadIndicatorLayout reloadLayout =
            game::BuildFpsReloadIndicatorLayout(full, 1.0f, 22);
    assert(Near(reloadLayout.center.x, full.center.x)
            && Near(reloadLayout.center.y, full.center.y));
    assert(reloadLayout.innerRadius > 10.0f);
    assert(reloadLayout.outerRadius > reloadLayout.innerRadius);
    assert(reloadLayout.textPosition.y
            > reloadLayout.center.y + reloadLayout.outerRadius);

    runtime.reload.phase = game::FpsWeaponReloadPhase::Waiting;
    runtime.reload.totalDurationSeconds = 2.0f;
    runtime.reload.totalElapsedSeconds = 0.5f;
    assert(Near(game::FpsWeaponReloadProgress(runtime), 0.25f));
    runtime.reload.phase = game::FpsWeaponReloadPhase::Completing;
    assert(Near(game::FpsWeaponReloadProgress(runtime), 1.0f));
    runtime.reload.phase = game::FpsWeaponReloadPhase::Inactive;
    assert(Near(game::FpsWeaponReloadProgress(runtime), 0.0f));

    const game::FpsVitalsLayout vitals = game::BuildFpsVitalsLayout(
            Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f},
            1.0f,
            22,
            true);
    assert(vitals.stamina.border.y > vitals.health.border.y);
    assert(Near(vitals.stamina.border.y + vitals.stamina.border.height,
            1080.0f - 22.0f));
    assert(vitals.health.textPosition.y >= 0.0f);
    const Vector2 ammoPosition = game::BuildFpsAmmoCounterPosition(
            vitals, 1.0f, 22, true);
    assert(ammoPosition.x == vitals.health.border.x);
    assert(ammoPosition.y < vitals.health.textPosition.y
            && ammoPosition.y < vitals.stamina.textPosition.y);
    const game::FpsVitalsLayout healthOnly = game::BuildFpsVitalsLayout(
            Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f},
            1.0f,
            22,
            false);
    assert(SameRectangle(healthOnly.health.border, vitals.stamina.border));

    runtime.attachment.handModelTransform = MatrixTranslate(10.0f, 20.0f, 30.0f);
    runtime.attachment.attachmentWorldTransform = MatrixRotateY(1.25f);
    const auto afterWeaponTransforms =
            game::BuildFpsCrosshairLayout(
                    crosshair,
                    Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f},
                    1.0f);
    assert(Near(afterWeaponTransforms.center.x, full.center.x));
    assert(Near(afterWeaponTransforms.center.y, full.center.y));
    for (size_t i = 0; i < full.segments.size(); ++i) {
        assert(SameRectangle(
                afterWeaponTransforms.segments[i].outline,
                full.segments[i].outline));
        assert(SameRectangle(
                afterWeaponTransforms.segments[i].inner,
                full.segments[i].inner));
    }
}

void RuntimeStateAndGating()
{
    game::FpsViewmodelRuntimeState state;
    assert(Near(state.brightnessAdjustment, 0.0f));
    assert(Near(state.brightnessMultiplier, 1.0f));
    assert(Near(state.attachment.lighting.brightnessAdjustment, 0.0f));
    assert(Near(state.attachment.brightnessMultiplier, 1.0f));
    assert(state.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(Near(state.equipProgress, 0.0f));
    assert(!game::IsFpsViewmodelReadyForUse(state));
    assert(!game::ToggleFpsViewmodelHolster(state,false,false));
    state.activeWeaponId = "pistol";
    state.loadState=game::FpsViewmodelLoadState::Pending;
    state.equipState = game::FpsViewmodelEquipState::Equipped;
    state.equipProgress = 1.0f;
    assert(!game::ToggleFpsViewmodelHolster(state,true,true));
    assert(game::ToggleFpsViewmodelHolster(state,true,false));
    assert(state.equipState == game::FpsViewmodelEquipState::Holstering);
    assert(!game::IsFpsViewmodelRenderable(state));
    state.loadState=game::FpsViewmodelLoadState::Failed;
    const float progressBeforeFailureUpdate = state.equipProgress;
    game::AdvanceFpsViewmodelEquipTransition(state, 0.1f);
    assert(state.equipProgress < progressBeforeFailureUpdate);
    assert(!game::IsFpsViewmodelRenderable(state));
    state.loadState=game::FpsViewmodelLoadState::Ready;
    state.modelInstance.poseReady=true; state.animationIndex=0;
    assert(game::IsFpsViewmodelRenderable(state));
    state.attachment.loadState =
            game::FpsViewmodelAttachmentLoadState::Pending;
    assert(!game::IsFpsViewmodelAttachmentRenderable(state));
    state.attachment.loadState =
            game::FpsViewmodelAttachmentLoadState::Failed;
    assert(game::IsFpsViewmodelRenderable(state));
    assert(!game::IsFpsViewmodelAttachmentRenderable(state));
    state.attachment.loadState =
            game::FpsViewmodelAttachmentLoadState::Ready;
    state.attachment.boneIndex = 7;
    state.attachment.handPoseValid = true;
    assert(game::IsFpsViewmodelAttachmentRenderable(state));
    game::AdvanceFpsViewmodelEquipTransition(state, 100.0f);
    assert(state.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(!game::IsFpsViewmodelRenderable(state));
    assert(!game::IsFpsViewmodelAttachmentRenderable(state));
    assert(game::ToggleFpsViewmodelHolster(state, true, false));
    assert(state.equipState == game::FpsViewmodelEquipState::Unholstering);
    assert(game::IsFpsViewmodelRenderable(state));
    assert(game::IsFpsViewmodelAttachmentRenderable(state));
    game::ResetFpsViewmodelRuntime(state);
    assert(state.loadState==game::FpsViewmodelLoadState::Inactive);
    assert(engine::IsNull(state.assetScope) && !game::IsFpsViewmodelRenderable(state));
    assert(Near(state.brightnessAdjustment, 0.0f));
    assert(Near(state.brightnessMultiplier, 1.0f));
    assert(engine::IsNull(state.attachment.model));
    assert(state.attachment.loadState
            == game::FpsViewmodelAttachmentLoadState::Inactive);
    assert(state.attachment.boneIndex == -1);
    assert(Near(state.attachment.lighting.brightnessAdjustment, 0.0f));
    assert(Near(state.attachment.brightnessMultiplier, 1.0f));
    assert(state.equipState == game::FpsViewmodelEquipState::Holstered);
    assert(Near(state.equipProgress, 0.0f));
    assert(!game::IsFpsViewmodelReadyForUse(state));
}

void FiringRuntimeAndTransforms()
{
    const float firstPitch = game::FpsWeaponShotPitch(1, 0x12345678u);
    const float repeatedPitch = game::FpsWeaponShotPitch(1, 0x12345678u);
    const float nextPitch = game::FpsWeaponShotPitch(2, 0x12345678u);
    assert(firstPitch >= 0.96f && firstPitch <= 1.04f);
    assert(nextPitch >= 0.96f && nextPitch <= 1.04f);
    assert(Near(firstPitch, repeatedPitch));
    assert(!Near(firstPitch, nextPitch, 0.000001f));

    game::FpsWeaponFiringDefinition clampedDefinition;
    clampedDefinition.muzzleFlash.lifetimeSeconds = 120.0f;
    clampedDefinition.muzzleFlash.edgeSoftness = 2.0f;
    clampedDefinition.cameraRecoil.pitchKickDegrees =
            std::numeric_limits<float>::quiet_NaN();
    clampedDefinition.cameraRecoil.yawVariationDegrees = -2.0f;
    clampedDefinition.cameraRecoil.springFrequencyHz =
            std::numeric_limits<float>::infinity();
    clampedDefinition = game::ClampFpsWeaponFiringDefinition(clampedDefinition);
    assert(Near(clampedDefinition.muzzleFlash.lifetimeSeconds, 60.0f));
    assert(Near(clampedDefinition.muzzleFlash.edgeSoftness, 1.0f));
    assert(Near(clampedDefinition.cameraRecoil.pitchKickDegrees, 0.0f));
    assert(Near(clampedDefinition.cameraRecoil.yawVariationDegrees, 0.0f));
    assert(Near(clampedDefinition.cameraRecoil.springFrequencyHz, 5.5f));

    game::FpsMuzzleFlashRuntimeState gradient;
    gradient.coreColor = Color{255, 255, 245, 255};
    gradient.hotColor = Color{255, 235, 120, 255};
    gradient.warmColor = Color{255, 90, 15, 230};
    gradient.edgeColor = Color{120, 15, 5, 150};
    gradient.edgeSoftness = 0.35f;
    const Color core = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.0f, 1.0f);
    const Color hot = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.22f, 1.0f);
    const Color warm = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.58f, 1.0f);
    const Color edge = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.82f, 1.0f);
    const Color boundary = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 1.0f, 1.0f);
    assert(core.r == 255 && core.g == 255 && core.b == 245 && core.a == 255);
    assert(hot.r == 255 && hot.g == 235 && hot.b == 120);
    assert(warm.r == 255 && warm.g == 90 && warm.b == 15);
    assert(edge.r == 120 && edge.g == 15 && edge.b == 5);
    assert(boundary.a == 0);
    const Color halfLife = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.0f, 0.5f);
    assert(halfLife.a == 128);
    gradient.edgeSoftness = 0.10f;
    const Color narrow = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.80f, 1.0f);
    gradient.edgeSoftness = 0.60f;
    const Color wide = game::EvaluateFpsMuzzleFlashGradient(
            gradient, 0.80f, 1.0f);
    assert(wide.a < narrow.a);
    const auto earlyTemporal =
            game::EvaluateFpsMuzzleFlashTemporalState(
                    0.005f, 0.033f);
    const auto lateTemporal =
            game::EvaluateFpsMuzzleFlashTemporalState(
                    0.02475f, 0.033f);
    const auto expiredTemporal =
            game::EvaluateFpsMuzzleFlashTemporalState(
                    0.033f, 0.033f);
    assert(Near(earlyTemporal.expansionScale, 1.0f));
    assert(Near(earlyTemporal.opacity, 1.0f));
    assert(lateTemporal.expansionScale > 1.0f
            && lateTemporal.expansionScale < 1.14f);
    assert(lateTemporal.opacity < 1.0f && lateTemporal.opacity > 0.0f);
    assert(Near(expiredTemporal.expansionScale, 1.14f));
    assert(Near(expiredTemporal.opacity, 0.0f));

    game::FpsViewmodelRuntimeState viewmodel;
    game::FpsFireRejectReason reason = game::FpsFireRejectReason::None;
    assert(!game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::NoActiveWeapon);
    viewmodel.activeWeaponId = "pistol";
    viewmodel.equipState = game::FpsViewmodelEquipState::Equipped;
    viewmodel.equipProgress = 1.0f;
    assert(!game::CanFireFpsWeapon(viewmodel, false, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::NotInGameplay3D);
    assert(!game::CanFireFpsWeapon(viewmodel, true, false, false, &reason));
    assert(reason == game::FpsFireRejectReason::MouseInputInactive);
    assert(!game::CanFireFpsWeapon(viewmodel, true, true, true, &reason));
    assert(reason == game::FpsFireRejectReason::UiCaptured);
    assert(game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    viewmodel.firing.ammunitionEnabled = true;
    viewmodel.firing.loadedRounds = 0;
    assert(!game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::EmptyMagazine);
    viewmodel.firing.loadedRounds = 1;
    assert(game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    viewmodel.reload.phase = game::FpsWeaponReloadPhase::Waiting;
    assert(!game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::Reloading);
    viewmodel.reload.phase = game::FpsWeaponReloadPhase::Inactive;
    viewmodel.firing.ammunitionEnabled = false;
    for (game::FpsViewmodelEquipState state : {
                 game::FpsViewmodelEquipState::Holstered,
                 game::FpsViewmodelEquipState::Holstering,
                 game::FpsViewmodelEquipState::Unholstering}) {
        viewmodel.equipState = state;
        assert(!game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
        assert(reason == game::FpsFireRejectReason::WeaponNotReady);
    }
    viewmodel.equipState = game::FpsViewmodelEquipState::Equipped;

    game::FpsShotResult shot;
    shot.accepted = true;
    shot.hit = true;
    shot.rayOrigin = Vector3{1.0f, 2.0f, 3.0f};
    shot.rayDirection = Vector3{0.0f, 0.0f, 1.0f};
    shot.position = Vector3{1.0f, 2.0f, 8.0f};
    shot.normal = Vector3{0.0f, 0.0f, -1.0f};
    shot.distance = 5.0f;
    shot.sectorId = 7;
    viewmodel.firing.randomState = 12345u;
    viewmodel.firing.definition.cameraRecoil =
            game::FpsWeaponCameraRecoilDefinition{
                    true, 0.4f, 0.08f, 0.15f, 0.07f,
                    5.5f, 0.97f, 1.25f, 0.6f, 0.2f};
    game::FpsWeaponFiringRuntimeState duplicate = viewmodel.firing;
    game::FpsMuzzleEmissionCapture emission;
    emission.valid = true;
    emission.cameraLocalTransform = MatrixTranslate(0.1f, -0.2f, 0.4f);
    game::ApplyFpsWeaponShotEffects(viewmodel.firing, shot, emission);
    game::ApplyFpsWeaponShotEffects(duplicate, shot, emission);
    assert(viewmodel.firing.shotSequence == 1);
    assert(viewmodel.firing.hasLastShot
            && viewmodel.firing.lastShot.hit
            && viewmodel.firing.lastShot.sectorId == 7);
    assert(Near(viewmodel.firing.cooldownRemainingSeconds, 0.18f));
    assert(Near(viewmodel.firing.recoil.translation.z, -0.03f));
    assert(Near(viewmodel.firing.recoil.rotationDegrees.z,
            duplicate.recoil.rotationDegrees.z));
    assert(viewmodel.firing.cameraRecoil.rotationDegrees.x >= 0.32f
            && viewmodel.firing.cameraRecoil.rotationDegrees.x <= 0.48f);
    assert(Near(
            viewmodel.firing.cameraRecoil.rotationDegrees.x,
            duplicate.cameraRecoil.rotationDegrees.x));
    assert(Near(viewmodel.firing.lastShot.rayDirection.x, 0.0f)
            && Near(viewmodel.firing.lastShot.rayDirection.y, 0.0f)
            && Near(viewmodel.firing.lastShot.rayDirection.z, 1.0f));
    assert(viewmodel.firing.flash.active && viewmodel.firing.light.active);
    assert(viewmodel.firing.flash.coreColor.r == 255);
    assert(Near(viewmodel.firing.flash.edgeSoftness, 0.35f));
    assert(viewmodel.firing.emission.valid);
    assert(viewmodel.firing.flash.shape.lobeCount >= 5
            && viewmodel.firing.flash.shape.lobeCount <= 8);
    assert(viewmodel.firing.flash.shape.seed
            == duplicate.flash.shape.seed);
    assert(Near(viewmodel.firing.flash.shape.phaseRadians,
            duplicate.flash.shape.phaseRadians));
    const game::FpsMuzzleFlashShape fixedShape =
            viewmodel.firing.flash.shape;
    assert(!game::CanFireFpsWeapon(viewmodel, true, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::Cooldown);
    game::AdvanceFpsWeaponFiringRuntime(viewmodel.firing, 0.01f);
    assert(viewmodel.firing.flash.active);
    assert(viewmodel.firing.flash.shape.seed == fixedShape.seed);
    assert(Near(viewmodel.firing.flash.shape.phaseRadians,
            fixedShape.phaseRadians));
    const float recoilBeforeSecondShot = viewmodel.firing.recoil.translation.z;

    game::ApplyFpsWeaponShotEffects(viewmodel.firing, shot, emission);
    assert(viewmodel.firing.shotSequence == 2);
    assert(viewmodel.firing.flash.shape.seed != fixedShape.seed);
    assert(Near(
            viewmodel.firing.recoil.translation.z,
            recoilBeforeSecondShot - 0.03f));
    assert(std::fabs(viewmodel.firing.recoil.rotationDegrees.x) <= 8.0f);
    game::AdvanceFpsWeaponFiringRuntime(viewmodel.firing, 0.18f);
    assert(Near(viewmodel.firing.cooldownRemainingSeconds, 0.0f));
    assert(!viewmodel.firing.flash.active && !viewmodel.firing.light.active);
    for (int i = 0; i < 240; ++i) {
        game::AdvanceFpsWeaponFiringRuntime(viewmodel.firing, 1.0f / 120.0f);
    }
    assert(Near(viewmodel.firing.recoil.translation.x, 0.0f, 0.0001f));
    assert(Near(viewmodel.firing.recoil.translation.y, 0.0f, 0.0001f));
    assert(Near(viewmodel.firing.recoil.translation.z, 0.0f, 0.0001f));
    assert(Near(viewmodel.firing.recoil.rotationDegrees.x, 0.0f, 0.001f));
    assert(Near(
            viewmodel.firing.cameraRecoil.rotationDegrees.x,
            0.0f,
            0.001f));
    game::ApplyFpsWeaponShotEffects(viewmodel.firing, shot, emission);
    game::AdvanceFpsWeaponFiringRuntime(viewmodel.firing, 10.0f);
    assert(std::isfinite(viewmodel.firing.recoil.translation.z));
    assert(std::isfinite(viewmodel.firing.recoil.rotationDegrees.x));
    assert(Near(viewmodel.firing.cooldownRemainingSeconds, 0.0f));
    game::AdvanceFpsWeaponFiringRuntime(
            viewmodel.firing, std::numeric_limits<float>::quiet_NaN());
    assert(std::isfinite(viewmodel.firing.recoil.translation.z));

    game::FpsMuzzleLightRuntimeState light;
    light.active = true;
    light.lifetimeSeconds = 0.1f;
    light.ageSeconds = 0.05f;
    light.intensity = 8.0f;
    light.decayExponent = 2.0f;
    assert(Near(game::FpsMuzzleLightCurrentIntensity(light), 2.0f));

    const Matrix pistol = MatrixMultiply(
            MatrixRotateY(0.5f), MatrixTranslate(4.0f, 5.0f, 6.0f));
    game::FpsWeaponMuzzleSocketDefinition socket;
    socket.position = Vector3{0.0f, 0.1f, 0.2f};
    socket.rotationDegrees = Vector3{10.0f, 20.0f, 30.0f};
    const Matrix expected = MatrixMultiply(
            MatrixMultiply(
                    MatrixRotateXYZ(Vector3{10.0f * DEG2RAD,
                            20.0f * DEG2RAD, 30.0f * DEG2RAD}),
                    MatrixTranslate(0.0f, 0.1f, 0.2f)),
            pistol);
    assert(SameTransform(
            game::BuildFpsViewmodelMuzzleTransform(pistol, socket),
            expected));

    game::FpsWeaponMuzzleFlashDefinition shapeDefinition;
    shapeDefinition.irregularity = 1.0f;
    shapeDefinition.forwardStretch = 1.8f;
    shapeDefinition.minimumLobeCount = 5;
    shapeDefinition.maximumLobeCount = 8;
    shapeDefinition.rearSuppression = 0.9f;
    shapeDefinition.sizeVariation = 0.12f;
    const game::FpsMuzzleFlashShape firstShape =
            game::GenerateFpsMuzzleFlashShape(shapeDefinition, 42u);
    const game::FpsMuzzleFlashShape repeatedShape =
            game::GenerateFpsMuzzleFlashShape(shapeDefinition, 42u);
    const game::FpsMuzzleFlashShape differentShape =
            game::GenerateFpsMuzzleFlashShape(shapeDefinition, 43u);
    assert(firstShape.lobeCount >= 5 && firstShape.lobeCount <= 8);
    assert(firstShape.lobeCount == repeatedShape.lobeCount);
    assert(Near(firstShape.phaseRadians, repeatedShape.phaseRadians));
    assert(Near(firstShape.overallScale, repeatedShape.overallScale));
    assert(firstShape.seed != differentShape.seed);
    assert(firstShape.dominantLengthScale > 1.6f);
    assert(firstShape.overallScale >= 0.88f
            && firstShape.overallScale <= 1.12f);
    assert(firstShape.lobes[1].visibilityAnchor);
    assert(firstShape.lobes[1].forwardComponent >= 0.10f
            && firstShape.lobes[1].forwardComponent <= 0.35f);
    assert(firstShape.lobes[1].lengthScale >= 0.70f
            && firstShape.lobes[1].lengthScale <= 0.90f);
    assert(firstShape.lobes[1].widthScale >= 0.90f
            && firstShape.lobes[1].widthScale <= 1.15f);
    for (int index = 0; index < firstShape.lobeCount; ++index) {
        const game::FpsMuzzleFlashLobe& lobe =
                firstShape.lobes[static_cast<size_t>(index)];
        assert(std::isfinite(lobe.azimuthRadians));
        assert(std::isfinite(lobe.forwardComponent));
        assert(lobe.lengthScale > 0.0f);
        assert(lobe.widthScale > 0.0f);
        if (index > 0) {
            assert(lobe.lengthScale < firstShape.dominantLengthScale);
            assert(lobe.forwardComponent >= -0.0351f);
        }
    }
    bool observedDifferentLobeCountOrPhase = false;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        const game::FpsMuzzleFlashShape generated =
                game::GenerateFpsMuzzleFlashShape(shapeDefinition, seed);
        assert(generated.lobeCount >= shapeDefinition.minimumLobeCount);
        assert(generated.lobeCount <= shapeDefinition.maximumLobeCount);
        assert(generated.overallScale >= 0.88f
                && generated.overallScale <= 1.12f);
        assert(generated.dominantLengthScale > 1.6f);
        int visibilityAnchorCount = 0;
        for (int index = 0; index < generated.lobeCount; ++index) {
            const game::FpsMuzzleFlashLobe& lobe =
                    generated.lobes[static_cast<size_t>(index)];
            assert(std::isfinite(lobe.azimuthRadians));
            assert(std::isfinite(lobe.forwardComponent));
            assert(std::isfinite(lobe.lengthScale)
                    && lobe.lengthScale > 0.0f);
            assert(std::isfinite(lobe.widthScale)
                    && lobe.widthScale > 0.0f);
            if (lobe.visibilityAnchor) {
                ++visibilityAnchorCount;
                assert(index == 1);
                assert(lobe.forwardComponent >= 0.10f
                        && lobe.forwardComponent <= 0.35f);
                assert(lobe.lengthScale >= 0.70f
                        && lobe.lengthScale <= 0.90f);
                assert(lobe.widthScale >= 0.90f
                        && lobe.widthScale <= 1.15f);
            }
            if (index > 0 && lobe.forwardComponent < 0.0f) {
                const float rearExtent = lobe.lengthScale
                        * -lobe.forwardComponent
                        / std::sqrt(
                                1.0f
                                + lobe.forwardComponent
                                        * lobe.forwardComponent);
                assert(rearExtent < 0.03f);
            }
        }
        assert(visibilityAnchorCount == 1);
        observedDifferentLobeCountOrPhase =
                observedDifferentLobeCountOrPhase
                || generated.lobeCount != firstShape.lobeCount
                || !Near(generated.phaseRadians, firstShape.phaseRadians);
    }
    assert(observedDifferentLobeCountOrPhase);

    const game::FpsMuzzleFlashRibbonAxes axialAxes =
            game::BuildFpsMuzzleFlashRibbonAxes(
                    Vector3{0.0f, 0.0f, 1.0f},
                    Vector3{1.0f, 0.0f, 0.0f});
    assert(axialAxes.valid);
    assert(Near(Vector3Length(axialAxes.first), 1.0f));
    assert(Near(Vector3Length(axialAxes.second), 1.0f));
    assert(Near(Vector3DotProduct(
            axialAxes.first, axialAxes.second), 0.0f));
    assert(Near(Vector3DotProduct(
            axialAxes.first, Vector3{0.0f, 0.0f, 1.0f}), 0.0f));
    assert(Near(Vector3DotProduct(
            axialAxes.second, Vector3{0.0f, 0.0f, 1.0f}), 0.0f));

    const game::FpsMuzzleFlashRibbonAxes fallbackAxes =
            game::BuildFpsMuzzleFlashRibbonAxes(
                    Vector3{0.0f, 1.0f, 0.0f},
                    Vector3{0.0f, 2.0f, 0.0f});
    assert(fallbackAxes.valid);
    assert(Near(Vector3Length(fallbackAxes.first), 1.0f));
    assert(Near(Vector3Length(fallbackAxes.second), 1.0f));
    assert(Near(Vector3DotProduct(
            fallbackAxes.first, fallbackAxes.second), 0.0f));
    assert(!game::BuildFpsMuzzleFlashRibbonAxes(
            Vector3{}, Vector3{1.0f, 0.0f, 0.0f}).valid);

    Camera3D shotCamera{};
    shotCamera.position = Vector3{2.0f, 3.0f, 4.0f};
    shotCamera.target = Vector3{2.2f, 3.1f, 5.0f};
    shotCamera.up = Vector3{0.0f, 1.0f, 0.0f};
    game::FpsViewmodelPresentation shotPresentation;
    shotPresentation.position = Vector3{0.1f, -0.3f, 0.4f};
    game::FpsRecoilRuntimeState previousRecoil;
    previousRecoil.translation = Vector3{0.01f, 0.0f, -0.02f};
    previousRecoil.rotationDegrees = Vector3{-1.0f, 0.0f, 0.1f};
    const Matrix preShotRoot = game::BuildFpsViewmodelAnimatedTransform(
            shotCamera,
            shotPresentation,
            game::FpsViewmodelHolsterPose{},
            previousRecoil);
    const Matrix preShotPistol = game::BuildFpsViewmodelAttachmentTransform(
            preShotRoot,
            MatrixTranslate(0.02f, 0.03f, 0.04f),
            game::FpsViewmodelGripCorrection{});
    const Matrix preShotMuzzle = game::BuildFpsViewmodelMuzzleTransform(
            preShotPistol, socket);
    const game::FpsMuzzleEmissionCapture captured =
            game::CaptureFpsMuzzleEmission(preShotMuzzle, shotCamera);
    assert(captured.valid);
    assert(SameTransform(
            game::ResolveFpsMuzzleEmissionTransform(captured, shotCamera),
            preShotMuzzle));

    game::FpsWeaponFiringRuntimeState captureOrderState;
    captureOrderState.recoil = previousRecoil;
    game::ApplyFpsWeaponShotEffects(captureOrderState, shot, captured);
    assert(SameTransform(
            captureOrderState.emission.cameraLocalTransform,
            captured.cameraLocalTransform));
    const Matrix postShotRoot = game::BuildFpsViewmodelAnimatedTransform(
            shotCamera,
            shotPresentation,
            game::FpsViewmodelHolsterPose{},
            captureOrderState.recoil);
    const Matrix postShotPistol = game::BuildFpsViewmodelAttachmentTransform(
            postShotRoot,
            MatrixTranslate(0.02f, 0.03f, 0.04f),
            game::FpsViewmodelGripCorrection{});
    const Matrix postShotMuzzle = game::BuildFpsViewmodelMuzzleTransform(
            postShotPistol, socket);
    assert(!SameTransform(postShotMuzzle, preShotMuzzle));
    assert(SameTransform(
            game::ResolveFpsMuzzleEmissionTransform(
                    captureOrderState.emission, shotCamera),
            preShotMuzzle));

    Camera3D movedCamera = shotCamera;
    movedCamera.position = Vector3{-3.0f, 1.0f, 7.0f};
    movedCamera.target = Vector3{-2.0f, 1.2f, 7.2f};
    const Matrix movedEmission = game::ResolveFpsMuzzleEmissionTransform(
            captured, movedCamera);
    const game::FpsMuzzleEmissionCapture recaptured =
            game::CaptureFpsMuzzleEmission(movedEmission, movedCamera);
    assert(recaptured.valid);
    assert(SameTransform(
            recaptured.cameraLocalTransform,
            captured.cameraLocalTransform));
    const Matrix changedHandPistol = game::BuildFpsViewmodelAttachmentTransform(
            postShotRoot,
            MatrixTranslate(2.0f, 3.0f, 4.0f),
            game::FpsViewmodelGripCorrection{});
    assert(!SameTransform(
            game::BuildFpsViewmodelMuzzleTransform(changedHandPistol, socket),
            movedEmission));
    assert(SameTransform(
            game::ResolveFpsMuzzleEmissionTransform(captured, movedCamera),
            movedEmission));

    game::FpsMuzzleEmissionCapture invalidEmission;
    game::ApplyFpsWeaponShotEffects(
            captureOrderState, shot, invalidEmission);
    assert(!captureOrderState.flash.active);
    assert(!captureOrderState.light.active);

    game::FpsViewmodelHolsterPose holster;
    game::FpsViewmodelPresentation presentation;
    Camera3D camera{};
    camera.position = Vector3{1.0f, 2.0f, 3.0f};
    camera.target = Vector3{1.0f, 2.0f, 4.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    game::FpsRecoilRuntimeState recoil;
    recoil.translation = Vector3{0.0f, 0.0f, -0.03f};
    recoil.rotationDegrees = Vector3{-3.0f, 0.0f, 0.0f};
    const Matrix withRecoil = game::BuildFpsViewmodelAnimatedTransform(
            camera, presentation, holster, recoil);
    assert(!SameTransform(withRecoil,
            game::BuildFpsViewmodelAnimatedTransform(
                    camera, presentation, holster)));
}

void CameraRecoilRuntime()
{
    game::FpsWeaponCameraRecoilDefinition definition{
            true, 0.4f, 0.08f, 0.15f, 0.07f,
            5.5f, 0.97f, 1.25f, 0.6f, 0.2f};

    uint32_t randomState = 0x12345678u;
    for (int shot = 0; shot < 512; ++shot) {
        const Vector3 kick = game::SampleFpsCameraRecoilKickDegrees(
                definition, randomState);
        assert(kick.x >= 0.32f && kick.x <= 0.48f);
        assert(kick.y >= -0.15f && kick.y <= 0.15f);
        assert(kick.z >= -0.07f && kick.z <= 0.07f);
    }

    game::FpsWeaponCameraRecoilDefinition nonDownward = definition;
    nonDownward.pitchKickDegrees = 0.02f;
    nonDownward.pitchVariationDegrees = 0.08f;
    for (int shot = 0; shot < 128; ++shot) {
        assert(game::SampleFpsCameraRecoilKickDegrees(
                nonDownward, randomState).x >= 0.0f);
    }

    game::FpsCameraRecoilRuntimeState accumulated;
    for (int shot = 0; shot < 32; ++shot) {
        game::ApplyFpsCameraRecoilImpulse(accumulated, definition);
        assert(std::fabs(accumulated.rotationDegrees.x)
                <= definition.maxPitchDegrees);
        assert(std::fabs(accumulated.rotationDegrees.y)
                <= definition.maxYawDegrees);
        assert(std::fabs(accumulated.rotationDegrees.z)
                <= definition.maxRollDegrees);
    }
    assert(Near(
            accumulated.rotationDegrees.x,
            definition.maxPitchDegrees));

    game::FpsCameraRecoilRuntimeState sixtyHz;
    game::FpsCameraRecoilRuntimeState oneTwentyHz;
    sixtyHz.randomState = 42u;
    oneTwentyHz.randomState = 42u;
    game::ApplyFpsCameraRecoilImpulse(sixtyHz, definition);
    game::ApplyFpsCameraRecoilImpulse(oneTwentyHz, definition);
    assert(Near(sixtyHz.rotationDegrees.x, oneTwentyHz.rotationDegrees.x));
    for (int frame = 0; frame < 60; ++frame) {
        game::AdvanceFpsCameraRecoil(sixtyHz, definition, 1.0f / 60.0f);
    }
    for (int frame = 0; frame < 120; ++frame) {
        game::AdvanceFpsCameraRecoil(
                oneTwentyHz, definition, 1.0f / 120.0f);
    }
    assert(Near(
            sixtyHz.rotationDegrees.x,
            oneTwentyHz.rotationDegrees.x,
            0.002f));
    assert(Near(sixtyHz.rotationDegrees.x, 0.0f, 0.001f));
    assert(Near(oneTwentyHz.rotationDegrees.y, 0.0f, 0.001f));
    assert(Near(oneTwentyHz.rotationDegrees.z, 0.0f, 0.001f));

    game::FpsWeaponCameraRecoilDefinition faster = definition;
    faster.springFrequencyHz = 11.0f;
    game::FpsCameraRecoilRuntimeState normalKick;
    game::FpsCameraRecoilRuntimeState fastKick;
    normalKick.randomState = 99u;
    fastKick.randomState = 99u;
    game::ApplyFpsCameraRecoilImpulse(normalKick, definition);
    game::ApplyFpsCameraRecoilImpulse(fastKick, faster);
    assert(Near(normalKick.rotationDegrees.x, fastKick.rotationDegrees.x));

    game::FpsViewmodelRuntimeState gatedViewmodel;
    gatedViewmodel.activeWeaponId = "pistol";
    gatedViewmodel.equipState = game::FpsViewmodelEquipState::Equipped;
    gatedViewmodel.equipProgress = 1.0f;
    gatedViewmodel.firing.cooldownRemainingSeconds = 0.1f;
    game::FpsFireRejectReason reason = game::FpsFireRejectReason::None;
    assert(!game::CanFireFpsWeapon(
            gatedViewmodel, true, true, false, &reason));
    assert(reason == game::FpsFireRejectReason::Cooldown);
    assert(Near(
            gatedViewmodel.firing.cameraRecoil.rotationDegrees,
            Vector3{}));

    accumulated.rotationDegrees.x =
            std::numeric_limits<float>::quiet_NaN();
    game::AdvanceFpsCameraRecoil(accumulated, definition, 1.0f / 60.0f);
    assert(std::isfinite(accumulated.rotationDegrees.x));
    game::ResetFpsCameraRecoil(accumulated);
    assert(Near(accumulated.rotationDegrees, Vector3{}));
    assert(Near(accumulated.rotationVelocityDegrees, Vector3{}));
}

} // namespace

int main()
{
    RegistrySuccess(); RegistryRoundTripAndSharedArmsConfiguration();
    PelletDirectionGeneration();
    RegistryValidation(); WeaponSlotSchemaAndKeys();
    SettingsResolutionAndPersistence();
    PreviewSettingsOverrideDeltaCoverage();
    CameraMath(); HolsterTransitionStateAndMath(); AnimationTiming();
    AttachmentMathAndBoneResolution();
    PreparedPistolFrameTwentyFit(); BrightnessMapping();
    MaterialOverrideApplication(); CrosshairVisibilityAndLayout();
    RuntimeStateAndGating(); FiringRuntimeAndTransforms();
    CameraRecoilRuntime();
    std::cout << "FPS viewmodel tests passed\n";
}
