#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "sector_editor/preview/SectorEditorPreviewHudRenderer.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"

#include <raymath.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
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
 "viewmodel":{"modelPath":"assets/models/weapons/pistol_arms.glb",
 "idleAnimation":"Pistol Idle","sourceFps":30,"firstFrame":1,"lastFrame":41,
 "playbackSpeed":1,"position":[0,-1.4,0.25],"rotationDegrees":[1,2,3],
 "scale":1,"verticalFovDegrees":65,"brightnessAdjustment":-0.15,
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

bool SameRectangle(Rectangle lhs, Rectangle rhs)
{
    return Near(lhs.x, rhs.x)
            && Near(lhs.y, rhs.y)
            && Near(lhs.width, rhs.width)
            && Near(lhs.height, rhs.height);
}

void RegistrySuccess()
{
    game::FpsWeaponRegistry registry; std::string error;
    assert(game::ParseFpsWeaponRegistry(ValidRegistry, registry, &error));
    const auto* pistol = game::FindFpsWeaponDefinition(registry, "pistol");
    assert(pistol && registry.initialWeaponId == "pistol");
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
    assert(pistol->viewmodel.modelPath == "assets/models/weapons/pistol_arms.glb");
    assert(pistol->viewmodel.idleAnimation == "Pistol Idle");
    assert(pistol->viewmodel.firstFrame == 1 && pistol->viewmodel.lastFrame == 41);
    assert(Near(pistol->viewmodel.presentation.position.y, -1.4f));
    assert(Near(pistol->viewmodel.presentation.rotationDegrees.z, 3));
    assert(Near(pistol->viewmodel.presentation.scale, 1));
    assert(Near(pistol->viewmodel.presentation.verticalFovDegrees, 65));
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
            "\"viewmodel\":", crosshairBegin);
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

    game::FpsApplicationSettings settings; std::string error;
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"position":[1,2,3],"scale":2,"gripCorrection":{"translation":[0.1,0.2,0.3],"rotationDegrees":[10,20,30],"scale":1.25},"attachmentLighting":{"brightnessAdjustment":0.2,"metallicFactor":0.45,"roughnessFactor":0.8}}}})",
            settings, &error));
    assert(game::FindFpsViewmodelOverride(settings, "pistol") != nullptr);
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
    game::SetFpsViewmodelGripCorrectionOverride(
            settings, "pistol", persistedGrip);
    game::SetFpsViewmodelAttachmentLightingOverride(
            settings, "pistol", persistedLighting);
    const std::filesystem::path path = std::filesystem::temp_directory_path()/"fps_viewmodel_settings_test.json";
    assert(game::SaveFpsApplicationSettings(path.string(), settings, &error));
    game::FpsApplicationSettings loaded;
    assert(game::LoadFpsApplicationSettings(path.string(), loaded, &error));
    assert(game::FindFpsViewmodelOverride(loaded, "pistol") != nullptr);
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            != nullptr);
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") != nullptr);
    std::error_code ignored; std::filesystem::remove(path, ignored);
    game::ClearFpsViewmodelOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelOverride(loaded, "pistol") == nullptr);
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            != nullptr);
    game::ClearFpsViewmodelGripCorrectionOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelGripCorrectionOverride(loaded, "pistol")
            == nullptr);
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") != nullptr);
    game::ClearFpsViewmodelAttachmentLightingOverride(loaded, "pistol");
    assert(game::FindFpsViewmodelAttachmentLightingOverride(
            loaded, "pistol") == nullptr);
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"scale":"bad"}}})", loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"gripCorrection":{"scale":"bad"}}}})",
            loaded, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"viewmodelOverrides":{"pistol":{"attachmentLighting":{"metallicFactor":"bad"}}}})",
            loaded, &error));
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

    const auto visible = [&](bool preview3DActive) {
        return game::ShouldDrawSectorEditorPreviewCrosshair(
                game::SectorEditorPreviewHudContext{
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

    runtime.holstered = true;
    assert(!visible(true));
    runtime.holstered = false;
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
    assert(Near(game::SectorEditorPreviewHudScale(
            Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f}), 1.0f));
    assert(Near(game::SectorEditorPreviewHudScale(
            Rectangle{100.0f, 50.0f, 960.0f, 540.0f}), 0.5f));
    assert(Near(game::SectorEditorPreviewHudScale(
            Rectangle{0.0f, 0.0f, 3840.0f, 2160.0f}), 2.0f));

    const auto full = game::BuildSectorEditorPreviewCrosshairLayout(
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
    const auto odd = game::BuildSectorEditorPreviewCrosshairLayout(
            crosshair, oddViewport, 1.0f);
    assert(Near(odd.center.x, 973.0f));
    assert(Near(odd.center.y, 557.0f));

    const auto half = game::BuildSectorEditorPreviewCrosshairLayout(
            crosshair,
            Rectangle{100.0f, 50.0f, 960.0f, 540.0f},
            0.5f);
    assert(Near(half.center.x, 580.0f) && Near(half.center.y, 320.0f));
    assert(Near(half.segments[0].inner.width, 3.0f));
    assert(Near(half.segments[0].inner.height, 1.0f));

    const auto twice = game::BuildSectorEditorPreviewCrosshairLayout(
            crosshair,
            Rectangle{0.0f, 0.0f, 3840.0f, 2160.0f},
            2.0f);
    assert(Near(twice.center.x, 1920.0f)
            && Near(twice.center.y, 1080.0f));
    assert(Near(twice.segments[0].inner.width, 12.0f));
    assert(Near(twice.segments[0].inner.height, 4.0f));

    runtime.attachment.handModelTransform = MatrixTranslate(10.0f, 20.0f, 30.0f);
    runtime.attachment.pistolWorldTransform = MatrixRotateY(1.25f);
    const auto afterWeaponTransforms =
            game::BuildSectorEditorPreviewCrosshairLayout(
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
    assert(!game::ToggleFpsViewmodelHolster(state,false,false));
    state.loadState=game::FpsViewmodelLoadState::Pending;
    assert(!game::ToggleFpsViewmodelHolster(state,true,true));
    assert(game::ToggleFpsViewmodelHolster(state,true,false) && state.holstered);
    assert(!game::IsFpsViewmodelRenderable(state));
    state.loadState=game::FpsViewmodelLoadState::Failed; state.holstered=false;
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
    state.holstered = true;
    assert(!game::IsFpsViewmodelRenderable(state));
    assert(!game::IsFpsViewmodelAttachmentRenderable(state));
    state.holstered = false;
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
}

} // namespace

int main()
{
    RegistrySuccess(); RegistryValidation(); SettingsResolutionAndPersistence();
    CameraMath(); AnimationTiming(); AttachmentMathAndBoneResolution();
    PreparedPistolFrameTwentyFit(); BrightnessMapping();
    MaterialOverrideApplication(); CrosshairVisibilityAndLayout();
    RuntimeStateAndGating();
    std::cout << "FPS viewmodel tests passed\n";
}
