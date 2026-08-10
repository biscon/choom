#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyTypes.h"
#include "game/FpsWeaponRegistry.h"
#include "game/FootstepAudio.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace game {

enum class TopologyTexturePickerTargetKind {
    None,
    Sector,
    SideDef,
    AuthoringFaceAnchor,
    AuthoringSide,
    MapSky,
    RuntimeDoor
};

enum class PreviewSettingsTab {
    General,
    Sky,
    Lighting,
    Fog,
    Viewmodel,
    Weapon
};

struct TexturePickerState {
    bool open = false;
    bool rebuildPreviewOnApply = false;
    bool authoringSurface3DFlatTarget = false;
    TopologyTexturePickerTargetKind topologyTargetKind = TopologyTexturePickerTargetKind::None;
    TopologyMaterialLayer topologyLayer = TopologyMaterialLayer::Base;
    TopologySectorTextureField topologyField = TopologySectorTextureField::None;
    int topologySectorId = -1;
    int topologySideDefId = -1;
    TopologyWallPart topologyWallPart = TopologyWallPart::Wall;
    int authoringFaceAnchorId = -1;
    int authoringLineId = -1;
    SectorTopologySideKind authoringSide = SectorTopologySideKind::Front;
    int runtimeObjectId = -1;
    int selectedTextureIndex = -1;
    engine::UIScrollState scroll;
    std::vector<std::string> textureIds;
    std::vector<const char*> optionLabels;
};

struct FootstepPickerState {
    bool open = false;
    int authoringFaceAnchorId = -1;
    int selectedSetIndex = -1;
    engine::UIScrollState scroll;
    FootstepCatalog catalog;
    std::vector<std::string> setIds;
    std::vector<std::string> labelStorage;
    std::vector<const char*> optionLabels;
    std::string message;
    engine::AssetScopeHandle previewScope = engine::NullAssetScopeHandle();
    LoadedFootstepSet previewSet;
    FootstepPlaybackState previewPlayback;
    bool previewPending = false;
};

struct AddMapTextureState {
    bool open = false;
    bool scanned = false;
    std::string scanMessage;
    engine::UIScrollState scroll;
    std::vector<std::string> paths;
    std::vector<const char*> optionLabels;
    int selectedPathIndex = -1;
    char textureIdBuffer[96] = {};
    SectorTextureFilter filter = SectorTextureFilter::Anisotropic8x;
    std::string validationMessage;
    engine::AssetScopeHandle previewScope = engine::NullAssetScopeHandle();
    engine::TextureHandle previewTexture = engine::NullTextureHandle();
    std::string previewPath;
    SectorTextureFilter previewFilter = SectorTextureFilter::Anisotropic8x;
};

enum class SectorEditorDoorSoundTarget {
    Open,
    Close
};

struct SectorEditorAudioPreviewState {
    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    engine::SoundHandle sound = engine::NullSoundHandle();
    engine::MusicHandle music = engine::NullMusicHandle();
    engine::SoundPlaybackHandle soundPlayback = engine::NullSoundPlaybackHandle();
    SectorSoundType type = SectorSoundType::Sound;
    bool pending = false;
    std::string key;
};

struct AddMapSoundState {
    bool open = false;
    bool scanned = false;
    std::string scanMessage;
    engine::UIScrollState scroll;
    std::vector<std::string> paths;
    std::vector<const char*> optionLabels;
    int selectedPathIndex = -1;
    char soundIdBuffer[96] = {};
    SectorSoundType type = SectorSoundType::Sound;
    std::string validationMessage;
    std::string previewMessage;
    SectorEditorAudioPreviewState preview;
};

struct SoundPickerState {
    bool open = false;
    SectorEditorDoorSoundTarget target = SectorEditorDoorSoundTarget::Open;
    int runtimeObjectId = -1;
    int selectedSoundIndex = -1;
    engine::UIScrollState scroll;
    std::vector<std::string> soundIds;
    std::vector<std::string> labelStorage;
    std::vector<const char*> optionLabels;
    std::string message;
    SectorEditorAudioPreviewState preview;
};

struct SectorSpriteMetadata {
    std::string spriteAnimationPath;
    std::string atlasImagePath;
    std::vector<std::string> clipNames;
};

struct SectorSpriteMetadataCatalog {
    bool scanned = false;
    std::string scanMessage;
    std::vector<SectorSpriteMetadata> sprites;
};

struct SpritePickerState {
    bool open = false;
    bool scanned = false;
    std::string scanMessage;
    engine::UIScrollState spriteScroll;
    std::vector<SectorSpriteMetadata> sprites;
    std::vector<const char*> spriteOptionLabels;
    int selectedSpriteIndex = -1;
    std::string requestedSpriteAnimationPath;
    engine::AssetScopeHandle previewScope = engine::NullAssetScopeHandle();
    engine::TextureHandle previewTexture = engine::NullTextureHandle();
    std::string previewAtlasPath;
};

struct SaveLevelModalState {
    bool open = false;
    char nameBuffer[96] = {};
    std::string errorMessage;
};

struct LevelListEntry {
    std::string name;
    std::string jsonAssetPath;
};

struct LoadLevelModalState {
    bool open = false;
    std::vector<LevelListEntry> levels;
    std::vector<const char*> optionLabels;
    int selectedIndex = -1;
    engine::UIScrollState scroll;
    std::string errorMessage;
};

struct ConfirmationModalState {
    bool open = false;
    std::string title;
    std::string message;
    std::function<void()> onOkay;
};

struct SectorEditorSetAllModalState {
    bool open = false;
    float ambientIntensity = 1.0f;
    Color ambientColor = WHITE;
    engine::UIFloatInputState ambientIntensityInput;
    engine::UIIntInputState ambientRedInput;
    engine::UIIntInputState ambientGreenInput;
    engine::UIIntInputState ambientBlueInput;
};

struct DecalTintModalState {
    bool open = false;
    TopologySurfaceEditTarget target;
    Vector3 tint = {1.0f, 1.0f, 1.0f};
    engine::UIFloatInputState redInput;
    engine::UIFloatInputState greenInput;
    engine::UIFloatInputState blueInput;
    std::string errorMessage;
};

struct DoorTextureSettingsModalState {
    bool open = false;
    int runtimeObjectId = -1;
    SectorDoorFace selectedFace = SectorDoorFace::Front;
    engine::UIFloatInputState scaleUInput;
    engine::UIFloatInputState scaleVInput;
    engine::UIFloatInputState offsetUInput;
    engine::UIFloatInputState offsetVInput;
    std::string statusMessage;
};

struct SectorPreviewSettingsModalState {
    bool open = false;
    PreviewSettingsTab activeTab = PreviewSettingsTab::General;
    std::string weaponId;
    SectorFpsControllerConfig draftConfig;
    SectorTopologySkySettings draftSkySettings;
    SectorTopologyDirectionalLightSettings draftDirectionalLight;
    SectorTopologyFogSettings draftFogSettings;
    SectorLightmapBakeSettings draftLightmapSettings;
    FpsViewmodelPresentation viewmodelDefaults;
    FpsViewmodelPresentation draftViewmodel;
    FpsViewmodelHolsterTransition viewmodelHolsterTransitionDefaults;
    FpsViewmodelHolsterTransition draftViewmodelHolsterTransition;
    FpsViewmodelGripCorrection viewmodelGripDefaults;
    FpsViewmodelGripCorrection draftViewmodelGrip;
    FpsViewmodelAttachmentLighting viewmodelAttachmentLightingDefaults;
    FpsViewmodelAttachmentLighting draftViewmodelAttachmentLighting;
    FpsWeaponFiringDefinition weaponFiringDefaults;
    FpsWeaponFiringDefinition draftWeaponFiring;
    engine::HdrBloomSettings draftHdrBloom;
    engine::UIFloatInputState walkSpeedInput;
    engine::UIFloatInputState runSpeedInput;
    engine::UIFloatInputState mouseSensitivityInput;
    engine::UIFloatInputState eyeHeightInput;
    engine::UIFloatInputState gravityInput;
    engine::UIFloatInputState playerRadiusInput;
    engine::UIFloatInputState playerHeightInput;
    engine::UIFloatInputState stepHeightInput;
    engine::UIFloatInputState jumpHeightInput;
    engine::UIFloatInputState headBobStrengthInput;
    engine::UIFloatInputState headBobFrequencyInput;
    engine::UIFloatInputState skyYawOffsetInput;
    engine::UIFloatInputState skyVerticalOffsetInput;
    engine::UIFloatInputState skyVerticalScaleInput;
    engine::UIIntInputState skyTopColorRedInput;
    engine::UIIntInputState skyTopColorGreenInput;
    engine::UIIntInputState skyTopColorBlueInput;
    engine::UIFloatInputState lightDirectionXInput;
    engine::UIFloatInputState lightDirectionYInput;
    engine::UIFloatInputState lightDirectionZInput;
    engine::UIFloatInputState lightIntensityInput;
    engine::UIFloatInputState objectProbeSpacingInput;
    engine::UIFloatInputState objectProbeLowerHeightInput;
    engine::UIFloatInputState objectProbeUpperHeightInput;
    engine::UIFloatInputState bloomThresholdInput;
    engine::UIFloatInputState bloomSoftKneeInput;
    engine::UIFloatInputState bloomIntensityInput;
    engine::UIFloatInputState bloomRadiusInput;
    engine::UIIntInputState lightColorRedInput;
    engine::UIIntInputState lightColorGreenInput;
    engine::UIIntInputState lightColorBlueInput;
    engine::UIFloatInputState fogStartDistanceInput;
    engine::UIFloatInputState fogDensityInput;
    engine::UIFloatInputState fogMaxOpacityInput;
    engine::UIFloatInputState fogReferenceHeightInput;
    engine::UIFloatInputState fogHeightFalloffInput;
    engine::UIIntInputState fogColorRedInput;
    engine::UIIntInputState fogColorGreenInput;
    engine::UIIntInputState fogColorBlueInput;
    engine::UIFloatInputState viewmodelPositionXInput;
    engine::UIFloatInputState viewmodelPositionYInput;
    engine::UIFloatInputState viewmodelPositionZInput;
    engine::UIFloatInputState viewmodelPitchInput;
    engine::UIFloatInputState viewmodelYawInput;
    engine::UIFloatInputState viewmodelRollInput;
    engine::UIFloatInputState viewmodelScaleInput;
    engine::UIFloatInputState viewmodelFovInput;
    engine::UIFloatInputState cameraRecoilPitchKickInput;
    engine::UIFloatInputState cameraRecoilPitchVariationInput;
    engine::UIFloatInputState cameraRecoilYawVariationInput;
    engine::UIFloatInputState cameraRecoilRollVariationInput;
    engine::UIFloatInputState cameraRecoilFrequencyInput;
    engine::UIFloatInputState cameraRecoilDampingInput;
    engine::UIFloatInputState cameraRecoilMaxPitchInput;
    engine::UIFloatInputState cameraRecoilMaxYawInput;
    engine::UIFloatInputState cameraRecoilMaxRollInput;
    engine::UIFloatInputState viewmodelHolsterDurationInput;
    engine::UIFloatInputState viewmodelUnholsterDurationInput;
    engine::UIFloatInputState viewmodelHiddenTranslationXInput;
    engine::UIFloatInputState viewmodelHiddenTranslationYInput;
    engine::UIFloatInputState viewmodelHiddenTranslationZInput;
    engine::UIFloatInputState viewmodelHiddenPitchInput;
    engine::UIFloatInputState viewmodelHiddenYawInput;
    engine::UIFloatInputState viewmodelHiddenRollInput;
    engine::UIFloatInputState viewmodelGripTranslationXInput;
    engine::UIFloatInputState viewmodelGripTranslationYInput;
    engine::UIFloatInputState viewmodelGripTranslationZInput;
    engine::UIFloatInputState viewmodelGripPitchInput;
    engine::UIFloatInputState viewmodelGripYawInput;
    engine::UIFloatInputState viewmodelGripRollInput;
    engine::UIFloatInputState viewmodelGripScaleInput;
    engine::UIFloatInputState viewmodelAttachmentBrightnessInput;
    engine::UIFloatInputState viewmodelAttachmentMetallicInput;
    engine::UIFloatInputState viewmodelAttachmentRoughnessInput;
    engine::UIFloatInputState weaponShotIntervalInput;
    engine::UIFloatInputState weaponRecoilTranslationXInput;
    engine::UIFloatInputState weaponRecoilTranslationYInput;
    engine::UIFloatInputState weaponRecoilTranslationZInput;
    engine::UIFloatInputState weaponRecoilPitchInput;
    engine::UIFloatInputState weaponRecoilYawInput;
    engine::UIFloatInputState weaponRecoilRollInput;
    engine::UIFloatInputState weaponRecoilRollVariationInput;
    engine::UIFloatInputState weaponRecoilFrequencyInput;
    engine::UIFloatInputState weaponRecoilDampingInput;
    engine::UIFloatInputState weaponMuzzlePositionXInput;
    engine::UIFloatInputState weaponMuzzlePositionYInput;
    engine::UIFloatInputState weaponMuzzlePositionZInput;
    engine::UIFloatInputState weaponMuzzlePitchInput;
    engine::UIFloatInputState weaponMuzzleYawInput;
    engine::UIFloatInputState weaponMuzzleRollInput;
    engine::UIFloatInputState weaponFlashLifetimeInput;
    engine::UIFloatInputState weaponFlashSizeInput;
    engine::UIFloatInputState weaponFlashRadianceStrengthInput;
    engine::UIFloatInputState weaponFlashSizeVariationInput;
    engine::UIFloatInputState weaponFlashIrregularityInput;
    engine::UIFloatInputState weaponFlashForwardStretchInput;
    engine::UIIntInputState weaponFlashMinimumLobesInput;
    engine::UIIntInputState weaponFlashMaximumLobesInput;
    engine::UIFloatInputState weaponFlashRearSuppressionInput;
    engine::UIFloatInputState weaponFlashEdgeSoftnessInput;
    engine::UIFloatInputState weaponLightIntensityInput;
    engine::UIFloatInputState weaponLightRadiusInput;
    engine::UIFloatInputState weaponLightLifetimeInput;
    engine::UIScrollState generalScroll;
    engine::UIScrollState skyScroll;
    engine::UIScrollState lightingScroll;
    engine::UIScrollState fogScroll;
    engine::UIScrollState viewmodelScroll;
    engine::UIScrollState weaponScroll;
    std::string errorMessage;
};

} // namespace game
