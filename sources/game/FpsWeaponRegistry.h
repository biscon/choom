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

struct FpsWeaponViewmodelDefinition {
    std::string modelPath;
    std::string idleAnimation;
    float sourceFps = 30.0f;
    int firstFrame = 1;
    int lastFrame = 41;
    float playbackSpeed = 1.0f;
    FpsViewmodelPresentation presentation;
    float brightnessAdjustment = 0.0f;
    FpsViewmodelMaterialOverride materialOverride;
    FpsViewmodelAttachmentDefinition attachment;
};

struct FpsWeaponDefinition {
    std::string id;
    FpsWeaponCrosshairDefinition crosshair;
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

struct FpsViewmodelAttachmentLightingOverride {
    std::optional<float> brightnessAdjustment;
    std::optional<float> metallicFactor;
    std::optional<float> roughnessFactor;
};

struct FpsApplicationSettingsEntry {
    std::string weaponId;
    FpsViewmodelPresentationOverride viewmodel;
    FpsViewmodelGripCorrectionOverride gripCorrection;
    FpsViewmodelAttachmentLightingOverride attachmentLighting;
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

} // namespace game
