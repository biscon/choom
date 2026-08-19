#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/SectorEditorLightmapAsyncTypes.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorPointTypes.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologySerialization.h"

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game {

enum class SectorEditorTool {
    Select,
    AuthoringLine,
    AuthoringRectangle,
    AuthoringInsertVertex,
    AuthoringMove,
    RuntimeObject,
    StaticModel,
    DynamicModel,
    Npc,
    Door,
    AuthoringFogVolume,
    Trigger,
    LevelMarker,
    StaticLight,
    StaticSpotLight,
    DynamicLight,
    DynamicSpotLight,
    Move
};

enum class SectorEditorMode {
    Edit2D,
    Preview3D
};

enum class TopologyUvFitMode {
    Width,
    Height,
    Both
};

enum class TopologyUAlignDirection {
    Previous,
    Next
};

struct TopologyMaterialPayload {
    bool valid = false;
    TopologySurfaceEditTargetKind kind = TopologySurfaceEditTargetKind::None;
    std::string textureId;
    SectorTopologyUvSettings uv;
};

struct SectorEditorState {
    std::string topologyRenderWarning;
    uint64_t topologyRenderRevision = 1;
    SectorEditorTopologyRenderCache topologyRenderCache;
    uint64_t lightmapSourceHashRevision = 0;
    std::string lightmapSourceHash;

    SectorEditorTool currentTool = SectorEditorTool::Select;
    SectorEditorMode mode = SectorEditorMode::Edit2D;

    Vector2 viewCenter = {0.0f, 0.0f};
    float viewZoom = 48.0f;
    int gridSize = SectorAuthoringEditorGridSizeDefault;

    Vector2 snappedMouseMap = {0.0f, 0.0f};
    Vector2 rawMouseMap = {0.0f, 0.0f};

    PendingAuthoringLineDraw pendingAuthoringLine;
    PendingAuthoringRectangleDraw pendingAuthoringRectangle;
    PendingAuthoringInsertVertex pendingAuthoringInsertVertex;
    float defaultSectorFloorZ = 0.0f;
    float defaultSectorCeilingZ = SectorWorldToAuthoringDistance(3.0f);
    std::string defaultFloorTextureId;
    std::string defaultCeilingTextureId;
    std::string defaultWallTextureId;
    std::string defaultLowerWallTextureId;
    std::string defaultUpperWallTextureId;

    bool showGrid = true;
    bool showAxes = true;
    bool showSectorIds = true;
    TexturePickerState texturePicker;
    FootstepPickerState footstepPicker;
    AddMapTextureState addMapTexture;
    AddMapSoundState addMapSound;
    SoundPickerState soundPicker;
    SaveLevelModalState saveLevelModal;
    LoadLevelModalState loadLevelModal;
    ConfirmationModalState confirmationModal;
    SectorEditorSetAllModalState setAllModal;
    DecalTintModalState decalTintModal;
    DoorTextureSettingsModalState doorTextureSettingsModal;
    SectorPreviewSettingsModalState previewSettingsModal;
};

struct SectorEditorUiState {
    engine::UIConfig config;
    engine::UIIntInputState gridSizeInput;
    engine::UIFloatInputState floorInput;
    engine::UIFloatInputState ceilingInput;
    engine::UIFloatInputState ambientIntensityInput;
    engine::UIIntInputState ambientRedInput;
    engine::UIIntInputState ambientGreenInput;
    engine::UIIntInputState ambientBlueInput;
    engine::UIFloatInputState lightXInput;
    engine::UIFloatInputState lightYInput;
    engine::UIFloatInputState lightZInput;
    engine::UIFloatInputState lightTargetXInput;
    engine::UIFloatInputState lightTargetYInput;
    engine::UIFloatInputState lightTargetZInput;
    engine::UIFloatInputState lightIntensityInput;
    engine::UIFloatInputState lightRadiusInput;
    engine::UIFloatInputState lightInnerConeInput;
    engine::UIFloatInputState lightOuterConeInput;
    engine::UIFloatInputState lightSourceRadiusInput;
    engine::UIFloatInputState lightFlickerSpeedInput;
    engine::UIFloatInputState lightFlickerAmountInput;
    engine::UIIntInputState lightShadowPriorityInput;
    engine::UIFloatInputState lightShadowBiasInput;
    engine::UIFloatInputState lightShadowStrengthInput;
    engine::UIFloatInputState lightShadowSoftnessInput;
    engine::UIFloatInputState lightHazeExtentScaleInput;
    engine::UIFloatInputState lightHazeHeightOffsetInput;
    engine::UIFloatInputState lightHazeDensityInput;
    engine::UIFloatInputState lightHazeEdgeSoftnessInput;
    engine::UIFloatInputState lightHazeNoiseAmountInput;
    engine::UIFloatInputState lightHazeNoiseScaleInput;
    engine::UIFloatInputState lightHazeFlowDirectionInput;
    engine::UIFloatInputState lightHazeFlowSpeedInput;
    engine::UIIntInputState lightHazeRedInput;
    engine::UIIntInputState lightHazeGreenInput;
    engine::UIIntInputState lightHazeBlueInput;
    engine::UIFloatInputState lightProxyHaloRadiusInput;
    engine::UIFloatInputState lightProxyHaloBrightnessInput;
    engine::UIFloatInputState lightProxyHaloMaxExtinctionInput;
    engine::UIFloatInputState lightProxyHaloSoftnessInput;
    engine::UIIntInputState lightProxyHaloRedInput;
    engine::UIIntInputState lightProxyHaloGreenInput;
    engine::UIIntInputState lightProxyHaloBlueInput;
    engine::UIFloatInputState lightProxyShaftLengthInput;
    engine::UIFloatInputState lightProxyShaftWidthInput;
    engine::UIFloatInputState lightProxyShaftBrightnessInput;
    engine::UIFloatInputState lightProxyShaftMaxExtinctionInput;
    engine::UIFloatInputState lightProxyShaftSoftnessInput;
    engine::UIIntInputState lightProxyShaftRedInput;
    engine::UIIntInputState lightProxyShaftGreenInput;
    engine::UIIntInputState lightProxyShaftBlueInput;
    engine::UIIntInputState lightDustAmountInput;
    engine::UIFloatInputState lightDustExtentScaleInput;
    engine::UIFloatInputState lightDustMinimumSizeInput;
    engine::UIFloatInputState lightDustMaximumSizeInput;
    engine::UIFloatInputState lightDustOpacityInput;
    engine::UIFloatInputState lightDustDriftSpeedInput;
    engine::UIFloatInputState lightDustTurbulenceInput;
    engine::UIIntInputState lightDustRedInput;
    engine::UIIntInputState lightDustGreenInput;
    engine::UIIntInputState lightDustBlueInput;
    engine::UIFloatInputState ambientOcclusionRadiusInput;
    engine::UIFloatInputState ambientOcclusionStrengthInput;
    engine::UIFloatInputState indirectBounceRadiusInput;
    engine::UIFloatInputState indirectBounceStrengthInput;
    engine::UIFloatInputState objectProbeDebugDrawMaxDistanceInput;
    engine::UIIntInputState lightRedInput;
    engine::UIIntInputState lightGreenInput;
    engine::UIIntInputState lightBlueInput;
    engine::UIScrollState toolsScroll;
    engine::UIScrollState inspectorScroll;
    bool keyboardCaptured = false;
};

} // namespace game
