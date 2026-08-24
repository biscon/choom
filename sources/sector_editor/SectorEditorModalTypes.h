#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/render/HdrEffectPolicy.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyTypes.h"
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
    Fog
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
    std::vector<std::string> allMaterialIds;
    std::vector<std::string> materialIds;
    std::vector<const char*> optionLabels;
    char filterBuffer[256] = {};
    std::string filterMessage;
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

enum class SectorEditorDoorSoundTarget {
    Open,
    Close
};

enum class SectorEditorSoundPickerTargetKind {
    DoorOpen,
    DoorClose,
    SoundEmitter
};

struct SoundPickerState {
    bool open = false;
    SectorEditorSoundPickerTargetKind targetKind =
            SectorEditorSoundPickerTargetKind::DoorOpen;
    int targetId = -1;
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

enum class SectorEditorSectorLightingScope {
    Selected,
    All
};

struct SectorEditorSetAllModalState {
    bool open = false;
    SectorEditorSectorLightingScope scope = SectorEditorSectorLightingScope::Selected;
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

struct SectorLightmapBakeSetupModalState {
    bool open = false;
    SectorLightmapBakeQualityPreset selectedQuality =
            SectorLightmapBakeQualityPreset::Standard;
    std::string errorMessage;
};

struct SectorPreviewSettingsModalState {
    bool open = false;
    PreviewSettingsTab activeTab = PreviewSettingsTab::General;
    SectorFpsControllerConfig draftConfig;
    bool draftNpcToNpcCollisionEnabled = true;
    SectorTopologySkySettings draftSkySettings;
    SectorTopologyDirectionalLightSettings draftDirectionalLight;
    SectorTopologyFogSettings draftFogSettings;
    SectorLightmapBakeSettings draftLightmapSettings;
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
    engine::UIFloatInputState fogEndDistanceInput;
    engine::UIFloatInputState fogFalloffExponentInput;
    engine::UIFloatInputState fogBrightnessInput;
    engine::UIFloatInputState fogDensityInput;
    engine::UIFloatInputState fogMaxOpacityInput;
    engine::UIFloatInputState fogReferenceHeightInput;
    engine::UIFloatInputState fogHeightFalloffInput;
    engine::UIIntInputState fogColorRedInput;
    engine::UIIntInputState fogColorGreenInput;
    engine::UIIntInputState fogColorBlueInput;
    engine::UIScrollState generalScroll;
    engine::UIScrollState skyScroll;
    engine::UIScrollState lightingScroll;
    engine::UIScrollState fogScroll;
    std::string errorMessage;
};

} // namespace game
