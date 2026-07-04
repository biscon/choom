#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorLightmapAsyncTypes.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorPreviewTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"
#include "sector_demo/SectorPointTypes.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

enum class SectorEditorTool {
    Select,
    AuthoringLine,
    AuthoringRectangle,
    AuthoringInsertVertex,
    AuthoringMove,
    RuntimeObject,
    Door,
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

enum class SectorEditorAuthoringDerivationState {
    InvalidNoDerived,
    ValidCurrent,
    ValidStale,
    InvalidLastValid
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
    SectorTopologyMap topologyMap;
    SectorAuthoringGraph authoringGraph;
    SectorAuthoringDerivationResult authoringDerivation;
    std::optional<SectorTopologyMap> lastValidAuthoringDerivedTopology;
    SectorEditorAuthoringDerivationState authoringDerivationState =
            SectorEditorAuthoringDerivationState::InvalidNoDerived;
    bool authoringDerivedTopologyStale = true;
    std::string authoringDerivationStatus;
    bool topologyDocumentInitialized = false;
    bool topologyDocumentDirty = false;
    std::string topologyDocumentStatus;
    std::string topologyRenderWarning;
    uint64_t topologyRenderRevision = 1;
    SectorEditorTopologyRenderCache topologyRenderCache;

    SectorEditorTool currentTool = SectorEditorTool::Select;
    SectorEditorMode mode = SectorEditorMode::Edit2D;

    Vector2 viewCenter = {0.0f, 0.0f};
    float viewZoom = 48.0f;
    int gridSize = 8;

    TopologySelectionKind topologySelectionKind = TopologySelectionKind::None;
    int selectedTopologySectorId = -1;
    int selectedTopologyVertexId = -1;
    int selectedTopologySideDefId = -1;
    int selectedTopologyLineDefId = -1;
    SectorTopologySideKind selectedTopologySideKind = SectorTopologySideKind::Front;
    TopologyWallPart selectedTopologyWallPart = TopologyWallPart::Wall;
    TopologyMaterialLayer activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
    int selectedTopologyLightId = -1;
    int selectedTopologyStaticSpotLightId = -1;
    int selectedTopologyDynamicLightId = -1;
    int selectedTopologyDynamicSpotLightId = -1;
    int selectedRuntimeObjectId = -1;
    SelectDragArmState selectDragArm;
    int hoveredTopologyLightId = -1;
    int hoveredTopologyStaticSpotLightId = -1;
    int hoveredTopologyDynamicLightId = -1;
    int hoveredTopologyDynamicSpotLightId = -1;
    bool hasHoveredVertex = false;
    int hoveredTopologyVertexId = -1;
    SectorTopologyCoordPoint hoveredTopologyVertexPoint = {};
    int inspectedTopologyVertexId = -1;
    SectorAuthoringSelectionTarget selectedAuthoring;
    SectorAuthoringSelectionTarget hoveredAuthoring;

    Vector2 snappedMouseMap = {0.0f, 0.0f};
    Vector2 rawMouseMap = {0.0f, 0.0f};

    PendingAuthoringLineDraw pendingAuthoringLine;
    PendingAuthoringRectangleDraw pendingAuthoringRectangle;
    PendingAuthoringInsertVertex pendingAuthoringInsertVertex;
    AuthoringVertexDragState authoringVertexDrag;
    LightDragState lightDrag;
    RuntimeObjectDragState runtimeObjectDrag;
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
    std::string currentLevelName;
    std::string currentLevelPath;
    bool hasCurrentLevelPath = false;
    bool hasUnsavedChanges = false;
    bool useBakedAmbientOcclusion = true;
    bool showObjectProbeDebugOverlay = false;
    SectorRuntimeObjectState runtimeObjects;
    bool previewUiHidden = false;
    PreviewDebugOverlayTab activePreviewDebugOverlayTab = PreviewDebugOverlayTab::None;
    SectorPreviewControlMode previewControlMode = SectorPreviewControlMode::FreeFly;
    SectorFreeflyControllerState freeflyController;
    SectorFpsControllerConfig fpsControllerConfig;
    SectorFpsControllerState fpsControllerState;
    SectorCollisionWorld sectorCollisionWorld;
    bool sectorCollisionWorldValid = false;
    std::string sectorCollisionWorldWarning;
    int previewCollisionSectorId = 0;
    SectorFpsVerticalResult previewVerticalResult;
    SectorCollisionMoveResult previewMoveResult;
    bool previewCollisionNoclipFallback = false;
    float visualStepOffsetY = 0.0f;
    SectorFpsHeadBobState headBobState;
    SectorFpsLandingDipState landingDipState;
    bool hasPreviewPose = false;
    SectorViewPose lastPreviewPose = {};
    SpotLightPilotState spotLightPilot;
    SectorSurfaceHit hoveredSurface3D;
    SectorSurfaceRef selectedSurface3D;
    TopologySurfaceEditTarget selectedTopologySurface3D;
    TopologyMaterialPayload copiedTopologyMaterial;

    engine::AssetScopeHandle editorTextureScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::TextureHandle> editorTextureHandlesById;
    TexturePickerState texturePicker;
    AddMapTextureState addMapTexture;
    SectorSpriteMetadataCatalog spriteMetadataCatalog;
    int billboardMetadataObjectId = -1;
    std::string billboardMetadataSpriteAnimationPath;
    bool billboardMetadataInitialRepairAttempted = false;
    SpritePickerState spritePicker;
    SaveLevelModalState saveLevelModal;
    LoadLevelModalState loadLevelModal;
    ConfirmationModalState confirmationModal;
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
    engine::UIFloatInputState ambientOcclusionRadiusInput;
    engine::UIFloatInputState ambientOcclusionStrengthInput;
    engine::UIFloatInputState indirectBounceRadiusInput;
    engine::UIFloatInputState indirectBounceStrengthInput;
    engine::UIFloatInputState objectProbeDebugDrawMaxDistanceInput;
    engine::UIIntInputState lightRedInput;
    engine::UIIntInputState lightGreenInput;
    engine::UIIntInputState lightBlueInput;
    engine::UIFloatInputState runtimeObjectXInput;
    engine::UIFloatInputState runtimeObjectYInput;
    engine::UIFloatInputState runtimeObjectZInput;
    engine::UIFloatInputState runtimeObjectYawInput;
    engine::UIFloatInputState runtimeObjectWidthInput;
    engine::UIFloatInputState runtimeObjectHeightInput;
    engine::UIFloatInputState runtimeObjectThicknessInput;
    engine::UIFloatInputState runtimeObjectNormalOffsetInput;
    engine::UIFloatInputState runtimeObjectOpenDistanceInput;
    engine::UIFloatInputState runtimeObjectSpeedInput;
    engine::UIFloatInputState runtimeObjectInitialOpenFractionInput;
    engine::UIFloatInputState runtimeObjectAutoOpenDistanceInput;
    engine::UIFloatInputState runtimeObjectInteractionDistanceInput;
    engine::UIFloatInputState runtimeObjectOriginXInput;
    engine::UIFloatInputState runtimeObjectOriginYInput;
    engine::UIFloatInputState surface3DUvScaleUInput;
    engine::UIFloatInputState surface3DUvScaleVInput;
    engine::UIFloatInputState surface3DUvOffsetUInput;
    engine::UIFloatInputState surface3DUvOffsetVInput;
    engine::UIFloatInputState surface3DDecalOpacityInput;
    engine::UIFloatInputState surface3DDecalBloomIntensityInput;
    engine::UIFloatInputState topologySectorUvInputs[20];
    engine::UIFloatInputState topologySectorDecalOpacityInputs[2];
    engine::UIFloatInputState topologySectorDecalBloomIntensityInputs[2];
    engine::UIFloatInputState topologySideDefUvInputs[4];
    engine::UIFloatInputState topologySideDefDecalOpacityInput;
    engine::UIFloatInputState topologySideDefDecalBloomIntensityInput;
    engine::UIScrollState toolsScroll;
    engine::UIScrollState inspectorScroll;
    char selectedSectorIdBuffer[64] = {};
    int idBufferSectorIndex = -1;
    char selectedLightIdBuffer[64] = {};
    int idBufferLightIndex = -1;
    std::string idEditError;
    bool keyboardCaptured = false;
};

} // namespace game
