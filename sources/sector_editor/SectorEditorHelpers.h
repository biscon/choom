#pragma once

#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorGeneratedGeometry.h"

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace game {

inline constexpr float EditorWidth = 1920.0f;
inline constexpr float EditorHeight = 1080.0f;
inline constexpr float LeftPanelWidth = 360.0f;
inline constexpr float RightPanelWidth = 384.0f;
inline constexpr float BottomPanelHeight = 78.0f;
inline constexpr float PanelGap = 10.0f;
inline constexpr float TopologyUvScaleMin = 0.001f;
inline constexpr float TopologyUvScaleMax = 64.0f;
inline constexpr float MinZoom = 8.0f;
inline constexpr float MaxZoom = 256.0f;
inline constexpr float PanPixelsPerSecond = 720.0f;
inline constexpr float GeometryEpsilon = 0.001f;
inline constexpr float ScreenVertexSnapPixels = 10.0f;
inline constexpr float ScreenEdgePickPixels = 10.0f;
inline constexpr float ScreenLightPickPixels = 12.0f;
inline constexpr float PreviewHighlightLift = 0.006f;
inline constexpr float GameplayFloorSnapEpsilon = 0.001f;

bool SameBoundaryCutPoint(const SectorTopologyBoundaryCutPoint& a, const SectorTopologyBoundaryCutPoint& b);
const SectorTextureDefinition* FindSectorTopologyTexture(const SectorTopologyMap& map, const std::string& id);
std::vector<std::string> SortedSectorTopologyTextureIds(const SectorTopologyMap& map);
Vector2 SectorTopologyVertexToMap(const SectorTopologyVertex& vertex);
SectorPoint SectorTopologyCoordPointToSectorPoint(SectorTopologyCoordPoint point);
bool SameTopologyPoint(SectorTopologyCoordPoint a, SectorTopologyCoordPoint b);
const char* PreviewControlModeName(SectorPreviewControlMode mode);
const char* VerticalTransitionName(SectorFpsVerticalTransition transition);
Vector2 SectorPointToVector2(SectorPoint point);
SectorPoint Vector2ToSectorPoint(Vector2 point);
bool Contains(Rectangle bounds, Vector2 point);
bool StartsWith(const std::string& value, const char* prefix);
std::string ResolveEditorAssetPath(const std::string& path);
bool HasPngExtension(const std::filesystem::path& path);
std::vector<std::string> ScanAssetImagePngs(std::string& message);
bool IsValidTextureIdCharacter(char ch);
bool IsValidTextureId(const std::string& id);
float ClampAmbientIntensity(float value);
float ClampLightIntensity(float value);
float ClampLightRadius(float value);
float ClampLightSourceRadius(float value, float lightRadius);
float ClampAmbientOcclusionRadius(float value);
float ClampAmbientOcclusionStrength(float value);
float ClampIndirectBounceRadius(float value);
float ClampIndirectBounceStrength(float value);
int ClampAmbientChannel(int value);
Color TopologySectorAmbientPreviewColor(const SectorTopologySector& sector, unsigned char alpha);
const char* LightmapBakePhaseText(SectorLightmapBakePhase phase);
float LightmapBakePhaseProgressBase(SectorLightmapBakePhase phase);
float LightmapBakePhaseProgressWeight(SectorLightmapBakePhase phase);
float LightmapBakeOverallProgress(SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork);
std::string MakeTemporaryLightmapPath(const std::string& finalOutputPath);
void DeleteFileIfExists(const std::string& path);
std::string GeneratedTextureIdBase(const std::string& assetPath);
const char* ToolName(SectorEditorTool tool);
bool IsGraphAuthoringTool(SectorEditorTool tool);
bool IsLegacyTopologyMutationTool(SectorEditorTool tool);
bool IsToolAvailableInGraphAuthoritativeMode(SectorEditorTool tool);
bool IsSectorEditorGraphAuthoritativeMode();
const char* LegacyTopologyMutationUnavailableMessage();
const char* SectorEditorPickKindName(SectorEditorPickKind kind);
bool SameSectorEditorPickTarget(SectorEditorPickTarget a, SectorEditorPickTarget b);
bool IsSectorEditorPickTargetMovable(SectorEditorPickTarget target);
bool ShouldStartSectorEditorSelectDrag(Vector2 pressPosition, Vector2 currentPosition);
std::vector<SectorEditorPickCandidate> SortSectorEditorPickCandidates(
        std::vector<SectorEditorPickCandidate> candidates);
SectorEditorPickTarget ChooseSectorEditorPickTarget(
        std::vector<SectorEditorPickCandidate> candidates,
        SectorEditorPickTarget currentSelection,
        int* outCycleIndex = nullptr,
        int* outCycleCount = nullptr);
const char* TopologyWallPartName(TopologyWallPart part);
const char* TopologyWallPartStatusName(TopologyWallPart part);
const char* TopologyMaterialLayerName(TopologyMaterialLayer layer);
const char* TopologyMaterialLayerStatusName(TopologyMaterialLayer layer);
const char* TopologyWallPartSurfaceStatusName(TopologyWallPart part);
const char* SurfaceKindName(SectorSurfaceKind kind);
bool IsWallSurface(SectorSurfaceKind kind);
SectorSurfaceKind ToEditorSurfaceKind(SectorGeneratedSurfaceKind kind);
TopologyWallPart SurfaceKindToTopologyWallPart(SectorSurfaceKind kind);
TopologySurfaceEditTargetKind SurfaceKindToTopologyEditTargetKind(SectorSurfaceKind kind);
TopologyWallPart TopologyEditTargetWallPart(TopologySurfaceEditTargetKind kind);
TopologySurfaceEditTargetKind TopologyWallPartEditTargetKind(TopologyWallPart part);
const char* TopologyMaterialKindName(TopologySurfaceEditTargetKind kind);
bool IsWallTopologyEditTarget(TopologySurfaceEditTargetKind kind);
const char* TopologyUvFitModeStatusName(TopologyUvFitMode mode);
const char* TopologyUAlignDirectionStatusName(TopologyUAlignDirection direction);
void ResetTopologyUv(SectorTopologyUvSettings& uv);
bool IsDefaultTopologyUv(SectorTopologyUvSettings uv);
bool IsWhiteTint(Vector3 tint);
bool IsValidDecalTint(Vector3 tint);
Vector3 ClampDecalTint(Vector3 tint);
float ClampDecalBloomIntensity(float value);
bool SameTint(Vector3 a, Vector3 b);
Color DecalTintPreviewColor(Vector3 tint);
bool IsDefaultDecalLayer(const SectorTopologyDecalLayer& decal);
bool IsDefaultWallPartSettings(const SectorTopologyWallPartSettings& part);
void ResetDecalLayer(SectorTopologyDecalLayer& decal);
TopologySectorTextureField TopologyEditTargetSectorTextureField(TopologySurfaceEditTargetKind kind);
SectorSurfaceRef ToEditorSurfaceRef(const SectorGeneratedSurfaceRef& ref);
Vector3 SectorPointToWorld(SectorPoint point, float height);
const char* ToolHelpText(SectorEditorTool tool);
const char* TopologySectorTextureFieldLabel(TopologySectorTextureField field);
const char* TopologyPickerTargetLabel(const TexturePickerState& picker);
Color WithAlpha(Color color, unsigned char alpha);
bool SameSkySettings(const SectorTopologySkySettings& left, const SectorTopologySkySettings& right);
bool SameDirectionalLightSettings(
        const SectorTopologyDirectionalLightSettings& left,
        const SectorTopologyDirectionalLightSettings& right);
bool SamePreviewSettings(const SectorPreviewSettings& left, const SectorPreviewSettings& right);
float Cross(Vector2 a, Vector2 b, Vector2 c);
float Cross(SectorPoint a, SectorPoint b, SectorPoint c);
float DistancePointToSegment(Vector2 point, Vector2 a, Vector2 b);
SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(SectorTopologySideDef& sideDef, TopologyWallPart part);
const SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        const SectorTopologySideDef& sideDef,
        TopologyWallPart part);
SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(SectorAuthoringLineSide& side, TopologyWallPart part);
const SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        const SectorAuthoringLineSide& side,
        TopologyWallPart part);
bool IsTopologyMiddleEligible(const SectorTopologyMap& map, const SectorTopologySideDef* sideDef);
TopologyWallPart ValidTopologyWallPartForSideDef(
        const SectorTopologyMap& map,
        const SectorTopologySideDef* sideDef,
        TopologyWallPart wallPart);
bool IsMiddleTopologyEditTarget(TopologySurfaceEditTargetKind kind);
TopologyMaterialLayer EffectiveTopologyMaterialLayer(
        TopologySurfaceEditTargetKind kind,
        TopologyMaterialLayer layer);
bool SectorTopologyWallLengthWorld(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& lineDef,
        float& outLengthWorld);
bool FindSectorLoopEdgeForSideDef(
        const SectorTopologyLoop& loop,
        int sideDefId,
        const SectorTopologyLoop*& outLoop,
        size_t& outEdgeIndex);
bool FindUniqueSectorLoopEdgeForSideDef(
        const SectorTopologyLoopSet& loops,
        int sideDefId,
        const SectorTopologyLoop*& outLoop,
        size_t& outEdgeIndex);
bool TopologyWallPartHasVisibleSpan(
        const SectorTopologyMap& map,
        const SectorTopologySideDef& sideDef,
        TopologyWallPart wallPart);
const SectorTopologyLoopEdge* FindVisibleTopologyWallPartNeighborEdge(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        size_t selectedEdgeIndex,
        int sectorId,
        TopologyUAlignDirection direction,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer,
        std::string& outError);
float PolygonArea2(const std::vector<SectorPoint>& points);
bool SamePoint(SectorPoint a, SectorPoint b);
bool PointOnSegment(SectorPoint point, SectorPoint a, SectorPoint b);
int Orientation(SectorPoint a, SectorPoint b, SectorPoint c);
bool SegmentsIntersect(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1);
bool ProperSegmentsCross(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1);
bool CollinearSegmentsOverlap(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1);
bool SameUndirectedSegment(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1);
bool EdgesAreAdjacent(size_t edgeA, size_t edgeB, size_t edgeCount);
bool PointOnPolygonBoundary(SectorPoint point, const std::vector<SectorPoint>& polygon);
bool StrictPointInPolygon(SectorPoint point, const std::vector<SectorPoint>& polygon);
bool FileExists(const std::string& path);
std::string ShortPath(const std::string& path);

} // namespace game
