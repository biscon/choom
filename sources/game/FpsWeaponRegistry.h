#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/render/HdrEffectPolicy.h"
#include "game/PlayerStamina.h"

#include <raylib.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine { class AssetManager; }

namespace game {

constexpr int MaxFpsMuzzleFlashLobes = 12;
constexpr int MaxFpsWeaponPellets = 32;
constexpr int MinFpsWeaponSlot = 1;
constexpr int MaxFpsWeaponSlot = 6;

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

struct FpsWeaponCameraRecoilDefinition {
    bool enabled = false;
    float pitchKickDegrees = 0.0f;
    float pitchVariationDegrees = 0.0f;
    float yawVariationDegrees = 0.0f;
    float rollVariationDegrees = 0.0f;
    float springFrequencyHz = 5.5f;
    float springDampingRatio = 0.97f;
    float maxPitchDegrees = 0.0f;
    float maxYawDegrees = 0.0f;
    float maxRollDegrees = 0.0f;
};

struct FpsWeaponMuzzleSocketDefinition {
    Vector3 position{0.0f, 0.035f, 0.105f};
    Vector3 rotationDegrees{0.0f, 0.0f, 0.0f};
};

struct FpsWeaponMuzzleFlashDefinition {
    bool enabled = true;
    float lifetimeSeconds = 0.033f;
    float sizeWorld = 0.10f;
    float sizeVariation = 0.12f;
    float irregularity = 0.65f;
    float forwardStretch = 1.80f;
    int minimumLobeCount = 5;
    int maximumLobeCount = 8;
    float rearSuppression = 0.90f;
    Color coreColor{255, 255, 245, 255};
    Color hotColor{255, 235, 120, 255};
    Color warmColor{255, 90, 15, 230};
    Color edgeColor{120, 15, 5, 150};
    float edgeSoftness = 0.35f;
    float radianceStrength = 8.0f;
};

struct FpsWeaponMuzzleLightDefinition {
    bool enabled = true;
    Color color{255, 165, 70, 255};
    float intensity = 6.0f;
    float radiusWorld = 2.5f;
    float lifetimeSeconds = 0.07f;
    float decayExponent = 2.5f;
};

struct FpsWeaponImpactParticlesDefinition {
    bool enabled = false;
    int particleCount = 0;
    float sizeScale = 1.0f;
    float intensity = 1.0f;
};

struct FpsWeaponImpactDefinition {
    int damage = 0;
    float staggerSeconds = 0.0f;
    float knockbackImpulseWorldPerSecond = 0.0f;
    FpsWeaponImpactParticlesDefinition blood;
    FpsWeaponImpactParticlesDefinition surfaceDebris;
};

struct FpsWeaponPelletDefinition {
    bool enabled = false;
    int count = 8;
    float spreadHalfAngleDegrees = 6.0f;
};

struct FpsWeaponFiringDefinition {
    float shotIntervalSeconds = 0.18f;
    float maximumRangeWorld = 100.0f;
    FpsWeaponPelletDefinition pellets;
    std::string shootSoundPath;
    engine::SoundHandle shootSound = engine::NullSoundHandle();
    FpsWeaponRecoilDefinition recoil;
    FpsWeaponCameraRecoilDefinition cameraRecoil;
    FpsWeaponMuzzleSocketDefinition muzzleSocket;
    FpsWeaponMuzzleFlashDefinition muzzleFlash;
    FpsWeaponMuzzleLightDefinition muzzleLight;
    FpsWeaponImpactDefinition impact;
};

struct FpsWeaponReloadDefinition {
    int magazineSize = 1;
    float durationSeconds = 1.0f;
    std::string dryFireSoundPath;
    engine::SoundHandle dryFireSound = engine::NullSoundHandle();
    std::string reloadSoundPath;
    engine::SoundHandle reloadSound = engine::NullSoundHandle();
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
    int weaponSlot = 0;
    FpsWeaponCrosshairDefinition crosshair;
    FpsWeaponFiringDefinition firing;
    FpsWeaponReloadDefinition reload;
    FpsWeaponViewmodelDefinition viewmodel;
};

struct FpsWeaponRegistry {
    int version = 3;
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
    std::optional<bool> cameraRecoilEnabled;
    std::optional<float> cameraRecoilPitchKickDegrees;
    std::optional<float> cameraRecoilPitchVariationDegrees;
    std::optional<float> cameraRecoilYawVariationDegrees;
    std::optional<float> cameraRecoilRollVariationDegrees;
    std::optional<float> cameraRecoilSpringFrequencyHz;
    std::optional<float> cameraRecoilSpringDampingRatio;
    std::optional<float> cameraRecoilMaxPitchDegrees;
    std::optional<float> cameraRecoilMaxYawDegrees;
    std::optional<float> cameraRecoilMaxRollDegrees;
    std::optional<Vector3> muzzlePosition;
    std::optional<Vector3> muzzleRotationDegrees;
    std::optional<float> flashLifetimeSeconds;
    std::optional<float> flashSizeWorld;
    std::optional<float> flashSizeVariation;
    std::optional<float> flashIrregularity;
    std::optional<float> flashForwardStretch;
    std::optional<int> flashMinimumLobeCount;
    std::optional<int> flashMaximumLobeCount;
    std::optional<float> flashRearSuppression;
    std::optional<float> flashEdgeSoftness;
    std::optional<float> flashRadianceStrength;
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

struct FootstepApplicationSettings {
    std::string defaultSet = "Tile_Mono";
    float volume = 0.65f;
    float landingImpactVolumeMultiplier = 1.35f;
};

struct PlayerSoundEventSettings {
    std::string id;
    std::string set;
    float volume = 1.0f;
};

struct PlayerSoundApplicationSettings {
    std::vector<PlayerSoundEventSettings> events{
            PlayerSoundEventSettings{"jump", "Jump", 1.0f},
            PlayerSoundEventSettings{"land", "Land", 1.0f}};
};

enum class FpsShadowQuality {
    Off,
    Low,
    Medium,
    High
};

inline constexpr int MinFpsHorizontalFovDegrees = 70;
inline constexpr int MaxFpsHorizontalFovDegrees = 120;
inline constexpr int DefaultFpsHorizontalFovDegrees = 108;
inline constexpr int MinFpsDynamicLights = 0;
inline constexpr int MaxFpsDynamicLights = 32;
inline constexpr int DefaultFpsDynamicLights = 32;
inline constexpr float MinFpsDynamicLightFadeInSeconds = 0.0f;
inline constexpr float MaxFpsDynamicLightFadeInSeconds = 2.0f;
inline constexpr float DefaultFpsDynamicLightFadeInSeconds = 0.25f;
inline constexpr int MinFpsShadowLightUpdatesPerFrame = 0;
inline constexpr int MaxFpsShadowLightUpdatesPerFrame = 32;
inline constexpr int DefaultFpsShadowLightUpdatesPerFrame = 2;

struct FpsGraphicsSettings {
    float renderScale = 1.5f;
    bool fxaa = true;
    FpsShadowQuality shadowQuality = FpsShadowQuality::High;
    int maxDynamicLights = DefaultFpsDynamicLights;
    int maxShadowLightUpdatesPerFrame = DefaultFpsShadowLightUpdatesPerFrame;
    float dynamicLightFadeInSeconds = DefaultFpsDynamicLightFadeInSeconds;
    bool depthPrepass = false;
    bool showFpsCounter = false;
    bool performanceOverlay = false;
    bool vsync = true;
    int horizontalFovDegrees = DefaultFpsHorizontalFovDegrees;
};

struct PlayerInventoryApplicationSettings {
    float maxCarryWeightKg = 30.0f;
    int maxSlots = 24;
    float pickupVacuumDurationSeconds = 0.65f;
    float pickupVacuumTargetHeightWorld = 0.75f;
};

struct FpsApplicationSettings {
    int version = 1;
    std::string firstLevel = "hub";
    bool consoleEnabled = true;
    FootstepApplicationSettings footsteps;
    PlayerSoundApplicationSettings playerSounds;
    PlayerStaminaApplicationSettings playerStamina;
    PlayerInventoryApplicationSettings playerInventory;
    FpsGraphicsSettings graphics;
    engine::HdrBloomSettings hdrBloom;
    std::vector<FpsApplicationSettingsEntry> weapons;
};

const char* FpsShadowQualityName(FpsShadowQuality quality);
FpsGraphicsSettings NormalizeFpsGraphicsSettings(FpsGraphicsSettings settings);
float FpsVerticalFovDegrees(int horizontalFovDegrees, float aspectRatio);

bool ParseFpsWeaponRegistry(
        std::string_view jsonText,
        FpsWeaponRegistry& outRegistry,
        std::string* outError = nullptr);
FpsWeaponDefinition MakeDefaultFpsWeaponDefinition();
bool ValidateFpsWeaponRegistry(
        const FpsWeaponRegistry& registry,
        std::string* outError = nullptr);
bool SerializeFpsWeaponRegistryJson(
        const FpsWeaponRegistry& registry,
        std::string& outJson,
        std::string* outError = nullptr);
bool LoadFpsWeaponRegistry(
        const std::string& path,
        FpsWeaponRegistry& outRegistry,
        std::string* outError = nullptr);
bool SaveFpsWeaponRegistry(
        const std::string& path,
        const FpsWeaponRegistry& registry,
        std::string* outError = nullptr);
void ApplyFpsApplicationWeaponOverrides(
        FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings);
void RequestFpsWeaponAudioAssets(
        engine::AssetManager& assets,
        FpsWeaponRegistry& registry);
const FpsWeaponDefinition* FindFpsWeaponDefinition(
        const FpsWeaponRegistry& registry,
        std::string_view id);
const FpsWeaponDefinition* FindFpsWeaponDefinitionForSlot(
        const FpsWeaponRegistry& registry,
        int weaponSlot);
int FpsWeaponSlotFromKey(int key);

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
FpsWeaponCameraRecoilDefinition ClampFpsWeaponCameraRecoilDefinition(
        FpsWeaponCameraRecoilDefinition value);
FpsWeaponFiringOverride BuildFpsWeaponFiringOverride(
        const FpsWeaponFiringDefinition& defaults,
        const FpsWeaponFiringDefinition& effective);
bool FpsWeaponFiringOverrideEmpty(const FpsWeaponFiringOverride& value);

} // namespace game
