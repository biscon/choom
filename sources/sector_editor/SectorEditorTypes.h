#pragma once

#include "engine/ui/UI.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorTypes.h"

#include <raylib.h>

#include <atomic>
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
    Light,
    Move,
    Erase
};

enum class SectorEditorMode {
    Edit2D,
    Preview3D
};

struct PendingSectorDraw {
    std::vector<SectorPoint> points;
    bool active = false;
    std::string errorMessage;
};

struct SectorVertexRef {
    int sectorIndex = -1;
    int pointIndex = -1;
};

struct SectorEdgeRef {
    int sectorIndex = -1;
    int edgeIndex = -1;
};

struct SectorEdgeHitCandidate {
    SectorEdgeRef edge;
    float screenDistance = 0.0f;
};

enum class TexturePickerTargetKind {
    None,
    SectorFloor,
    SectorCeiling,
    SectorWall,
    SectorLowerWall,
    SectorUpperWall,
    EdgeWall,
    EdgeLowerWall,
    EdgeUpperWall
};

enum class EdgeUvPart {
    Wall,
    Lower,
    Upper
};

enum class SectorSurfaceKind {
    None,
    Floor,
    Ceiling,
    Wall,
    LowerWall,
    UpperWall
};

struct SectorSurfaceRef {
    SectorSurfaceKind kind = SectorSurfaceKind::None;
    int sectorIndex = -1;
    int edgeIndex = -1;
};

struct SectorSurfaceHit {
    bool hit = false;
    SectorSurfaceRef surface;
    Vector3 worldPosition = {};
    float distance = 0.0f;
};

struct TexturePickerState {
    bool open = false;
    TexturePickerTargetKind target = TexturePickerTargetKind::None;
    int sectorIndex = -1;
    int edgeIndex = -1;
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
    SectorTextureFilter filter = SectorTextureFilter::Bilinear;
    std::string validationMessage;
    engine::AssetScopeHandle previewScope = engine::NullAssetScopeHandle();
    engine::TextureHandle previewTexture = engine::NullTextureHandle();
    std::string previewPath;
    SectorTextureFilter previewFilter = SectorTextureFilter::Bilinear;
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

struct VertexDragState {
    bool active = false;
    SectorPoint originalPoint = {};
    SectorPoint currentPoint = {};
    SectorPoint snappedPoint = {};
    std::vector<SectorVertexRef> affectedVertices;
    std::string errorMessage;
};

struct LightDragState {
    bool active = false;
    int lightIndex = -1;
    Vector3 originalPosition = {};
    Vector3 snappedPosition = {};
};

struct SectorEditorState {
    SectorMap map;

    SectorEditorTool currentTool = SectorEditorTool::Select;
    SectorEditorMode mode = SectorEditorMode::Edit2D;

    Vector2 viewCenter = {0.0f, 0.0f};
    float viewZoom = 48.0f;
    int gridSize = 8;

    int selectedSectorIndex = -1;
    int selectedEdgeIndex = -1;
    int selectedLightIndex = -1;
    EdgeUvPart selectedEdgeUvPart = EdgeUvPart::Wall;
    int hoveredSectorIndex = -1;
    int hoveredEdgeSectorIndex = -1;
    int hoveredEdgeIndex = -1;
    int hoveredLightIndex = -1;
    bool hasHoveredVertex = false;
    SectorPoint hoveredVertexPoint = {};
    std::vector<SectorVertexRef> hoveredVertexRefs;

    Vector2 snappedMouseMap = {0.0f, 0.0f};
    Vector2 rawMouseMap = {0.0f, 0.0f};

    PendingSectorDraw pendingSector;
    VertexDragState vertexDrag;
    LightDragState lightDrag;
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
    bool hasPreviewPose = false;
    SectorMeshPreviewPose lastPreviewPose = {};
    SectorSurfaceHit hoveredSurface3D;
    SectorSurfaceRef selectedSurface3D;

    engine::AssetScopeHandle editorTextureScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::TextureHandle> editorTextureHandlesById;
    TexturePickerState texturePicker;
    AddMapTextureState addMapTexture;
    SaveLevelModalState saveLevelModal;
    LoadLevelModalState loadLevelModal;
    ConfirmationModalState confirmationModal;
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
    engine::UIFloatInputState edgeUvScaleUInput;
    engine::UIFloatInputState edgeUvScaleVInput;
    engine::UIFloatInputState edgeUvOffsetUInput;
    engine::UIFloatInputState edgeUvOffsetVInput;
    engine::UIFloatInputState surface3DUvScaleUInput;
    engine::UIFloatInputState surface3DUvScaleVInput;
    engine::UIFloatInputState surface3DUvOffsetUInput;
    engine::UIFloatInputState surface3DUvOffsetVInput;
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
