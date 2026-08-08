#pragma once

#include <raylib.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace game {

struct FpsViewmodelPresentation {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotationDegrees{0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
    float verticalFovDegrees = 65.0f;
};

struct FpsViewmodelHolsterTransition {
    float holsterDurationSeconds = 0.25f;
    float unholsterDurationSeconds = 0.34f;
    Vector3 hiddenTranslation{0.0f, 0.0f, 0.0f};
    Vector3 hiddenRotationDegrees{0.0f, 0.0f, 0.0f};
};

struct FpsViewmodelMaterialOverride {
    bool enabled = false;
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    bool useMetallicRoughnessTexture = true;
};

struct FpsViewmodelGripCorrection {
    Vector3 translation{0.0f, 0.0f, 0.0f};
    Vector3 rotationDegrees{0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
};

struct FpsViewmodelAttachmentLighting {
    float brightnessAdjustment = 0.0f;
    FpsViewmodelMaterialOverride materialOverride;
};

struct FpsViewmodelAttachmentDefinition {
    std::string modelPath;
    std::string boneName;
    FpsViewmodelGripCorrection gripCorrection;
    FpsViewmodelAttachmentLighting lighting;
};

struct FpsWeaponCrosshairDefinition {
    bool enabled = false;
    Color innerColor{235, 235, 225, 255};
    Color outlineColor{0, 0, 0, 220};
    float centerGapPixels = 4.0f;
    float segmentLengthPixels = 6.0f;
    float innerThicknessPixels = 2.0f;
    float outlineThicknessPixels = 1.0f;
};

struct FpsWeaponRecoilDefinition {
    Vector3 translationImpulse{0.0f, 0.0f, -0.03f};
    Vector3 rotationImpulseDegrees{-3.0f, 0.0f, 0.0f};
    float rollVariationDegrees = 0.45f;
    float springFrequencyHz = 8.0f;
    float dampingRatio = 0.82f;
    Vector3 maximumTranslation{0.05f, 0.05f, 0.08f};
    Vector3 maximumRotationDegrees{8.0f, 2.0f, 2.0f};
};

struct FpsWeaponMuzzleSocketDefinition {
    Vector3 position{0.0f, 0.035f, 0.105f};
    Vector3 rotationDegrees{0.0f, 0.0f, 0.0f};
};

struct FpsWeaponMuzzleFlashDefinition {
    bool enabled = true;
    float lifetimeSeconds = 0.055f;
    float sizeWorld = 0.10f;
    float sizeVariation = 0.20f;
    Color coreColor{255, 255, 245, 255};
    Color hotColor{255, 235, 120, 255};
    Color warmColor{255, 90, 15, 230};
    Color edgeColor{120, 15, 5, 150};
    float edgeSoftness = 0.35f;
};

struct FpsWeaponMuzzleLightDefinition {
    bool enabled = true;
    Color color{255, 165, 70, 255};
    float intensity = 6.0f;
    float radiusWorld = 2.5f;
    float lifetimeSeconds = 0.07f;
    float decayExponent = 2.5f;
};

struct FpsWeaponFiringDefinition {
    float shotIntervalSeconds = 0.18f;
    float maximumRangeWorld = 100.0f;
    FpsWeaponRecoilDefinition recoil;
    FpsWeaponMuzzleSocketDefinition muzzleSocket;
    FpsWeaponMuzzleFlashDefinition muzzleFlash;
    FpsWeaponMuzzleLightDefinition muzzleLight;
};

struct FpsWeaponViewmodelDefinition {
    std::string modelPath;
    std::string idleAnimation;
    float sourceFps = 30.0f;
    int firstFrame = 1;
    int lastFrame = 41;
    float playbackSpeed = 1.0f;
    FpsViewmodelPresentation presentation;
    FpsViewmodelHolsterTransition holsterTransition;
    float brightnessAdjustment = 0.0f;
    FpsViewmodelMaterialOverride materialOverride;
    FpsViewmodelAttachmentDefinition attachment;
};

struct FpsWeaponDefinition {
    std::string id;
    FpsWeaponCrosshairDefinition crosshair;
    FpsWeaponFiringDefinition firing;
    FpsWeaponViewmodelDefinition viewmodel;
};

struct FpsWeaponRegistry {
    int version = 1;
    std::string initialWeaponId;
    std::vector<FpsWeaponDefinition> weapons;
};

struct FpsViewmodelPresentationOverride {
    std::optional<Vector3> position;
    std::optional<Vector3> rotationDegrees;
    std::optional<float> scale;
    std::optional<float> verticalFovDegrees;
};

struct FpsViewmodelGripCorrectionOverride {
    std::optional<Vector3> translation;
    std::optional<Vector3> rotationDegrees;
    std::optional<float> scale;
};

struct FpsViewmodelHolsterTransitionOverride {
    std::optional<float> holsterDurationSeconds;
    std::optional<float> unholsterDurationSeconds;
    std::optional<Vector3> hiddenTranslation;
    std::optional<Vector3> hiddenRotationDegrees;
};

struct FpsViewmodelAttachmentLightingOverride {
    std::optional<float> brightnessAdjustment;
    std::optional<float> metallicFactor;
    std::optional<float> roughnessFactor;
};

struct FpsWeaponFiringOverride {
    std::optional<float> shotIntervalSeconds;
    std::optional<Vector3> recoilTranslationImpulse;
    std::optional<Vector3> recoilRotationImpulseDegrees;
    std::optional<float> recoilRollVariationDegrees;
    std::optional<float> recoilSpringFrequencyHz;
    std::optional<float> recoilDampingRatio;
    std::optional<Vector3> muzzlePosition;
    std::optional<Vector3> muzzleRotationDegrees;
    std::optional<float> flashLifetimeSeconds;
    std::optional<float> flashSizeWorld;
    std::optional<float> flashEdgeSoftness;
    std::optional<float> muzzleLightIntensity;
    std::optional<float> muzzleLightRadiusWorld;
    std::optional<float> muzzleLightLifetimeSeconds;
};

struct FpsApplicationSettingsEntry {
    std::string weaponId;
    FpsViewmodelPresentationOverride viewmodel;
    FpsViewmodelHolsterTransitionOverride holsterTransition;
    FpsViewmodelGripCorrectionOverride gripCorrection;
    FpsViewmodelAttachmentLightingOverride attachmentLighting;
    FpsWeaponFiringOverride firing;
};

struct FpsApplicationSettings {
    int version = 1;
    std::vector<FpsApplicationSettingsEntry> weapons;
};

bool ParseFpsWeaponRegistry(
        std::string_view jsonText,
        FpsWeaponRegistry& outRegistry,
        std::string* outError = nullptr);
bool LoadFpsWeaponRegistry(
        const std::string& path,
        FpsWeaponRegistry& outRegistry,
        std::string* outError = nullptr);
const FpsWeaponDefinition* FindFpsWeaponDefinition(
        const FpsWeaponRegistry& registry,
        std::string_view id);

bool ParseFpsApplicationSettings(
        std::string_view jsonText,
        FpsApplicationSettings& outSettings,
        std::string* outError = nullptr);
bool LoadFpsApplicationSettings(
        const std::string& path,
        FpsApplicationSettings& outSettings,
        std::string* outError = nullptr);
bool SaveFpsApplicationSettings(
        const std::string& path,
        const FpsApplicationSettings& settings,
        std::string* outError = nullptr);

const FpsViewmodelPresentationOverride* FindFpsViewmodelOverride(
        const FpsApplicationSettings& settings,
        std::string_view weaponId);
void SetFpsViewmodelOverride(
        FpsApplicationSettings& settings,
        std::string weaponId,
        const FpsViewmodelPresentationOverride& value);
void ClearFpsViewmodelOverride(FpsApplicationSettings& settings, std::string_view weaponId);

const FpsViewmodelHolsterTransitionOverride*
FindFpsViewmodelHolsterTransitionOverride(
        const FpsApplicationSettings& settings,
        std::string_view weaponId);
void SetFpsViewmodelHolsterTransitionOverride(
        FpsApplicationSettings& settings,
        std::string weaponId,
        const FpsViewmodelHolsterTransitionOverride& value);
void ClearFpsViewmodelHolsterTransitionOverride(
        FpsApplicationSettings& settings,
        std::string_view weaponId);

const FpsViewmodelGripCorrectionOverride* FindFpsViewmodelGripCorrectionOverride(
        const FpsApplicationSettings& settings,
        std::string_view weaponId);
void SetFpsViewmodelGripCorrectionOverride(
        FpsApplicationSettings& settings,
        std::string weaponId,
        const FpsViewmodelGripCorrectionOverride& value);
void ClearFpsViewmodelGripCorrectionOverride(
        FpsApplicationSettings& settings,
        std::string_view weaponId);

const FpsViewmodelAttachmentLightingOverride* FindFpsViewmodelAttachmentLightingOverride(
        const FpsApplicationSettings& settings,
        std::string_view weaponId);
void SetFpsViewmodelAttachmentLightingOverride(
        FpsApplicationSettings& settings,
        std::string weaponId,
        const FpsViewmodelAttachmentLightingOverride& value);
void ClearFpsViewmodelAttachmentLightingOverride(
        FpsApplicationSettings& settings,
        std::string_view weaponId);

FpsViewmodelPresentation ResolveFpsViewmodelPresentation(
        const FpsViewmodelPresentation& defaults,
        const FpsViewmodelPresentationOverride* overrideValue);
FpsViewmodelPresentation ClampFpsViewmodelPresentation(FpsViewmodelPresentation value);
FpsViewmodelPresentationOverride BuildFpsViewmodelOverride(
        const FpsViewmodelPresentation& defaults,
        const FpsViewmodelPresentation& effective);
bool FpsViewmodelOverrideEmpty(const FpsViewmodelPresentationOverride& value);

FpsViewmodelHolsterTransition ResolveFpsViewmodelHolsterTransition(
        const FpsViewmodelHolsterTransition& defaults,
        const FpsViewmodelHolsterTransitionOverride* overrideValue);
FpsViewmodelHolsterTransition ClampFpsViewmodelHolsterTransition(
        FpsViewmodelHolsterTransition value);
FpsViewmodelHolsterTransitionOverride BuildFpsViewmodelHolsterTransitionOverride(
        const FpsViewmodelHolsterTransition& defaults,
        const FpsViewmodelHolsterTransition& effective);
bool FpsViewmodelHolsterTransitionOverrideEmpty(
        const FpsViewmodelHolsterTransitionOverride& value);

FpsViewmodelGripCorrection ResolveFpsViewmodelGripCorrection(
        const FpsViewmodelGripCorrection& defaults,
        const FpsViewmodelGripCorrectionOverride* overrideValue);
FpsViewmodelGripCorrection ClampFpsViewmodelGripCorrection(
        FpsViewmodelGripCorrection value);
FpsViewmodelGripCorrectionOverride BuildFpsViewmodelGripCorrectionOverride(
        const FpsViewmodelGripCorrection& defaults,
        const FpsViewmodelGripCorrection& effective);
bool FpsViewmodelGripCorrectionOverrideEmpty(
        const FpsViewmodelGripCorrectionOverride& value);

FpsViewmodelAttachmentLighting ResolveFpsViewmodelAttachmentLighting(
        const FpsViewmodelAttachmentLighting& defaults,
        const FpsViewmodelAttachmentLightingOverride* overrideValue);
FpsViewmodelAttachmentLighting ClampFpsViewmodelAttachmentLighting(
        FpsViewmodelAttachmentLighting value);
FpsViewmodelAttachmentLightingOverride BuildFpsViewmodelAttachmentLightingOverride(
        const FpsViewmodelAttachmentLighting& defaults,
        const FpsViewmodelAttachmentLighting& effective);
bool FpsViewmodelAttachmentLightingOverrideEmpty(
        const FpsViewmodelAttachmentLightingOverride& value);

const FpsWeaponFiringOverride* FindFpsWeaponFiringOverride(
        const FpsApplicationSettings& settings,
        std::string_view weaponId);
void SetFpsWeaponFiringOverride(
        FpsApplicationSettings& settings,
        std::string weaponId,
        const FpsWeaponFiringOverride& value);
void ClearFpsWeaponFiringOverride(
        FpsApplicationSettings& settings,
        std::string_view weaponId);
FpsWeaponFiringDefinition ResolveFpsWeaponFiringDefinition(
        const FpsWeaponFiringDefinition& defaults,
        const FpsWeaponFiringOverride* overrideValue);
FpsWeaponFiringDefinition ClampFpsWeaponFiringDefinition(
        FpsWeaponFiringDefinition value);
FpsWeaponFiringOverride BuildFpsWeaponFiringOverride(
        const FpsWeaponFiringDefinition& defaults,
        const FpsWeaponFiringDefinition& effective);
bool FpsWeaponFiringOverrideEmpty(const FpsWeaponFiringOverride& value);

} // namespace game
