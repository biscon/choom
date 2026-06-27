#pragma once

#include "engine/ui/UI.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorPointTypes.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace game {

enum class SectorEditorTool {
    Select,
    Sector,
    AuthoringLine,
    AuthoringMove,
    InsertSectorInside,
    Light,
    Move,
    Erase
};

enum class PendingSectorDrawKind {
    NewSector,
    InsertInside
};

enum class SectorEditorMode {
    Edit2D,
    Preview3D
};

enum class SectorPreviewControlMode {
    FreeFly,
    Gameplay
};

enum class SectorEditorAuthoringDerivationState {
    InvalidNoDerived,
    ValidCurrent,
    ValidStale,
    InvalidLastValid
};

struct PendingSectorDraw {
    std::vector<SectorPoint> points;
    bool active = false;
    PendingSectorDrawKind kind = PendingSectorDrawKind::NewSector;
    int parentTopologySectorId = -1;
    std::string parentSectorLabel;
    std::string errorMessage;
};

struct PendingAuthoringLineDraw {
    bool active = false;
    SectorTopologyCoordPoint startPoint = {};
    int startVertexId = -1;
    std::string errorMessage;
};

enum class TopologyTexturePickerTargetKind {
    None,
    Sector,
    SideDef,
    MapSky
};

enum class PreviewSettingsTab {
    General,
    Sky,
    Lighting
};

enum class TopologySelectionKind {
    None,
    Sector,
    Vertex,
    SideDef,
    LineDef,
    Light
};

enum class SectorAuthoringSelectionKind {
    None,
    Line,
    Vertex
};

struct SectorAuthoringSelectionTarget {
    SectorAuthoringSelectionKind kind = SectorAuthoringSelectionKind::None;
    int lineId = -1;
    int vertexId = -1;
};

enum class TopologyWallPart {
    Wall,
    Lower,
    Upper,
    Middle
};

enum class TopologyMaterialLayer {
    Base,
    Decal
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

enum class TopologySectorTextureField {
    None,
    Floor,
    Ceiling,
    DefaultWall,
    DefaultLower,
    DefaultUpper
};

enum class SectorSurfaceKind {
    None,
    Floor,
    Ceiling,
    Wall,
    LowerWall,
    UpperWall,
    Middle
};

struct SectorSurfaceRef {
    SectorSurfaceKind kind = SectorSurfaceKind::None;
    int topologySectorId = -1;
    int topologyLineDefId = -1;
    int topologySideDefId = -1;
    SectorTopologySideKind topologySide = SectorTopologySideKind::Front;
};

struct SectorSurfaceHit {
    bool hit = false;
    SectorSurfaceRef surface;
    Vector3 worldPosition = {};
    float distance = 0.0f;
};

enum class TopologySurfaceEditTargetKind {
    None,
    SectorFloor,
    SectorCeiling,
    SideDefWall,
    SideDefLower,
    SideDefUpper,
    SideDefMiddle
};

struct TopologySurfaceEditTarget {
    TopologySurfaceEditTargetKind kind = TopologySurfaceEditTargetKind::None;
    int sectorId = -1;
    int lineDefId = -1;
    int sideDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
};

struct TopologyMaterialPayload {
    bool valid = false;
    TopologySurfaceEditTargetKind kind = TopologySurfaceEditTargetKind::None;
    std::string textureId;
    SectorTopologyUvSettings uv;
};

struct TexturePickerState {
    bool open = false;
    bool rebuildPreviewOnApply = false;
    TopologyTexturePickerTargetKind topologyTargetKind = TopologyTexturePickerTargetKind::None;
    TopologyMaterialLayer topologyLayer = TopologyMaterialLayer::Base;
    TopologySectorTextureField topologyField = TopologySectorTextureField::None;
    int topologySectorId = -1;
    int topologySideDefId = -1;
    TopologyWallPart topologyWallPart = TopologyWallPart::Wall;
    int selectedTextureIndex = -1;
    engine::UIScrollState scroll;
    std::vector<std::string> textureIds;
    std::vector<const char*> optionLabels;
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

struct DecalTintModalState {
    bool open = false;
    TopologySurfaceEditTarget target;
    Vector3 tint = {1.0f, 1.0f, 1.0f};
    engine::UIFloatInputState redInput;
    engine::UIFloatInputState greenInput;
    engine::UIFloatInputState blueInput;
    std::string errorMessage;
};

struct SectorPreviewSettingsModalState {
    bool open = false;
    PreviewSettingsTab activeTab = PreviewSettingsTab::General;
    SectorFpsControllerConfig draftConfig;
    SectorTopologySkySettings draftSkySettings;
    SectorTopologyDirectionalLightSettings draftDirectionalLight;
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
    engine::UIIntInputState lightColorRedInput;
    engine::UIIntInputState lightColorGreenInput;
    engine::UIIntInputState lightColorBlueInput;
    engine::UIScrollState generalScroll;
    engine::UIScrollState skyScroll;
    engine::UIScrollState lightingScroll;
    std::string errorMessage;
};

struct VertexDragState {
    bool active = false;
    int topologyVertexId = -1;
    SectorTopologyCoordPoint originalPoint = {};
    SectorTopologyCoordPoint previewPoint = {};
    SectorTopologyCoordPoint lastValidatedPoint = {};
    bool hasPreviewPoint = false;
    bool hasValidatedPreview = false;
    bool lastPreviewValid = true;
    bool hasMergeTarget = false;
    int mergeTargetVertexId = -1;
    std::string errorMessage;
};

struct AuthoringVertexDragState {
    bool active = false;
    int vertexId = -1;
    SectorTopologyCoordPoint originalPoint = {};
    SectorTopologyCoordPoint previewPoint = {};
    bool hasPreviewPoint = false;
    std::string errorMessage;
};

struct LightDragState {
    bool active = false;
    int topologyLightId = -1;
    Vector3 originalPosition = {};
    Vector3 snappedPosition = {};
};

struct PendingTopologyLineSplitAtPoint {
    bool active = false;
    int lineDefId = -1;
    int sideDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    TopologyWallPart wallPart = TopologyWallPart::Wall;
    SectorTopologyCoordPoint candidatePoint = {};
    bool hasCandidatePoint = false;
    bool hasValidCandidate = false;
    std::string message;
};

struct PendingTopologyVertexMerge {
    bool active = false;
    int sourceVertexId = -1;
    int hoveredTargetVertexId = -1;
    bool hasValidTarget = false;
    std::string message;
};

struct PendingTopologySectorCut {
    bool active = false;
    int sectorId = -1;
    bool hasFirstPoint = false;
    SectorTopologyBoundaryCutPoint firstPoint;
    SectorTopologyBoundaryCutPoint candidatePoint;
    bool hasCandidatePoint = false;
    bool hasValidCandidate = false;
    bool cacheHasFirstPoint = false;
    SectorTopologyBoundaryCutPoint cachedFirstPoint;
    SectorTopologyBoundaryCutPoint cachedCandidatePoint;
    int cachedSectorId = -1;
    bool cachedValid = false;
    std::string cachedError;
    std::string message;
};

struct CachedTopologyOutlineSegment {
    Vector2 a = {};
    Vector2 b = {};
    bool hole = false;
};

struct CachedTopologySectorDraw {
    int sectorId = -1;
    std::string label;
    Vector2 labelCenter = {};
    std::vector<Vector2> fillTrianglePoints;
    std::vector<CachedTopologyOutlineSegment> outlineSegments;
};

struct CachedTopologyLineDraw {
    int lineDefId = -1;
    int frontSideDefId = -1;
    int backSideDefId = -1;
    Vector2 start = {};
    Vector2 end = {};
    bool validEndpoints = false;
    bool hasFront = false;
    bool hasBack = false;
    bool hasPartialEndpoint = false;
    Vector2 partialEndpoint = {};
};

struct CachedTopologyVertexDraw {
    int vertexId = -1;
    SectorTopologyCoordPoint point = {};
    Vector2 map = {};
};

struct CachedTopologyLightDraw {
    int lightId = -1;
    Vector2 map = {};
    Color color = WHITE;
    float radiusPixelsAtZoomOne = 0.0f;
    float sourceRadiusPixelsAtZoomOne = 0.0f;
};

struct CachedAuthoringVertexDraw {
    int vertexId = -1;
    SectorTopologyCoordPoint point = {};
    Vector2 map = {};
};

struct CachedAuthoringLineDraw {
    int lineId = -1;
    Vector2 start = {};
    Vector2 end = {};
    bool validEndpoints = false;
    bool hasPartialEndpoint = false;
    Vector2 partialEndpoint = {};
};

struct CachedAuthoringDiagnosticDraw {
    SectorAuthoringDerivationDiagnosticKind kind =
            SectorAuthoringDerivationDiagnosticKind::AuthoringReference;
    SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error;
    int objectId = -1;
    int relatedObjectId = -1;
    Vector2 map = {};
    bool hasPosition = false;
    std::string message;
};

struct SectorEditorTopologyRenderCache {
    bool valid = false;
    uint64_t revision = 0;
    std::string warning;
    std::vector<CachedTopologySectorDraw> sectors;
    std::vector<CachedTopologyLineDraw> lineDefs;
    std::vector<CachedTopologyVertexDraw> vertices;
    std::vector<CachedTopologyLightDraw> staticLights;
    std::vector<CachedAuthoringLineDraw> authoringLines;
    std::vector<CachedAuthoringVertexDraw> authoringVertices;
    std::vector<CachedAuthoringDiagnosticDraw> authoringDiagnostics;
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
    int hoveredTopologyLightId = -1;
    bool hasHoveredVertex = false;
    int hoveredTopologyVertexId = -1;
    SectorTopologyCoordPoint hoveredTopologyVertexPoint = {};
    int inspectedTopologyVertexId = -1;
    SectorAuthoringSelectionTarget selectedAuthoring;
    SectorAuthoringSelectionTarget hoveredAuthoring;

    Vector2 snappedMouseMap = {0.0f, 0.0f};
    Vector2 rawMouseMap = {0.0f, 0.0f};

    PendingSectorDraw pendingSector;
    PendingAuthoringLineDraw pendingAuthoringLine;
    VertexDragState vertexDrag;
    AuthoringVertexDragState authoringVertexDrag;
    LightDragState lightDrag;
    PendingTopologyLineSplitAtPoint pendingTopologyLineSplitAtPoint;
    PendingTopologyVertexMerge pendingTopologyVertexMerge;
    PendingTopologySectorCut pendingTopologySectorCut;
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
    bool previewUiHidden = false;
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
    SectorSurfaceHit hoveredSurface3D;
    SectorSurfaceRef selectedSurface3D;
    TopologySurfaceEditTarget selectedTopologySurface3D;
    TopologyMaterialPayload copiedTopologyMaterial;

    engine::AssetScopeHandle editorTextureScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::TextureHandle> editorTextureHandlesById;
    TexturePickerState texturePicker;
    AddMapTextureState addMapTexture;
    SaveLevelModalState saveLevelModal;
    LoadLevelModalState loadLevelModal;
    ConfirmationModalState confirmationModal;
    DecalTintModalState decalTintModal;
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
    engine::UIFloatInputState lightIntensityInput;
    engine::UIFloatInputState lightRadiusInput;
    engine::UIFloatInputState lightSourceRadiusInput;
    engine::UIFloatInputState ambientOcclusionRadiusInput;
    engine::UIFloatInputState ambientOcclusionStrengthInput;
    engine::UIFloatInputState indirectBounceRadiusInput;
    engine::UIFloatInputState indirectBounceStrengthInput;
    engine::UIIntInputState lightRedInput;
    engine::UIIntInputState lightGreenInput;
    engine::UIIntInputState lightBlueInput;
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

struct SectorLightmapBakeAsyncResult {
    bool succeeded = false;
    bool cancelled = false;
    std::string errorMessage;
    std::string bakeReportText;
    SectorLightmapBakeResult bakeResult;
    std::string expectedSourceHash;
    uint64_t sourceMapRevision = 0;
    std::string finalOutputPath;
    std::string temporaryOutputPath;
};

struct LightmapBakeProgress {
    std::atomic<SectorLightmapBakePhase> phase{SectorLightmapBakePhase::Idle};
    std::atomic<uint32_t> completedWork{0};
    std::atomic<uint32_t> totalWork{0};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> running{false};
};

struct LightmapBakeAsyncState {
    std::thread worker;
    LightmapBakeProgress progress;
    bool modalOpen = false;
    bool awaitingAcknowledgement = false;
    bool cancelButtonPressed = false;
    double startTimeSeconds = 0.0;
    double completedTimeSeconds = 0.0;
    std::string terminalMessage;
    bool terminalSuccess = false;
    bool terminalCancelled = false;
    std::string temporaryOutputPath;
    std::mutex resultMutex;
    std::optional<SectorLightmapBakeAsyncResult> pendingResult;
};

} // namespace game
