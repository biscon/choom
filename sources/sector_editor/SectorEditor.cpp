#include "sector_editor/SectorEditor.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_demo/SectorMap.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float EditorWidth = 1920.0f;
constexpr float EditorHeight = 1080.0f;
constexpr float LeftPanelWidth = 264.0f;
constexpr float RightPanelWidth = 384.0f;
constexpr float BottomPanelHeight = 78.0f;
constexpr float PanelGap = 10.0f;
constexpr float MinZoom = 8.0f;
constexpr float MaxZoom = 256.0f;
constexpr float PanPixelsPerSecond = 720.0f;
constexpr float GeometryEpsilon = 0.001f;
constexpr float ScreenVertexSnapPixels = 10.0f;
constexpr float ScreenEdgePickPixels = 10.0f;

Vector2 SectorPointToVector2(SectorPoint point)
{
    return Vector2{point.x, point.y};
}

SectorPoint Vector2ToSectorPoint(Vector2 point)
{
    return SectorPoint{point.x, point.y};
}

bool Contains(Rectangle bounds, Vector2 point)
{
    return CheckCollisionPointRec(point, bounds);
}

bool StartsWith(const std::string& value, const char* prefix)
{
    const std::string prefixString(prefix);
    return value.compare(0, prefixString.size(), prefixString) == 0;
}

std::string ResolveEditorAssetPath(const std::string& path)
{
    if (StartsWith(path, "assets/")) {
        return std::string(ASSETS_PATH) + path.substr(7);
    }
    return path;
}

const char* ToolName(SectorEditorTool tool)
{
    switch (tool) {
        case SectorEditorTool::Select: return "Select";
        case SectorEditorTool::Sector: return "Sector";
        case SectorEditorTool::Move: return "Move";
        case SectorEditorTool::Erase: return "Erase";
    }
    return "Unknown";
}

const char* EdgeUvPartName(EdgeUvPart part)
{
    switch (part) {
        case EdgeUvPart::Wall: return "Wall";
        case EdgeUvPart::Lower: return "Lower";
        case EdgeUvPart::Upper: return "Upper";
    }
    return "Wall";
}

const char* EdgeUvPartStatusName(EdgeUvPart part)
{
    switch (part) {
        case EdgeUvPart::Wall: return "wall";
        case EdgeUvPart::Lower: return "lower";
        case EdgeUvPart::Upper: return "upper";
    }
    return "wall";
}

const char* ToolHelpText(SectorEditorTool tool)
{
    switch (tool) {
        case SectorEditorTool::Select: return "Select: click edges or sectors in canvas";
        case SectorEditorTool::Sector: return "Sector: left click add, click first closes, right click/Esc cancels, Backspace removes";
        case SectorEditorTool::Move: return "Move: drag vertex";
        case SectorEditorTool::Erase: return "Erase: click sector to delete";
    }
    return "";
}

const char* TextureTargetLabel(TexturePickerTargetKind target)
{
    switch (target) {
        case TexturePickerTargetKind::SectorFloor: return "floor texture";
        case TexturePickerTargetKind::SectorCeiling: return "ceiling texture";
        case TexturePickerTargetKind::SectorWall: return "wall texture";
        case TexturePickerTargetKind::SectorLowerWall: return "lower wall texture";
        case TexturePickerTargetKind::SectorUpperWall: return "upper wall texture";
        case TexturePickerTargetKind::EdgeWall: return "edge wall texture";
        case TexturePickerTargetKind::EdgeLowerWall: return "edge lower wall texture";
        case TexturePickerTargetKind::EdgeUpperWall: return "edge upper wall texture";
        case TexturePickerTargetKind::None: break;
    }
    return "texture";
}

Color WithAlpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

Vector2 AverageSectorPoint(const SectorDefinition& sector)
{
    Vector2 center{};
    if (sector.points.empty()) {
        return center;
    }

    for (SectorPoint point : sector.points) {
        center.x += point.x;
        center.y += point.y;
    }
    const float invCount = 1.0f / static_cast<float>(sector.points.size());
    center.x *= invCount;
    center.y *= invCount;
    return center;
}

float Cross(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

float Cross(SectorPoint a, SectorPoint b, SectorPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

float DistancePointToSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length2 = dx * dx + dy * dy;
    if (length2 <= 0.000001f) {
        const float px = point.x - a.x;
        const float py = point.y - a.y;
        return std::sqrt(px * px + py * py);
    }

    const float t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length2, 0.0f, 1.0f);
    const Vector2 closest{a.x + dx * t, a.y + dy * t};
    const float px = point.x - closest.x;
    const float py = point.y - closest.y;
    return std::sqrt(px * px + py * py);
}

float PolygonArea2(const std::vector<SectorPoint>& points)
{
    float area2 = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const SectorPoint a = points[i];
        const SectorPoint b = points[(i + 1) % points.size()];
        area2 += a.x * b.y - a.y * b.x;
    }
    return area2;
}

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= GeometryEpsilon && std::fabs(a.y - b.y) <= GeometryEpsilon;
}

bool PointOnSegment(SectorPoint point, SectorPoint a, SectorPoint b)
{
    if (std::fabs(Cross(a, b, point)) > GeometryEpsilon) {
        return false;
    }

    return point.x >= std::fmin(a.x, b.x) - GeometryEpsilon
            && point.x <= std::fmax(a.x, b.x) + GeometryEpsilon
            && point.y >= std::fmin(a.y, b.y) - GeometryEpsilon
            && point.y <= std::fmax(a.y, b.y) + GeometryEpsilon;
}

int Orientation(SectorPoint a, SectorPoint b, SectorPoint c)
{
    const float cross = Cross(a, b, c);
    if (cross > GeometryEpsilon) {
        return 1;
    }
    if (cross < -GeometryEpsilon) {
        return -1;
    }
    return 0;
}

bool SegmentsIntersect(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    const int o1 = Orientation(a0, a1, b0);
    const int o2 = Orientation(a0, a1, b1);
    const int o3 = Orientation(b0, b1, a0);
    const int o4 = Orientation(b0, b1, a1);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    return (o1 == 0 && PointOnSegment(b0, a0, a1))
            || (o2 == 0 && PointOnSegment(b1, a0, a1))
            || (o3 == 0 && PointOnSegment(a0, b0, b1))
            || (o4 == 0 && PointOnSegment(a1, b0, b1));
}

bool ProperSegmentsCross(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    const int o1 = Orientation(a0, a1, b0);
    const int o2 = Orientation(a0, a1, b1);
    const int o3 = Orientation(b0, b1, a0);
    const int o4 = Orientation(b0, b1, a1);
    return o1 != 0 && o2 != 0 && o3 != 0 && o4 != 0 && o1 != o2 && o3 != o4;
}

bool CollinearSegmentsOverlap(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    if (Orientation(a0, a1, b0) != 0 || Orientation(a0, a1, b1) != 0) {
        return false;
    }

    const bool useX = std::fabs(a0.x - a1.x) >= std::fabs(a0.y - a1.y);
    const float aMin = std::min(useX ? a0.x : a0.y, useX ? a1.x : a1.y);
    const float aMax = std::max(useX ? a0.x : a0.y, useX ? a1.x : a1.y);
    const float bMin = std::min(useX ? b0.x : b0.y, useX ? b1.x : b1.y);
    const float bMax = std::max(useX ? b0.x : b0.y, useX ? b1.x : b1.y);
    return std::max(aMin, bMin) < std::min(aMax, bMax) - GeometryEpsilon;
}

bool SameUndirectedSegment(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    return (SamePoint(a0, b0) && SamePoint(a1, b1))
            || (SamePoint(a0, b1) && SamePoint(a1, b0));
}

bool EdgesAreAdjacent(size_t edgeA, size_t edgeB, size_t edgeCount)
{
    if (edgeA == edgeB) {
        return true;
    }

    return (edgeA + 1) % edgeCount == edgeB || (edgeB + 1) % edgeCount == edgeA;
}

bool PointOnPolygonBoundary(SectorPoint point, const std::vector<SectorPoint>& polygon)
{
    for (size_t i = 0; i < polygon.size(); ++i) {
        if (PointOnSegment(point, polygon[i], polygon[(i + 1) % polygon.size()])) {
            return true;
        }
    }
    return false;
}

bool StrictPointInPolygon(SectorPoint point, const std::vector<SectorPoint>& polygon)
{
    if (polygon.size() < 3 || PointOnPolygonBoundary(point, polygon)) {
        return false;
    }

    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const SectorPoint a = polygon[i];
        const SectorPoint b = polygon[j];
        const bool intersects = ((a.y > point.y) != (b.y > point.y))
                && (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) == 0.0f ? 0.00001f : (b.y - a.y)) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

SectorPoint PolygonCentroidApprox(const std::vector<SectorPoint>& points)
{
    SectorPoint centroid{};
    if (points.empty()) {
        return centroid;
    }

    for (SectorPoint point : points) {
        centroid.x += point.x;
        centroid.y += point.y;
    }
    const float invCount = 1.0f / static_cast<float>(points.size());
    centroid.x *= invCount;
    centroid.y *= invCount;
    return centroid;
}

bool FileExists(const std::string& path)
{
    std::ifstream file(path);
    return static_cast<bool>(file);
}

std::string ShortPath(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool IsEmptyEdgeOverride(const SectorEdgeOverride& edgeOverride)
{
    return !edgeOverride.hasWallTexture
            && !edgeOverride.hasLowerWallTexture
            && !edgeOverride.hasUpperWallTexture
            && !edgeOverride.wallUv.hasUvScale
            && !edgeOverride.wallUv.hasUvOffset
            && !edgeOverride.lowerUv.hasUvScale
            && !edgeOverride.lowerUv.hasUvOffset
            && !edgeOverride.upperUv.hasUvScale
            && !edgeOverride.upperUv.hasUvOffset;
}

bool IsConvexPolygon(const SectorDefinition& sector)
{
    if (sector.points.size() < 3) {
        return false;
    }

    int sign = 0;
    for (size_t i = 0; i < sector.points.size(); ++i) {
        const Vector2 a = SectorPointToVector2(sector.points[i]);
        const Vector2 b = SectorPointToVector2(sector.points[(i + 1) % sector.points.size()]);
        const Vector2 c = SectorPointToVector2(sector.points[(i + 2) % sector.points.size()]);
        const float cross = Cross(a, b, c);
        if (std::fabs(cross) < 0.0001f) {
            continue;
        }

        const int nextSign = cross > 0.0f ? 1 : -1;
        if (sign != 0 && nextSign != sign) {
            return false;
        }
        sign = nextSign;
    }

    return true;
}

bool ValidateSectorShape(const std::vector<SectorPoint>& points, std::string& error)
{
    if (points.size() < 3) {
        error = "Need at least 3 points to close sector";
        return false;
    }

    for (size_t i = 0; i < points.size(); ++i) {
        const SectorPoint a = points[i];
        const SectorPoint b = points[(i + 1) % points.size()];
        if (SamePoint(a, b)) {
            error = "Duplicate point";
            return false;
        }
        for (size_t j = i + 1; j < points.size(); ++j) {
            if (SamePoint(points[i], points[j])) {
                error = "Duplicate point";
                return false;
            }
        }
    }

    if (std::fabs(PolygonArea2(points)) <= GeometryEpsilon) {
        error = "Sector polygon has zero area";
        return false;
    }

    for (size_t edgeA = 0; edgeA < points.size(); ++edgeA) {
        const SectorPoint a0 = points[edgeA];
        const SectorPoint a1 = points[(edgeA + 1) % points.size()];
        for (size_t edgeB = edgeA + 1; edgeB < points.size(); ++edgeB) {
            if (EdgesAreAdjacent(edgeA, edgeB, points.size())) {
                continue;
            }

            const SectorPoint b0 = points[edgeB];
            const SectorPoint b1 = points[(edgeB + 1) % points.size()];
            if (SegmentsIntersect(a0, a1, b0, b1)) {
                error = "Sector polygon self-intersects";
                return false;
            }
        }
    }

    return true;
}

bool ValidateSectorAgainstSector(
        const std::vector<SectorPoint>& points,
        const std::vector<SectorPoint>& other,
        std::string& error)
{
    if (other.size() < 3) {
        return true;
    }

    for (size_t a = 0; a < points.size(); ++a) {
        const SectorPoint a0 = points[a];
        const SectorPoint a1 = points[(a + 1) % points.size()];
        for (size_t b = 0; b < other.size(); ++b) {
            const SectorPoint b0 = other[b];
            const SectorPoint b1 = other[(b + 1) % other.size()];

            if (SameUndirectedSegment(a0, a1, b0, b1)) {
                continue;
            }

            if (ProperSegmentsCross(a0, a1, b0, b1)) {
                error = "Sector edge crosses existing sector";
                return false;
            }

            if (CollinearSegmentsOverlap(a0, a1, b0, b1)) {
                error = "Partial shared edges are not supported";
                return false;
            }

            if (PointOnSegment(a0, b0, b1) && !SamePoint(a0, b0) && !SamePoint(a0, b1)) {
                error = "T-junctions are not supported";
                return false;
            }
            if (PointOnSegment(a1, b0, b1) && !SamePoint(a1, b0) && !SamePoint(a1, b1)) {
                error = "T-junctions are not supported";
                return false;
            }
            if (PointOnSegment(b0, a0, a1) && !SamePoint(b0, a0) && !SamePoint(b0, a1)) {
                error = "T-junctions are not supported";
                return false;
            }
            if (PointOnSegment(b1, a0, a1) && !SamePoint(b1, a0) && !SamePoint(b1, a1)) {
                error = "T-junctions are not supported";
                return false;
            }
        }
    }

    // TODO: Replace centroid/vertex tests with robust polygon clipping if authoring needs holes or nested sectors.
    if (StrictPointInPolygon(PolygonCentroidApprox(points), other)
            || StrictPointInPolygon(PolygonCentroidApprox(other), points)) {
        error = "Sector overlaps existing sector";
        return false;
    }
    for (SectorPoint point : points) {
        if (StrictPointInPolygon(point, other)) {
            error = "Sector overlaps existing sector";
            return false;
        }
    }
    for (SectorPoint point : other) {
        if (StrictPointInPolygon(point, points)) {
            error = "Sector overlaps existing sector";
            return false;
        }
    }

    return true;
}

} // namespace

bool SectorEditor::Init(engine::AssetManager& assets, const char* path)
{
    Shutdown(assets);
    LoadInitialMap(path);
    if (!initialized) {
        return false;
    }

    state.viewCenter = Vector2{9.0f, 6.0f};
    state.viewZoom = 48.0f;
    state.gridSize = 1;
    state.selectedSectorIndex = -1;
    state.selectedEdgeIndex = -1;
    state.hoveredSectorIndex = -1;
    state.hoveredEdgeSectorIndex = -1;
    state.hoveredEdgeIndex = -1;
    RefreshEditorTextureAssets(assets);
    return true;
}

void SectorEditor::Shutdown(engine::AssetManager& assets)
{
    preview.Shutdown(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    canvasRect = {};
    statusText.clear();
    mapPath.clear();
    fallbackMapPath.clear();
    initialized = false;
}

void SectorEditor::Update(engine::Input& input, float dt)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        UpdatePreview3D(input, dt);
        return;
    }

    canvasRect = BuildCanvasRect();
    if (state.texturePicker.open) {
        return;
    }
    UpdateHoverAndMouse(input);
    HandleCanvasInput(input, dt);
}

void SectorEditor::Render(engine::AssetManager& assets)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        RenderPreview3D(assets);
        return;
    }

    canvasRect = BuildCanvasRect();
    DrawRectangleRec(canvasRect, Color{12, 15, 20, 255});

    BeginScissorMode(
            static_cast<int>(std::round(canvasRect.x)),
            static_cast<int>(std::round(canvasRect.y)),
            static_cast<int>(std::round(canvasRect.width)),
            static_cast<int>(std::round(canvasRect.height))
    );

    if (state.showGrid) {
        DrawGrid();
    }
    DrawSectors();
    DrawPendingSector();
    DrawVertexMoveOverlay();
    EndScissorMode();

    DrawRectangleLinesEx(canvasRect, 2.0f, Color{67, 76, 93, 255});
}

void SectorEditor::RenderUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        ui.hotId = 0;
        ui.activeId = 0;
        ui.openOptionId = 0;
        ui.focusedId = 0;
        uiState.keyboardCaptured = false;
        DrawPreviewOverlay(config, assets, font);
        return;
    }

    engine::BeginUI(ui, input);
    if (state.texturePicker.open) {
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }

    DrawToolsPanel(ui, config, input, assets, font);
    DrawSectorsPanel(ui, config, input, assets, font);
    DrawStatusPanel(ui, config, assets, font);
    DrawTexturePickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0;
    if (state.texturePicker.open) {
        uiState.keyboardCaptured = true;
    }
    engine::EndUI(ui, config, input, assets);
}

Vector2 SectorEditor::MapToScreen(Vector2 map) const
{
    return Vector2{
            canvasRect.x + canvasRect.width * 0.5f + (map.x - state.viewCenter.x) * state.viewZoom,
            canvasRect.y + canvasRect.height * 0.5f + (map.y - state.viewCenter.y) * state.viewZoom
    };
}

Vector2 SectorEditor::ScreenToMap(Vector2 screen) const
{
    return Vector2{
            state.viewCenter.x + (screen.x - (canvasRect.x + canvasRect.width * 0.5f)) / state.viewZoom,
            state.viewCenter.y + (screen.y - (canvasRect.y + canvasRect.height * 0.5f)) / state.viewZoom
    };
}

Vector2 SectorEditor::SnapMapPoint(Vector2 map) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(map.x / grid) * grid,
            std::round(map.y / grid) * grid
    };

    if (state.currentTool != SectorEditorTool::Sector) {
        return snapped;
    }

    const float threshold = std::max(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom), grid * 0.20f);
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    for (const SectorDefinition& sector : state.map.sectors) {
        for (SectorPoint point : sector.points) {
            const Vector2 vertex = SectorPointToVector2(point);
            const float dx = vertex.x - map.x;
            const float dy = vertex.y - map.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = vertex;
                found = true;
            }
        }
    }

    return found ? best : snapped;
}

Rectangle SectorEditor::BuildLeftPanelRect() const
{
    return Rectangle{0.0f, 0.0f, LeftPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildRightPanelRect() const
{
    return Rectangle{EditorWidth - RightPanelWidth, 0.0f, RightPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildBottomPanelRect() const
{
    return Rectangle{0.0f, EditorHeight - BottomPanelHeight, EditorWidth, BottomPanelHeight};
}

Rectangle SectorEditor::BuildCanvasRect() const
{
    return Rectangle{
            LeftPanelWidth + PanelGap,
            PanelGap,
            EditorWidth - LeftPanelWidth - RightPanelWidth - PanelGap * 2.0f,
            EditorHeight - BottomPanelHeight - PanelGap * 2.0f
    };
}

bool SectorEditor::IsMouseOverCanvas(const engine::Input& input) const
{
    return Contains(canvasRect, input.MousePosition());
}

void SectorEditor::UpdateHoverAndMouse(engine::Input& input)
{
    state.rawMouseMap = ScreenToMap(input.MousePosition());
    state.snappedMouseMap = SnapMapPoint(state.rawMouseMap);
    state.hasHoveredVertex = false;
    state.hoveredVertexRefs.clear();
    state.hoveredEdgeSectorIndex = -1;
    state.hoveredEdgeIndex = -1;

    if (!initialized || !IsMouseOverCanvas(input)) {
        state.hoveredSectorIndex = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::Select) {
        SectorEdgeRef edge{};
        if (ResolveEdgeHit(input.MousePosition(), state.rawMouseMap, edge)) {
            state.hoveredEdgeSectorIndex = edge.sectorIndex;
            state.hoveredEdgeIndex = edge.edgeIndex;
        }
    }
    state.hoveredSectorIndex = FindSectorAt(state.rawMouseMap);
    if (state.currentTool == SectorEditorTool::Move && !state.vertexDrag.active) {
        state.hasHoveredVertex = FindVertexNearScreenPoint(
                input.MousePosition(),
                state.hoveredVertexPoint,
                state.hoveredVertexRefs
        );
        if (state.hasHoveredVertex) {
            statusText = state.hoveredVertexRefs.size() > 1
                    ? TextFormat("Move: shared vertex used by %zu points", state.hoveredVertexRefs.size())
                    : "Move: drag vertex";
        }
    }
}

void SectorEditor::HandleCanvasInput(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    if (state.vertexDrag.active) {
                        CancelVertexDrag("Cancelled vertex move");
                    } else if (state.pendingSector.active) {
                        CancelPendingSector("Cancelled sector");
                    } else if (state.selectedSectorIndex >= 0) {
                        ClearSelection();
                    } else {
                        state.currentTool = SectorEditorTool::Select;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_BACKSPACE) {
                    RemoveLastPendingSectorPoint();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_ENTER) {
                    FinalizePendingSector();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (DeleteSelectedSector()) {
                        engine::ConsumeEvent(event);
                    }
                    return;
                }
            }
    );

    if (state.vertexDrag.active) {
        UpdateVertexDrag(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelVertexDrag("Cancelled vertex move");
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseButtonReleased,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_LEFT_BUTTON) {
                        FinishVertexDrag();
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseWheel,
                true,
                [](engine::InputEvent& event) {
                    engine::ConsumeEvent(event);
                }
        );
        return;
    }

    if (!uiState.keyboardCaptured) {
        Vector2 pan{};
        if (input.IsKeyDown(KEY_A)) {
            pan.x -= 1.0f;
        }
        if (input.IsKeyDown(KEY_D)) {
            pan.x += 1.0f;
        }
        if (input.IsKeyDown(KEY_W)) {
            pan.y -= 1.0f;
        }
        if (input.IsKeyDown(KEY_S)) {
            pan.y += 1.0f;
        }

        if (pan.x != 0.0f || pan.y != 0.0f) {
            const float length = std::sqrt(pan.x * pan.x + pan.y * pan.y);
            pan.x /= length;
            pan.y /= length;
            const float mapUnits = (PanPixelsPerSecond * dt) / std::max(1.0f, state.viewZoom);
            state.viewCenter.x += pan.x * mapUnits;
            state.viewCenter.y += pan.y * mapUnits;
        }
    }

    if (!IsMouseOverCanvas(input)) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this](engine::InputEvent& event) {
                if (state.currentTool != SectorEditorTool::Move
                        || event.mouseButton.button != MOUSE_LEFT_BUTTON
                        || !Contains(canvasRect, event.mouseButton.position)) {
                    return;
                }

                SectorPoint point{};
                std::vector<SectorVertexRef> refs;
                if (FindVertexNearScreenPoint(event.mouseButton.position, point, refs)) {
                    StartVertexDrag(point, refs);
                    engine::ConsumeEvent(event);
                }
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseWheel,
            true,
            [this, &input](engine::InputEvent& event) {
                const Vector2 mouseBefore = ScreenToMap(input.MousePosition());
                const float zoomFactor = event.wheel.value > 0.0f ? 1.12f : 1.0f / 1.12f;
                state.viewZoom = std::clamp(state.viewZoom * zoomFactor, MinZoom, MaxZoom);
                const Vector2 mouseAfter = ScreenToMap(input.MousePosition());
                state.viewCenter.x += mouseBefore.x - mouseAfter.x;
                state.viewCenter.y += mouseBefore.y - mouseAfter.y;
                engine::ConsumeEvent(event);
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this](engine::InputEvent& event) {
                if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                    return;
                }

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON && state.pendingSector.active) {
                    CancelPendingSector("Cancelled sector");
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (state.currentTool == SectorEditorTool::Select) {
                    SectorEdgeRef edge{};
                    const Vector2 rawMap = ScreenToMap(event.mouseClick.releasePosition);
                    if (ResolveEdgeHit(event.mouseClick.releasePosition, rawMap, edge)) {
                        SelectEdge(edge.sectorIndex, edge.edgeIndex);
                    } else {
                        SelectSector(FindSectorAt(rawMap));
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Sector) {
                    const SectorPoint point = Vector2ToSectorPoint(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    if (CanClosePendingSectorAt(point)) {
                        FinalizePendingSector();
                    } else {
                        AddPendingSectorPoint(point);
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Erase) {
                    const int sectorIndex = FindSectorAt(ScreenToMap(event.mouseClick.releasePosition));
                    if (sectorIndex >= 0) {
                        DeleteSectorAt(sectorIndex);
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Move) {
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::StartVertexDrag(SectorPoint point, const std::vector<SectorVertexRef>& refs)
{
    if (refs.empty()) {
        return;
    }

    state.vertexDrag.active = true;
    state.vertexDrag.originalPoint = point;
    state.vertexDrag.currentPoint = point;
    state.vertexDrag.snappedPoint = point;
    state.vertexDrag.affectedVertices = refs;
    state.vertexDrag.errorMessage.clear();
    statusText = refs.size() > 1
            ? TextFormat("Moving shared vertex used by %zu points", refs.size())
            : "Moving vertex";
}

void SectorEditor::UpdateVertexDrag(engine::Input& input)
{
    if (!state.vertexDrag.active) {
        return;
    }

    state.vertexDrag.currentPoint = Vector2ToSectorPoint(ScreenToMap(input.MousePosition()));
    state.vertexDrag.snappedPoint = SnapVertexMoveTarget(SectorPointToVector2(state.vertexDrag.currentPoint));

    std::string error;
    if (SamePoint(state.vertexDrag.snappedPoint, state.vertexDrag.originalPoint)) {
        state.vertexDrag.errorMessage.clear();
        statusText = "Moving vertex: original point";
    } else if (ValidateMovedVertexGroup(state.vertexDrag.snappedPoint, error)) {
        state.vertexDrag.errorMessage.clear();
        statusText = state.vertexDrag.affectedVertices.size() > 1
                ? TextFormat("Moving shared vertex used by %zu points", state.vertexDrag.affectedVertices.size())
                : "Moving vertex";
    } else {
        state.vertexDrag.errorMessage = error;
        statusText = TextFormat("Move rejected: %s", error.c_str());
    }
}

void SectorEditor::FinishVertexDrag()
{
    if (!state.vertexDrag.active) {
        return;
    }

    const SectorPoint original = state.vertexDrag.originalPoint;
    const SectorPoint target = state.vertexDrag.snappedPoint;
    const std::vector<SectorVertexRef> affected = state.vertexDrag.affectedVertices;

    if (SamePoint(target, original)) {
        state.vertexDrag = VertexDragState{};
        statusText = "Vertex unchanged";
        return;
    }

    std::string error;
    if (!ValidateMovedVertexGroup(target, error)) {
        state.vertexDrag = VertexDragState{};
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    state.map = BuildMapWithMovedVertexGroup(target);
    if (state.selectedSectorIndex < 0 && !affected.empty()) {
        SelectSector(affected.front().sectorIndex);
    }
    state.dirty = true;
    state.vertexDrag = VertexDragState{};
    statusText = TextFormat(
            "Moved vertex %.2f,%.2f -> %.2f,%.2f",
            original.x,
            original.y,
            target.x,
            target.y
    );
}

void SectorEditor::CancelVertexDrag(const char* message)
{
    state.vertexDrag = VertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::UpdatePreview3D(engine::Input& input, float dt)
{
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_TAB || event.key.key == KEY_ESCAPE) {
                    LeavePreview3D();
                    engine::ConsumeEvent(event);
                }
            }
    );

    if (state.mode == SectorEditorMode::Preview3D) {
        preview.Update(input, dt);
    }
}

void SectorEditor::CancelPendingSector(const char* message)
{
    state.pendingSector.points.clear();
    state.pendingSector.active = false;
    state.pendingSector.errorMessage.clear();
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::RemoveLastPendingSectorPoint()
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    state.pendingSector.points.pop_back();
    state.pendingSector.errorMessage.clear();
    if (state.pendingSector.points.empty()) {
        CancelPendingSector("Cancelled sector");
    } else {
        statusText = TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
    }
}

void SectorEditor::AddPendingSectorPoint(SectorPoint point)
{
    std::string error;
    if (!ValidatePendingPoint(point, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    state.pendingSector.active = true;
    state.pendingSector.points.push_back(point);
    state.pendingSector.errorMessage.clear();
    statusText = TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
}

void SectorEditor::FinalizePendingSector()
{
    if (!state.pendingSector.active || state.pendingSector.points.size() < 3) {
        state.pendingSector.errorMessage = "Need at least 3 points to close sector";
        statusText = state.pendingSector.errorMessage;
        return;
    }

    std::string error;
    if (!ValidateSectorPolygon(state.pendingSector.points, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    SectorDefinition sector;
    sector.id = GenerateUniqueSectorId();
    sector.points = state.pendingSector.points;
    if (PolygonArea2(sector.points) < 0.0f) {
        std::reverse(sector.points.begin(), sector.points.end());
    }
    sector.floorZ = state.defaultSectorFloorZ;
    sector.ceilingZ = state.defaultSectorCeilingZ;
    sector.floorTextureId = state.defaultFloorTextureId;
    sector.ceilingTextureId = state.defaultCeilingTextureId;
    sector.wallTextureId = state.defaultWallTextureId;
    sector.lowerWallTextureId = state.defaultLowerWallTextureId.empty() ? sector.wallTextureId : state.defaultLowerWallTextureId;
    sector.upperWallTextureId = state.defaultUpperWallTextureId.empty() ? sector.wallTextureId : state.defaultUpperWallTextureId;

    state.map.sectors.push_back(std::move(sector));
    SelectSector(static_cast<int>(state.map.sectors.size()) - 1);
    const std::string createdId = state.map.sectors.back().id;
    state.pendingSector.points.clear();
    state.pendingSector.active = false;
    state.pendingSector.errorMessage.clear();
    state.dirty = true;
    statusText = TextFormat("Created sector %s", createdId.c_str());
}

bool SectorEditor::CanClosePendingSectorAt(SectorPoint point) const
{
    return state.pendingSector.active
            && state.pendingSector.points.size() >= 3
            && SamePoint(point, state.pendingSector.points.front());
}

SectorPoint SectorEditor::CurrentSnappedSectorPoint() const
{
    return Vector2ToSectorPoint(state.snappedMouseMap);
}

std::string SectorEditor::GenerateUniqueSectorId() const
{
    for (int id = 1; id < 10000; ++id) {
        char candidate[32];
        std::snprintf(candidate, sizeof(candidate), "sector_%03d", id);
        const bool used = std::any_of(
                state.map.sectors.begin(),
                state.map.sectors.end(),
                [candidate](const SectorDefinition& sector) { return sector.id == candidate; }
        );
        if (!used) {
            return candidate;
        }
    }

    return TextFormat("sector_%zu", state.map.sectors.size() + 1);
}

void SectorEditor::SyncSelectedSectorIdBuffer()
{
    if (state.selectedSectorIndex < 0
            || state.selectedSectorIndex >= static_cast<int>(state.map.sectors.size())) {
        uiState.selectedSectorIdBuffer[0] = '\0';
        uiState.idBufferSectorIndex = -1;
        uiState.idEditError.clear();
        return;
    }

    if (uiState.idBufferSectorIndex == state.selectedSectorIndex) {
        return;
    }

    const std::string& id = state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)].id;
    std::snprintf(uiState.selectedSectorIdBuffer, sizeof(uiState.selectedSectorIdBuffer), "%s", id.c_str());
    uiState.idBufferSectorIndex = state.selectedSectorIndex;
    uiState.idEditError.clear();
}

bool SectorEditor::TryRenameSelectedSector()
{
    if (state.selectedSectorIndex < 0
            || state.selectedSectorIndex >= static_cast<int>(state.map.sectors.size())) {
        uiState.idEditError = "No sector selected";
        statusText = uiState.idEditError;
        return false;
    }

    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)];
    const std::string newId = uiState.selectedSectorIdBuffer;
    if (newId == sector.id) {
        uiState.idEditError.clear();
        return true;
    }

    if (newId.empty()) {
        uiState.idEditError = "Sector id cannot be empty";
        statusText = uiState.idEditError;
        return false;
    }

    if (std::isspace(static_cast<unsigned char>(newId.front()))
            || std::isspace(static_cast<unsigned char>(newId.back()))) {
        uiState.idEditError = "Sector id cannot start or end with whitespace";
        statusText = uiState.idEditError;
        return false;
    }

    for (char ch : newId) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!(std::isalnum(value) || ch == '_' || ch == '-')) {
            uiState.idEditError = "Use letters, digits, underscore, or dash";
            statusText = uiState.idEditError;
            return false;
        }
    }

    for (size_t i = 0; i < state.map.sectors.size(); ++i) {
        if (static_cast<int>(i) != state.selectedSectorIndex
                && state.map.sectors[i].id == newId) {
            uiState.idEditError = "Sector id already exists";
            statusText = uiState.idEditError;
            return false;
        }
    }

    sector.id = newId;
    state.dirty = true;
    uiState.idEditError.clear();
    statusText = TextFormat("Renamed sector to %s", sector.id.c_str());
    return true;
}

bool SectorEditor::DeleteSelectedSector()
{
    return DeleteSectorAt(state.selectedSectorIndex);
}

bool SectorEditor::DeleteSectorAt(int sectorIndex)
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(state.map.sectors.size())) {
        return false;
    }

    const std::string deletedId = state.map.sectors[static_cast<size_t>(sectorIndex)].id;
    state.map.sectors.erase(state.map.sectors.begin() + sectorIndex);
    state.pendingSector = PendingSectorDraw{};
    state.hoveredSectorIndex = -1;

    if (state.selectedSectorIndex == sectorIndex) {
        ClearSelection();
    } else if (state.selectedSectorIndex > sectorIndex) {
        --state.selectedSectorIndex;
    } else if (state.selectedEdgeIndex >= 0
            && (state.selectedSectorIndex < 0
                    || state.selectedSectorIndex >= static_cast<int>(state.map.sectors.size())
                    || state.selectedEdgeIndex >= static_cast<int>(state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)].points.size()))) {
        state.selectedEdgeIndex = -1;
    }

    SyncSelectedSectorIdBuffer();
    state.dirty = true;
    statusText = TextFormat("Deleted sector %s", deletedId.c_str());
    return true;
}

std::vector<SectorVertexRef> SectorEditor::FindVerticesAtPoint(SectorPoint point) const
{
    std::vector<SectorVertexRef> refs;
    for (size_t sectorIndex = 0; sectorIndex < state.map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = state.map.sectors[sectorIndex];
        for (size_t pointIndex = 0; pointIndex < sector.points.size(); ++pointIndex) {
            if (SamePoint(sector.points[pointIndex], point)) {
                refs.push_back(SectorVertexRef{
                        static_cast<int>(sectorIndex),
                        static_cast<int>(pointIndex)
                });
            }
        }
    }
    return refs;
}

bool SectorEditor::FindVertexNearScreenPoint(
        Vector2 screenPoint,
        SectorPoint& outPoint,
        std::vector<SectorVertexRef>& outRefs) const
{
    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    bool found = false;
    SectorPoint bestPoint{};

    for (const SectorDefinition& sector : state.map.sectors) {
        for (SectorPoint point : sector.points) {
            const Vector2 screenVertex = MapToScreen(SectorPointToVector2(point));
            const float dx = screenVertex.x - screenPoint.x;
            const float dy = screenVertex.y - screenPoint.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                bestPoint = point;
                found = true;
            }
        }
    }

    if (!found) {
        outRefs.clear();
        return false;
    }

    outPoint = bestPoint;
    outRefs = FindVerticesAtPoint(bestPoint);
    return !outRefs.empty();
}

SectorPoint SectorEditor::SnapVertexMoveTarget(Vector2 mapPoint) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    SectorPoint snapped{
            std::round(mapPoint.x / grid) * grid,
            std::round(mapPoint.y / grid) * grid
    };

    const float threshold = std::max(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom), grid * 0.20f);
    float bestDistance2 = threshold * threshold;
    bool found = false;
    SectorPoint best = snapped;

    for (size_t sectorIndex = 0; sectorIndex < state.map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = state.map.sectors[sectorIndex];
        for (size_t pointIndex = 0; pointIndex < sector.points.size(); ++pointIndex) {
            const bool isDraggedRef = std::any_of(
                    state.vertexDrag.affectedVertices.begin(),
                    state.vertexDrag.affectedVertices.end(),
                    [sectorIndex, pointIndex](SectorVertexRef ref) {
                        return ref.sectorIndex == static_cast<int>(sectorIndex)
                                && ref.pointIndex == static_cast<int>(pointIndex);
                    }
            );
            if (isDraggedRef) {
                continue;
            }

            const SectorPoint point = sector.points[pointIndex];
            const float dx = point.x - mapPoint.x;
            const float dy = point.y - mapPoint.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = point;
                found = true;
            }
        }
    }

    return found ? best : snapped;
}

bool SectorEditor::ValidateExistingSectorMap(const SectorMap& map, std::string& error) const
{
    for (size_t i = 0; i < map.sectors.size(); ++i) {
        if (!ValidateSectorShape(map.sectors[i].points, error)) {
            return false;
        }
    }

    for (size_t a = 0; a < map.sectors.size(); ++a) {
        for (size_t b = a + 1; b < map.sectors.size(); ++b) {
            if (!ValidateSectorAgainstSector(map.sectors[a].points, map.sectors[b].points, error)) {
                return false;
            }
        }
    }

    return true;
}

SectorMap SectorEditor::BuildMapWithMovedVertexGroup(SectorPoint targetPoint) const
{
    SectorMap moved = state.map;
    for (SectorVertexRef ref : state.vertexDrag.affectedVertices) {
        if (ref.sectorIndex < 0
                || ref.sectorIndex >= static_cast<int>(moved.sectors.size())) {
            continue;
        }

        SectorDefinition& sector = moved.sectors[static_cast<size_t>(ref.sectorIndex)];
        if (ref.pointIndex < 0 || ref.pointIndex >= static_cast<int>(sector.points.size())) {
            continue;
        }

        sector.points[static_cast<size_t>(ref.pointIndex)] = targetPoint;
    }
    return moved;
}

bool SectorEditor::ValidateMovedVertexGroup(SectorPoint targetPoint, std::string& error) const
{
    const SectorMap moved = BuildMapWithMovedVertexGroup(targetPoint);
    return ValidateExistingSectorMap(moved, error);
}

bool SectorEditor::ValidatePendingPoint(SectorPoint point, std::string& error) const
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return true;
    }

    const std::vector<SectorPoint>& points = state.pendingSector.points;
    const SectorPoint previous = points.back();
    if (SamePoint(point, previous)) {
        error = "Duplicate point";
        return false;
    }

    if (points.size() >= 2 && SamePoint(point, points.front())) {
        error = points.size() >= 3 ? "Click first point to close sector" : "Need at least 3 points to close sector";
        return false;
    }

    for (size_t i = 0; i < points.size(); ++i) {
        if (SamePoint(point, points[i])) {
            error = "Duplicate point";
            return false;
        }
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        if (i + 1 == points.size() - 1) {
            continue;
        }
        if (SegmentsIntersect(previous, point, points[i], points[i + 1])) {
            error = "Segment intersects pending polygon";
            return false;
        }
    }

    return true;
}

bool SectorEditor::ValidateSectorPolygon(const std::vector<SectorPoint>& points, std::string& error) const
{
    if (!ValidateSectorShape(points, error)) {
        return false;
    }

    const std::string newId = GenerateUniqueSectorId();
    const bool idUsed = std::any_of(
            state.map.sectors.begin(),
            state.map.sectors.end(),
            [&newId](const SectorDefinition& sector) { return sector.id == newId; }
    );
    if (idUsed) {
        error = "Sector id is not unique";
        return false;
    }

    for (const SectorDefinition& existing : state.map.sectors) {
        if (!ValidateSectorAgainstSector(points, existing.points, error)) {
            return false;
        }
    }

    return true;
}

void SectorEditor::RenderPreview3D(engine::AssetManager& assets)
{
    preview.Render(assets);
}

void SectorEditor::DrawPreviewOverlay(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font) const
{
    const Rectangle panel{32.0f, 32.0f, 760.0f, 172.0f};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const Vector3 position = preview.Position();
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 14.0f, panel.width - 36.0f, 34.0f},
            font,
            "3D Preview",
            engine::UITextJustify::Left
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 54.0f, panel.width - 36.0f, 30.0f},
            font,
            "WASD move | Mouse look | Space/Ctrl up/down | F11 cursor | Tab/Escape return",
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 92.0f, panel.width - 36.0f, 30.0f},
            font,
            TextFormat(
                    "pos %.2f %.2f %.2f | sectors %zu | batches %zu | triangles %d",
                    position.x,
                    position.y,
                    position.z,
                    preview.SectorCount(),
                    preview.BatchCount(),
                    preview.TriangleCount()
            ),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 130.0f, panel.width - 36.0f, 30.0f},
            font,
            TextFormat(
                    "assets %.0f%% | %s%s",
                    preview.AssetProgress(assets) * 100.0f,
                    statusText.empty() ? "Ready" : statusText.c_str(),
                    state.dirty ? " | unsaved changes" : ""
            ),
            engine::UITextJustify::Left,
            state.dirty ? Color{236, 196, 92, 255} : config.mutedTextColor
    );
}

void SectorEditor::DrawGrid() const
{
    const int grid = std::max(1, state.gridSize);
    const Vector2 minMap = ScreenToMap(Vector2{canvasRect.x, canvasRect.y});
    const Vector2 maxMap = ScreenToMap(Vector2{canvasRect.x + canvasRect.width, canvasRect.y + canvasRect.height});
    const int startX = static_cast<int>(std::floor(minMap.x / static_cast<float>(grid))) * grid;
    const int endX = static_cast<int>(std::ceil(maxMap.x / static_cast<float>(grid))) * grid;
    const int startY = static_cast<int>(std::floor(minMap.y / static_cast<float>(grid))) * grid;
    const int endY = static_cast<int>(std::ceil(maxMap.y / static_cast<float>(grid))) * grid;

    const Color gridColor{44, 50, 62, 155};
    const Color majorColor{62, 70, 86, 185};
    const Color axisColor{112, 148, 148, 230};

    for (int x = startX; x <= endX; x += grid) {
        const Vector2 a = MapToScreen(Vector2{static_cast<float>(x), minMap.y});
        const Vector2 b = MapToScreen(Vector2{static_cast<float>(x), maxMap.y});
        const bool axis = x == 0;
        const bool major = grid < 8 && x % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    for (int y = startY; y <= endY; y += grid) {
        const Vector2 a = MapToScreen(Vector2{minMap.x, static_cast<float>(y)});
        const Vector2 b = MapToScreen(Vector2{maxMap.x, static_cast<float>(y)});
        const bool axis = y == 0;
        const bool major = grid < 8 && y % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    if (state.showAxes) {
        const Vector2 origin = MapToScreen(Vector2{0.0f, 0.0f});
        DrawCircleV(origin, 5.0f, Color{180, 210, 190, 255});
    }
}

void SectorEditor::DrawSectors() const
{
    if (!initialized) {
        DrawText("Sector map failed to load", static_cast<int>(canvasRect.x + 24.0f), static_cast<int>(canvasRect.y + 24.0f), 28, RED);
        return;
    }

    for (size_t i = 0; i < state.map.sectors.size(); ++i) {
        DrawSector(state.map.sectors[i], static_cast<int>(i));
    }

    if (Contains(canvasRect, GetMousePosition())) {
        const Vector2 snap = MapToScreen(state.snappedMouseMap);
        DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
        DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
    }
}

void SectorEditor::DrawPendingSector() const
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    const SectorPoint cursorPoint = CurrentSnappedSectorPoint();
    const bool closeable = CanClosePendingSectorAt(cursorPoint);
    std::string previewError = state.pendingSector.errorMessage;
    if (!closeable && state.pendingSector.active) {
        std::string candidateError;
        if (!ValidatePendingPoint(cursorPoint, candidateError)
                && candidateError != "Click first point to close sector") {
            previewError = candidateError;
        }
    }

    if (state.pendingSector.points.size() >= 3 && previewError.empty()) {
        std::string finalError;
        if (!ValidateSectorPolygon(state.pendingSector.points, finalError)) {
            previewError = finalError;
        }
    }

    const bool invalid = !previewError.empty() && previewError != "Click first point to close sector";
    const Color lineColor = invalid ? Color{220, 88, 88, 245} : Color{236, 196, 92, 245};
    const Color previewColor = invalid ? Color{220, 88, 88, 170} : Color{236, 196, 92, 175};
    const Color pointColor = Color{245, 226, 154, 255};
    const Color firstColor = closeable ? Color{120, 230, 154, 255} : Color{245, 226, 154, 255};

    for (size_t i = 0; i + 1 < state.pendingSector.points.size(); ++i) {
        const Vector2 a = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const Vector2 b = MapToScreen(SectorPointToVector2(state.pendingSector.points[i + 1]));
        DrawLineEx(a, b, 3.0f, lineColor);
    }

    const Vector2 last = MapToScreen(SectorPointToVector2(state.pendingSector.points.back()));
    const Vector2 cursor = MapToScreen(SectorPointToVector2(cursorPoint));
    if (!SamePoint(cursorPoint, state.pendingSector.points.back())) {
        DrawLineEx(last, cursor, 2.0f, previewColor);
    }

    if (state.pendingSector.points.size() >= 2) {
        const Vector2 first = MapToScreen(SectorPointToVector2(state.pendingSector.points.front()));
        DrawLineEx(cursor, first, 1.5f, WithAlpha(previewColor, closeable ? 230 : 120));
    }

    for (size_t i = 0; i < state.pendingSector.points.size(); ++i) {
        const Vector2 point = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const bool first = i == 0;
        DrawCircleV(point, first ? 6.0f : 5.0f, first ? firstColor : pointColor);
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), first ? 8.0f : 7.0f, Color{20, 24, 32, 255});
    }
}

void SectorEditor::DrawVertexMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (!state.vertexDrag.active && state.hasHoveredVertex) {
        const Vector2 point = MapToScreen(SectorPointToVector2(state.hoveredVertexPoint));
        const Color color = state.hoveredVertexRefs.size() > 1
                ? Color{122, 220, 244, 255}
                : Color{245, 226, 154, 255};
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), 11.0f, color);
        DrawCircleV(point, 4.5f, color);
        return;
    }

    if (!state.vertexDrag.active) {
        return;
    }

    const bool invalid = !state.vertexDrag.errorMessage.empty();
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};

    for (size_t sectorIndex = 0; sectorIndex < state.map.sectors.size(); ++sectorIndex) {
        const bool affected = std::any_of(
                state.vertexDrag.affectedVertices.begin(),
                state.vertexDrag.affectedVertices.end(),
                [sectorIndex](SectorVertexRef ref) {
                    return ref.sectorIndex == static_cast<int>(sectorIndex);
                }
        );
        if (!affected) {
            continue;
        }

        const SectorDefinition& sector = state.map.sectors[sectorIndex];
        if (sector.points.size() < 2) {
            continue;
        }

        for (size_t pointIndex = 0; pointIndex < sector.points.size(); ++pointIndex) {
            SectorPoint a = sector.points[pointIndex];
            SectorPoint b = sector.points[(pointIndex + 1) % sector.points.size()];
            for (SectorVertexRef ref : state.vertexDrag.affectedVertices) {
                if (ref.sectorIndex == static_cast<int>(sectorIndex)
                        && ref.pointIndex == static_cast<int>(pointIndex)) {
                    a = state.vertexDrag.snappedPoint;
                }
                if (ref.sectorIndex == static_cast<int>(sectorIndex)
                        && ref.pointIndex == static_cast<int>((pointIndex + 1) % sector.points.size())) {
                    b = state.vertexDrag.snappedPoint;
                }
            }
            DrawLineEx(
                    MapToScreen(SectorPointToVector2(a)),
                    MapToScreen(SectorPointToVector2(b)),
                    4.0f,
                    previewColor
            );
        }
    }

    const Vector2 original = MapToScreen(SectorPointToVector2(state.vertexDrag.originalPoint));
    const Vector2 target = MapToScreen(SectorPointToVector2(state.vertexDrag.snappedPoint));
    DrawLineEx(original, target, 2.0f, WithAlpha(targetColor, 180));
    DrawCircleLines(static_cast<int>(std::round(original.x)), static_cast<int>(std::round(original.y)), 10.0f, originalColor);
    DrawCircleLines(static_cast<int>(std::round(target.x)), static_cast<int>(std::round(target.y)), 13.0f, targetColor);
    DrawCircleV(target, 5.0f, targetColor);
}

void SectorEditor::DrawSector(const SectorDefinition& sector, int sectorIndex) const
{
    if (sector.points.size() < 3) {
        return;
    }

    const bool hovered = sectorIndex == state.hoveredSectorIndex;
    const bool selected = sectorIndex == state.selectedSectorIndex;
    const bool eraseHovered = hovered && state.currentTool == SectorEditorTool::Erase;
    const Color fill = eraseHovered
            ? Color{205, 72, 72, 88}
            : selected ? Color{88, 170, 120, 85}
            : hovered ? Color{125, 164, 214, 72} : Color{82, 112, 154, 48};
    const Color outline = eraseHovered
            ? Color{240, 95, 95, 255}
            : selected ? Color{118, 224, 156, 255}
            : hovered ? Color{157, 194, 245, 255} : Color{116, 139, 174, 255};

    std::vector<Vector2> screenPoints;
    screenPoints.reserve(sector.points.size());
    for (SectorPoint point : sector.points) {
        screenPoints.push_back(MapToScreen(SectorPointToVector2(point)));
    }

    if (IsConvexPolygon(sector)) {
        for (size_t i = 1; i + 1 < screenPoints.size(); ++i) {
            DrawTriangle(screenPoints[0], screenPoints[i], screenPoints[i + 1], fill);
        }
    }

    for (size_t i = 0; i < screenPoints.size(); ++i) {
        const Vector2 a = screenPoints[i];
        const Vector2 b = screenPoints[(i + 1) % screenPoints.size()];
        DrawLineEx(a, b, selected ? 3.0f : 2.0f, outline);
    }

    for (Vector2 point : screenPoints) {
        DrawRectangleRec(Rectangle{point.x - 4.0f, point.y - 4.0f, 8.0f, 8.0f}, WithAlpha(outline, 255));
    }

    if (state.hoveredEdgeSectorIndex == sectorIndex && state.hoveredEdgeIndex >= 0) {
        const size_t edgeIndex = static_cast<size_t>(state.hoveredEdgeIndex);
        if (edgeIndex < screenPoints.size()) {
            DrawLineEx(
                    screenPoints[edgeIndex],
                    screenPoints[(edgeIndex + 1) % screenPoints.size()],
                    5.0f,
                    Color{245, 226, 154, 230}
            );
        }
    }

    if (state.selectedSectorIndex == sectorIndex && state.selectedEdgeIndex >= 0) {
        const size_t edgeIndex = static_cast<size_t>(state.selectedEdgeIndex);
        if (edgeIndex < screenPoints.size()) {
            const Vector2 a = screenPoints[edgeIndex];
            const Vector2 b = screenPoints[(edgeIndex + 1) % screenPoints.size()];
            const Vector2 inwardNormal = SelectedEdgeInwardNormal(sector, state.selectedEdgeIndex);
            const Vector2 offset{
                    inwardNormal.x * 8.0f,
                    inwardNormal.y * 8.0f
            };
            const Vector2 offsetA{a.x + offset.x, a.y + offset.y};
            const Vector2 offsetB{b.x + offset.x, b.y + offset.y};
            DrawLineEx(a, b, 6.0f, Color{122, 220, 244, 255});
            DrawLineEx(offsetA, offsetB, 3.0f, Color{196, 244, 255, 255});
            const Vector2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            const Vector2 markerEnd{
                    mid.x + inwardNormal.x * 22.0f,
                    mid.y + inwardNormal.y * 22.0f
            };
            DrawLineEx(mid, markerEnd, 3.0f, Color{196, 244, 255, 255});
            DrawCircleV(markerEnd, 4.0f, Color{196, 244, 255, 255});
            DrawText(TextFormat("%d", state.selectedEdgeIndex), static_cast<int>(mid.x + 8.0f), static_cast<int>(mid.y + 8.0f), 18, Color{122, 220, 244, 255});
        }
    }

    if (state.showSectorIds) {
        const Vector2 center = MapToScreen(AverageSectorPoint(sector));
        DrawText(sector.id.c_str(), static_cast<int>(center.x - 24.0f), static_cast<int>(center.y - 10.0f), 18, RAYWHITE);
    }
}

Vector2 SectorEditor::SelectedEdgeInwardNormal(const SectorDefinition& sector, int edgeIndex) const
{
    if (edgeIndex < 0 || edgeIndex >= static_cast<int>(sector.points.size())) {
        return Vector2{0.0f, 0.0f};
    }

    const SectorPoint from = sector.points[static_cast<size_t>(edgeIndex)];
    const SectorPoint to = sector.points[(static_cast<size_t>(edgeIndex) + 1) % sector.points.size()];
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= GeometryEpsilon) {
        return Vector2{0.0f, 0.0f};
    }

    const Vector2 normalA{-dy / length, dx / length};
    const Vector2 normalB{dy / length, -dx / length};
    const Vector2 mid{(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
    const float probeDistance = std::max(0.05f, 3.0f / std::max(1.0f, state.viewZoom));
    const Vector2 probeA{mid.x + normalA.x * probeDistance, mid.y + normalA.y * probeDistance};
    if (PointInSectorPolygon(probeA, sector)) {
        return normalA;
    }

    const Vector2 probeB{mid.x + normalB.x * probeDistance, mid.y + normalB.y * probeDistance};
    if (PointInSectorPolygon(probeB, sector)) {
        return normalB;
    }

    const Vector2 center = AverageSectorPoint(sector);
    Vector2 fallback{center.x - mid.x, center.y - mid.y};
    const float fallbackLength = std::sqrt(fallback.x * fallback.x + fallback.y * fallback.y);
    if (fallbackLength > GeometryEpsilon) {
        fallback.x /= fallbackLength;
        fallback.y /= fallbackLength;
        return fallback;
    }

    return normalA;
}

void SectorEditor::DrawToolsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_tools",
            BuildLeftPanelRect(),
            font,
            "Tools"
    );

    float y = panel.contentRect.y;
    const float rowH = 46.0f;
    const float gap = config.rowSpacing;
    const SectorEditorTool tools[] = {
            SectorEditorTool::Select,
            SectorEditorTool::Sector,
            SectorEditorTool::Move,
            SectorEditorTool::Erase
    };

    for (SectorEditorTool tool : tools) {
        if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                TextFormat("sector_editor_tool_%s", ToolName(tool)),
                Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH},
                font,
                ToolName(tool),
                state.currentTool == tool)) {
            if (state.currentTool == SectorEditorTool::Sector && tool != SectorEditorTool::Sector) {
                CancelPendingSector("Cancelled sector");
            }
            if (state.vertexDrag.active && tool != SectorEditorTool::Move) {
                CancelVertexDrag("Cancelled vertex move");
            }
            state.currentTool = tool;
        }
        y += rowH + gap;
    }

    engine::Separator(config, Rectangle{panel.contentRect.x, y, panel.contentRect.width, 12.0f});
    y += 22.0f;

    const float halfButtonW = (panel.contentRect.width - gap) * 0.5f;
    if (engine::Button(ui, config, input, assets, "sector_editor_save", Rectangle{panel.contentRect.x, y, halfButtonW, rowH}, font, "Save")) {
        SaveMap();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_reload", Rectangle{panel.contentRect.x + halfButtonW + gap, y, halfButtonW, rowH}, font, "Reload")) {
        ReloadMap(assets);
    }
    y += rowH + gap;

    engine::Separator(config, Rectangle{panel.contentRect.x, y, panel.contentRect.width, 12.0f});
    y += 22.0f;

    const engine::UILabelFieldRowResult gridRow = engine::LabelFieldRow(
            config,
            assets,
            Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH},
            font,
            "Grid",
            64.0f
    );
    engine::IntInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_grid",
            gridRow.fieldRect,
            font,
            state.gridSize,
            uiState.gridSizeInput,
            1,
            64,
            1
    );
    y += rowH + gap;

    engine::Checkbox(ui, config, input, assets, "sector_editor_show_grid", Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH}, font, "Show grid", state.showGrid);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_axes", Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH}, font, "Show axes", state.showAxes);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_ids", Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH}, font, "Show ids", state.showSectorIds);
    y += rowH + gap;

    engine::Separator(config, Rectangle{panel.contentRect.x, y, panel.contentRect.width, 12.0f});
    y += 22.0f;

    if (engine::Button(ui, config, input, assets, "sector_editor_preview_3d", Rectangle{panel.contentRect.x, y, panel.contentRect.width, rowH}, font, "Preview 3D")) {
        TryEnterPreview3D(assets, ui);
    }

    engine::EndPanel(ui, config, panel);
}

void SectorEditor::DrawSectorsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_sectors",
            BuildRightPanelRect(),
            font,
            "Inspector"
    );

    SyncSelectedSectorIdBuffer();

    const bool hasSelectedSector = state.selectedSectorIndex >= 0
            && state.selectedSectorIndex < static_cast<int>(state.map.sectors.size());
    if (hasSelectedSector) {
        const SectorDefinition& selectedSector = state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)];
        if (state.selectedEdgeIndex >= 0 && state.selectedEdgeIndex >= static_cast<int>(selectedSector.points.size())) {
            state.selectedEdgeIndex = -1;
        }
    }

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    const auto inspectorContentHeight = [&]() {
        if (!hasSelectedSector) {
            return 42.0f;
        }

        float height = 38.0f; // Sector title.
        height += rowH + gap; // Id.
        if (!uiState.idEditError.empty()) {
            height += 36.0f;
        }
        height += rowH + gap; // Delete.
        height += rowH + gap; // Floor.
        height += rowH + gap; // Ceiling.

        if (state.selectedEdgeIndex < 0) {
            height += 4.0f;
            height += 5.0f * (36.0f + gap);
            return height;
        }

        height += 8.0f; // Spacing before separator.
        height += 18.0f; // Separator.
        height += 34.0f; // Selected side.
        height += 30.0f; // From/to.
        height += 34.0f; // Opposite side.

        const EdgeNeighborInfo neighbor = FindReverseEdgeNeighbor(
                state.map,
                state.selectedSectorIndex,
                state.selectedEdgeIndex
        );
        if (neighbor.hasNeighbor) {
            height += 38.0f + gap;
        }

        height += 3.0f * (36.0f + gap); // Edge texture rows.
        height += 4.0f;
        height += 38.0f + gap; // UV part buttons.
        height += 62.0f + gap; // Scale inputs.
        height += 62.0f + gap; // Offset inputs.
        height += 38.0f; // Reset button.
        return height;
    };
    const float contentH = inspectorContentHeight();
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_inspector_scroll",
            panel.contentRect,
            Vector2{scrollContentW, contentH},
            uiState.inspectorScroll,
            false
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;

    if (!hasSelectedSector) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 42.0f}, font, "Selected: none", engine::UITextJustify::Left, config.mutedTextColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)];
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Sector: %s", sector.id.c_str()), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 70.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult idResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_selected_sector_id",
            Rectangle{labelW, y, contentW - labelW, rowH},
            font,
            uiState.selectedSectorIdBuffer,
            sizeof(uiState.selectedSectorIdBuffer),
            0,
            sizeof(uiState.selectedSectorIdBuffer) - 1,
            engine::UITextJustify::Left
    );
    if (idResult.submitted) {
        TryRenameSelectedSector();
    }
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_sector", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Sector")) {
        DeleteSelectedSector();
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }
    y += rowH + gap;

    const float numberLabelW = 92.0f;
    const float numberFieldW = 102.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, numberLabelW, rowH}, font, "Floor:", engine::UITextJustify::Right, config.mutedTextColor);
    const engine::UINumericInputResult floorResult = engine::FloatInput(ui, config, input, assets, "sector_editor_floor", Rectangle{numberLabelW, y, numberFieldW, rowH}, font, sector.floorZ, uiState.floorInput, -64.0f, 64.0f, 2);
    if (floorResult.changed) {
        state.dirty = true;
        statusText = TextFormat("Edited sector %s", sector.id.c_str());
    }
    y += rowH + gap;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, numberLabelW, rowH}, font, "Ceiling:", engine::UITextJustify::Right, config.mutedTextColor);
    const engine::UINumericInputResult ceilingResult = engine::FloatInput(ui, config, input, assets, "sector_editor_ceiling", Rectangle{numberLabelW, y, numberFieldW, rowH}, font, sector.ceilingZ, uiState.ceilingInput, -64.0f, 64.0f, 2);
    if (ceilingResult.changed) {
        state.dirty = true;
        statusText = TextFormat("Edited sector %s", sector.id.c_str());
    }
    y += rowH + gap;

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TexturePickerTargetKind target, int edgeIndex, bool overridden) {
        const float buttonW = 38.0f;
        const float labelColumnW = 74.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                font,
                textureId.c_str(),
                engine::UITextJustify::Left,
                overridden ? config.accentColor : config.mutedTextColor
        );
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            OpenTexturePicker(target, state.selectedSectorIndex, edgeIndex);
        }
        y += row.height + gap;
    };

    if (state.selectedEdgeIndex < 0) {
        y += 4.0f;
        drawTextureRow("sector_editor_pick_floor", "Floor:", sector.floorTextureId, TexturePickerTargetKind::SectorFloor, -1, false);
        drawTextureRow("sector_editor_pick_ceiling", "Ceil:", sector.ceilingTextureId, TexturePickerTargetKind::SectorCeiling, -1, false);
        drawTextureRow("sector_editor_pick_wall", "Wall:", sector.wallTextureId, TexturePickerTargetKind::SectorWall, -1, false);
        drawTextureRow("sector_editor_pick_lower_wall", "Lower:", sector.lowerWallTextureId, TexturePickerTargetKind::SectorLowerWall, -1, false);
        drawTextureRow("sector_editor_pick_upper_wall", "Upper:", sector.upperWallTextureId, TexturePickerTargetKind::SectorUpperWall, -1, false);
    } else {
        y += 8.0f;
        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;

        const int edgeIndex = state.selectedEdgeIndex;
        const SectorPoint from = sector.points[static_cast<size_t>(edgeIndex)];
        const SectorPoint to = sector.points[(static_cast<size_t>(edgeIndex) + 1) % sector.points.size()];
        const EdgeNeighborInfo neighbor = FindReverseEdgeNeighbor(state.map, state.selectedSectorIndex, edgeIndex);
        const EffectiveEdgeSettings effective = GetEffectiveEdgeSettings(sector, edgeIndex);
        const SectorEdgeOverride* edgeOverride = FindEdgeOverride(state.selectedSectorIndex, edgeIndex);
        const bool wallOverridden = edgeOverride != nullptr && edgeOverride->hasWallTexture;
        const bool lowerOverridden = edgeOverride != nullptr && edgeOverride->hasLowerWallTexture;
        const bool upperOverridden = edgeOverride != nullptr && edgeOverride->hasUpperWallTexture;

        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Selected side: %s edge %d", sector.id.c_str(), edgeIndex),
                engine::UITextJustify::Left,
                config.textColor
        );
        y += 34.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, TextFormat("From %.2f, %.2f  To %.2f, %.2f", from.x, from.y, to.x, to.y), engine::UITextJustify::Left, config.mutedTextColor);
        y += 30.0f;
        const char* neighborText = "Opposite side: none (outer edge)";
        if (neighbor.hasNeighbor) {
            neighborText = TextFormat(
                    "Opposite side: %s edge %d",
                    state.map.sectors[static_cast<size_t>(neighbor.sectorIndex)].id.c_str(),
                    neighbor.edgeIndex
            );
        }
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, neighborText, engine::UITextJustify::Left, config.mutedTextColor);
        y += 34.0f;
        if (neighbor.hasNeighbor) {
            if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_edit_opposite_edge",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Edit opposite side")) {
                SelectEdge(neighbor.sectorIndex, neighbor.edgeIndex);
                statusText = TextFormat(
                        "Selected opposite side %s edge %d",
                        state.map.sectors[static_cast<size_t>(neighbor.sectorIndex)].id.c_str(),
                        neighbor.edgeIndex
                );
                engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
                engine::EndPanel(ui, config, panel);
                return;
            }
            y += 38.0f + gap;
        }

        drawTextureRow("sector_editor_pick_edge_wall", "Wall:", effective.wall.textureId, TexturePickerTargetKind::EdgeWall, edgeIndex, wallOverridden);
        drawTextureRow("sector_editor_pick_edge_lower", "Lower:", effective.lower.textureId, TexturePickerTargetKind::EdgeLowerWall, edgeIndex, lowerOverridden);
        drawTextureRow("sector_editor_pick_edge_upper", "Upper:", effective.upper.textureId, TexturePickerTargetKind::EdgeUpperWall, edgeIndex, upperOverridden);

        y += 4.0f;
        const float partButtonW = (contentW - gap * 2.0f) / 3.0f;
        const EdgeUvPart parts[] = {EdgeUvPart::Wall, EdgeUvPart::Lower, EdgeUvPart::Upper};
        for (int i = 0; i < 3; ++i) {
            const EdgeUvPart part = parts[i];
            if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("sector_editor_edge_uv_part_%d", i),
                    Rectangle{static_cast<float>(i) * (partButtonW + gap), y, partButtonW, 38.0f},
                    font,
                    EdgeUvPartName(part),
                    state.selectedEdgeUvPart == part)) {
                state.selectedEdgeUvPart = part;
                uiState.edgeUvScaleUInput = engine::UIFloatInputState{};
                uiState.edgeUvScaleVInput = engine::UIFloatInputState{};
                uiState.edgeUvOffsetUInput = engine::UIFloatInputState{};
                uiState.edgeUvOffsetVInput = engine::UIFloatInputState{};
                statusText = TextFormat("Editing %s UV", EdgeUvPartStatusName(part));
            }
        }
        y += 38.0f + gap;

        auto selectedEffectivePart = [&effective](EdgeUvPart part) -> const EffectiveEdgePartSettings& {
            switch (part) {
                case EdgeUvPart::Wall: return effective.wall;
                case EdgeUvPart::Lower: return effective.lower;
                case EdgeUvPart::Upper: return effective.upper;
            }
            return effective.wall;
        };
        auto selectedMutableUv = [](SectorEdgeOverride& edgeOverride, EdgeUvPart part) -> SectorEdgePartUvOverride& {
            switch (part) {
                case EdgeUvPart::Wall: return edgeOverride.wallUv;
                case EdgeUvPart::Lower: return edgeOverride.lowerUv;
                case EdgeUvPart::Upper: return edgeOverride.upperUv;
            }
            return edgeOverride.wallUv;
        };
        const EffectiveEdgePartSettings& selectedPart = selectedEffectivePart(state.selectedEdgeUvPart);

        auto drawUvInput = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
            engine::Text(ui, config, assets, Rectangle{bounds.x, bounds.y, bounds.width, 26.0f}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
            float editedValue = value;
            const engine::UINumericInputResult result = engine::FloatInput(ui, config, input, assets, id, Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f}, font, editedValue, inputState, minValue, maxValue, 3);
            if (result.changed && editedValue != value) {
                SectorEdgeOverride& mutableOverride = EnsureEdgeOverride(state.selectedSectorIndex, edgeIndex);
                SectorEdgePartUvOverride& uv = selectedMutableUv(mutableOverride, state.selectedEdgeUvPart);
                applyValue(uv, editedValue);
                state.dirty = true;
                statusText = TextFormat("Updated %s UV %s", EdgeUvPartStatusName(state.selectedEdgeUvPart), label);
            }
        };

        const float uvColumnW = (contentW - gap) * 0.5f;
        const float uvBlockH = 62.0f;
        drawUvInput(
                "sector_editor_edge_uv_scale_u",
                "Scale U",
                selectedPart.uvScale.x,
                uiState.edgeUvScaleUInput,
                0.01f,
                64.0f,
                Rectangle{0.0f, y, uvColumnW, uvBlockH},
                [](SectorEdgePartUvOverride& uv, float value) {
                    uv.uvScale.x = value;
                    uv.hasUvScale = true;
                }
        );
        drawUvInput(
                "sector_editor_edge_uv_scale_v",
                "Scale V",
                selectedPart.uvScale.y,
                uiState.edgeUvScaleVInput,
                0.01f,
                64.0f,
                Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
                [](SectorEdgePartUvOverride& uv, float value) {
                    uv.uvScale.y = value;
                    uv.hasUvScale = true;
                }
        );
        y += uvBlockH + gap;
        drawUvInput(
                "sector_editor_edge_uv_offset_u",
                "Offset U",
                selectedPart.uvOffset.x,
                uiState.edgeUvOffsetUInput,
                -1024.0f,
                1024.0f,
                Rectangle{0.0f, y, uvColumnW, uvBlockH},
                [](SectorEdgePartUvOverride& uv, float value) {
                    uv.uvOffset.x = value;
                    uv.hasUvOffset = true;
                }
        );
        drawUvInput(
                "sector_editor_edge_uv_offset_v",
                "Offset V",
                selectedPart.uvOffset.y,
                uiState.edgeUvOffsetVInput,
                -1024.0f,
                1024.0f,
                Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
                [](SectorEdgePartUvOverride& uv, float value) {
                    uv.uvOffset.y = value;
                    uv.hasUvOffset = true;
                }
        );
        y += uvBlockH + gap;

        if (engine::Button(ui, config, input, assets, "sector_editor_edge_reset_uv", Rectangle{0.0f, y, contentW, 38.0f}, font, TextFormat("Reset %s UV", EdgeUvPartName(state.selectedEdgeUvPart)))) {
            SectorEdgeOverride* mutableOverride = FindMutableEdgeOverride(state.selectedSectorIndex, edgeIndex);
            if (mutableOverride != nullptr) {
                SectorEdgePartUvOverride& uv = selectedMutableUv(*mutableOverride, state.selectedEdgeUvPart);
                if (uv.hasUvScale || uv.hasUvOffset) {
                    uv.uvScale = Vector2{1.0f, 1.0f};
                    uv.uvOffset = Vector2{0.0f, 0.0f};
                    uv.hasUvScale = false;
                    uv.hasUvOffset = false;
                    RemoveEdgeOverrideIfEmpty(state.selectedSectorIndex, edgeIndex);
                    state.dirty = true;
                    statusText = TextFormat("Reset %s UV", EdgeUvPartStatusName(state.selectedEdgeUvPart));
                }
            }
        }
    }

    // TODO: Add undo/redo and save/load.
    // TODO: Add validation issue highlighting.

    engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
    engine::EndPanel(ui, config, panel);
}

void SectorEditor::DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.texturePicker = TexturePickerState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    ApplyTexturePickerSelection();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!picker.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 820.0f) * 0.5f,
            (EditorHeight - 620.0f) * 0.5f,
            820.0f,
            620.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f}, font, TextFormat("Pick %s", TextureTargetLabel(picker.target)));
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 350.0f, 420.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(picker.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_texture_picker_scroll",
            listBounds,
            contentSize,
            picker.scroll
    );
    if (!picker.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_texture_picker_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                picker.optionLabels.data(),
                picker.optionLabels.size(),
                picker.selectedTextureIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);

    std::string previewTextureId;
    if (picker.selectedTextureIndex >= 0 && picker.selectedTextureIndex < static_cast<int>(picker.textureIds.size())) {
        previewTextureId = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    }
    if (previewTextureId.empty()) {
        previewTextureId = CurrentTextureForTarget(picker.target, picker.sectorIndex, picker.edgeIndex);
    }

    const Rectangle previewBounds{modal.x + 402.0f, y, 376.0f, 300.0f};
    engine::Image(config, assets, previewBounds, EditorTextureHandleForId(previewTextureId));
    y += 316.0f;

    const auto pathIt = state.map.texturePathsById.find(previewTextureId);
    const std::string path = pathIt == state.map.texturePathsById.end() ? std::string{} : pathIt->second;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 34.0f}, font, TextFormat("Id: %s", previewTextureId.empty() ? "<none>" : previewTextureId.c_str()), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 72.0f}, font, TextFormat("Path: %s", path.empty() ? "<sector default>" : path.c_str()), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_select", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Select")) {
        ApplyTexturePickerSelection();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.texturePicker = TexturePickerState{};
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void SectorEditor::DrawStatusPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    (void)ui;
    const Rectangle panel = BuildBottomPanelRect();
    DrawRectangleRec(panel, config.panelColor);
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string selectedLabel = "none";
    if (state.selectedSectorIndex >= 0
            && state.selectedSectorIndex < static_cast<int>(state.map.sectors.size())) {
        const SectorDefinition& selectedSector = state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)];
        if (state.selectedEdgeIndex >= 0 && state.selectedEdgeIndex < static_cast<int>(selectedSector.points.size())) {
            const SectorEdgeOverride* edgeOverride = FindEdgeOverride(state.selectedSectorIndex, state.selectedEdgeIndex);
            const bool hasTextureOverride = edgeOverride != nullptr
                    && (edgeOverride->hasWallTexture || edgeOverride->hasLowerWallTexture || edgeOverride->hasUpperWallTexture);
            selectedLabel = TextFormat(
                    "%s edge %d %s UV %s",
                    selectedSector.id.c_str(),
                    state.selectedEdgeIndex,
                    hasTextureOverride ? "override" : "inherited",
                    EdgeUvPartName(state.selectedEdgeUvPart)
            );
        } else {
            selectedLabel = selectedSector.id;
        }
    }

    std::string pendingText;
    if (state.pendingSector.active) {
        pendingText = TextFormat(" | pending %zu pts", state.pendingSector.points.size());
    }
    const std::string shortMapPath = ShortPath(mapPath);
    const char* text = TextFormat(
            "%s%s | %s%s | map %s%s | grid %d | selected %s",
            statusText.empty() ? "Map not loaded" : statusText.c_str(),
            state.dirty ? " *modified" : "",
            ToolHelpText(state.currentTool),
            pendingText.c_str(),
            shortMapPath.c_str(),
            state.dirty ? "*" : "",
            state.gridSize,
            selectedLabel.c_str()
    );

    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 17.0f, panel.width - 36.0f, 44.0f},
            font,
            text,
            engine::UITextJustify::Left
    );
}

void SectorEditor::LoadInitialMap(const char* requestedPath)
{
    mapPath = requestedPath == nullptr ? "" : requestedPath;
    fallbackMapPath = std::string(ASSETS_PATH) + "sector_demo/sector_demo_level.json";
    const std::string loadPath = FileExists(mapPath) ? mapPath : fallbackMapPath;

    if (!LoadSectorMap(loadPath.c_str(), state.map)) {
        statusText = "Failed to load map";
        initialized = false;
        return;
    }

    RefreshDefaultTextures();
    state.pendingSector = PendingSectorDraw{};
    state.vertexDrag = VertexDragState{};
    state.selectedSectorIndex = -1;
    state.selectedEdgeIndex = -1;
    state.hoveredSectorIndex = -1;
    state.hasPreviewPose = false;
    state.dirty = false;
    initialized = true;
    statusText = loadPath == mapPath
            ? TextFormat("Map loaded: %zu sectors", state.map.sectors.size())
            : TextFormat("Loaded sample map: %zu sectors", state.map.sectors.size());
}

void SectorEditor::ReloadMap(engine::AssetManager& assets)
{
    const std::string selectedId = state.selectedSectorIndex >= 0
            && state.selectedSectorIndex < static_cast<int>(state.map.sectors.size())
            ? state.map.sectors[static_cast<size_t>(state.selectedSectorIndex)].id
            : std::string{};

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    SectorMap loaded;
    const std::string loadPath = FileExists(mapPath) ? mapPath : fallbackMapPath;
    if (!LoadSectorMap(loadPath.c_str(), loaded)) {
        statusText = TextFormat("Reload failed: %s", ShortPath(loadPath).c_str());
        return;
    }

    state.map = std::move(loaded);
    RefreshDefaultTextures();
    ClearSelection();
    if (!selectedId.empty()) {
        for (size_t i = 0; i < state.map.sectors.size(); ++i) {
            if (state.map.sectors[i].id == selectedId) {
                SelectSector(static_cast<int>(i));
                break;
            }
        }
    }
    RefreshEditorTextureAssets(assets);
    state.dirty = false;
    state.hasPreviewPose = false;
    initialized = true;
    statusText = TextFormat("Reloaded %s", ShortPath(loadPath).c_str());
}

void SectorEditor::SaveMap()
{
    if (!initialized) {
        statusText = "No map loaded";
        return;
    }

    if (SaveSectorMap(mapPath.c_str(), state.map)) {
        state.dirty = false;
        statusText = TextFormat("Saved %s", ShortPath(mapPath).c_str());
    } else {
        statusText = TextFormat("Save failed: %s", ShortPath(mapPath).c_str());
    }
}

bool SectorEditor::TryEnterPreview3D(engine::AssetManager& assets, engine::UIContext& ui)
{
    if (!initialized) {
        statusText = "Preview failed: no map loaded";
        return false;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string error;
    if (!preview.Rebuild(assets, state.map, "sector_editor_preview", error)) {
        state.mode = SectorEditorMode::Edit2D;
        statusText = error.empty() ? "Preview failed" : error;
        return false;
    }

    if (state.hasPreviewPose) {
        preview.ApplyPose(state.lastPreviewPose);
    }

    state.mode = SectorEditorMode::Preview3D;
    statusText = TextFormat(
            "3D preview rebuilt: %zu batches, %d triangles",
            preview.BatchCount(),
            preview.TriangleCount()
    );
    return true;
}

void SectorEditor::LeavePreview3D()
{
    state.lastPreviewPose = preview.Pose();
    state.hasPreviewPose = true;
    state.mode = SectorEditorMode::Edit2D;
    preview.Leave();
    statusText = "Returned to 2D editor";
}

void SectorEditor::RefreshDefaultTextures()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = state.map.texturePathsById.find(preferred);
        if (preferredIt != state.map.texturePathsById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        return state.map.texturePathsById.empty() ? std::string{} : state.map.texturePathsById.begin()->first;
    };

    state.defaultWallTextureId = findTexture("wall");
    state.defaultFloorTextureId = findTexture("floor");
    state.defaultCeilingTextureId = findTexture("ceiling");
    state.defaultLowerWallTextureId = findTexture("step_wall", state.defaultWallTextureId);
    state.defaultUpperWallTextureId = findTexture("upper_wall", state.defaultWallTextureId);
}

void SectorEditor::RefreshEditorTextureAssets(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
        state.editorTextureScope = engine::NullAssetScopeHandle();
    }
    state.editorTextureHandlesById.clear();

    if (state.map.texturePathsById.empty()) {
        return;
    }

    state.editorTextureScope = assets.CreateScope("sector_editor_textures");
    if (engine::IsNull(state.editorTextureScope)) {
        return;
    }

    for (const auto& entry : state.map.texturePathsById) {
        const std::string resolvedPath = ResolveEditorAssetPath(entry.second);
        state.editorTextureHandlesById.emplace(
                entry.first,
                assets.RequestTexture(
                        state.editorTextureScope,
                        entry.first.c_str(),
                        resolvedPath.c_str(),
                        engine::TextureLoad_BilinearFilter
                )
        );
    }
}

engine::TextureHandle SectorEditor::EditorTextureHandleForId(const std::string& textureId) const
{
    const auto it = state.editorTextureHandlesById.find(textureId);
    return it == state.editorTextureHandlesById.end() ? engine::NullTextureHandle() : it->second;
}

bool SectorEditor::PointInSectorPolygon(Vector2 mapPoint, const SectorDefinition& sector) const
{
    bool inside = false;
    const size_t count = sector.points.size();
    if (count < 3) {
        return false;
    }

    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const Vector2 a = SectorPointToVector2(sector.points[i]);
        const Vector2 b = SectorPointToVector2(sector.points[j]);
        const bool intersects = ((a.y > mapPoint.y) != (b.y > mapPoint.y))
                && (mapPoint.x < (b.x - a.x) * (mapPoint.y - a.y) / ((b.y - a.y) == 0.0f ? 0.00001f : (b.y - a.y)) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }

    return inside;
}

int SectorEditor::FindSectorAt(Vector2 mapPoint) const
{
    // Later editor passes can replace this with explicit front/back ordering if sectors overlap.
    for (int i = static_cast<int>(state.map.sectors.size()) - 1; i >= 0; --i) {
        if (PointInSectorPolygon(mapPoint, state.map.sectors[static_cast<size_t>(i)])) {
            return i;
        }
    }
    // TODO: Add FindVertexAt.
    return -1;
}

bool SectorEditor::FindEdgeNearScreenPoint(Vector2 screenPoint, SectorEdgeRef& outEdge) const
{
    return ResolveEdgeHit(screenPoint, ScreenToMap(screenPoint), outEdge);
}

std::vector<SectorEdgeHitCandidate> SectorEditor::FindEdgeHitCandidates(Vector2 screenPoint) const
{
    std::vector<SectorEdgeHitCandidate> candidates;

    for (size_t sectorIndex = 0; sectorIndex < state.map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = state.map.sectors[sectorIndex];
        if (sector.points.size() < 2) {
            continue;
        }

        for (size_t edgeIndex = 0; edgeIndex < sector.points.size(); ++edgeIndex) {
            const Vector2 a = MapToScreen(SectorPointToVector2(sector.points[edgeIndex]));
            const Vector2 b = MapToScreen(SectorPointToVector2(sector.points[(edgeIndex + 1) % sector.points.size()]));
            const float distance = DistancePointToSegment(screenPoint, a, b);
            if (distance <= ScreenEdgePickPixels) {
                candidates.push_back(SectorEdgeHitCandidate{
                        SectorEdgeRef{static_cast<int>(sectorIndex), static_cast<int>(edgeIndex)},
                        distance
                });
            }
        }
    }

    return candidates;
}

bool SectorEditor::ResolveEdgeHit(Vector2 screenPoint, Vector2 rawMapPoint, SectorEdgeRef& outEdge) const
{
    std::vector<SectorEdgeHitCandidate> candidates = FindEdgeHitCandidates(screenPoint);
    if (candidates.empty()) {
        return false;
    }

    std::sort(
            candidates.begin(),
            candidates.end(),
            [](const SectorEdgeHitCandidate& a, const SectorEdgeHitCandidate& b) {
                if (std::fabs(a.screenDistance - b.screenDistance) > 0.001f) {
                    return a.screenDistance < b.screenDistance;
                }
                if (a.edge.sectorIndex != b.edge.sectorIndex) {
                    return a.edge.sectorIndex < b.edge.sectorIndex;
                }
                return a.edge.edgeIndex < b.edge.edgeIndex;
            }
    );

    const SectorEdgeHitCandidate nearest = candidates.front();
    const SectorDefinition& nearestSector = state.map.sectors[static_cast<size_t>(nearest.edge.sectorIndex)];
    const SectorPoint nearestA = nearestSector.points[static_cast<size_t>(nearest.edge.edgeIndex)];
    const SectorPoint nearestB = nearestSector.points[(static_cast<size_t>(nearest.edge.edgeIndex) + 1) % nearestSector.points.size()];

    for (const SectorEdgeHitCandidate& candidate : candidates) {
        const SectorDefinition& sector = state.map.sectors[static_cast<size_t>(candidate.edge.sectorIndex)];
        const SectorPoint a = sector.points[static_cast<size_t>(candidate.edge.edgeIndex)];
        const SectorPoint b = sector.points[(static_cast<size_t>(candidate.edge.edgeIndex) + 1) % sector.points.size()];
        if (!SameUndirectedSegment(nearestA, nearestB, a, b)) {
            continue;
        }

        if (PointInSectorPolygon(rawMapPoint, sector)) {
            outEdge = candidate.edge;
            return true;
        }
    }

    outEdge = nearest.edge;
    return true;
}

void SectorEditor::SelectSector(int sectorIndex)
{
    state.selectedSectorIndex = sectorIndex;
    state.selectedEdgeIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
}

void SectorEditor::SelectEdge(int sectorIndex, int edgeIndex)
{
    state.selectedSectorIndex = sectorIndex;
    state.selectedEdgeIndex = edgeIndex;
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
}

void SectorEditor::ClearSelection()
{
    state.selectedSectorIndex = -1;
    state.selectedEdgeIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
}

SectorEdgeOverride* SectorEditor::FindMutableEdgeOverride(int sectorIndex, int edgeIndex)
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(state.map.sectors.size())) {
        return nullptr;
    }
    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(sectorIndex)];
    for (SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (edgeOverride.edgeIndex == edgeIndex) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

const SectorEdgeOverride* SectorEditor::FindEdgeOverride(int sectorIndex, int edgeIndex) const
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(state.map.sectors.size())) {
        return nullptr;
    }
    const SectorDefinition& sector = state.map.sectors[static_cast<size_t>(sectorIndex)];
    for (const SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (edgeOverride.edgeIndex == edgeIndex) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

SectorEdgeOverride& SectorEditor::EnsureEdgeOverride(int sectorIndex, int edgeIndex)
{
    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(sectorIndex)];
    if (SectorEdgeOverride* existing = FindMutableEdgeOverride(sectorIndex, edgeIndex)) {
        return *existing;
    }

    SectorEdgeOverride edgeOverride;
    edgeOverride.edgeIndex = edgeIndex;
    sector.edgeOverrides.push_back(std::move(edgeOverride));
    return sector.edgeOverrides.back();
}

void SectorEditor::RemoveEdgeOverrideIfEmpty(int sectorIndex, int edgeIndex)
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(state.map.sectors.size())) {
        return;
    }
    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(sectorIndex)];
    sector.edgeOverrides.erase(
            std::remove_if(
                    sector.edgeOverrides.begin(),
                    sector.edgeOverrides.end(),
                    [edgeIndex](const SectorEdgeOverride& edgeOverride) {
                        return edgeOverride.edgeIndex == edgeIndex && IsEmptyEdgeOverride(edgeOverride);
                    }
            ),
            sector.edgeOverrides.end()
    );
}

bool SectorEditor::TargetAllowsSectorDefault(TexturePickerTargetKind target) const
{
    return target == TexturePickerTargetKind::EdgeWall
            || target == TexturePickerTargetKind::EdgeLowerWall
            || target == TexturePickerTargetKind::EdgeUpperWall;
}

std::string SectorEditor::CurrentTextureForTarget(TexturePickerTargetKind target, int sectorIndex, int edgeIndex) const
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(state.map.sectors.size())) {
        return std::string{};
    }

    const SectorDefinition& sector = state.map.sectors[static_cast<size_t>(sectorIndex)];
    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(sectorIndex, edgeIndex);
    switch (target) {
        case TexturePickerTargetKind::SectorFloor: return sector.floorTextureId;
        case TexturePickerTargetKind::SectorCeiling: return sector.ceilingTextureId;
        case TexturePickerTargetKind::SectorWall: return sector.wallTextureId;
        case TexturePickerTargetKind::SectorLowerWall: return sector.lowerWallTextureId;
        case TexturePickerTargetKind::SectorUpperWall: return sector.upperWallTextureId;
        case TexturePickerTargetKind::EdgeWall:
            return edgeOverride != nullptr && edgeOverride->hasWallTexture ? edgeOverride->wallTextureId : sector.wallTextureId;
        case TexturePickerTargetKind::EdgeLowerWall:
            return edgeOverride != nullptr && edgeOverride->hasLowerWallTexture ? edgeOverride->lowerWallTextureId : sector.lowerWallTextureId;
        case TexturePickerTargetKind::EdgeUpperWall:
            return edgeOverride != nullptr && edgeOverride->hasUpperWallTexture ? edgeOverride->upperWallTextureId : sector.upperWallTextureId;
        case TexturePickerTargetKind::None:
            break;
    }
    return std::string{};
}

void SectorEditor::OpenTexturePicker(TexturePickerTargetKind target, int sectorIndex, int edgeIndex)
{
    TexturePickerState& picker = state.texturePicker;
    picker.open = true;
    picker.target = target;
    picker.sectorIndex = sectorIndex;
    picker.edgeIndex = edgeIndex;
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    if (TargetAllowsSectorDefault(target)) {
        picker.textureIds.push_back(std::string{});
    }

    for (const auto& entry : state.map.texturePathsById) {
        picker.textureIds.push_back(entry.first);
    }
    std::sort(
            picker.textureIds.begin() + (TargetAllowsSectorDefault(target) ? 1 : 0),
            picker.textureIds.end()
    );

    const std::string currentTexture = CurrentTextureForTarget(target, sectorIndex, edgeIndex);
    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(sectorIndex, edgeIndex);
    const bool inheritedEdgeTexture =
            (target == TexturePickerTargetKind::EdgeWall && (edgeOverride == nullptr || !edgeOverride->hasWallTexture))
            || (target == TexturePickerTargetKind::EdgeLowerWall && (edgeOverride == nullptr || !edgeOverride->hasLowerWallTexture))
            || (target == TexturePickerTargetKind::EdgeUpperWall && (edgeOverride == nullptr || !edgeOverride->hasUpperWallTexture));

    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        if (picker.textureIds[i].empty()) {
            picker.optionLabels.push_back("<sector default>");
            if (inheritedEdgeTexture) {
                picker.selectedTextureIndex = static_cast<int>(i);
            }
            continue;
        }

        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (!inheritedEdgeTexture && picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

void SectorEditor::ApplyTexturePickerSelection()
{
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open
            || picker.sectorIndex < 0
            || picker.sectorIndex >= static_cast<int>(state.map.sectors.size())
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        picker = TexturePickerState{};
        return;
    }

    SectorDefinition& sector = state.map.sectors[static_cast<size_t>(picker.sectorIndex)];
    const std::string selectedTexture = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    bool changed = false;

    auto assignSectorTexture = [&](std::string& field) {
        if (!selectedTexture.empty() && field != selectedTexture) {
            field = selectedTexture;
            changed = true;
        }
    };

    switch (picker.target) {
        case TexturePickerTargetKind::SectorFloor:
            assignSectorTexture(sector.floorTextureId);
            break;
        case TexturePickerTargetKind::SectorCeiling:
            assignSectorTexture(sector.ceilingTextureId);
            break;
        case TexturePickerTargetKind::SectorWall:
            assignSectorTexture(sector.wallTextureId);
            break;
        case TexturePickerTargetKind::SectorLowerWall:
            assignSectorTexture(sector.lowerWallTextureId);
            break;
        case TexturePickerTargetKind::SectorUpperWall:
            assignSectorTexture(sector.upperWallTextureId);
            break;
        case TexturePickerTargetKind::EdgeWall:
        case TexturePickerTargetKind::EdgeLowerWall:
        case TexturePickerTargetKind::EdgeUpperWall: {
            if (picker.edgeIndex < 0 || picker.edgeIndex >= static_cast<int>(sector.points.size())) {
                break;
            }

            SectorEdgeOverride& edgeOverride = EnsureEdgeOverride(picker.sectorIndex, picker.edgeIndex);
            bool* hasTexture = nullptr;
            std::string* textureId = nullptr;
            if (picker.target == TexturePickerTargetKind::EdgeWall) {
                hasTexture = &edgeOverride.hasWallTexture;
                textureId = &edgeOverride.wallTextureId;
            } else if (picker.target == TexturePickerTargetKind::EdgeLowerWall) {
                hasTexture = &edgeOverride.hasLowerWallTexture;
                textureId = &edgeOverride.lowerWallTextureId;
            } else {
                hasTexture = &edgeOverride.hasUpperWallTexture;
                textureId = &edgeOverride.upperWallTextureId;
            }

            if (selectedTexture.empty()) {
                if (*hasTexture) {
                    *hasTexture = false;
                    textureId->clear();
                    changed = true;
                }
            } else if (!*hasTexture || *textureId != selectedTexture) {
                *hasTexture = true;
                *textureId = selectedTexture;
                changed = true;
            }

            RemoveEdgeOverrideIfEmpty(picker.sectorIndex, picker.edgeIndex);
            break;
        }
        case TexturePickerTargetKind::None:
            break;
    }

    if (changed) {
        state.dirty = true;
        statusText = TextFormat("Changed %s", TextureTargetLabel(picker.target));
    }

    picker = TexturePickerState{};
}

} // namespace game
