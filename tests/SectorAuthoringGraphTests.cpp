#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorUnits.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentActions.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTopologyRenderCache.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "util/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;
using Json = nlohmann::ordered_json;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

std::filesystem::path TempJsonPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    Check(static_cast<bool>(file), TextFormat("wrote temp file %s", path.string().c_str()));
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream contents;
    contents << file.rdbuf();
    Check(static_cast<bool>(file) || file.eof(), TextFormat("read temp file %s", path.string().c_str()));
    return contents.str();
}

std::filesystem::path TempDirectoryPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void RecreateTempDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    ec.clear();
    std::filesystem::create_directories(path, ec);
    Check(!ec, TextFormat("created temp directory %s", path.string().c_str()));
}

bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

bool Near(Vector3 a, Vector3 b)
{
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
}

game::SectorPlacedRuntimeObject MakeBillboardRuntimeObject(
        int id,
        const char* spritePath,
        Vector3 position,
        float yawRadians)
{
    game::SectorPlacedRuntimeObject object;
    object.id = id;
    object.kind = "billboard";
    object.position = position;
    object.yawRadians = yawRadians;
    object.billboard.spriteAnimationPath = spritePath;
    return object;
}

bool HasIssueFor(
        const std::vector<game::SectorAuthoringValidationIssue>& issues,
        game::SectorAuthoringObjectKind objectKind,
        int objectId)
{
    for (const game::SectorAuthoringValidationIssue& issue : issues) {
        if (issue.objectKind == objectKind && issue.objectId == objectId) {
            return true;
        }
    }
    return false;
}

bool HasIssueContaining(
        const std::vector<game::SectorAuthoringValidationIssue>& issues,
        game::SectorAuthoringObjectKind objectKind,
        int objectId,
        const std::string& messageText)
{
    for (const game::SectorAuthoringValidationIssue& issue : issues) {
        if (issue.objectKind == objectKind && issue.objectId == objectId
                && issue.message.find(messageText) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool HasPlanarDiagnostic(
        const std::vector<game::SectorAuthoringPlanarDiagnostic>& diagnostics,
        game::SectorAuthoringPlanarDiagnosticKind kind)
{
    for (const game::SectorAuthoringPlanarDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

bool HasFaceDiagnostic(
        const std::vector<game::SectorAuthoringFaceDiagnostic>& diagnostics,
        game::SectorAuthoringFaceDiagnosticKind kind)
{
    for (const game::SectorAuthoringFaceDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

bool HasDerivationDiagnostic(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind) {
            return true;
        }
    }
    return false;
}

bool HasDerivationDiagnosticMessageContaining(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind,
        const std::string& messageText)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind && diagnostic.message.find(messageText) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool HasDerivationDiagnosticFor(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind,
        int objectId)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind && diagnostic.objectId == objectId) {
            return true;
        }
    }
    return false;
}

const game::SectorAuthoringDerivationDiagnostic* FindDerivationDiagnosticFor(
        const std::vector<game::SectorAuthoringDerivationDiagnostic>& diagnostics,
        game::SectorAuthoringDerivationDiagnosticKind kind,
        int objectId)
{
    for (const game::SectorAuthoringDerivationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.kind == kind && diagnostic.objectId == objectId) {
            return &diagnostic;
        }
    }
    return nullptr;
}

void AddAuthoringVertexWithId(game::SectorAuthoringGraph& graph, int id, game::SectorCoord x, game::SectorCoord y)
{
    graph.vertices.push_back(game::SectorAuthoringVertex{id, x, y});
}

void AddAuthoringLineWithId(game::SectorAuthoringGraph& graph, int id, int startVertexId, int endVertexId)
{
    game::SectorAuthoringLine line;
    line.id = id;
    line.startVertexId = startVertexId;
    line.endVertexId = endVertexId;
    graph.lines.push_back(line);
}

bool TextContains(const char* text, const char* expected)
{
    return text != nullptr && std::string{text}.find(expected) != std::string::npos;
}

Vector2 VisibleAuthoringPoint(game::SectorCoord x, game::SectorCoord y)
{
    return Vector2{
            game::SectorCoordToVisibleAuthoring(x),
            game::SectorCoordToVisibleAuthoring(y)};
}

void SelectTextureInPicker(game::TexturePickerState& picker, const std::string& textureId);

game::SectorAuthoringGraph MakeGraphFromLines(
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& endpoints)
{
    game::SectorAuthoringGraph graph;
    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        AddAuthoringVertexWithId(
                graph,
                static_cast<int>(index) + 1,
                endpoints[index].first,
                endpoints[index].second);
    }
    for (std::size_t index = 0; index + 1 < endpoints.size(); index += 2) {
        AddAuthoringLineWithId(
                graph,
                10 + static_cast<int>(index / 2),
                static_cast<int>(index) + 1,
                static_cast<int>(index) + 2);
    }
    return graph;
}

game::SectorAuthoringGraph MakeGraphFromConnectedLines(
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& vertices,
        const std::vector<std::pair<int, int>>& lines)
{
    game::SectorAuthoringGraph graph;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        AddAuthoringVertexWithId(
                graph,
                static_cast<int>(index) + 1,
                vertices[index].first,
                vertices[index].second);
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        AddAuthoringLineWithId(
                graph,
                10 + static_cast<int>(index),
                lines[index].first,
                lines[index].second);
    }
    return graph;
}

game::SectorAuthoringGraph MakeNestedRectangleGraph(int rectangleCount)
{
    game::SectorAuthoringGraph graph;
    std::vector<std::pair<game::SectorCoord, game::SectorCoord>> vertices;
    std::vector<std::pair<int, int>> lines;
    vertices.reserve(static_cast<std::size_t>(rectangleCount) * 4);
    lines.reserve(static_cast<std::size_t>(rectangleCount) * 4);

    for (int rectangleIndex = 0; rectangleIndex < rectangleCount; ++rectangleIndex) {
        const game::SectorCoord inset = static_cast<game::SectorCoord>(rectangleIndex * 32);
        const game::SectorCoord extent = static_cast<game::SectorCoord>(rectangleCount * 64 - inset);
        const int firstVertexId = static_cast<int>(vertices.size()) + 1;
        vertices.push_back({inset, inset});
        vertices.push_back({extent, inset});
        vertices.push_back({extent, extent});
        vertices.push_back({inset, extent});
        lines.push_back({firstVertexId, firstVertexId + 1});
        lines.push_back({firstVertexId + 1, firstVertexId + 2});
        lines.push_back({firstVertexId + 2, firstVertexId + 3});
        lines.push_back({firstVertexId + 3, firstVertexId});
    }

    return MakeGraphFromConnectedLines(vertices, lines);
}

void AddFaceAnchor(
        game::SectorAuthoringGraph& graph,
        int id,
        game::SectorCoord x,
        game::SectorCoord y,
        const std::string& name)
{
    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = id;
    anchor.x = x;
    anchor.y = y;
    anchor.name = name;
    graph.faceAnchors.push_back(anchor);
}

void InitializeEditorStateWithAuthoringGraph(
        game::SectorEditorState& state,
        game::SectorEditorDocumentState& documentState,
        game::SectorAuthoringGraph& authoringGraph,
        const game::SectorAuthoringGraph& graph,
        bool refreshDerivation = true)
{
    game::SelectionState selectionState;
    documentState.map.topologyMap = game::CreateEmptySectorTopologyDocument();
    authoringGraph = graph;
    if (refreshDerivation) {
        game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph,
                game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    }
}

game::LevelPaths MakeTempLevelPaths(const char* name)
{
    game::LevelPaths paths;
    paths.directoryPath = std::filesystem::temp_directory_path();
    paths.jsonFilePath = paths.directoryPath / name;
    paths.lightmapFilePath = paths.directoryPath / "unused.lightmap.png";
    paths.jsonAssetPath = paths.jsonFilePath.string();
    paths.lightmapAssetPath = paths.lightmapFilePath.string();
    return paths;
}

Json SaveEditorStateToJson(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        const char* fileName,
        std::string* outText = nullptr)
{
    const game::LevelPaths paths = MakeTempLevelPaths(fileName);
    std::error_code removeError;
    std::filesystem::remove(paths.jsonFilePath, removeError);

    std::string error;
    Check(game::SaveSectorEditorAuthoringDocument(
                  paths,
                  authoringGraph,
                  documentState.map.topologyMap,
                  documentState.derivation.authoringDerivation,
                  error),
          TextFormat("editor authoring save succeeds for %s", fileName));
    Check(error.empty(), "successful editor authoring save clears error");
    const std::string text = ReadTextFile(paths.jsonFilePath);
    if (outText != nullptr) {
        *outText = text;
    }
    std::filesystem::remove(paths.jsonFilePath, removeError);
    return Json::parse(text);
}

game::SectorEditorState MakeEditorStateFromLoadedDocument(
        const game::SectorEditorLoadedDocument& loaded,
        game::SectorEditorDocumentState& documentState,
        game::SectorAuthoringGraph& authoringGraph,
        bool* outDerivationCurrent = nullptr)
{
    game::SectorEditorState state;
    game::SelectionState selectionState;
    if (loaded.format == game::SectorEditorDocumentFormat::TopologyV2Import) {
        documentState.map.topologyMap = loaded.mapData;
        game::InitializeSectorEditorAuthoringStateFromTopology(
                authoringGraph,
                game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                documentState.map.topologyMap);
        if (outDerivationCurrent != nullptr) {
            *outDerivationCurrent =
                    game::IsSectorEditorAuthoringDerivationCurrent(
                            game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
        }
        return state;
    }

    documentState.map.topologyMap = loaded.mapData;
    const game::SectorLightmapMetadata loadedBakedLightmap = documentState.map.topologyMap.bakedLightmap;
    authoringGraph = loaded.authoringGraph;
    const bool refreshed = game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph,
            game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    if (refreshed) {
        documentState.map.topologyMap.bakedLightmap = loadedBakedLightmap;
        documentState.derivation.authoringDerivation.topology.bakedLightmap = loadedBakedLightmap;
        if (documentState.derivation.lastValidAuthoringDerivedTopology.has_value()) {
            documentState.derivation.lastValidAuthoringDerivedTopology->bakedLightmap = loadedBakedLightmap;
        }
    }
    if (outDerivationCurrent != nullptr) {
        *outDerivationCurrent = refreshed
                && game::IsSectorEditorAuthoringDerivationCurrent(
                        game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    }
    return state;
}

bool IsAuthoringDerivationCurrent(const game::SectorEditorDocumentState& documentState)
{
    return game::IsSectorEditorAuthoringDerivationCurrent(
            game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
}

bool FindEditorAuthoringSelectionAtMapPoint(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        Vector2 mapPoint,
        float vertexMaxDistance,
        float lineMaxDistance,
        game::SectorAuthoringSelectionTarget* outTarget = nullptr,
        game::SectorTopologyCoordPoint* outVertexPoint = nullptr,
        std::string* outStatus = nullptr)
{
    return game::FindSectorEditorAuthoringSelectionAtMapPoint(
            authoringGraph,
            documentState.derivation.authoringDerivation,
            IsAuthoringDerivationCurrent(documentState),
            mapPoint,
            vertexMaxDistance,
            lineMaxDistance,
            outTarget,
            outVertexPoint,
            outStatus);
}

game::SectorEditorInspectorTarget ResolveEditorInspectorTarget(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        const game::SelectionState& selectionState)
{
    return game::ResolveSectorEditorInspectorTarget(
            documentState.map.topologyMap,
            authoringGraph,
            documentState.derivation.authoringDerivation,
            IsAuthoringDerivationCurrent(documentState),
            selectionState);
}

bool ResolveEditorAuthoringSurfaceTarget(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        game::SectorSurfaceRef surface,
        game::SectorEditorAuthoringSurfaceTarget& outTarget,
        std::string* outStatus = nullptr)
{
    return game::ResolveSectorEditorAuthoringSurfaceTarget(
            documentState.map.topologyMap,
            authoringGraph,
            documentState.derivation.authoringDerivation,
            IsAuthoringDerivationCurrent(documentState),
            surface,
            outTarget,
            outStatus);
}

std::string BuildEditorSurface3DTargetLabel(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        game::SectorSurfaceRef surface,
        game::TopologySurfaceEditTarget target)
{
    return game::BuildSectorEditorSurface3DTargetLabel(
            documentState.map.topologyMap,
            authoringGraph,
            documentState.derivation.authoringDerivation,
            IsAuthoringDerivationCurrent(documentState),
            surface,
            target);
}

bool ClearSelectedEditorSurface3DIfAuthoringMappingUnavailable(
        const game::SectorEditorState& state,
        const game::SectorEditorDocumentState& documentState,
        const game::SectorAuthoringGraph& authoringGraph,
        game::SectorEditorPreviewSelectionState& previewSelectionState,
        std::string* outStatus = nullptr)
{
    return game::ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
            documentState.map.topologyMap,
            authoringGraph,
            documentState.derivation.authoringDerivation,
            IsAuthoringDerivationCurrent(documentState),
            previewSelectionState,
            outStatus);
}

bool PointEqualsInteger(
        const game::SectorAuthoringPlanarPoint& point,
        game::SectorCoord x,
        game::SectorCoord y)
{
    return game::SectorAuthoringPlanarRationalsEqual(
                   point.x,
                   game::SectorAuthoringPlanarRational{static_cast<int64_t>(x), 1})
            && game::SectorAuthoringPlanarRationalsEqual(
                   point.y,
                   game::SectorAuthoringPlanarRational{static_cast<int64_t>(y), 1});
}

const game::SectorAuthoringPlanarVertex* FindPlanarVertexAt(
        const game::SectorAuthoringPlanarizationResult& result,
        game::SectorCoord x,
        game::SectorCoord y)
{
    for (const game::SectorAuthoringPlanarVertex& vertex : result.vertices) {
        if (PointEqualsInteger(vertex.point, x, y)) {
            return &vertex;
        }
    }
    return nullptr;
}

bool AllDerivedSectorsHaveExactlyOneValidFaceAnchorMapping(
        const game::SectorAuthoringGraph& graph,
        const game::SectorAuthoringDerivationResult& result)
{
    for (const game::SectorTopologySector& sector : result.topology.sectors) {
        int count = 0;
        for (const game::SectorAuthoringDerivedSectorMapping& mapping : result.mapping.sectors) {
            if (mapping.topologySectorId == sector.id
                    && game::FindSectorAuthoringFaceAnchor(graph, mapping.faceAnchorId) != nullptr) {
                ++count;
            }
        }
        if (count != 1) {
            return false;
        }
    }
    return true;
}

int CountPlanarEdgesForLine(const game::SectorAuthoringPlanarizationResult& result, int lineId)
{
    int count = 0;
    for (const game::SectorAuthoringPlanarEdge& edge : result.edges) {
        if (edge.sourceLineId == lineId) {
            ++count;
        }
    }
    return count;
}

int CountDerivedLineMappingsForAuthoringLine(
        const game::SectorAuthoringDerivationMapping& mapping,
        int lineId)
{
    int count = 0;
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : mapping.lines) {
        if (lineMapping.authoringLineId == lineId) {
            ++count;
        }
    }
    return count;
}

const game::SectorAuthoringDerivedSectorMapping* FindSectorMappingForAnchor(
        const game::SectorAuthoringDerivationMapping& mapping,
        int anchorId)
{
    for (const game::SectorAuthoringDerivedSectorMapping& sectorMapping : mapping.sectors) {
        if (sectorMapping.faceAnchorId == anchorId) {
            return &sectorMapping;
        }
    }
    return nullptr;
}

const game::SectorTopologyLineDef* FindDerivedLineDefForAuthoringLine(
        const game::SectorAuthoringDerivationResult& result,
        int authoringLineId)
{
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId == authoringLineId) {
            return game::FindSectorTopologyLineDef(result.topology, lineMapping.topologyLineDefId);
        }
    }
    return nullptr;
}

const game::SectorTopologySideDef* FindDerivedSideDefForAuthoringSide(
        const game::SectorAuthoringDerivationResult& result,
        int authoringLineId,
        game::SectorTopologySideKind side)
{
    for (const game::SectorAuthoringDerivedSideMapping& sideMapping : result.mapping.sides) {
        if (sideMapping.authoringLineId == authoringLineId && sideMapping.authoringSide == side) {
            return game::FindSectorTopologySideDef(result.topology, sideMapping.topologySideDefId);
        }
    }
    return nullptr;
}

game::SectorAuthoringFaceExtractionResult ExtractFacesFromGraph(const game::SectorAuthoringGraph& graph)
{
    const game::SectorAuthoringPlanarizationResult planar = game::PlanarizeSectorAuthoringGraph(graph);
    Check(planar.diagnostics.empty(), "face extraction test graph planarizes without diagnostics");
    return game::ExtractSectorAuthoringFaces(planar);
}

game::SectorTopologyWallPartSettings WallPart(
        const std::string& textureId,
        float scaleX,
        float scaleY,
        float offsetX,
        float offsetY)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    part.uv.scale = Vector2{scaleX, scaleY};
    part.uv.offset = Vector2{offsetX, offsetY};
    return part;
}

game::TopologySurfaceEditTarget SideDefMaterialTarget(const game::SectorTopologySideDef& sideDef)
{
    return game::TopologySurfaceEditTarget{
            game::TopologySurfaceEditTargetKind::SideDefWall,
            sideDef.sectorId,
            sideDef.lineDefId,
            sideDef.id,
            sideDef.side};
}

game::MaterialEditingUiState& TestMaterialEditingUiState();
game::SelectionState& TestSelectionState();
game::SectorEditorPreviewSelectionState& TestPreviewSelectionState();

game::SectorEditorMaterialEditingService MakeMaterialEditingService(
        game::SectorEditorState& state,
        game::SectorEditorDocumentState& documentState,
        game::SectorAuthoringGraph& authoringGraph,
        game::SectorEditorUiState& uiState,
        std::string& statusText,
        bool* previewRebuildRequested = nullptr)
{
    static game::MaterialEditingState materialState;
    game::SelectionState& selectionState = TestSelectionState();
    game::SectorEditorPreviewSelectionState& previewSelectionState = TestPreviewSelectionState();
    game::MaterialEditingUiState& materialUiState = TestMaterialEditingUiState();
    (void)uiState;
    selectionState = game::SelectionState{};
    materialState = game::MaterialEditingState{};
    materialUiState = game::MaterialEditingUiState{};
    return game::SectorEditorMaterialEditingService{
            game::SectorEditorMaterialEditingServiceContext{
                    game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle),
                    documentState.map.topologyMap,
                    authoringGraph,
                    game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                    state.topologyRenderWarning,
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    previewSelectionState,
                    selectionState,
                    materialState,
                    materialUiState,
                    state.texturePicker,
                    state.decalTintModal,
                    statusText,
                    [previewRebuildRequested](engine::AssetManager*) {
                        if (previewRebuildRequested != nullptr) {
                            *previewRebuildRequested = true;
                        }
                        return true;
                    }}};
}

game::MaterialEditingUiState& TestMaterialEditingUiState()
{
    static game::MaterialEditingUiState materialUiState;
    return materialUiState;
}

game::SectorEditorPreviewSelectionState& TestPreviewSelectionState()
{
    static game::SectorEditorPreviewSelectionState previewSelectionState;
    return previewSelectionState;
}

game::SelectionState& TestSelectionState()
{
    static game::SelectionState selectionState;
    return selectionState;
}

game::ManipulationState& TestManipulationState()
{
    static game::ManipulationState manipulationState;
    return manipulationState;
}

game::SectorEditorLightEditingService MakeLightEditingService(
        game::SectorEditorState& state,
        game::SectorEditorDocumentState& documentState,
        game::SectorTopologyMap& topologyMap,
        game::SectorEditorUiState& uiState,
        std::string& statusText)
{
    static game::LightEditingState lightState;
    static game::InspectorIdUiState inspectorIdUiState;
    game::SelectionState& selectionState = TestSelectionState();
    game::ManipulationState& manipulationState = TestManipulationState();
    lightState = game::LightEditingState{};
    inspectorIdUiState = game::InspectorIdUiState{};
    selectionState = game::SelectionState{};
    manipulationState = game::ManipulationState{};
    return game::SectorEditorLightEditingService{
            game::SectorEditorLightEditingServiceContext{
                    topologyMap,
                    lightState,
                    game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle),
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    {
                            manipulationState,
                            state.runtimeObjectDrag,
                            selectionState.topologySelectionKind,
                            selectionState.selectedTopologySectorId,
                            selectionState.selectedTopologyVertexId,
                            selectionState.selectedTopologySideDefId,
                            selectionState.selectedTopologyLineDefId,
                            selectionState.selectedTopologyLightId,
                            selectionState.selectedTopologyStaticSpotLightId,
                            selectionState.selectedTopologyDynamicLightId,
                            selectionState.selectedTopologyDynamicSpotLightId,
                            selectionState.selectedRuntimeObjectId,
                            selectionState.selectedTopologySideKind,
                            selectionState.inspectedTopologyVertexId,
                            TestPreviewSelectionState().selectedSurface3D,
                            TestPreviewSelectionState().selectedTopologySurface3D,
                            selectionState.selectedAuthoring,
                            selectionState.hoveredTopologyLightId,
                            selectionState.hoveredTopologyStaticSpotLightId,
                            selectionState.hoveredTopologyDynamicLightId,
                            selectionState.hoveredTopologyDynamicSpotLightId,
                    },
                    {
                            uiState.inspectorScroll,
                            uiState.lightXInput,
                            uiState.lightYInput,
                            uiState.lightZInput,
                            uiState.lightTargetXInput,
                            uiState.lightTargetYInput,
                            uiState.lightTargetZInput,
                            uiState.lightIntensityInput,
                            uiState.lightRadiusInput,
                            uiState.lightInnerConeInput,
                            uiState.lightOuterConeInput,
                            uiState.lightSourceRadiusInput,
                            uiState.lightFlickerSpeedInput,
                            uiState.lightFlickerAmountInput,
                            uiState.lightShadowPriorityInput,
                            uiState.lightShadowBiasInput,
                            uiState.lightShadowStrengthInput,
                            uiState.lightShadowSoftnessInput,
                            uiState.lightRedInput,
                            uiState.lightGreenInput,
                            uiState.lightBlueInput,
                            inspectorIdUiState,
                    },
                    statusText}};
}

game::SectorTopologySideDef SideDef(
        int id,
        int lineDefId,
        game::SectorTopologySideKind side,
        int sectorId,
        const std::string& prefix)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = id;
    sideDef.lineDefId = lineDefId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = WallPart(prefix + "_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    sideDef.lower = WallPart(prefix + "_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    sideDef.upper = WallPart(prefix + "_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    sideDef.middle = WallPart(prefix + "_middle", 4.0f, 5.0f, 6.0f, 7.0f);
    sideDef.middle.decal.textureId = prefix + "_middle_decal";
    sideDef.middle.decal.opacity = 0.5f;
    return sideDef;
}

game::SectorTopologySector Sector(int id, const std::string& name)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.name = name;
    sector.floorZ = -2.0f;
    sector.ceilingZ = 40.0f;
    sector.floorTextureId = name + "_floor";
    sector.ceilingTextureId = name + "_ceiling";
    sector.ceilingSky = true;
    sector.floorUv.scale = Vector2{2.0f, 3.0f};
    sector.floorUv.offset = Vector2{4.0f, 5.0f};
    sector.ceilingUv.scale = Vector2{6.0f, 7.0f};
    sector.ceilingUv.offset = Vector2{8.0f, 9.0f};
    sector.floorDecal.textureId = name + "_floor_decal";
    sector.ceilingDecal.textureId = name + "_ceiling_decal";
    sector.ambientColor = Color{8, 16, 32, 255};
    sector.ambientIntensity = 0.35f;
    sector.defaultWall = WallPart(name + "_default_wall", 1.0f, 1.5f, 2.0f, 2.5f);
    sector.defaultLower = WallPart(name + "_default_lower", 2.0f, 2.5f, 3.0f, 3.5f);
    sector.defaultUpper = WallPart(name + "_default_upper", 3.0f, 3.5f, 4.0f, 4.5f);
    return sector;
}

game::SectorTopologyLineDef LineDef(
        int id,
        int startVertexId,
        int endVertexId,
        int frontSideDefId,
        int backSideDefId,
        bool blocksPlayer = false)
{
    game::SectorTopologyLineDef lineDef;
    lineDef.id = id;
    lineDef.startVertexId = startVertexId;
    lineDef.endVertexId = endVertexId;
    lineDef.frontSideDefId = frontSideDefId;
    lineDef.backSideDefId = backSideDefId;
    lineDef.flags.blocksPlayer = blocksPlayer;
    return lineDef;
}

game::SectorTopologyMap MakeSingleSectorSquareMap()
{
    game::SectorTopologyMap map;
    map.vertices.push_back(game::SectorTopologyVertex{1, 0, 0});
    map.vertices.push_back(game::SectorTopologyVertex{2, 64, 0});
    map.vertices.push_back(game::SectorTopologyVertex{3, 64, 64});
    map.vertices.push_back(game::SectorTopologyVertex{4, 0, 64});

    map.lineDefs.push_back(LineDef(10, 1, 2, 100, -1, true));
    map.lineDefs.push_back(LineDef(11, 2, 3, 101, -1));
    map.lineDefs.push_back(LineDef(12, 3, 4, 102, -1));
    map.lineDefs.push_back(LineDef(13, 4, 1, 103, -1));

    map.sideDefs.push_back(SideDef(100, 10, game::SectorTopologySideKind::Front, 200, "south"));
    map.sideDefs.push_back(SideDef(101, 11, game::SectorTopologySideKind::Front, 200, "east"));
    map.sideDefs.push_back(SideDef(102, 12, game::SectorTopologySideKind::Front, 200, "north"));
    map.sideDefs.push_back(SideDef(103, 13, game::SectorTopologySideKind::Front, 200, "west"));
    map.sectors.push_back(Sector(200, "room"));
    return map;
}

game::SectorTopologyMap MakeAdjacentSectorMap()
{
    game::SectorTopologyMap map;
    map.vertices.push_back(game::SectorTopologyVertex{1, 0, 0});
    map.vertices.push_back(game::SectorTopologyVertex{2, 64, 0});
    map.vertices.push_back(game::SectorTopologyVertex{3, 128, 0});
    map.vertices.push_back(game::SectorTopologyVertex{4, 128, 64});
    map.vertices.push_back(game::SectorTopologyVertex{5, 64, 64});
    map.vertices.push_back(game::SectorTopologyVertex{6, 0, 64});

    map.lineDefs.push_back(LineDef(10, 1, 2, 100, -1));
    map.lineDefs.push_back(LineDef(11, 2, 5, 101, 104, true));
    map.lineDefs.push_back(LineDef(12, 5, 6, 102, -1));
    map.lineDefs.push_back(LineDef(13, 6, 1, 103, -1));
    map.lineDefs.push_back(LineDef(14, 2, 3, 105, -1));
    map.lineDefs.push_back(LineDef(15, 3, 4, 106, -1));
    map.lineDefs.push_back(LineDef(16, 4, 5, 107, -1));

    map.sideDefs.push_back(SideDef(100, 10, game::SectorTopologySideKind::Front, 200, "left_south"));
    map.sideDefs.push_back(SideDef(101, 11, game::SectorTopologySideKind::Front, 200, "portal_left"));
    map.sideDefs.push_back(SideDef(102, 12, game::SectorTopologySideKind::Front, 200, "left_north"));
    map.sideDefs.push_back(SideDef(103, 13, game::SectorTopologySideKind::Front, 200, "left_west"));
    map.sideDefs.push_back(SideDef(104, 11, game::SectorTopologySideKind::Back, 201, "portal_right"));
    map.sideDefs.push_back(SideDef(105, 14, game::SectorTopologySideKind::Front, 201, "right_south"));
    map.sideDefs.push_back(SideDef(106, 15, game::SectorTopologySideKind::Front, 201, "right_east"));
    map.sideDefs.push_back(SideDef(107, 16, game::SectorTopologySideKind::Front, 201, "right_north"));
    map.sectors.push_back(Sector(200, "left"));
    map.sectors.push_back(Sector(201, "right"));
    return map;
}

bool SameUv(game::SectorTopologyUvSettings a, game::SectorTopologyUvSettings b)
{
    return a.scale.x == b.scale.x
           && a.scale.y == b.scale.y
           && a.offset.x == b.offset.x
           && a.offset.y == b.offset.y;
}

void CheckWallPartCopied(
        const game::SectorTopologyWallPartSettings& imported,
        const game::SectorTopologyWallPartSettings& original,
        const char* description)
{
    Check(imported.textureId == original.textureId, description);
    Check(SameUv(imported.uv, original.uv), description);
    Check(imported.decal.textureId == original.decal.textureId, description);
    Check(imported.decal.opacity == original.decal.opacity, description);
}

void CheckSectorPropertiesCopied(
        const game::SectorAuthoringFaceAnchor& anchor,
        const game::SectorTopologySector& sector,
        const char* description)
{
    Check(anchor.id == sector.id, description);
    Check(anchor.name == sector.name, description);
    Check(anchor.floorZ == sector.floorZ, description);
    Check(anchor.ceilingZ == sector.ceilingZ, description);
    Check(anchor.floorTextureId == sector.floorTextureId, description);
    Check(anchor.ceilingTextureId == sector.ceilingTextureId, description);
    Check(anchor.ceilingSky == sector.ceilingSky, description);
    Check(SameUv(anchor.floorUv, sector.floorUv), description);
    Check(SameUv(anchor.ceilingUv, sector.ceilingUv), description);
    Check(anchor.floorDecal.textureId == sector.floorDecal.textureId, description);
    Check(anchor.ceilingDecal.textureId == sector.ceilingDecal.textureId, description);
    Check(anchor.ambientColor.r == sector.ambientColor.r
                  && anchor.ambientColor.g == sector.ambientColor.g
                  && anchor.ambientColor.b == sector.ambientColor.b
                  && anchor.ambientColor.a == sector.ambientColor.a,
          description);
    Check(anchor.ambientIntensity == sector.ambientIntensity, description);
    CheckWallPartCopied(anchor.defaultWall, sector.defaultWall, description);
    CheckWallPartCopied(anchor.defaultLower, sector.defaultLower, description);
    CheckWallPartCopied(anchor.defaultUpper, sector.defaultUpper, description);
}

void TestEmptyGraph()
{
    game::SectorAuthoringGraph graph;

    Check(graph.vertices.empty(), "empty graph has no vertices");
    Check(graph.lines.empty(), "empty graph has no lines");
    Check(graph.lineSides.empty(), "empty graph has no line sides");
    Check(graph.faceAnchors.empty(), "empty graph has no face anchors");
    Check(game::AllocateSectorAuthoringVertexId(graph) == 1, "empty graph next vertex ID is positive");
    Check(game::AllocateSectorAuthoringLineId(graph) == 1, "empty graph next line ID is positive");
    Check(game::AllocateSectorAuthoringFaceAnchorId(graph) == 1, "empty graph next face anchor ID is positive");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "empty graph validates");
}

void TestStablePositiveIdAllocation()
{
    game::SectorAuthoringGraph graph;
    graph.vertices.push_back(game::SectorAuthoringVertex{7, 0, 0});
    graph.vertices.push_back(game::SectorAuthoringVertex{3, 16, 0});
    graph.lines.push_back(game::SectorAuthoringLine{11, 7, 3});
    graph.faceAnchors.push_back(game::SectorAuthoringFaceAnchor{5});

    Check(game::AllocateSectorAuthoringVertexId(graph) == 8, "vertex ID allocation uses max existing ID");
    Check(game::AllocateSectorAuthoringLineId(graph) == 12, "line ID allocation uses max existing ID");
    Check(game::AllocateSectorAuthoringFaceAnchorId(graph) == 6, "face anchor ID allocation uses max existing ID");
}

void TestAddVerticesAndLines()
{
    game::SectorAuthoringGraph graph;

    int firstVertexId = -1;
    int secondVertexId = -1;
    int lineId = -1;

    Check(game::AddSectorAuthoringVertex(graph, 0, 0, &firstVertexId), "add first authoring vertex");
    Check(game::AddSectorAuthoringVertex(graph, 32, 16, &secondVertexId), "add second authoring vertex");
    Check(firstVertexId == 1, "first vertex ID is stable positive");
    Check(secondVertexId == 2, "second vertex ID is stable positive");
    Check(game::AddSectorAuthoringLine(graph, firstVertexId, secondVertexId, &lineId), "add authoring line");
    Check(lineId == 1, "first line ID is stable positive");

    const game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, lineId);
    Check(line != nullptr, "find added line");
    if (line != nullptr) {
        Check(line->startVertexId == firstVertexId, "line stores start vertex");
        Check(line->endVertexId == secondVertexId, "line stores end vertex");
        Check(!line->flags.blocksPlayer, "line flags default to current topology default");
    }
}

void TestInvalidEndpointDiagnosticsAndRejection()
{
    game::SectorAuthoringGraph graph;
    int firstVertexId = -1;
    Check(game::AddSectorAuthoringVertex(graph, 0, 0, &firstVertexId), "add vertex for invalid endpoint test");

    Check(!game::AddSectorAuthoringLine(graph, firstVertexId, 999, nullptr), "reject missing end vertex");
    Check(!game::AddSectorAuthoringLine(graph, firstVertexId, firstVertexId, nullptr), "reject identical endpoints");

    game::SectorAuthoringLine invalidLine;
    invalidLine.id = 42;
    invalidLine.startVertexId = firstVertexId;
    invalidLine.endVertexId = 999;
    graph.lines.push_back(invalidLine);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);
    Check(game::HasSectorAuthoringValidationErrors(issues), "invalid references produce validation errors");
    Check(HasIssueFor(issues, game::SectorAuthoringObjectKind::Line, 42), "invalid line issue includes line ID");
}

void TestValidateAuthoringGraphRejectsDuplicateVertexIds()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 1, 16, 0);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate vertex IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Vertex,
                  1,
                  "Duplicate"),
          "duplicate vertex ID issue includes vertex ID");
}

void TestValidateAuthoringGraphRejectsDuplicateLineIds()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 16, 0);
    AddAuthoringVertexWithId(graph, 3, 16, 16);
    AddAuthoringLineWithId(graph, 7, 1, 2);
    AddAuthoringLineWithId(graph, 7, 2, 3);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate line IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Line,
                  7,
                  "Duplicate"),
          "duplicate line ID issue includes line ID");
}

void TestValidateAuthoringGraphRejectsDuplicateFaceAnchorIds()
{
    game::SectorAuthoringGraph graph;
    game::SectorAuthoringFaceAnchor firstAnchor;
    firstAnchor.id = 3;
    game::SectorAuthoringFaceAnchor secondAnchor;
    secondAnchor.id = 3;
    graph.faceAnchors.push_back(firstAnchor);
    graph.faceAnchors.push_back(secondAnchor);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate face anchor IDs produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::FaceAnchor,
                  3,
                  "Duplicate"),
          "duplicate face anchor ID issue includes anchor ID");
}

void TestValidateAuthoringGraphRejectsDuplicateSideIdentity()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 16, 0);
    AddAuthoringLineWithId(graph, 7, 1, 2);

    game::SectorAuthoringLineSide firstSide;
    firstSide.id = game::SectorAuthoringSideId{7, game::SectorTopologySideKind::Front};
    game::SectorAuthoringLineSide secondSide;
    secondSide.id = game::SectorAuthoringSideId{7, game::SectorTopologySideKind::Front};
    graph.lineSides.push_back(firstSide);
    graph.lineSides.push_back(secondSide);

    const std::vector<game::SectorAuthoringValidationIssue> issues =
            game::ValidateSectorAuthoringGraphReferences(graph);

    Check(game::HasSectorAuthoringValidationErrors(issues), "duplicate side identities produce validation errors");
    Check(HasIssueContaining(
                  issues,
                  game::SectorAuthoringObjectKind::Side,
                  7,
                  "Duplicate"),
          "duplicate side identity issue includes line ID");
}

void TestSideIdentityHelpers()
{
    const game::SectorAuthoringSideId front{17, game::SectorTopologySideKind::Front};
    const game::SectorAuthoringSideId sameFront{17, game::SectorTopologySideKind::Front};
    const game::SectorAuthoringSideId back{17, game::SectorTopologySideKind::Back};

    Check(game::SectorAuthoringSideIdsEqual(front, sameFront), "same line and side compare equal");
    Check(!game::SectorAuthoringSideIdsEqual(front, back), "different side does not compare equal");
    Check(game::SectorAuthoringSideIdsEqual(
                  game::OppositeSectorAuthoringSideId(front),
                  back),
          "opposite authoring side helper flips side only");

    game::SectorAuthoringGraph graph;
    graph.lines.push_back(game::SectorAuthoringLine{17, 1, 2});
    game::SectorAuthoringLineSide side;
    side.id = front;
    side.wall.textureId = "wall";
    graph.lineSides.push_back(side);

    const game::SectorAuthoringLineSide* found = game::FindSectorAuthoringLineSide(graph, front);
    Check(found != nullptr, "find authoring side by line plus side");
    if (found != nullptr) {
        Check(found->wall.textureId == "wall", "authoring side carries wall material metadata");
        Check(found->lower.textureId.empty(), "authoring side lower material defaults match topology defaults");
        Check(found->upper.textureId.empty(), "authoring side upper material defaults match topology defaults");
        Check(found->middle.textureId.empty(), "authoring side middle material defaults match topology defaults");
    }
}

void TestFaceAnchorDefaultsMatchTopologyDefaults()
{
    game::SectorAuthoringFaceAnchor anchor;
    game::SectorTopologySector sector;

    Check(anchor.floorZ == sector.floorZ, "face anchor floor height default matches topology sector");
    Check(anchor.ceilingZ == sector.ceilingZ, "face anchor ceiling height default matches topology sector");
    Check(anchor.floorTextureId == sector.floorTextureId, "face anchor floor texture default matches topology sector");
    Check(anchor.ceilingTextureId == sector.ceilingTextureId, "face anchor ceiling texture default matches topology sector");
    Check(anchor.ceilingSky == sector.ceilingSky, "face anchor sky default matches topology sector");
    Check(anchor.ambientColor.r == sector.ambientColor.r
                  && anchor.ambientColor.g == sector.ambientColor.g
                  && anchor.ambientColor.b == sector.ambientColor.b
                  && anchor.ambientColor.a == sector.ambientColor.a,
          "face anchor ambient color default matches topology sector");
    Check(anchor.ambientIntensity == sector.ambientIntensity, "face anchor ambient intensity default matches topology sector");
    Check(anchor.defaultWall.textureId == sector.defaultWall.textureId, "face anchor default wall material matches topology sector");
    Check(anchor.defaultLower.textureId == sector.defaultLower.textureId, "face anchor default lower material matches topology sector");
    Check(anchor.defaultUpper.textureId == sector.defaultUpper.textureId, "face anchor default upper material matches topology sector");
}

void TestImportEmptyTopologyMap()
{
    const game::SectorTopologyMap map;
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    Check(graph.vertices.empty(), "empty topology import has no authoring vertices");
    Check(graph.lines.empty(), "empty topology import has no authoring lines");
    Check(graph.lineSides.empty(), "empty topology import has no authoring sides");
    Check(graph.faceAnchors.empty(), "empty topology import has no face anchors");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "empty imported graph validates");
}

void TestImportSingleSectorSquare()
{
    const game::SectorTopologyMap map = MakeSingleSectorSquareMap();
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    Check(graph.vertices.size() == map.vertices.size(), "single sector import preserves vertex count");
    Check(graph.lines.size() == map.lineDefs.size(), "single sector import preserves line count");
    Check(graph.lineSides.size() == map.sideDefs.size(), "single sector import preserves side count");
    Check(graph.faceAnchors.size() == map.sectors.size(), "single sector import preserves sector anchor count");

    const game::SectorAuthoringVertex* vertex = game::FindSectorAuthoringVertex(graph, 3);
    Check(vertex != nullptr && vertex->x == 64 && vertex->y == 64, "import preserves vertex ID and coordinates");

    const game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, 10);
    Check(line != nullptr, "import preserves linedef ID as authoring line ID");
    if (line != nullptr) {
        Check(line->startVertexId == 1 && line->endVertexId == 2, "import preserves linedef orientation");
        Check(line->flags.blocksPlayer, "import preserves linedef blocksPlayer flag");
    }

    const game::SectorAuthoringLineSide* side = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* originalSide = game::FindSectorTopologySideDef(map, 100);
    Check(side != nullptr && originalSide != nullptr, "import preserves front side identity");
    if (side != nullptr && originalSide != nullptr) {
        CheckWallPartCopied(side->wall, originalSide->wall, "import copies wall material metadata");
        CheckWallPartCopied(side->lower, originalSide->lower, "import copies lower material metadata");
        CheckWallPartCopied(side->upper, originalSide->upper, "import copies upper material metadata");
        CheckWallPartCopied(side->middle, originalSide->middle, "import copies middle material metadata");
    }

    const game::SectorAuthoringFaceAnchor* anchor = game::FindSectorAuthoringFaceAnchor(graph, 200);
    const game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 200);
    Check(anchor != nullptr && sector != nullptr, "import preserves sector ID as face anchor ID");
    if (anchor != nullptr && sector != nullptr) {
        CheckSectorPropertiesCopied(*anchor, *sector, "import copies sector properties to face anchor");
        Check(anchor->x == 32 && anchor->y == 32, "import assigns deterministic face anchor center");
    }

    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "single sector imported graph validates");
}

void TestImportAdjacentSectors()
{
    const game::SectorTopologyMap map = MakeAdjacentSectorMap();
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(map);

    const game::SectorAuthoringLine* portalLine = game::FindSectorAuthoringLine(graph, 11);
    Check(portalLine != nullptr, "adjacent import preserves shared linedef");
    if (portalLine != nullptr) {
        Check(portalLine->startVertexId == 2 && portalLine->endVertexId == 5, "adjacent import preserves shared linedef orientation");
        Check(portalLine->flags.blocksPlayer, "adjacent import preserves shared linedef blocksPlayer");
    }

    const game::SectorAuthoringLineSide* front = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Front});
    const game::SectorAuthoringLineSide* back = game::FindSectorAuthoringLineSide(
            graph,
            game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Back});
    Check(front != nullptr, "adjacent import keeps front side metadata for shared linedef");
    Check(back != nullptr, "adjacent import keeps back side metadata for shared linedef");
    if (front != nullptr) {
        Check(front->wall.textureId == "portal_left_wall", "adjacent import copies front wall metadata");
    }
    if (back != nullptr) {
        Check(back->wall.textureId == "portal_right_wall", "adjacent import copies back wall metadata");
    }

    Check(game::FindSectorAuthoringFaceAnchor(graph, 200) != nullptr, "adjacent import keeps first sector anchor");
    Check(game::FindSectorAuthoringFaceAnchor(graph, 201) != nullptr, "adjacent import keeps second sector anchor");
    Check(game::ValidateSectorAuthoringGraphReferences(graph).empty(), "adjacent imported graph validates");
}

void TestPlanarizeCrossingLines()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 64},
            {0, 64}, {64, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "crossing lines planarize without diagnostics");
    Check(result.vertices.size() == 5, "crossing lines add one intersection vertex");
    Check(result.edges.size() == 4, "crossing lines split into four planar edges");
    Check(FindPlanarVertexAt(result, 32, 32) != nullptr, "crossing intersection point is deterministic");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "first crossing source line maps to two planar edges");
    Check(CountPlanarEdgesForLine(result, 11) == 2, "second crossing source line maps to two planar edges");
}

void TestPlanarizeTJunction()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "T-junction planarizes without diagnostics");
    Check(result.vertices.size() == 4, "T-junction reuses endpoint as split vertex");
    Check(result.edges.size() == 3, "T-junction splits only the containing segment");
    Check(FindPlanarVertexAt(result, 32, 0) != nullptr, "T-junction keeps endpoint-on-segment point");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "T-junction containing source line maps to two edges");
    Check(CountPlanarEdgesForLine(result, 11) == 1, "T-junction endpoint source line stays one edge");
}

void TestPlanarizeEndpointOnSegment()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, -32}, {32, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "endpoint-on-segment planarizes without diagnostics");
    Check(result.edges.size() == 3, "endpoint-on-segment splits containing segment");
    Check(CountPlanarEdgesForLine(result, 10) == 2, "endpoint-on-segment maps containing source line to two edges");
    Check(CountPlanarEdgesForLine(result, 11) == 1, "endpoint-on-segment source endpoint line stays one edge");
}

void TestPlanarizeNonCollinearCoincidentEndpointDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::CoincidentEndpoint),
          "non-collinear endpoint touch with distinct vertex IDs produces diagnostic");
}

void TestPlanarizeMultipleCrossingsOnOneLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {96, 0},
            {32, -32}, {32, 32},
            {64, -32}, {64, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "multiple crossings planarize without diagnostics");
    Check(result.vertices.size() == 8, "multiple crossings insert two vertices");
    Check(result.edges.size() == 7, "multiple crossings split all crossing segments");
    Check(CountPlanarEdgesForLine(result, 10) == 3, "source line with two crossings maps to ordered child segments");
    Check(FindPlanarVertexAt(result, 32, 0) != nullptr, "first crossing exists");
    Check(FindPlanarVertexAt(result, 64, 0) != nullptr, "second crossing exists");
}

void TestPlanarizeNonIntersectingLines()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {0, 16}, {32, 16}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(result.diagnostics.empty(), "non-intersecting lines planarize without diagnostics");
    Check(result.vertices.size() == 4, "non-intersecting lines keep only endpoints");
    Check(result.edges.size() == 2, "non-intersecting lines keep one edge per source line");
}

void TestPlanarizeDuplicateLineDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::DuplicateLine),
          "duplicate lines produce diagnostic");
    Check(result.edges.empty(), "duplicate lines are not silently emitted as planar edges");
}

void TestPlanarizeCollinearOverlapDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {96, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::CollinearOverlap),
          "collinear overlapping lines produce diagnostic");
    Check(result.edges.empty(), "collinear overlapping lines are not silently emitted as planar edges");
}

void TestPlanarizeZeroLengthLineDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {8, 8}, {8, 8}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::ZeroLengthLine),
          "zero-length line produces diagnostic");
    Check(result.edges.empty(), "zero-length line emits no planar edges");
}

void TestPlanarizeNearMissDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 1}, {32, 32}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    Check(HasPlanarDiagnostic(result.diagnostics, game::SectorAuthoringPlanarDiagnosticKind::NearMiss),
          "near-miss endpoint within one grid coordinate produces deterministic diagnostic");
    Check(result.edges.size() == 2, "near-miss does not auto-snap or split");
}

void TestPlanarizeMetadataMapping()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {64, 64},
            {0, 64}, {64, 0}});

    const game::SectorAuthoringPlanarizationResult result = game::PlanarizeSectorAuthoringGraph(graph);

    for (const game::SectorAuthoringPlanarEdge& edge : result.edges) {
        Check(edge.sourceLineId == 10 || edge.sourceLineId == 11, "planar edge preserves source authoring line ID");
        Check(edge.followsSourceLineDirection, "planar edge records source direction mapping");
    }
}

void TestExtractFacesSingleSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "single square produces one bounded face");
    Check(result.faces[0].boundary.size() == 4, "single square face has four boundary edges");
    Check(result.faces[0].signedArea > 0.0, "single square face has deterministic positive winding");
    Check(result.diagnostics.empty(), "single square produces no face diagnostics");
    for (const game::SectorAuthoringFaceBoundaryEdge& edge : result.faces[0].boundary) {
        Check(edge.sourceLineId >= 10 && edge.sourceLineId <= 13, "face boundary preserves source line mapping");
        Check(edge.sourceSide == game::SectorTopologySideKind::Front, "CCW square maps bounded face to front sides");
    }
}

void TestExtractFacesAdjacentSquares()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "two adjacent squares produce two bounded faces");
    Check(result.diagnostics.empty(), "two adjacent squares produce no face diagnostics");
    Check(result.faces[0].id == 1 && result.faces[1].id == 2, "adjacent square face IDs are deterministic");
}

void TestExtractFacesRectangleCutByLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 64}, {0, 64}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "rectangle cut by a boundary-to-boundary line produces two faces");
    Check(result.diagnostics.empty(), "rectangle cut by line produces no face diagnostics");
}

void TestExtractFacesCrossingDiagonals()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringPlanarizationResult planar = game::PlanarizeSectorAuthoringGraph(graph);
    const game::SectorAuthoringFaceExtractionResult result = game::ExtractSectorAuthoringFaces(planar);

    Check(planar.diagnostics.empty(), "crossing diagonals planarize without diagnostics");
    Check(result.faces.size() == 4, "crossing diagonals in a square produce four bounded faces");
    Check(result.diagnostics.empty(), "crossing diagonals produce no face diagnostics");
}

void TestExtractFacesOpenChainDiagnostics()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.empty(), "open chain produces no bounded faces");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "open chain produces dangling edge diagnostics");
}

void TestExtractFacesDanglingLineDoesNotCorruptSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {96, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {3, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "dangling line attached to square keeps the square face");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "dangling line attached to square produces dangling edge diagnostic");
}

void TestExtractFacesRejectsOuterFace()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 1, "outer face is not returned as a bounded face");
}

void TestExtractFacesSliverThreshold()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {1, 0}, {0, 1}},
            {{1, 2}, {2, 3}, {3, 1}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.empty(), "sub-grid sliver triangle is not returned as a face");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::TinySliverFace),
          "sub-grid sliver triangle produces a sliver diagnostic");
}

void TestExtractFacesNestedDisconnectedLoopsProducesContainedFaces()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}, {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "nested disconnected loops are returned as contained faces");
    Check(result.diagnostics.empty(), "supported nested disconnected loops produce no face diagnostics");
}

void TestExtractDisconnectedClosedLoopsProducesFaces()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {128, 0}, {192, 0}, {192, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "disconnected closed loops are returned as independent faces");
    Check(result.diagnostics.empty(), "disconnected closed loops produce no face diagnostics");
}

void TestExtractDisconnectedClosedLoopsWithDanglingLineReportsOnlyDanglingLine()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64},
             {128, 0}, {192, 0}, {192, 64}, {128, 64},
             {224, 0}, {256, 0}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}});

    const game::SectorAuthoringFaceExtractionResult result = ExtractFacesFromGraph(graph);

    Check(result.faces.size() == 2, "dangling line does not corrupt disconnected closed loop faces");
    Check(HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::DanglingEdge),
          "dangling line in disconnected graph reports dangling diagnostic");
    Check(!HasFaceDiagnostic(result.diagnostics, game::SectorAuthoringFaceDiagnosticKind::AmbiguousTopology),
          "dangling line in disconnected graph does not report ambiguous topology");
}

void CheckDerivedTopologyIsValid(
        const game::SectorAuthoringDerivationResult& result,
        const char* description)
{
    Check(result.success, description);
    Check(result.diagnostics.empty(), description);
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(result.topology)),
          description);
}

std::size_t CountSectorHoles(
        const game::SectorTopologyMap& topology,
        int sectorId,
        const char* description)
{
    game::SectorTopologyLoopSet loops;
    std::vector<game::SectorTopologyValidationIssue> issues;
    Check(game::ExtractSectorTopologyLoops(topology, sectorId, loops, &issues), description);
    return loops.holes.size();
}

int CountTwoSidedLineDefs(const game::SectorTopologyMap& topology)
{
    int count = 0;
    for (const game::SectorTopologyLineDef& lineDef : topology.lineDefs) {
        if (lineDef.frontSideDefId > 0 && lineDef.backSideDefId > 0) {
            ++count;
        }
    }
    return count;
}

int CountOneSidedLineDefs(const game::SectorTopologyMap& topology)
{
    int count = 0;
    for (const game::SectorTopologyLineDef& lineDef : topology.lineDefs) {
        const bool hasFront = lineDef.frontSideDefId > 0;
        const bool hasBack = lineDef.backSideDefId > 0;
        if (hasFront != hasBack) {
            ++count;
        }
    }
    return count;
}

const game::SectorTopologyLineDef* FindLineDefByEndpoints(
        const game::SectorTopologyMap& topology,
        game::SectorCoord ax,
        game::SectorCoord ay,
        game::SectorCoord bx,
        game::SectorCoord by)
{
    for (const game::SectorTopologyLineDef& lineDef : topology.lineDefs) {
        const game::SectorTopologyVertex* start =
                game::FindSectorTopologyVertex(topology, lineDef.startVertexId);
        const game::SectorTopologyVertex* end =
                game::FindSectorTopologyVertex(topology, lineDef.endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }
        const bool forward = start->x == ax && start->y == ay && end->x == bx && end->y == by;
        const bool reverse = start->x == bx && start->y == by && end->x == ax && end->y == ay;
        if (forward || reverse) {
            return &lineDef;
        }
    }
    return nullptr;
}

const game::SectorTopologySideDef* FindOnlySideDef(
        const game::SectorTopologyMap& topology,
        const game::SectorTopologyLineDef& lineDef)
{
    if (lineDef.frontSideDefId > 0 && lineDef.backSideDefId <= 0) {
        return game::FindSectorTopologySideDef(topology, lineDef.frontSideDefId);
    }
    if (lineDef.backSideDefId > 0 && lineDef.frontSideDefId <= 0) {
        return game::FindSectorTopologySideDef(topology, lineDef.backSideDefId);
    }
    return nullptr;
}

int CountWallSurfacesForLine(
        const game::SectorGeneratedGeometry& geometry,
        int lineDefId)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.topologyLineDefId != lineDefId) {
            continue;
        }
        if (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                || surface.ref.kind == game::SectorGeneratedSurfaceKind::LowerWall
                || surface.ref.kind == game::SectorGeneratedSurfaceKind::UpperWall) {
            ++count;
        }
    }
    return count;
}

bool CollisionHasBlockingEdgeForLine(
        const game::SectorTopologyMap& topology,
        int lineDefId)
{
    game::SectorCollisionWorld world;
    std::string error;
    if (!world.BuildFromTopology(topology, &error)) {
        return false;
    }
    for (const game::SectorTopologySector& sector : topology.sectors) {
        const std::vector<game::SectorCollisionEdge>* edges = world.GetSectorEdges(sector.id);
        if (edges == nullptr) {
            continue;
        }
        for (const game::SectorCollisionEdge& edge : *edges) {
            if (edge.lineDefId == lineDefId
                    && edge.kind == game::SectorCollisionEdgeKind::BlockingWall) {
                return true;
            }
        }
    }
    return false;
}

void TestDeriveSingleSquare()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "single square derives valid topology");
    Check(result.topology.vertices.size() == 4, "single square derives four vertices");
    Check(result.topology.lineDefs.size() == 4, "single square derives four linedefs");
    Check(result.topology.sideDefs.size() == 4, "single square derives four sidedefs");
    Check(result.topology.sectors.size() == 1, "single square derives one sector");
    Check(result.mapping.vertices.size() == 4, "single square records vertex mapping");
    Check(result.mapping.lines.size() == 4, "single square records line mapping");
    Check(result.mapping.sides.size() == 4, "single square records side mapping");
    Check(result.mapping.sectors.size() == 1, "single square records sector mapping");
}

void TestDeriveAdjacentSquares()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "adjacent squares derive valid topology");
    Check(result.topology.sectors.size() == 2, "adjacent squares derive two sectors");
    Check(result.topology.lineDefs.size() == 7, "adjacent squares share one derived linedef");

    int twoSidedLineCount = 0;
    for (const game::SectorTopologyLineDef& lineDef : result.topology.lineDefs) {
        if (lineDef.frontSideDefId > 0 && lineDef.backSideDefId > 0) {
            ++twoSidedLineCount;
        }
    }
    Check(twoSidedLineCount == 1, "adjacent squares derive one two-sided linedef");
}

void TestVoidFaceExcludedFromDerivedTopologyAndJson()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(graph, 200, 32, 32, "void-room");
    graph.faceAnchors.back().isVoid = true;

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "single void face derives empty valid topology");
    Check(result.topology.vertices.empty(), "single void face emits no vertices");
    Check(result.topology.lineDefs.empty(), "single void face emits no linedefs");
    Check(result.topology.sideDefs.empty(), "single void face emits no sidedefs");
    Check(result.topology.sectors.empty(), "single void face emits no sectors");
    Check(result.mapping.sectors.empty(), "single void face emits no sector mappings");
    Check(result.mapping.resolvedFaces.size() == 1
                  && result.mapping.resolvedFaces.front().kind
                        == game::SectorAuthoringFaceResolutionKind::VoidNoDerivedSector,
          "single void face records void face resolution");
    Check(game::FindSectorAuthoringFaceAnchor(graph, 200) != nullptr,
          "single void face keeps authoring anchor");

    game::SectorAuthoringDocument document;
    document.graph = graph;
    document.derivation = result;
    std::string jsonText;
    std::string error;
    Check(game::SaveSectorAuthoringDocumentToJsonString(document, jsonText, &error),
          "void face graph-native save succeeds");
    Check(jsonText.find("\"isVoid\": true") != std::string::npos,
          "void face graph-native save persists true flag");

    game::SectorAuthoringDocument loaded;
    Check(game::LoadSectorAuthoringDocumentFromJsonString(jsonText, loaded, &error),
          "void face graph-native load succeeds");
    const game::SectorAuthoringFaceAnchor* loadedAnchor =
            game::FindSectorAuthoringFaceAnchor(loaded.graph, 200);
    Check(loadedAnchor != nullptr && loadedAnchor->isVoid,
          "void face graph-native load preserves true flag");

    Json parsed = Json::parse(jsonText);
    parsed["authoringGraph"]["faceAnchors"][0].erase("isVoid");
    Check(game::LoadSectorAuthoringDocumentFromJsonString(parsed.dump(2), loaded, &error),
          "missing void flag graph-native load succeeds");
    loadedAnchor = game::FindSectorAuthoringFaceAnchor(loaded.graph, 200);
    Check(loadedAnchor != nullptr && !loadedAnchor->isVoid,
          "missing void flag defaults to normal sector");
}

void TestAdjacentVoidFaceCreatesOneSidedWallAndProjectsNonVoidMaterial()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    AddFaceAnchor(graph, 200, 32, 32, "left-room");
    AddFaceAnchor(graph, 201, 96, 32, "void-room");
    graph.faceAnchors.back().isVoid = true;

    game::SectorAuthoringLineSide sharedSide;
    sharedSide.id = game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Front};
    sharedSide.wall.textureId = "non_void_wall";
    sharedSide.lower.textureId = "non_void_lower";
    sharedSide.upper.textureId = "non_void_upper";
    sharedSide.middle.textureId = "non_void_middle";
    graph.lineSides.push_back(sharedSide);

    game::SectorAuthoringLineSide voidSide;
    voidSide.id = game::SectorAuthoringSideId{11, game::SectorTopologySideKind::Back};
    voidSide.wall.textureId = "void_wall";
    graph.lineSides.push_back(voidSide);

    game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "adjacent void derives valid topology");
    Check(result.topology.sectors.size() == 1, "adjacent void emits only non-void sector");
    Check(result.mapping.sectors.size() == 1, "adjacent void emits only non-void sector mapping");
    Check(CountTwoSidedLineDefs(result.topology) == 0,
          "adjacent void emits no two-sided shared portal");

    const game::SectorTopologyLineDef* sharedLine =
            FindLineDefByEndpoints(result.topology, 64, 0, 64, 64);
    Check(sharedLine != nullptr, "adjacent void emits shared boundary linedef");
    if (sharedLine != nullptr) {
        const game::SectorTopologySideDef* sideDef = FindOnlySideDef(result.topology, *sharedLine);
        Check(sideDef != nullptr, "adjacent void shared boundary has exactly one sidedef");
        Check(sideDef != nullptr && sideDef->sectorId == 200,
              "adjacent void shared boundary sidedef belongs to non-void sector");
        Check(sideDef != nullptr
                      && sideDef->wall.textureId == "non_void_wall"
                      && sideDef->lower.textureId == "non_void_lower"
                      && sideDef->upper.textureId == "non_void_upper"
                      && sideDef->middle.textureId == "non_void_middle",
              "adjacent void shared boundary projects non-void side material");

        game::SectorGeneratedGeometry geometry;
        std::string error;
        Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
              "adjacent void generated geometry builds");
        Check(CountWallSurfacesForLine(geometry, sharedLine->id) > 0,
              "adjacent void shared boundary emits wall geometry");
        Check(CollisionHasBlockingEdgeForLine(result.topology, sharedLine->id),
              "adjacent void shared boundary is collision blocking");
    }

    game::SectorAuthoringFaceAnchor* voidAnchor =
            game::FindSectorAuthoringFaceAnchor(graph, 201);
    if (voidAnchor != nullptr) {
        voidAnchor->isVoid = false;
    }
    result = game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    CheckDerivedTopologyIsValid(result, "unvoided adjacent face derives valid topology");
    Check(result.topology.sectors.size() == 2, "unvoided adjacent face emits two sectors");
    sharedLine = FindLineDefByEndpoints(result.topology, 64, 0, 64, 64);
    Check(sharedLine != nullptr
                  && sharedLine->frontSideDefId > 0
                  && sharedLine->backSideDefId > 0,
          "unvoided adjacent face restores two-sided boundary");
    if (sharedLine != nullptr) {
        const game::SectorTopologySideDef* backSide =
                game::FindSectorTopologySideDef(result.topology, sharedLine->backSideDefId);
        Check(backSide != nullptr && backSide->wall.textureId == "void_wall",
              "unvoided adjacent face restores preserved void-side material");
    }
}

void TestEnclosedVoidFaceCreatesHoleAndSolidInnerWalls()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(2);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 64, 64, "inner-void");
    graph.faceAnchors.back().isVoid = true;

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "enclosed void face derives valid topology");
    Check(result.topology.sectors.size() == 1, "enclosed void emits only outer sector");
    Check(result.mapping.sectors.size() == 1, "enclosed void emits only outer sector mapping");
    Check(CountSectorHoles(result.topology, 200, "enclosed void outer sector extracts loops") == 1,
          "enclosed void creates one floor/ceiling hole");

    const game::SectorTopologyLineDef* innerLine =
            FindLineDefByEndpoints(result.topology, 32, 32, 96, 32);
    Check(innerLine != nullptr, "enclosed void emits inner boundary linedef");
    if (innerLine != nullptr) {
        Check(FindOnlySideDef(result.topology, *innerLine) != nullptr,
              "enclosed void inner boundary is one-sided");
        game::SectorGeneratedGeometry geometry;
        std::string error;
        Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
              "enclosed void generated geometry builds");
        Check(CountWallSurfacesForLine(geometry, innerLine->id) > 0,
              "enclosed void inner boundary emits wall geometry");
        Check(CollisionHasBlockingEdgeForLine(result.topology, innerLine->id),
              "enclosed void inner boundary is collision blocking");
    }

    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    game::SectorAuthoringSelectionTarget target;
    std::string status;
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  VisibleAuthoringPoint(64, 64),
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "enclosed void face remains selectable in 2D");
    Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && target.faceAnchorId == 201,
          "enclosed void 2D selection resolves void anchor");
}

void TestCenterVoidPreventsAccidentalSameHeightPortals()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {
                    {0, 0}, {32, 0}, {64, 0}, {96, 0},
                    {0, 32}, {32, 32}, {64, 32}, {96, 32},
                    {0, 64}, {32, 64}, {64, 64}, {96, 64},
                    {0, 96}, {32, 96}, {64, 96}, {96, 96}
            },
            {
                    {1, 2}, {2, 3}, {3, 4},
                    {5, 6}, {6, 7}, {7, 8},
                    {9, 10}, {10, 11}, {11, 12},
                    {13, 14}, {14, 15}, {15, 16},
                    {1, 5}, {5, 9}, {9, 13},
                    {2, 6}, {6, 10}, {10, 14},
                    {3, 7}, {7, 11}, {11, 15},
                    {4, 8}, {8, 12}, {12, 16}
            });
    AddFaceAnchor(graph, 200, 48, 48, "center-void");
    graph.faceAnchors.back().isVoid = true;

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "center void grid derives valid topology");
    Check(result.topology.sectors.size() == 8,
          "center void grid emits neighboring sectors but no center sector");
    Check(result.mapping.sectors.size() == 8,
          "center void grid has no derived sector mapping for center void");

    const game::SectorTopologyLineDef* top =
            FindLineDefByEndpoints(result.topology, 32, 32, 64, 32);
    const game::SectorTopologyLineDef* right =
            FindLineDefByEndpoints(result.topology, 64, 32, 64, 64);
    const game::SectorTopologyLineDef* bottom =
            FindLineDefByEndpoints(result.topology, 32, 64, 64, 64);
    const game::SectorTopologyLineDef* left =
            FindLineDefByEndpoints(result.topology, 32, 32, 32, 64);
    const game::SectorTopologyLineDef* centerBoundaries[] = {top, right, bottom, left};
    for (const game::SectorTopologyLineDef* lineDef : centerBoundaries) {
        Check(lineDef != nullptr, "center void boundary linedef exists");
        if (lineDef != nullptr) {
            Check(FindOnlySideDef(result.topology, *lineDef) != nullptr,
                  "center void boundary is one-sided");
        }
    }

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "center void grid generated geometry builds");
    Check(CountTwoSidedLineDefs(result.topology) == 8,
          "center void grid keeps portals only between non-void neighboring sectors");
}

void TestDeriveCrossingDiagonals()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "crossing diagonals derive valid topology");
    Check(result.topology.vertices.size() == 5, "crossing diagonals derive inserted intersection vertex");
    Check(result.topology.sectors.size() == 4, "crossing diagonals derive four sectors after planarization");
}

void TestDeriveDisconnectedClosedLoopsProducesSectors()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {128, 0}, {192, 0}, {192, 64}, {128, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "disconnected closed loops derive valid topology");
    Check(result.topology.sectors.size() == 2, "disconnected closed loops derive two sectors");
    Check(result.mapping.sectors.size() == 2, "disconnected closed loops record two sector mappings");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "disconnected closed loops derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "disconnected closed loops generate surfaces");
}

void TestDeriveNestedLoopCreatesHoleTopology()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}, {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "nested loop derives valid topology");
    Check(result.topology.sectors.size() == 2, "nested loop derives outer and contained sectors");
    Check(result.topology.lineDefs.size() == 8, "nested loop keeps eight physical boundary linedefs");
    Check(result.mapping.sectors.size() == 2, "nested loop records sector mappings");

    int sectorWithHoleCount = 0;
    int twoSidedLineCount = 0;
    for (const game::SectorTopologySector& sector : result.topology.sectors) {
        game::SectorTopologyLoopSet loops;
        std::vector<game::SectorTopologyValidationIssue> issues;
        Check(game::ExtractSectorTopologyLoops(result.topology, sector.id, loops, &issues),
              "nested loop sector extracts topology loops");
        if (!loops.holes.empty()) {
            ++sectorWithHoleCount;
            Check(loops.holes.size() == 1, "nested loop outer sector has one hole loop");
        }
    }
    for (const game::SectorTopologyLineDef& lineDef : result.topology.lineDefs) {
        if (lineDef.frontSideDefId > 0 && lineDef.backSideDefId > 0) {
            ++twoSidedLineCount;
        }
    }
    Check(sectorWithHoleCount == 1, "nested loop derives exactly one sector with a hole");
    Check(twoSidedLineCount == 4, "nested loop inner boundary derives four two-sided linedefs");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "nested loop derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "nested loop derived topology generates surfaces");
}

void TestDeriveNestedLoopProjectsInnerBoundaryProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}, {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});
    if (game::SectorAuthoringLine* innerLine = game::FindSectorAuthoringLine(graph, 14)) {
        innerLine->flags.blocksPlayer = true;
    }
    game::SectorAuthoringLineSide innerFront;
    innerFront.id = game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Front};
    innerFront.wall = WallPart("inner_front_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    graph.lineSides.push_back(innerFront);
    game::SectorAuthoringLineSide innerBack;
    innerBack.id = game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Back};
    innerBack.wall = WallPart("inner_back_wall", 5.0f, 6.0f, 7.0f, 8.0f);
    graph.lineSides.push_back(innerBack);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "nested loop property projection derives valid topology");
    const game::SectorTopologyLineDef* lineDef = FindDerivedLineDefForAuthoringLine(result, 14);
    Check(lineDef != nullptr && lineDef->flags.blocksPlayer,
          "nested loop inner authoring line flag projects to shared linedef");
    const game::SectorTopologySideDef* frontSide = FindDerivedSideDefForAuthoringSide(
            result,
            14,
            game::SectorTopologySideKind::Front);
    const game::SectorTopologySideDef* backSide = FindDerivedSideDefForAuthoringSide(
            result,
            14,
            game::SectorTopologySideKind::Back);
    Check(frontSide != nullptr && frontSide->wall.textureId == "inner_front_wall",
          "nested loop front side material projects to contained boundary side");
    Check(backSide != nullptr && backSide->wall.textureId == "inner_back_wall",
          "nested loop back side material projects to outer hole boundary side");
}

void TestDeriveNestedLoopFaceAnchorsResolveThroughHoles()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}, {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5}});
    game::SectorAuthoringFaceAnchor outerAnchor;
    outerAnchor.id = 200;
    outerAnchor.x = 16;
    outerAnchor.y = 16;
    outerAnchor.name = "outer";
    graph.faceAnchors.push_back(outerAnchor);
    game::SectorAuthoringFaceAnchor innerAnchor;
    innerAnchor.id = 201;
    innerAnchor.x = 64;
    innerAnchor.y = 64;
    innerAnchor.name = "inner";
    graph.faceAnchors.push_back(innerAnchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "nested loop anchored derivation is valid");
    const game::SectorAuthoringDerivedSectorMapping* outerMapping =
            FindSectorMappingForAnchor(result.mapping, 200);
    const game::SectorAuthoringDerivedSectorMapping* innerMapping =
            FindSectorMappingForAnchor(result.mapping, 201);
    Check(outerMapping != nullptr && outerMapping->topologySectorId == 200,
          "outer nested-loop anchor maps to outer sector");
    Check(innerMapping != nullptr && innerMapping->topologySectorId == 201,
          "inner nested-loop anchor maps to contained sector");
    Check(game::FindSectorTopologySector(result.topology, 200) != nullptr,
          "outer anchored sector exists");
    Check(game::FindSectorTopologySector(result.topology, 201) != nullptr,
          "inner anchored sector exists");
}

void TestNestedLoopsDepthTwoDerivesValidTopology()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "middle");
    AddFaceAnchor(graph, 202, 96, 96, "inner");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "depth-two nested loops derive valid topology");
    Check(result.topology.sectors.size() == 3, "depth-two nested loops derive three sectors");
    Check(result.mapping.sectors.size() == 3, "depth-two nested loops record three sector mappings");
    Check(CountSectorHoles(result.topology, 200, "depth-two outer sector extracts loops") == 1,
          "depth-two outer sector has only the middle loop as a direct hole");
    Check(CountSectorHoles(result.topology, 201, "depth-two middle sector extracts loops") == 1,
          "depth-two middle sector has only the inner loop as a direct hole");
    Check(CountSectorHoles(result.topology, 202, "depth-two inner sector extracts loops") == 0,
          "depth-two inner sector has no descendant holes");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "depth-two nested loop derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "depth-two nested loop derived topology generates surfaces");
}

void TestNestedLoopsDepthThreeDerivesValidTopology()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(4);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "middle");
    AddFaceAnchor(graph, 202, 80, 80, "inner");
    AddFaceAnchor(graph, 203, 128, 128, "deepest");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "depth-three nested loops derive valid topology");
    Check(result.topology.sectors.size() == 4, "depth-three nested loops derive four sectors");
    Check(CountSectorHoles(result.topology, 200, "depth-three outer sector extracts loops") == 1,
          "depth-three outer sector has one direct hole");
    Check(CountSectorHoles(result.topology, 201, "depth-three second sector extracts loops") == 1,
          "depth-three second sector has one direct hole");
    Check(CountSectorHoles(result.topology, 202, "depth-three third sector extracts loops") == 1,
          "depth-three third sector has one direct hole");
    Check(CountSectorHoles(result.topology, 203, "depth-three deepest sector extracts loops") == 0,
          "depth-three deepest sector has no holes");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "depth-three nested loop derived topology builds generated geometry");
}

void TestNestedLoopFaceAnchorsResolveToDeepestContainingSector()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "middle");
    AddFaceAnchor(graph, 202, 96, 96, "inner");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "nested loop deepest anchor resolution derives valid topology");
    const game::SectorAuthoringDerivedSectorMapping* outerMapping =
            FindSectorMappingForAnchor(result.mapping, 200);
    const game::SectorAuthoringDerivedSectorMapping* middleMapping =
            FindSectorMappingForAnchor(result.mapping, 201);
    const game::SectorAuthoringDerivedSectorMapping* innerMapping =
            FindSectorMappingForAnchor(result.mapping, 202);
    Check(outerMapping != nullptr && outerMapping->topologySectorId == 200,
          "outer nested-loop anchor maps to outer sector");
    Check(middleMapping != nullptr && middleMapping->topologySectorId == 201,
          "middle nested-loop anchor maps to middle sector");
    Check(innerMapping != nullptr && innerMapping->topologySectorId == 202,
          "inner nested-loop anchor maps to deepest sector");

    game::SectorAuthoringGraph ambiguousGraph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(ambiguousGraph, 300, 48, 48, "middle-a");
    AddFaceAnchor(ambiguousGraph, 301, 56, 56, "middle-b");
    const game::SectorAuthoringDerivationResult ambiguousResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(ambiguousGraph);
    Check(!ambiguousResult.success, "multiple anchors in one nested face fail derivation cleanly");
    Check(HasDerivationDiagnostic(
                  ambiguousResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor),
          "multiple anchors in one nested face report ambiguous face anchor diagnostic");
}

void TestNestedLoopDirectChildHolesOnly()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(4);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "middle");
    AddFaceAnchor(graph, 202, 80, 80, "inner");
    AddFaceAnchor(graph, 203, 128, 128, "deepest");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "direct-child nested loop derivation is valid");
    Check(CountSectorHoles(result.topology, 200, "direct-child outer sector extracts loops") == 1,
          "direct-child outer sector does not receive all descendants as holes");
    Check(CountSectorHoles(result.topology, 201, "direct-child middle sector extracts loops") == 1,
          "direct-child middle sector receives only its child as a hole");
    Check(CountSectorHoles(result.topology, 202, "direct-child inner sector extracts loops") == 1,
          "direct-child inner sector receives only its child as a hole");
    Check(CountSectorHoles(result.topology, 203, "direct-child deepest sector extracts loops") == 0,
          "direct-child deepest sector has no holes");
}

void TestNestedLoopDepthLimitReportsDiagnostic()
{
    const game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(10);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "nested loop beyond supported depth fails derivation");
    Check(result.topology.sectors.empty(), "nested loop beyond supported depth returns no half-valid topology");
    Check(result.mapping.sectors.empty(), "nested loop beyond supported depth returns no half-valid mapping");
    Check(HasDerivationDiagnosticMessageContaining(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::FaceExtraction,
                  "supported maximum of 8"),
          "nested loop beyond supported depth reports the supported maximum");
}

void TestDeriveOverlappingNestedLoopsProduceDiagnostics()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {192, 0}, {192, 192}, {0, 192},
             {32, 32}, {96, 32}, {96, 96}, {32, 96},
             {64, 32}, {128, 32}, {128, 96}, {64, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}, {10, 11}, {11, 12}, {12, 9}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "overlapping nested loops fail derivation");
    Check(result.topology.sectors.empty(), "overlapping nested loops return no half-valid topology");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::CollinearOverlap),
          "overlapping nested loops report overlap diagnostics");
}

void TestDeriveNonIntegerIntersectionFails()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {64, 33}},
            {{1, 2}, {2, 5}, {5, 3}, {3, 4}, {4, 1}, {1, 5}, {4, 2}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    bool hasNonIntegerPlanarVertex = false;
    for (const game::SectorAuthoringPlanarVertex& vertex : result.planar.vertices) {
        if (!game::SectorAuthoringPlanarRationalIsInteger(vertex.point.x)
                || !game::SectorAuthoringPlanarRationalIsInteger(vertex.point.y)) {
            hasNonIntegerPlanarVertex = true;
            break;
        }
    }

    Check(!result.success, "non-integer intersection fails derivation");
    Check(result.topology.vertices.empty(), "non-integer intersection returns no half-valid topology vertices");
    Check(result.topology.lineDefs.empty(), "non-integer intersection returns no half-valid topology linedefs");
    Check(result.topology.sideDefs.empty(), "non-integer intersection returns no half-valid topology sidedefs");
    Check(result.topology.sectors.empty(), "non-integer intersection returns no half-valid topology sectors");
    Check(result.mapping.vertices.empty(), "non-integer intersection returns no half-valid vertex mapping");
    Check(result.mapping.lines.empty(), "non-integer intersection returns no half-valid line mapping");
    Check(result.mapping.sides.empty(), "non-integer intersection returns no half-valid side mapping");
    Check(result.mapping.sectors.empty(), "non-integer intersection returns no half-valid sector mapping");
    Check(result.planar.diagnostics.empty(), "non-integer intersection is not rejected during planarization");
    Check(!result.faces.faces.empty(), "non-integer intersection reaches face extraction before failing");
    Check(result.faces.diagnostics.empty(), "non-integer intersection is not rejected during face extraction");
    Check(hasNonIntegerPlanarVertex, "non-integer intersection keeps fractional planar vertex coordinates");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex),
          "non-integer intersection reports non-integer vertex diagnostic");
}

void TestDeriveSquareWithMixedLineDirections()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {3, 2}, {3, 4}, {4, 1}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "mixed-direction square derives valid topology");
    Check(result.topology.vertices.size() == 4, "mixed-direction square derives four vertices");
    Check(result.topology.lineDefs.size() == 4, "mixed-direction square derives four linedefs");
    Check(result.topology.sideDefs.size() == 4, "mixed-direction square derives four sidedefs");
    Check(result.topology.sectors.size() == 1, "mixed-direction square derives one sector");

    bool hasBackBoundarySide = false;
    for (const game::SectorTopologyLineDef& lineDef : result.topology.lineDefs) {
        const bool hasFront = lineDef.frontSideDefId > 0;
        const bool hasBack = lineDef.backSideDefId > 0;
        Check(hasFront != hasBack, "mixed-direction square derives exactly one side per boundary linedef");
        hasBackBoundarySide = hasBackBoundarySide || hasBack;
    }
    Check(hasBackBoundarySide, "mixed-direction square keeps reversed authored edge on back side");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "mixed-direction square derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "mixed-direction square derived topology generates surfaces");
}

void TestDeriveOpenGraphFails()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "open graph fails derivation");
    Check(result.topology.sectors.empty(), "open graph does not return half-valid topology");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::FaceExtraction),
          "open graph reports face extraction diagnostic");
}

void TestDeriveDuplicateLinesFail()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "duplicate line graph fails derivation");
    Check(result.topology.lineDefs.empty(), "duplicate line graph returns no derived linedefs");
    Check(HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DuplicateLine),
          "duplicate line graph reports duplicate line diagnostic");
}

void TestDeriveDiagnosticsUseSpecificKindsAndSources()
{
    const game::SectorAuthoringGraph openGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}});
    const game::SectorAuthoringDerivationResult openResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(openGraph);
    Check(HasDerivationDiagnostic(
                  openResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DanglingLine),
          "open graph derivation reports explicit dangling line diagnostic");

    const game::SectorAuthoringGraph duplicateGraph = MakeGraphFromLines({
            {0, 0}, {32, 0},
            {32, 0}, {0, 0}});
    const game::SectorAuthoringDerivationResult duplicateResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(duplicateGraph);
    Check(HasDerivationDiagnosticFor(
                  duplicateResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DuplicateLine,
                  10),
          "duplicate line derivation diagnostic includes source line ID");
    Check(!duplicateResult.diagnostics.empty()
                  && duplicateResult.diagnostics.front().relatedObjectId == 11,
          "duplicate line derivation diagnostic includes related line ID");

    const game::SectorAuthoringGraph overlapGraph = MakeGraphFromLines({
            {0, 0}, {64, 0},
            {32, 0}, {96, 0}});
    const game::SectorAuthoringDerivationResult overlapResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(overlapGraph);
    Check(HasDerivationDiagnostic(
                  overlapResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::CollinearOverlap),
          "overlap derivation reports explicit collinear overlap diagnostic");
}

void TestDeriveDanglingLineDiagnosticUsesAuthoringLineId()
{
    const int danglingLineId = 14;
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}, {96, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {3, 5}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "dangling line graph fails derivation");
    const game::SectorAuthoringDerivationDiagnostic* diagnostic = FindDerivationDiagnosticFor(
            result.diagnostics,
            game::SectorAuthoringDerivationDiagnosticKind::DanglingLine,
            danglingLineId);
    Check(diagnostic != nullptr, "dangling line diagnostic uses authoring line ID");

    int danglingPlanarEdgeId = -1;
    for (const game::SectorAuthoringPlanarEdge& edge : result.planar.edges) {
        if (edge.sourceLineId == danglingLineId) {
            danglingPlanarEdgeId = edge.id;
            break;
        }
    }
    Check(danglingPlanarEdgeId > 0, "dangling line planar edge is available for comparison");
    if (diagnostic != nullptr && danglingPlanarEdgeId > 0) {
        Check(diagnostic->objectId != danglingPlanarEdgeId,
              "dangling line diagnostic does not use internal planar edge ID as primary object");
        Check(diagnostic->relatedObjectId == danglingPlanarEdgeId,
              "dangling line diagnostic keeps internal planar edge ID as related data");
    }
}

void TestDeriveDisconnectedClosedLoopsWithDanglingLineFailsCleanly()
{
    const int danglingLineId = 18;
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64},
             {128, 0}, {192, 0}, {192, 64}, {128, 64},
             {224, 0}, {256, 0}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "disconnected loops plus dangling line fail derivation");
    Check(result.topology.sectors.empty(), "dangling disconnected graph returns no half-valid topology");
    Check(result.mapping.sectors.empty(), "dangling disconnected graph returns no half-valid mapping");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::DanglingLine,
                  danglingLineId),
          "dangling disconnected graph reports dangling authoring line ID");
    Check(!HasDerivationDiagnostic(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::FaceExtraction),
          "dangling disconnected graph does not report generic face extraction ambiguity");
}

void TestDeriveSplitLineMapping()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "split-line mapping graph derives valid topology");
    Check(CountDerivedLineMappingsForAuthoringLine(result.mapping, 14) == 2,
          "planarized diagonal maps one authoring line to two derived linedefs");
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId == 14) {
            Check(lineMapping.sourceLineId == 14, "line mapping records derived back-source line ID");
            Check(lineMapping.topologyLineDefId > 0, "split line mapping records derived linedef ID");
        }
    }
}

void TestDeriveFaceAnchorMapping()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = 200;
    anchor.x = 32;
    anchor.y = 32;
    graph.faceAnchors.push_back(anchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "anchored square derives valid topology");
    const game::SectorAuthoringDerivedSectorMapping* sectorMapping =
            FindSectorMappingForAnchor(result.mapping, 200);
    Check(sectorMapping != nullptr, "face anchor maps to derived sector");
    if (sectorMapping != nullptr) {
        Check(sectorMapping->extractedFaceId > 0, "sector mapping records extracted face ID");
        Check(sectorMapping->faceAnchorId == 200, "sector mapping records face anchor ID");
        Check(sectorMapping->topologySectorId == 200, "derived sector uses unambiguous face anchor ID");
    }
}

void TestDeriveFaceAnchorDiagnostics()
{
    game::SectorAuthoringGraph unresolvedGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor unresolvedAnchor;
    unresolvedAnchor.id = 300;
    unresolvedAnchor.x = 128;
    unresolvedAnchor.y = 128;
    unresolvedGraph.faceAnchors.push_back(unresolvedAnchor);

    const game::SectorAuthoringDerivationResult unresolvedResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(unresolvedGraph);
    Check(!unresolvedResult.success, "unresolved face anchor fails derivation cleanly");
    Check(HasDerivationDiagnosticFor(
                  unresolvedResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor,
                  300),
          "unresolved face anchor reports explicit diagnostic with anchor ID");

    game::SectorAuthoringGraph ambiguousGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor firstAnchor;
    firstAnchor.id = 400;
    firstAnchor.x = 32;
    firstAnchor.y = 32;
    ambiguousGraph.faceAnchors.push_back(firstAnchor);
    game::SectorAuthoringFaceAnchor secondAnchor;
    secondAnchor.id = 401;
    secondAnchor.x = 48;
    secondAnchor.y = 48;
    ambiguousGraph.faceAnchors.push_back(secondAnchor);

    const game::SectorAuthoringDerivationResult ambiguousResult =
            game::DeriveSectorTopologyMapFromAuthoringGraph(ambiguousGraph);
    Check(!ambiguousResult.success, "multiple anchors in one face fail derivation cleanly");
    Check(HasDerivationDiagnostic(
                  ambiguousResult.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor),
          "multiple anchors in one face report ambiguous face anchor diagnostic");
}

void TestDeriveInvalidSideProjectionDiagnostic()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringLineSide side;
    side.id.lineId = 999;
    side.id.side = game::SectorTopologySideKind::Front;
    graph.lineSides.push_back(side);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "invalid side projection fails derivation cleanly");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection,
                  999),
          "invalid side projection reports explicit diagnostic with source line ID");
}

void TestDeriveProjectsFaceAnchorProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = 200;
    anchor.name = "projected";
    anchor.x = 32;
    anchor.y = 32;
    anchor.floorZ = -4.0f;
    anchor.ceilingZ = 48.0f;
    anchor.floorTextureId = "floor_projected";
    anchor.ceilingTextureId = "ceiling_projected";
    anchor.ceilingSky = true;
    anchor.floorUv.scale = Vector2{2.0f, 3.0f};
    anchor.floorUv.offset = Vector2{4.0f, 5.0f};
    anchor.ceilingUv.scale = Vector2{6.0f, 7.0f};
    anchor.ceilingUv.offset = Vector2{8.0f, 9.0f};
    anchor.floorDecal.textureId = "floor_decal_projected";
    anchor.ceilingDecal.textureId = "ceiling_decal_projected";
    anchor.ambientColor = Color{12, 24, 36, 255};
    anchor.ambientIntensity = 0.45f;
    anchor.defaultWall = WallPart("anchor_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    anchor.defaultLower = WallPart("anchor_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    anchor.defaultUpper = WallPart("anchor_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    graph.faceAnchors.push_back(anchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "face anchor property projection derives valid topology");
    const game::SectorTopologySector* sector = game::FindSectorTopologySector(result.topology, 200);
    Check(sector != nullptr, "projected face anchor uses anchor ID as derived sector ID");
    if (sector != nullptr) {
        Check(anchor.id == sector->id, "projected sector keeps anchor ID");
        Check(anchor.name == sector->name, "projected sector keeps anchor name");
        Check(anchor.floorZ == sector->floorZ, "projected sector keeps floor height");
        Check(anchor.ceilingZ == sector->ceilingZ, "projected sector keeps ceiling height");
        Check(anchor.floorTextureId == sector->floorTextureId, "projected sector keeps floor texture");
        Check(anchor.ceilingTextureId == sector->ceilingTextureId, "projected sector keeps ceiling texture");
        Check(sector->ceilingSky, "projected sector keeps ceiling sky");
        Check(SameUv(anchor.floorUv, sector->floorUv), "projected sector keeps floor UV");
        Check(SameUv(anchor.ceilingUv, sector->ceilingUv), "projected sector keeps ceiling UV");
        Check(anchor.floorDecal.textureId == sector->floorDecal.textureId, "projected sector keeps floor decal");
        Check(anchor.ceilingDecal.textureId == sector->ceilingDecal.textureId, "projected sector keeps ceiling decal");
        Check(anchor.ambientColor.r == sector->ambientColor.r
                      && anchor.ambientColor.g == sector->ambientColor.g
                      && anchor.ambientColor.b == sector->ambientColor.b
                      && anchor.ambientColor.a == sector->ambientColor.a,
              "projected sector keeps ambient color");
        Check(anchor.ambientIntensity == sector->ambientIntensity, "projected sector keeps ambient intensity");
        CheckWallPartCopied(sector->defaultWall, anchor.defaultWall, "projected sector keeps default wall");
        CheckWallPartCopied(sector->defaultLower, anchor.defaultLower, "projected sector keeps default lower");
        CheckWallPartCopied(sector->defaultUpper, anchor.defaultUpper, "projected sector keeps default upper");
    }
}

void TestDeriveProjectsSideMaterialsAndLineFlags()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    if (game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, 10)) {
        line->flags.blocksPlayer = true;
    }
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("side_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    side.lower = WallPart("side_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    side.upper = WallPart("side_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    side.middle = WallPart("side_middle", 4.0f, 5.0f, 6.0f, 7.0f);
    side.middle.decal.textureId = "side_middle_decal";
    side.middle.decal.opacity = 0.25f;
    graph.lineSides.push_back(side);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "side material projection derives valid topology");
    const game::SectorTopologyLineDef* lineDef = FindDerivedLineDefForAuthoringLine(result, 10);
    Check(lineDef != nullptr && lineDef->flags.blocksPlayer, "authoring line blocksPlayer projects to derived linedef");
    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            result,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "authoring side maps to a derived sidedef");
    if (sideDef != nullptr) {
        CheckWallPartCopied(sideDef->wall, side.wall, "authoring side wall projects to derived sidedef");
        CheckWallPartCopied(sideDef->lower, side.lower, "authoring side lower projects to derived sidedef");
        CheckWallPartCopied(sideDef->upper, side.upper, "authoring side upper projects to derived sidedef");
        CheckWallPartCopied(sideDef->middle, side.middle, "authoring side middle projects to derived sidedef");
    }
}

void TestDeriveSplitLineDuplicatesProjectedProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});
    if (game::SectorAuthoringLine* diagonal = game::FindSectorAuthoringLine(graph, 14)) {
        diagonal->flags.blocksPlayer = true;
    }
    game::SectorAuthoringLineSide frontSide;
    frontSide.id = game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Front};
    frontSide.wall = WallPart("split_front_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    graph.lineSides.push_back(frontSide);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "split-line property projection derives valid topology");
    int projectedLineCount = 0;
    int projectedFrontSideCount = 0;
    for (const game::SectorAuthoringDerivedLineMapping& lineMapping : result.mapping.lines) {
        if (lineMapping.authoringLineId != 14) {
            continue;
        }
        const game::SectorTopologyLineDef* lineDef =
                game::FindSectorTopologyLineDef(result.topology, lineMapping.topologyLineDefId);
        Check(lineDef != nullptr && lineDef->flags.blocksPlayer, "split derived linedef keeps source line flag");
        ++projectedLineCount;
    }
    for (const game::SectorAuthoringDerivedSideMapping& sideMapping : result.mapping.sides) {
        if (sideMapping.authoringLineId != 14
                || sideMapping.authoringSide != game::SectorTopologySideKind::Front) {
            continue;
        }
        const game::SectorTopologySideDef* sideDef =
                game::FindSectorTopologySideDef(result.topology, sideMapping.topologySideDefId);
        Check(sideDef != nullptr && sideDef->wall.textureId == "split_front_wall",
              "split derived sidedef keeps source side wall material");
        ++projectedFrontSideCount;
    }
    Check(projectedLineCount == 2, "split source line projects flags to both child linedefs");
    Check(projectedFrontSideCount == 2, "split source side projects materials to both child sidedefs");
}

void TestDeriveUnresolvedAnchorPreservesAuthoringProperties()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::SectorAuthoringFaceAnchor unresolvedAnchor;
    unresolvedAnchor.id = 300;
    unresolvedAnchor.x = 128;
    unresolvedAnchor.y = 128;
    unresolvedAnchor.floorTextureId = "unresolved_floor";
    graph.faceAnchors.push_back(unresolvedAnchor);

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    Check(!result.success, "unresolved property anchor fails derivation");
    Check(HasDerivationDiagnosticFor(
                  result.diagnostics,
                  game::SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor,
                  300),
          "unresolved property anchor reports diagnostic");
    const game::SectorAuthoringFaceAnchor* preserved =
            game::FindSectorAuthoringFaceAnchor(graph, 300);
    Check(preserved != nullptr && preserved->floorTextureId == "unresolved_floor",
          "unresolved anchor properties remain in authoring graph");
}

void TestDerivedTopologyBuildsGeometry()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    CheckDerivedTopologyIsValid(result, "geometry derivation input is valid");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(), "derived topology generates surfaces");
}

void TestFreshDerivedTopologyUsesDefaultMaterials()
{
    const game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    CheckDerivedTopologyIsValid(result, "fresh material default derivation input is valid");

    Check(result.topology.sectors.size() == 1, "fresh material default graph derives one sector");
    if (!result.topology.sectors.empty()) {
        const game::SectorTopologySector& sector = result.topology.sectors.front();
        Check(sector.floorTextureId == "floor", "fresh derived sector gets default floor texture");
        Check(sector.ceilingTextureId == "ceiling", "fresh derived sector gets default ceiling texture");
        Check(sector.defaultWall.textureId == "wall", "fresh derived sector gets default wall texture");
        Check(sector.defaultLower.textureId == "wall", "fresh derived sector gets default lower wall texture");
        Check(sector.defaultUpper.textureId == "wall", "fresh derived sector gets default upper wall texture");
    }

    for (const game::SectorTopologySideDef& sideDef : result.topology.sideDefs) {
        Check(sideDef.wall.textureId == "wall", "fresh derived sidedef gets default wall texture");
        Check(sideDef.lower.textureId == "wall", "fresh derived sidedef gets default lower wall texture");
        Check(sideDef.upper.textureId == "wall", "fresh derived sidedef gets default upper wall texture");
    }

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "fresh derived default-material topology builds generated geometry");
    bool sawFloor = false;
    bool sawCeiling = false;
    bool sawWall = false;
    bool sawEmptyTexture = false;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        sawEmptyTexture = sawEmptyTexture || surface.textureId.empty();
        sawFloor = sawFloor || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Floor
                && surface.textureId == "floor");
        sawCeiling = sawCeiling || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Ceiling
                && surface.textureId == "ceiling");
        sawWall = sawWall || (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                && surface.textureId == "wall");
    }
    Check(!sawEmptyTexture, "fresh derived generated geometry surfaces have non-empty textures");
    Check(sawFloor, "fresh derived generated geometry has default floor texture");
    Check(sawCeiling, "fresh derived generated geometry has default ceiling texture");
    Check(sawWall, "fresh derived generated geometry has default wall texture");
}

void TestEditorAuthoringRefreshSynthesizedOuterSectorGetsDefaultMaterials()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "fresh outer refresh synthesizes and derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 1,
          "fresh outer refresh synthesizes one face anchor");
    const game::SectorAuthoringFaceAnchor* anchor = authoringGraph.faceAnchors.empty()
            ? nullptr
            : &authoringGraph.faceAnchors.front();
    Check(anchor != nullptr && anchor->name == "Sector 1",
          "fresh outer synthesized anchor gets first generated label");
    Check(anchor != nullptr
                  && !anchor->floorTextureId.empty()
                  && !anchor->ceilingTextureId.empty()
                  && !anchor->defaultWall.textureId.empty()
                  && !anchor->defaultLower.textureId.empty()
                  && !anchor->defaultUpper.textureId.empty(),
          "fresh outer synthesized anchor gets non-empty default materials");
    Check(anchor != nullptr
                  && anchor->floorTextureId == "floor"
                  && anchor->ceilingTextureId == "ceiling"
                  && anchor->defaultWall.textureId == "wall"
                  && anchor->defaultLower.textureId == "wall"
                  && anchor->defaultUpper.textureId == "wall",
          "fresh outer synthesized anchor uses normal graph-authored defaults");
    Check(AllDerivedSectorsHaveExactlyOneValidFaceAnchorMapping(
                  authoringGraph,
                  documentState.derivation.authoringDerivation),
          "fresh outer synthesized anchor maps to derived sector");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(documentState.map.topologyMap, geometry, &error),
          "fresh outer reconciled topology builds generated geometry");
    bool sawEmptyTexture = false;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        sawEmptyTexture = sawEmptyTexture || surface.textureId.empty();
    }
    Check(!sawEmptyTexture,
          "fresh outer reconciled generated geometry does not rely on empty texture IDs");
}

void TestEditorAuthoringRefreshAddingInnerSectorPreservesOuterAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "outer-only anchor preservation setup derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 1,
          "outer-only anchor preservation setup synthesizes one anchor");
    if (authoringGraph.faceAnchors.empty()) {
        return;
    }

    const int outerAnchorId = authoringGraph.faceAnchors.front().id;
    game::SectorAuthoringFaceAnchor* outerAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, outerAnchorId);
    Check(outerAnchor != nullptr, "outer anchor exists before adding inner loop");
    if (outerAnchor == nullptr) {
        return;
    }
    outerAnchor->name = "Sector 1";
    outerAnchor->floorZ = -6.0f;
    outerAnchor->ceilingZ = 42.0f;
    outerAnchor->floorTextureId = "outer_floor";
    outerAnchor->ceilingTextureId = "outer_ceiling";
    outerAnchor->defaultWall = WallPart("outer_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    outerAnchor->defaultLower = WallPart("outer_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    outerAnchor->defaultUpper = WallPart("outer_upper", 3.0f, 4.0f, 5.0f, 6.0f);

    AddAuthoringVertexWithId(authoringGraph, 5, 32, 32);
    AddAuthoringVertexWithId(authoringGraph, 6, 96, 32);
    AddAuthoringVertexWithId(authoringGraph, 7, 96, 96);
    AddAuthoringVertexWithId(authoringGraph, 8, 32, 96);
    AddAuthoringLineWithId(authoringGraph, 14, 5, 6);
    AddAuthoringLineWithId(authoringGraph, 15, 6, 7);
    AddAuthoringLineWithId(authoringGraph, 16, 7, 8);
    AddAuthoringLineWithId(authoringGraph, 17, 8, 5);

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "adding inner loop reconciles and derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 2,
          "adding inner loop synthesizes only one new face anchor");

    const game::SectorAuthoringFaceAnchor* preservedOuter =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, outerAnchorId);
    Check(preservedOuter != nullptr && preservedOuter->name == "Sector 1",
          "outer anchor keeps original generated label");
    Check(preservedOuter != nullptr
                  && preservedOuter->floorZ == -6.0f
                  && preservedOuter->ceilingZ == 42.0f
                  && preservedOuter->floorTextureId == "outer_floor"
                  && preservedOuter->ceilingTextureId == "outer_ceiling"
                  && preservedOuter->defaultWall.textureId == "outer_wall"
                  && preservedOuter->defaultLower.textureId == "outer_lower"
                  && preservedOuter->defaultUpper.textureId == "outer_upper",
          "outer anchor keeps material and height properties");

    game::SectorAuthoringSelectionTarget outerTarget;
    game::SectorAuthoringSelectionTarget innerTarget;
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  VisibleAuthoringPoint(16, 16),
                  0.25f,
                  0.25f,
                  &outerTarget),
          "outer ring point selects an authoring face after adding inner loop");
    Check(outerTarget.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && outerTarget.faceAnchorId == outerAnchorId,
          "outer ring point still selects original outer anchor");
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  VisibleAuthoringPoint(64, 64),
                  0.25f,
                  0.25f,
                  &innerTarget),
          "inner face point selects an authoring face after adding inner loop");
    Check(innerTarget.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && innerTarget.faceAnchorId != outerAnchorId,
          "inner face point selects newly synthesized anchor");

    const game::SectorAuthoringFaceAnchor* innerAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, innerTarget.faceAnchorId);
    Check(innerAnchor != nullptr && innerAnchor->name == "Sector 2",
          "inner synthesized anchor gets next generated label");
    Check(innerAnchor != nullptr
                  && !innerAnchor->floorTextureId.empty()
                  && !innerAnchor->ceilingTextureId.empty()
                  && !innerAnchor->defaultWall.textureId.empty()
                  && !innerAnchor->defaultLower.textureId.empty()
                  && !innerAnchor->defaultUpper.textureId.empty(),
          "inner synthesized anchor gets valid material defaults");
}

void TestEditorAuthoringGraphMutationMarksDirtyAndStale()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});
    const uint64_t originalRevision = state.topologyRenderRevision;

    int firstVertexId = -1;
    Check(game::AddSectorAuthoringVertex(authoringGraph, 0, 0, &firstVertexId),
          "editor authoring mutation test adds vertex");
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed");

    Check(documentState.lifecycle.topologyDocumentDirty, "graph mutation marks document dirty");
    Check(documentState.derivation.authoringDerivedTopologyStale, "graph mutation marks derived topology stale");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidNoDerived,
          "graph mutation without last valid topology has invalid/no-derived state");
    Check(!state.topologyRenderCache.valid, "graph mutation invalidates topology render cache");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "graph mutation bumps topology render revision");
}

void TestAuthoringOverlayRenderCacheIncludesLooseGraph()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(!derivation.success, "loose authoring line has no valid derived topology");

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    derivation,
                    7);

    Check(cache.valid, "authoring overlay cache builds without valid derived topology");
    Check(cache.revision == 7, "authoring overlay cache stores revision");
    Check(cache.sectors.empty(), "authoring overlay cache does not require derived sector fills");
    Check(cache.authoringVertices.size() == 2, "authoring overlay cache includes authoring vertices");
    Check(cache.authoringLines.size() == 1, "authoring overlay cache includes loose authoring line");
    if (!cache.authoringLines.empty()) {
        Check(cache.authoringLines.front().lineId == 10, "authoring overlay cache preserves line ID");
        Check(cache.authoringLines.front().validEndpoints, "authoring overlay cache resolves loose line endpoints");
    }
}

void TestAuthoringDiagnosticRenderCacheDoesNotRequireDerivedTopology()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringVertexWithId(graph, 3, 64, 64);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    AddAuthoringLineWithId(graph, 11, 2, 3);

    game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(!derivation.success, "open authoring graph fails derivation for diagnostic cache test");

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    derivation,
                    8);

    Check(!cache.authoringDiagnostics.empty(),
          "authoring diagnostic cache includes failed-derivation diagnostics");
    bool foundPositionedDiagnostic = false;
    for (const game::CachedAuthoringDiagnosticDraw& diagnostic : cache.authoringDiagnostics) {
        if (diagnostic.hasPosition) {
            foundPositionedDiagnostic = true;
            break;
        }
    }
    Check(foundPositionedDiagnostic,
          "authoring diagnostic cache maps at least one diagnostic to an authoring graph position");
}

void TestAuthoringOverlayRenderCacheIncludesReferenceDiagnostics()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringLineWithId(graph, 10, 1, 99);

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    game::SectorTopologyMap{},
                    graph,
                    game::SectorAuthoringDerivationResult{},
                    9);

    Check(cache.authoringLines.size() == 1, "authoring overlay cache includes line with missing endpoint");
    if (!cache.authoringLines.empty()) {
        Check(!cache.authoringLines.front().validEndpoints,
              "authoring overlay cache marks missing endpoint line invalid");
        Check(cache.authoringLines.front().hasPartialEndpoint,
              "authoring overlay cache keeps visible partial endpoint for invalid line");
    }
    Check(!cache.authoringDiagnostics.empty(),
          "authoring diagnostic cache includes reference validation diagnostics");
}

void TestAuthoringOverlaySuppressesLegacyTopologySelectionHighlights()
{
    Check(!game::ShouldDrawLegacyTopologySelectionHighlight(true, game::TopologySelectionKind::LineDef),
          "authoring overlay suppresses legacy selected edge highlight");
    Check(!game::ShouldDrawLegacyTopologySelectionHighlight(true, game::TopologySelectionKind::SideDef),
          "authoring overlay suppresses legacy selected side highlight");
    Check(!game::ShouldDrawLegacyTopologySelectionHighlight(true, game::TopologySelectionKind::Sector),
          "authoring overlay suppresses legacy selected sector highlight");
    Check(!game::ShouldDrawLegacyTopologySelectionHighlight(true, game::TopologySelectionKind::Vertex),
          "authoring overlay suppresses legacy selected vertex highlight");
    Check(game::ShouldDrawLegacyTopologySelectionHighlight(false, game::TopologySelectionKind::LineDef),
          "legacy selected edge highlight remains available without authoring graph data");
}

void TestAuthoringOverlaySelectionHighlightDecisions()
{
    game::SectorAuthoringSelectionTarget lineTarget =
            game::MakeSectorAuthoringLineSelectionTarget(10);
    game::SectorAuthoringSelectionTarget vertexTarget =
            game::MakeSectorAuthoringVertexSelectionTarget(2);

    Check(game::ShouldDrawAuthoringLineSelectionHighlight(lineTarget, 10),
          "authoring overlay requests selected line highlight");
    Check(!game::ShouldDrawAuthoringLineSelectionHighlight(lineTarget, 11),
          "authoring overlay line highlight requires selected line ID");
    Check(game::ShouldDrawAuthoringVertexSelectionHighlight(vertexTarget, 2),
          "authoring overlay requests selected vertex highlight");
    Check(!game::ShouldDrawAuthoringVertexSelectionHighlight(vertexTarget, 1),
          "authoring overlay vertex highlight requires selected vertex ID");
}

void TestAuthoringOverlayFaceAnchorHighlightResolvesVisibleBoundary()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "ring");
    AddFaceAnchor(graph, 202, 96, 96, "inner");

    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    documentState.map.topologyMap,
                    authoringGraph,
                    documentState.derivation.authoringDerivation,
                    10);

    const game::CachedAuthoringFaceHighlightDraw* outer =
            game::FindCachedAuthoringFaceHighlight(cache, 200);
    const game::CachedAuthoringFaceHighlightDraw* ring =
            game::FindCachedAuthoringFaceHighlight(cache, 201);
    const game::CachedAuthoringFaceHighlightDraw* inner =
            game::FindCachedAuthoringFaceHighlight(cache, 202);
    Check(outer != nullptr, "outer face anchor resolves highlight geometry");
    Check(ring != nullptr, "ring face anchor resolves highlight geometry");
    Check(inner != nullptr, "inner face anchor resolves highlight geometry");
    if (outer == nullptr || ring == nullptr || inner == nullptr) {
        return;
    }

    Check(outer->topologySectorId != ring->topologySectorId
                  && ring->topologySectorId != inner->topologySectorId,
          "nested face anchors resolve to distinct visible faces");
    Check(ring->topologySectorId != inner->topologySectorId,
          "ring face highlight does not resolve to child face");

    bool ringHasHoleBoundary = false;
    for (const game::CachedTopologyOutlineSegment& segment : ring->outlineSegments) {
        if (segment.hole) {
            ringHasHoleBoundary = true;
            break;
        }
    }
    Check(ringHasHoleBoundary, "ring face highlight includes available hole boundary");

    game::SectorEditorTopologyDrawContext context;
    context.selectedAuthoring = game::MakeSectorAuthoringFaceAnchorSelectionTarget(201);
    Check(game::ShouldDrawAuthoringFaceSelectionHighlight(cache, context, 201),
          "authoring overlay requests selected face-boundary highlight");
}

void TestAuthoringOverlayFaceAnchorHighlightFailsClosedWithoutCurrentMapping()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(2);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 64, 64, "inner");

    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    documentState.map.topologyMap,
                    authoringGraph,
                    documentState.derivation.authoringDerivation,
                    11);

    game::SectorEditorTopologyDrawContext context;
    context.selectedAuthoring = game::MakeSectorAuthoringFaceAnchorSelectionTarget(200);
    context.derivedTopologyStale = true;
    Check(!game::ShouldDrawAuthoringFaceSelectionHighlight(cache, context, 200),
          "stale authoring derivation suppresses face-boundary highlight");

    game::SectorAuthoringDerivationResult missingMapping = documentState.derivation.authoringDerivation;
    missingMapping.mapping.sectors.clear();
    missingMapping.mapping.resolvedFaces.clear();
    cache = game::BuildSectorEditorTopologyRenderCache(
            documentState.map.topologyMap,
            authoringGraph,
            missingMapping,
            12);
    context.derivedTopologyStale = false;
    Check(game::FindCachedAuthoringFaceHighlight(cache, 200) == nullptr,
          "missing face-anchor mapping resolves no face highlight geometry");
    Check(!game::ShouldDrawAuthoringFaceSelectionHighlight(cache, context, 200),
          "missing face-anchor mapping does not fall back to topology selection");

    game::SectorAuthoringDerivationResult ambiguousMapping = documentState.derivation.authoringDerivation;
    ambiguousMapping.mapping.sectors.push_back(ambiguousMapping.mapping.sectors.front());
    ambiguousMapping.mapping.resolvedFaces.push_back(ambiguousMapping.mapping.resolvedFaces.front());
    cache = game::BuildSectorEditorTopologyRenderCache(
            documentState.map.topologyMap,
            authoringGraph,
            ambiguousMapping,
            13);
    Check(game::FindCachedAuthoringFaceHighlight(cache, 200) == nullptr,
          "ambiguous face-anchor mapping resolves no face highlight geometry");
}

void TestEditorAuthoringSuccessfulDerivationUpdatesState()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("wall", game::SectorTextureDefinition{"wall", "assets/images/wall.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring square changed");

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "successful editor derivation returns true");
    Check(documentState.derivation.authoringDerivation.success, "successful editor derivation stores successful result");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "successful editor derivation marks state current");
    Check(!documentState.derivation.authoringDerivedTopologyStale, "successful editor derivation clears stale flag");
    Check(documentState.derivation.lastValidAuthoringDerivedTopology.has_value(),
          "successful editor derivation stores memory-only last-valid topology");
    Check(documentState.map.topologyMap.sectors.size() == 1,
          "successful editor derivation updates current derived topology");
    Check(documentState.derivation.authoringDerivation.mapping.sectors.size() == 1,
          "successful editor derivation preserves mapping for editor state");
    Check(documentState.map.topologyMap.texturesById.find("wall") != documentState.map.topologyMap.texturesById.end(),
          "successful editor derivation preserves map-level texture data");
}

void TestEditorAuthoringFaceAnchorInspectorWritesProjectAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "face anchor inspector write setup derives valid topology");
    Check(game::FindSectorEditorAuthoringFaceAnchorIdForTopologySector(authoringGraph, documentState.derivation.authoringDerivation, 200) == 200,
          "derived sector maps back to face anchor");

    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(game::MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  "Updated authoring face anchor properties",
                  [](game::SectorAuthoringFaceAnchor& anchor) {
                      anchor.name = "edited-room";
                      anchor.floorZ = -8.0f;
                      anchor.ceilingZ = 40.0f;
                      anchor.ceilingSky = true;
                      anchor.ambientColor = Color{12, 34, 56, 255};
                      anchor.ambientIntensity = 0.375f;
                      anchor.floorUv.scale = Vector2{2.0f, 3.0f};
                      return true;
                  }),
          "face anchor inspector write helper accepts mapped derived sector");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(anchor != nullptr && anchor->name == "edited-room" && anchor->ceilingSky,
          "face anchor inspector write updates authoring anchor source");
    Check(sector != nullptr && sector->name == "edited-room"
                  && sector->floorZ == -8.0f
                  && sector->ceilingZ == 40.0f
                  && sector->ceilingSky
                  && sector->ambientColor.g == 34
                  && sector->ambientIntensity == 0.375f
                  && sector->floorUv.scale.x == 2.0f,
          "face anchor inspector write projects to derived sector after derivation");
    Check(documentState.lifecycle.topologyDocumentDirty, "face anchor inspector write marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "face anchor inspector write marks unsaved changes");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "face anchor inspector write leaves successful derivation current");
    Check(!documentState.derivation.authoringDerivedTopologyStale,
          "face anchor inspector write clears stale flag after successful derivation");
    Check(!state.topologyRenderCache.valid,
          "face anchor inspector write invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "face anchor inspector write bumps topology render revision");
}

void TestEditorAuthoringFaceAnchorInspectorWriteDoesNotDirectlyMutateDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed face anchor write setup derives valid topology");

    const game::SectorTopologyMap lastValid = documentState.map.topologyMap;
    AddFaceAnchor(authoringGraph, 201, 128, 128, "unresolved-room");

    Check(!game::MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  "Updated authoring face anchor while graph is invalid",
                  [](game::SectorAuthoringFaceAnchor& anchor) {
                      anchor.floorZ = -16.0f;
                      return true;
                  }),
          "face anchor inspector write reports failed derivation when graph is invalid");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    const game::SectorTopologySector* lastValidSector =
            game::FindSectorTopologySector(lastValid, 200);
    Check(anchor != nullptr && anchor->floorZ == -16.0f,
          "failed face anchor inspector write keeps authoring anchor edit");
    Check(sector != nullptr && lastValidSector != nullptr
                  && sector->floorZ == lastValidSector->floorZ,
          "failed face anchor inspector write does not directly mutate derived topology");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed face anchor inspector write records invalid last-valid state");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "failed face anchor inspector write leaves derived topology stale");
}

void TestEditorAuthoringSideMaterialInspectorWritesProjectAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "side material inspector write setup derives valid topology");

    const game::SectorTopologySideDef* initialSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(initialSideDef != nullptr, "derived sidedef maps back to authoring side");
    if (initialSideDef == nullptr) {
        return;
    }

    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(game::MutateSectorEditorAuthoringSideForTopologySideDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  initialSideDef->id,
                  "Updated authoring side material",
                  [](game::SectorAuthoringLineSide& side) {
                      side.wall = WallPart("edited_wall", 2.0f, 3.0f, 4.0f, 5.0f);
                      side.lower = WallPart("edited_lower", 3.0f, 4.0f, 5.0f, 6.0f);
                      side.upper = WallPart("edited_upper", 4.0f, 5.0f, 6.0f, 7.0f);
                      side.middle = WallPart("edited_middle", 5.0f, 6.0f, 7.0f, 8.0f);
                      return true;
                  }),
          "side material inspector write helper accepts mapped derived sidedef");

    const game::SectorAuthoringLineSide* authoringSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(authoringSide != nullptr && authoringSide->wall.textureId == "edited_wall",
          "side material inspector write creates or updates authoring side source");
    Check(projectedSideDef != nullptr
                  && projectedSideDef->wall.textureId == "edited_wall"
                  && projectedSideDef->lower.textureId == "edited_lower"
                  && projectedSideDef->upper.textureId == "edited_upper"
                  && projectedSideDef->middle.textureId == "edited_middle",
          "side material inspector write projects to derived sidedef after derivation");
    Check(documentState.lifecycle.topologyDocumentDirty, "side material inspector write marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "side material inspector write marks unsaved changes");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "side material inspector write leaves successful derivation current");
    Check(!documentState.derivation.authoringDerivedTopologyStale,
          "side material inspector write clears stale flag after successful derivation");
    Check(!state.topologyRenderCache.valid,
          "side material inspector write invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "side material inspector write bumps topology render revision");
}

void TestEditorAuthoringSideMaterialInspectorWritesProjectToSplitDerivedSideDefs()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "split side material inspector write setup derives valid topology");

    int selectedSideDefId = -1;
    int projectedBeforeCount = 0;
    for (const game::SectorAuthoringDerivedSideMapping& mapping : documentState.derivation.authoringDerivation.mapping.sides) {
        if (mapping.authoringLineId == 14
                && mapping.authoringSide == game::SectorTopologySideKind::Front) {
            selectedSideDefId = mapping.topologySideDefId;
            ++projectedBeforeCount;
        }
    }
    Check(selectedSideDefId > 0, "split source side maps to at least one derived sidedef");
    Check(projectedBeforeCount == 2, "split source side maps to both child sidedefs before edit");

    Check(game::MutateSectorEditorAuthoringSideForTopologySideDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectedSideDefId,
                  "Updated split authoring side material",
                  [](game::SectorAuthoringLineSide& side) {
                      side.wall = WallPart("split_edited_wall", 2.0f, 3.0f, 4.0f, 5.0f);
                      return true;
                  }),
          "split side material inspector write helper accepts one child derived sidedef");

    int projectedAfterCount = 0;
    for (const game::SectorAuthoringDerivedSideMapping& mapping : documentState.derivation.authoringDerivation.mapping.sides) {
        if (mapping.authoringLineId != 14
                || mapping.authoringSide != game::SectorTopologySideKind::Front) {
            continue;
        }
        const game::SectorTopologySideDef* sideDef =
                game::FindSectorTopologySideDef(documentState.map.topologyMap, mapping.topologySideDefId);
        Check(sideDef != nullptr && sideDef->wall.textureId == "split_edited_wall",
              "split side material inspector write projects to each child derived sidedef");
        ++projectedAfterCount;
    }
    Check(projectedAfterCount == 2, "split side material inspector write keeps both child side mappings");
}

void TestEditorAuthoringSideMaterialInspectorWriteDoesNotDirectlyMutateDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed side material write setup derives valid topology");

    const game::SectorTopologySideDef* initialSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(initialSideDef != nullptr, "failed side material write setup has mapped side");
    if (initialSideDef == nullptr) {
        return;
    }

    const int sideDefId = initialSideDef->id;
    const game::SectorTopologyMap lastValid = documentState.map.topologyMap;
    AddFaceAnchor(authoringGraph, 201, 128, 128, "unresolved-room");

    Check(!game::MutateSectorEditorAuthoringSideForTopologySideDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  sideDefId,
                  "Updated authoring side while graph is invalid",
                  [](game::SectorAuthoringLineSide& side) {
                      side.wall.textureId = "invalid_graph_wall";
                      return true;
                  }),
          "side material inspector write reports failed derivation when graph is invalid");

    const game::SectorAuthoringLineSide* authoringSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* sideDef =
            game::FindSectorTopologySideDef(documentState.map.topologyMap, sideDefId);
    const game::SectorTopologySideDef* lastValidSideDef =
            game::FindSectorTopologySideDef(lastValid, sideDefId);
    Check(authoringSide != nullptr && authoringSide->wall.textureId == "invalid_graph_wall",
          "failed side material inspector write keeps authoring side edit");
    Check(sideDef != nullptr && lastValidSideDef != nullptr
                  && sideDef->wall.textureId == lastValidSideDef->wall.textureId,
          "failed side material inspector write does not directly mutate derived sidedef");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed side material inspector write records invalid last-valid state");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "failed side material inspector write leaves derived topology stale");
}

void TestEditorLightEditingServiceStaticEditMarksDirtyAndInvalidatesCache()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    state.topologyRenderCache.valid = true;
    state.topologyRenderRevision = 41;
    state.topologyRenderWarning = "keep warning";
    game::SectorEditorUiState uiState;
    std::string statusText = "old";
    game::SectorEditorLightEditingService service =
            MakeLightEditingService(state, documentState, documentState.map.topologyMap, uiState, statusText);

    game::SectorTopologyStaticPointLight light;
    light.id = 7;
    light.radius = 4.0f;

    Check(service.SetStaticLightRadius(light, 6.0f),
          "light service static radius edit succeeds");
    Check(Near(light.radius, 6.0f),
          "light service static radius edit changes field");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "light service static edit marks document dirty and unsaved");
    Check(!state.topologyRenderCache.valid,
          "light service static edit invalidates topology render cache");
    Check(state.topologyRenderRevision == 42,
          "light service static edit increments topology render revision");
    Check(state.topologyRenderWarning == "keep warning",
          "light service static edit preserves topology render warning like old dirty path");
    Check(statusText == "Updated static light 7",
          "light service static edit updates status");
}

void TestEditorLightEditingServiceDynamicEditPreservesDirtyBehavior()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    state.topologyRenderCache.valid = true;
    state.topologyRenderRevision = 9;
    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorLightEditingService service =
            MakeLightEditingService(state, documentState, documentState.map.topologyMap, uiState, statusText);

    game::SectorTopologyDynamicPointLight light;
    light.id = 12;
    light.enabled = true;

    Check(service.SetDynamicLightEnabled(light, false),
          "light service dynamic enabled edit succeeds");
    Check(!light.enabled,
          "light service dynamic enabled edit changes field");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "light service dynamic edit marks document dirty and unsaved");
    Check(!state.topologyRenderCache.valid,
          "light service dynamic edit invalidates topology render cache");
    Check(state.topologyRenderRevision == 10,
          "light service dynamic edit increments topology render revision");
    Check(statusText == "Updated dynamic light 12 enabled",
          "light service dynamic edit updates status");
}

void TestEditorLightEditingServiceNoOpDoesNotDirtyOrUpdateStatus()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    state.topologyRenderCache.valid = true;
    state.topologyRenderRevision = 5;
    game::SectorEditorUiState uiState;
    std::string statusText = "unchanged";
    game::SectorEditorLightEditingService service =
            MakeLightEditingService(state, documentState, documentState.map.topologyMap, uiState, statusText);

    game::SectorTopologyDynamicSpotLight light;
    light.id = 33;
    light.shadowBias = game::DynamicSpotLightMinShadowBias;

    Check(!service.SetDynamicSpotLightShadowBias(light, -1.0f),
          "light service clamp-first no-op returns false");
    Check(!documentState.lifecycle.topologyDocumentDirty && !documentState.lifecycle.hasUnsavedChanges,
          "light service no-op does not mark document dirty");
    Check(state.topologyRenderCache.valid,
          "light service no-op does not invalidate topology render cache");
    Check(state.topologyRenderRevision == 5,
          "light service no-op does not increment topology render revision");
    Check(statusText == "unchanged",
          "light service no-op does not update status");
}

void TestEditorMaterialEditingServiceSideDefBaseUvWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service sidedef base UV setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service sidedef base UV setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    bool previewRebuildRequested = false;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText, &previewRebuildRequested);

    Check(service.ApplyInspectorSideDefUvValue(
                  SideDefMaterialTarget(*sideDef),
                  game::TopologyMaterialLayer::Base,
                  0,
                  2.25f,
                  assets),
          "service sidedef base UV apply succeeds");

    const game::SectorAuthoringLineSide* editedSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(editedSide != nullptr && Near(editedSide->wall.uv.scale.x, 2.25f),
          "service sidedef base UV apply writes authoring side");
    Check(projectedSideDef != nullptr && Near(projectedSideDef->wall.uv.scale.x, 2.25f),
          "service sidedef base UV apply refreshes derived topology");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "service sidedef base UV apply marks document dirty");
    Check(!previewRebuildRequested,
          "service sidedef base UV apply does not rebuild preview for non-middle wall part");
}

void TestEditorMaterialEditingServiceNoAuthoringMaterialEditFailsWithoutMutatingTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap = MakeSingleSectorSquareMap();
    game::SectorTopologySector* sector = game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(sector != nullptr, "invalid no-authoring material edit setup finds sector");
    if (sector == nullptr) {
        return;
    }
    sector->floorDecal.textureId = "mark";
    sector->floorDecal.opacity = 0.25f;

    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);
    const game::TopologySurfaceEditTarget target{
            game::TopologySurfaceEditTargetKind::SectorFloor,
            200,
            -1,
            -1,
            game::SectorTopologySideKind::Front};

    Check(!service.ApplyDecalOpacity(target, 0.75f, nullptr),
          "invalid no-authoring material edit fails");
    const game::SectorTopologySector* afterSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(statusText.find("authoring data is required") != std::string::npos,
          "invalid no-authoring material edit reports authoring requirement");
    Check(afterSector != nullptr && afterSector->floorDecal.opacity == 0.25f,
          "invalid no-authoring material edit does not mutate derived topology");
}

void TestEditorMaterialEditingServiceSideDefDecalUvWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "wall";
    side.wall.decal.textureId = "poster";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service sidedef decal UV setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service sidedef decal UV setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ApplyInspectorSideDefUvValue(
                  SideDefMaterialTarget(*sideDef),
                  game::TopologyMaterialLayer::Decal,
                  2,
                  7.5f,
                  assets),
          "service sidedef decal UV apply succeeds");

    const game::SectorAuthoringLineSide* editedSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(editedSide != nullptr && Near(editedSide->wall.decal.uv.offset.x, 7.5f),
          "service sidedef decal UV apply writes authoring side");
    Check(projectedSideDef != nullptr && Near(projectedSideDef->wall.decal.uv.offset.x, 7.5f),
          "service sidedef decal UV apply refreshes derived topology");
}

void TestEditorMaterialEditingServiceSideDefBaseUvResetWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("wall", 2.0f, 3.0f, 4.0f, 5.0f);
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service sidedef base UV reset setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service sidedef base UV reset setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);
    game::MaterialEditingUiState& materialUiState = TestMaterialEditingUiState();
    materialUiState.topologySideDefUvInputs[0].buffer[0] = 'x';
    materialUiState.topologySideDefUvInputs[0].editing = true;

    Check(service.ResetInspectorSideDefUv(
                  SideDefMaterialTarget(*sideDef),
                  game::TopologyMaterialLayer::Base,
                  assets),
          "service sidedef base UV reset succeeds");

    const game::SectorAuthoringLineSide* editedSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(editedSide != nullptr
                  && Near(editedSide->wall.uv.scale.x, 1.0f)
                  && Near(editedSide->wall.uv.scale.y, 1.0f)
                  && Near(editedSide->wall.uv.offset.x, 0.0f)
                  && Near(editedSide->wall.uv.offset.y, 0.0f),
          "service sidedef base UV reset writes authoring defaults");
    Check(projectedSideDef != nullptr
                  && Near(projectedSideDef->wall.uv.scale.x, 1.0f)
                  && Near(projectedSideDef->wall.uv.offset.x, 0.0f),
          "service sidedef base UV reset refreshes derived topology");
    Check(materialUiState.topologySideDefUvInputs[0].buffer[0] == '\0'
                  && !materialUiState.topologySideDefUvInputs[0].editing,
          "service sidedef base UV reset clears inspector input buffers");
}

void TestEditorMaterialEditingServiceSideDefDecalUvResetWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "wall";
    side.wall.decal.textureId = "poster";
    side.wall.decal.uv.scale = Vector2{2.0f, 3.0f};
    side.wall.decal.uv.offset = Vector2{4.0f, 5.0f};
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service sidedef decal UV reset setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service sidedef decal UV reset setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ResetInspectorSideDefUv(
                  SideDefMaterialTarget(*sideDef),
                  game::TopologyMaterialLayer::Decal,
                  assets),
          "service sidedef decal UV reset succeeds");

    const game::SectorAuthoringLineSide* editedSide = game::FindSectorAuthoringLineSide(
            authoringGraph,
            side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(editedSide != nullptr
                  && Near(editedSide->wall.decal.uv.scale.x, 1.0f)
                  && Near(editedSide->wall.decal.uv.offset.x, 0.0f),
          "service sidedef decal UV reset writes authoring defaults");
    Check(projectedSideDef != nullptr
                  && Near(projectedSideDef->wall.decal.uv.scale.x, 1.0f)
                  && Near(projectedSideDef->wall.decal.uv.offset.x, 0.0f),
          "service sidedef decal UV reset refreshes derived topology");
}

void TestEditorMaterialEditingServiceSideDefUvMissingMappingDoesNotMutateTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service sidedef missing mapping setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service sidedef missing mapping setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }
    const game::TopologySurfaceEditTarget target = SideDefMaterialTarget(*sideDef);
    const game::SectorTopologyMap beforeTopology = documentState.map.topologyMap;
    documentState.derivation.authoringDerivation.mapping.sides.clear();

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(!service.ApplyInspectorSideDefUvValue(
                  target,
                  game::TopologyMaterialLayer::Base,
                  0,
                  2.0f,
                  assets),
          "service sidedef UV apply fails without authoring side mapping");

    const game::SectorTopologySideDef* afterSideDef =
            game::FindSectorTopologySideDef(documentState.map.topologyMap, target.sideDefId);
    const game::SectorTopologySideDef* beforeSideDef =
            game::FindSectorTopologySideDef(beforeTopology, target.sideDefId);
    Check(statusText.find("not mapped to an authoring line") != std::string::npos,
          "service sidedef UV missing mapping reports clear status");
    Check(afterSideDef != nullptr
                  && beforeSideDef != nullptr
                  && Near(afterSideDef->wall.uv.scale.x, beforeSideDef->wall.uv.scale.x)
                  && Near(afterSideDef->wall.uv.offset.x, beforeSideDef->wall.uv.offset.x),
          "service sidedef UV missing mapping does not mutate derived topology");
}

void TestEditorMaterialEditingServiceSideDecalOpacityWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "wall";
    side.wall.decal.textureId = "poster";
    side.wall.decal.opacity = 0.25f;
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service side decal opacity setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service side decal opacity setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ApplyDecalOpacity(SideDefMaterialTarget(*sideDef), 0.75f, nullptr),
          "service side decal opacity apply succeeds");

    const game::SectorAuthoringLineSide* editedSide =
            game::FindSectorAuthoringLineSide(authoringGraph, side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(editedSide != nullptr && Near(editedSide->wall.decal.opacity, 0.75f),
          "service side decal opacity writes authoring side");
    Check(projectedSideDef != nullptr && Near(projectedSideDef->wall.decal.opacity, 0.75f),
          "service side decal opacity refreshes derived topology");
}

void TestEditorMaterialEditingServiceFlatDecalFitWritesThroughFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    Check(anchor != nullptr, "service flat decal fit setup has anchor");
    if (anchor == nullptr) {
        return;
    }
    anchor->floorDecal.textureId = "floor_mark";
    anchor->floorDecal.uv.scale = Vector2{2.0f, 2.0f};
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service flat decal fit setup derives valid topology");

    game::TopologySurfaceEditTarget target;
    target.kind = game::TopologySurfaceEditTargetKind::SectorFloor;
    target.sectorId = 200;
    TestPreviewSelectionState().selectedSurface3D.kind = game::SectorSurfaceKind::Floor;
    TestPreviewSelectionState().selectedSurface3D.topologySectorId = 200;

    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.FitSelectedFlatDecal(target, nullptr),
          "service flat decal fit succeeds");

    const game::SectorAuthoringFaceAnchor* editedAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* projectedSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(editedAnchor != nullptr
                  && (!Near(editedAnchor->floorDecal.uv.scale.x, 2.0f)
                          || !Near(editedAnchor->floorDecal.uv.scale.y, 2.0f)),
          "service flat decal fit writes authoring face anchor UV");
    Check(projectedSector != nullptr
                  && Near(projectedSector->floorDecal.uv.scale.x, editedAnchor->floorDecal.uv.scale.x)
                  && Near(projectedSector->floorDecal.uv.scale.y, editedAnchor->floorDecal.uv.scale.y),
          "service flat decal fit refreshes derived topology");
}

void TestEditorMaterialEditingServiceDerivedSidePickerWritesAuthoringSideDirectly()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_wall", game::SectorTextureDefinition{"old_wall", "old_wall.png"});
    documentState.map.topologyMap.texturesById.emplace("new_wall", game::SectorTextureDefinition{"new_wall", "new_wall.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "old_wall";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "service derived side picker setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "service derived side picker setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);
    Check(service.OpenMaterialPickerForDerivedSideDef(
                  sideDef->id,
                  game::TopologyWallPart::Wall,
                  game::TopologyMaterialLayer::Base),
          "service derived side picker opens through material service");
    SelectTextureInPicker(state.texturePicker, "new_wall");
    const game::SectorEditorTexturePickerApplyResult result =
            service.ApplyTexturePickerSelection(nullptr);

    const game::SectorAuthoringLineSide* editedSide =
            game::FindSectorAuthoringLineSide(authoringGraph, side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(result.changed, "service derived side picker reports texture change");
    Check(editedSide != nullptr && editedSide->wall.textureId == "new_wall",
          "service derived side picker writes authoring side");
    Check(projectedSideDef != nullptr && projectedSideDef->wall.textureId == "new_wall",
          "service derived side picker refreshes derived topology");
    Check(!state.texturePicker.open, "service derived side picker closes after apply");
}

void TestEditorAuthoringLineFlagInspectorWritesProjectAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "line flag inspector write setup derives valid topology");

    const game::SectorTopologyLineDef* initialLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(initialLineDef != nullptr, "derived linedef maps back to authoring line");
    if (initialLineDef == nullptr) {
        return;
    }

    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(game::MutateSectorEditorAuthoringLineForTopologyLineDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  initialLineDef->id,
                  "Updated authoring line flags",
                  [](game::SectorAuthoringLine& line) {
                      line.flags.blocksPlayer = true;
                      return true;
                  }),
          "line flag inspector write helper accepts mapped derived linedef");

    const game::SectorAuthoringLine* authoringLine =
            game::FindSectorAuthoringLine(authoringGraph, 11);
    const game::SectorTopologyLineDef* projectedLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(authoringLine != nullptr && authoringLine->flags.blocksPlayer,
          "line flag inspector write updates authoring line source");
    Check(projectedLineDef != nullptr && projectedLineDef->flags.blocksPlayer,
          "line flag inspector write projects to derived linedef after derivation");
    Check(documentState.lifecycle.topologyDocumentDirty, "line flag inspector write marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "line flag inspector write marks unsaved changes");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "line flag inspector write leaves successful derivation current");
    Check(!documentState.derivation.authoringDerivedTopologyStale,
          "line flag inspector write clears stale flag after successful derivation");
    Check(!state.topologyRenderCache.valid,
          "line flag inspector write invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "line flag inspector write bumps topology render revision");
}

void TestEditorNoAuthoringBlocksPlayerEditFailsWithoutMutatingTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    const game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(derivation.success, "invalid no-authoring blocksPlayer setup derives topology");
    documentState.map.topologyMap = derivation.topology;

    const game::SectorTopologyLineDef* initialLineDef =
            FindDerivedLineDefForAuthoringLine(derivation, 11);
    Check(initialLineDef != nullptr, "invalid no-authoring blocksPlayer setup finds portal");
    if (initialLineDef == nullptr) {
        return;
    }

    std::string status;
    Check(!game::SetSectorEditorAuthoringLineDefBlocksPlayer(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  initialLineDef->id,
                  true,
                  &status),
          "invalid no-authoring blocksPlayer edit fails");
    const game::SectorTopologyLineDef* afterLineDef =
            game::FindSectorTopologyLineDef(documentState.map.topologyMap, initialLineDef->id);
    Check(status.find("authoring data is required") != std::string::npos,
          "invalid no-authoring blocksPlayer edit reports authoring requirement");
    Check(afterLineDef != nullptr && !afterLineDef->flags.blocksPlayer,
          "invalid no-authoring blocksPlayer edit does not mutate derived topology");
}

void TestEditorAuthoringBlocksPlayerEditWritesAuthoringLine()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring blocksPlayer edit setup derives valid topology");

    const game::SectorTopologyLineDef* initialLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(initialLineDef != nullptr, "authoring blocksPlayer edit setup finds portal");
    if (initialLineDef == nullptr) {
        return;
    }

    std::string status;
    Check(game::SetSectorEditorAuthoringLineDefBlocksPlayer(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  initialLineDef->id,
                  true,
                  &status),
          "authoring blocksPlayer edit succeeds");
    const game::SectorAuthoringLine* authoringLine =
            game::FindSectorAuthoringLine(authoringGraph, 11);
    const game::SectorTopologyLineDef* projectedLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(authoringLine != nullptr && authoringLine->flags.blocksPlayer,
          "authoring blocksPlayer edit mutates authoring line");
    Check(projectedLineDef != nullptr && projectedLineDef->flags.blocksPlayer,
          "authoring blocksPlayer edit refreshes derived topology");
    Check(status.find("authoring portal") != std::string::npos,
          "authoring blocksPlayer edit reports authoring status");
}

void TestEditorNoAuthoringSectorPropertyEditFailsWithoutMutatingTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap = MakeSingleSectorSquareMap();
    const game::SectorTopologySector* beforeSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(beforeSector != nullptr, "invalid no-authoring sector property setup finds sector");
    if (beforeSector == nullptr) {
        return;
    }
    const float originalFloorZ = beforeSector->floorZ;

    Check(!game::MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  "Updated sector height",
                  [](game::SectorAuthoringFaceAnchor& anchor) {
                      anchor.floorZ = 8.0f;
                      return true;
                  }),
          "invalid no-authoring sector property edit fails");
    const game::SectorTopologySector* afterSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(afterSector != nullptr && afterSector->floorZ == originalFloorZ,
          "invalid no-authoring sector property edit does not mutate derived topology");
}

void TestEditorAuthoringLineFlagInspectorWritesProjectToSplitDerivedLineDefs()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "split line flag inspector write setup derives valid topology");

    int selectedLineDefId = -1;
    int projectedBeforeCount = 0;
    for (const game::SectorAuthoringDerivedLineMapping& mapping : documentState.derivation.authoringDerivation.mapping.lines) {
        if (mapping.authoringLineId == 14) {
            selectedLineDefId = mapping.topologyLineDefId;
            ++projectedBeforeCount;
        }
    }
    Check(selectedLineDefId > 0, "split source line maps to at least one derived linedef");
    Check(projectedBeforeCount == 2, "split source line maps to both child linedefs before edit");

    Check(game::MutateSectorEditorAuthoringLineForTopologyLineDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectedLineDefId,
                  "Updated split authoring line flags",
                  [](game::SectorAuthoringLine& line) {
                      line.flags.blocksPlayer = true;
                      return true;
                  }),
          "split line flag inspector write helper accepts one child derived linedef");

    int projectedAfterCount = 0;
    for (const game::SectorAuthoringDerivedLineMapping& mapping : documentState.derivation.authoringDerivation.mapping.lines) {
        if (mapping.authoringLineId != 14) {
            continue;
        }
        const game::SectorTopologyLineDef* lineDef =
                game::FindSectorTopologyLineDef(documentState.map.topologyMap, mapping.topologyLineDefId);
        Check(lineDef != nullptr && lineDef->flags.blocksPlayer,
              "split line flag inspector write projects to each child derived linedef");
        ++projectedAfterCount;
    }
    Check(projectedAfterCount == 2, "split line flag inspector write keeps both child line mappings");
}

void TestEditorAuthoringLineFlagInspectorWriteDoesNotDirectlyMutateDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed line flag write setup derives valid topology");

    const game::SectorTopologyLineDef* initialLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(initialLineDef != nullptr, "failed line flag write setup has mapped line");
    if (initialLineDef == nullptr) {
        return;
    }

    const int lineDefId = initialLineDef->id;
    const game::SectorTopologyMap lastValid = documentState.map.topologyMap;
    AddFaceAnchor(authoringGraph, 201, 256, 256, "unresolved-room");

    Check(!game::MutateSectorEditorAuthoringLineForTopologyLineDef(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  lineDefId,
                  "Updated authoring line while graph is invalid",
                  [](game::SectorAuthoringLine& line) {
                      line.flags.blocksPlayer = true;
                      return true;
                  }),
          "line flag inspector write reports failed derivation when graph is invalid");

    const game::SectorAuthoringLine* authoringLine =
            game::FindSectorAuthoringLine(authoringGraph, 11);
    const game::SectorTopologyLineDef* lineDef =
            game::FindSectorTopologyLineDef(documentState.map.topologyMap, lineDefId);
    const game::SectorTopologyLineDef* lastValidLineDef =
            game::FindSectorTopologyLineDef(lastValid, lineDefId);
    Check(authoringLine != nullptr && authoringLine->flags.blocksPlayer,
          "failed line flag inspector write keeps authoring line edit");
    Check(lineDef != nullptr && lastValidLineDef != nullptr
                  && lineDef->flags.blocksPlayer == lastValidLineDef->flags.blocksPlayer,
          "failed line flag inspector write does not directly mutate derived linedef");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed line flag inspector write records invalid last-valid state");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "failed line flag inspector write leaves derived topology stale");
}

void TestEditorSelectedAuthoringLineInspectorTargetDoesNotNeedTopologySelection()
{
    game::SectorAuthoringGraph graph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    Check(game::SelectSectorEditorAuthoringLine(graph, selectionState, 10),
          "selected authoring line inspector target setup selects line");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologyLineDefId == -1
                  && selectionState.selectedTopologySideDefId == -1,
          "selected authoring line inspector target does not require topology selection");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && selectionState.selectedAuthoring.lineId == 10,
          "selected authoring line inspector target stores authoring line ID");
    Check(game::IsSectorAuthoringSelectionTargetValid(graph, selectionState.selectedAuthoring),
          "selected authoring line inspector target validates against authoring graph");
}

void TestEditorMappedTopologySideSelectionUsesAuthoringInspectorTarget()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "mapped side inspector target setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "mapped side inspector target setup finds derived sidedef");
    if (sideDef == nullptr) {
        return;
    }

    selectionState.topologySelectionKind = game::TopologySelectionKind::SideDef;
    selectionState.selectedTopologySideDefId = sideDef->id;
    selectionState.selectedTopologyLineDefId = sideDef->lineDefId;
    selectionState.selectedTopologySideKind = sideDef->side;

    const game::SectorEditorInspectorTarget target =
            ResolveEditorInspectorTarget(state, documentState, authoringGraph, selectionState);
    Check(target.kind == game::SectorEditorInspectorTargetKind::AuthoringLine,
          "mapped topology side selection resolves to authoring inspector");
    Check(target.lineId == 10
                  && target.side.lineId == 10
                  && target.side.side == game::SectorTopologySideKind::Front,
          "mapped topology side inspector target keeps authoring side identity");
}

void TestEditorMappedTopologySectorSelectionUsesAuthoringInspectorTarget()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "mapped sector inspector target setup derives valid topology");

    selectionState.topologySelectionKind = game::TopologySelectionKind::Sector;
    selectionState.selectedTopologySectorId = 200;

    const game::SectorEditorInspectorTarget target =
            ResolveEditorInspectorTarget(state, documentState, authoringGraph, selectionState);
    Check(target.kind == game::SectorEditorInspectorTargetKind::AuthoringFaceAnchor,
          "mapped topology sector selection resolves to authoring face inspector");
    Check(target.faceAnchorId == 200,
          "mapped topology sector inspector target keeps face anchor ID");
}

void TestEditorMappedTopologyMissingOrStaleMappingDoesNotUseLegacyInspectorTarget()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale inspector target setup derives valid topology");

    selectionState.topologySelectionKind = game::TopologySelectionKind::Sector;
    selectionState.selectedTopologySectorId = 200;
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "stale mapping for inspector target test");

    game::SectorEditorInspectorTarget target =
            ResolveEditorInspectorTarget(state, documentState, authoringGraph, selectionState);
    Check(target.kind == game::SectorEditorInspectorTargetKind::AuthoringUnavailable,
          "stale mapped topology selection resolves to unavailable authoring target");
    Check(target.status.find("not current") != std::string::npos,
          "stale mapped topology selection reports stale mapping");

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "missing mapping inspector target setup rederives current topology");
    documentState.derivation.authoringDerivation.mapping.sectors.clear();
    target = ResolveEditorInspectorTarget(state, documentState, authoringGraph, selectionState);
    Check(target.kind == game::SectorEditorInspectorTargetKind::AuthoringUnavailable,
          "missing mapped topology selection resolves to unavailable authoring target");
    Check(target.status.find("no face anchor mapping") != std::string::npos,
          "missing mapped topology selection reports missing mapping");
}

void TestEditorAuthoringSideClearMiddleAndDecalProjectsAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.middle.textureId = "bars";
    side.wall.decal.textureId = "poster";
    side.wall.decal.opacity = 0.5f;
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring side clear setup derives valid topology");

    Check(game::MutateSectorEditorAuthoringSideById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  side.id,
                  "Cleared authoring side optional materials",
                  [](game::SectorAuthoringLineSide& editedSide) {
                      editedSide.middle = game::SectorTopologyWallPartSettings{};
                      game::ResetDecalLayer(editedSide.wall.decal);
                      return true;
                  }),
          "authoring side clear optional material mutation succeeds");

    const game::SectorAuthoringLineSide* authoringSide =
            game::FindSectorAuthoringLineSide(authoringGraph, side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(authoringSide != nullptr
                  && authoringSide->middle.textureId.empty()
                  && authoringSide->wall.decal.textureId.empty(),
          "authoring side clear updates authoring metadata");
    Check(projectedSideDef != nullptr
                  && projectedSideDef->middle.textureId.empty()
                  && projectedSideDef->wall.decal.textureId.empty(),
          "authoring side clear projects to derived sidedef");
}

void TestEditorAuthoringFaceClearDecalProjectsAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    Check(anchor != nullptr, "authoring face clear setup has anchor");
    if (anchor == nullptr) {
        return;
    }
    anchor->floorDecal.textureId = "floor_mark";
    anchor->floorDecal.opacity = 0.25f;
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring face clear setup derives valid topology");

    Check(game::MutateSectorEditorAuthoringFaceAnchorById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  "Cleared authoring face decal",
                  [](game::SectorAuthoringFaceAnchor& editedAnchor) {
                      game::ResetDecalLayer(editedAnchor.floorDecal);
                      return true;
                  }),
          "authoring face clear decal mutation succeeds");

    const game::SectorAuthoringFaceAnchor* updatedAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* projectedSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(updatedAnchor != nullptr && updatedAnchor->floorDecal.textureId.empty(),
          "authoring face clear decal updates face anchor");
    Check(projectedSector != nullptr && projectedSector->floorDecal.textureId.empty(),
          "authoring face clear decal projects to derived sector");
}

void TestEditorAuthoringSideDecalPropertyEditProjectsAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.decal.textureId = "poster";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring side decal property setup derives valid topology");

    Check(game::MutateSectorEditorAuthoringSideById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  side.id,
                  "Updated authoring side decal opacity",
                  [](game::SectorAuthoringLineSide& editedSide) {
                      editedSide.wall.decal.opacity = 0.35f;
                      editedSide.wall.decal.tint = Vector3{0.2f, 0.4f, 0.6f};
                      return true;
                  }),
          "authoring side decal property mutation succeeds");

    const game::SectorAuthoringLineSide* authoringSide =
            game::FindSectorAuthoringLineSide(authoringGraph, side.id);
    const game::SectorTopologySideDef* projectedSideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(authoringSide != nullptr
                  && authoringSide->wall.decal.opacity == 0.35f
                  && Near(authoringSide->wall.decal.tint, Vector3{0.2f, 0.4f, 0.6f}),
          "authoring side decal property edit updates authoring side");
    Check(projectedSideDef != nullptr
                  && projectedSideDef->wall.decal.opacity == 0.35f
                  && Near(projectedSideDef->wall.decal.tint, Vector3{0.2f, 0.4f, 0.6f}),
          "authoring side decal property edit projects to derived sidedef");
}

void TestEditorAuthoringFaceDecalPropertyEditProjectsAfterDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    Check(anchor != nullptr, "authoring face decal property setup has anchor");
    if (anchor == nullptr) {
        return;
    }
    anchor->floorDecal.textureId = "floor_mark";
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring face decal property setup derives valid topology");

    Check(game::MutateSectorEditorAuthoringFaceAnchorById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  "Updated authoring face decal opacity",
                  [](game::SectorAuthoringFaceAnchor& editedAnchor) {
                      editedAnchor.floorDecal.opacity = 0.45f;
                      editedAnchor.floorDecal.tint = Vector3{0.7f, 0.5f, 0.3f};
                      return true;
                  }),
          "authoring face decal property mutation succeeds");

    const game::SectorAuthoringFaceAnchor* updatedAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* projectedSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(updatedAnchor != nullptr
                  && updatedAnchor->floorDecal.opacity == 0.45f
                  && Near(updatedAnchor->floorDecal.tint, Vector3{0.7f, 0.5f, 0.3f}),
          "authoring face decal property edit updates face anchor");
    Check(projectedSector != nullptr
                  && projectedSector->floorDecal.opacity == 0.45f
                  && Near(projectedSector->floorDecal.tint, Vector3{0.7f, 0.5f, 0.3f}),
          "authoring face decal property edit projects to derived sector");
}

void TestEditorAuthoringFaceDefaultDecalTexturePickerWritesThroughAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("default_poster", game::SectorTextureDefinition{"default_poster", "default_poster.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring face default decal picker setup derives valid topology");

    Check(game::OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::DefaultWall,
                  game::TopologyMaterialLayer::Decal),
          "authoring face default wall decal picker opens");
    SelectTextureInPicker(state.texturePicker, "default_poster");
    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(result.changed, "authoring face default wall decal picker reports change");
    Check(anchor != nullptr && anchor->defaultWall.decal.textureId == "default_poster",
          "authoring face default wall decal picker writes face anchor");
    Check(sector != nullptr && sector->defaultWall.decal.textureId == "default_poster",
          "authoring face default wall decal picker projects to derived sector defaults");
}

void TestEditorSurface3DMappedPanelLabelMentionsAuthoringAndDerivedIds()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D label setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "3D label setup finds derived side");
    if (sideDef == nullptr) {
        return;
    }

    const game::SectorSurfaceRef wallSurface{
            game::SectorSurfaceKind::Wall,
            sideDef->sectorId,
            sideDef->lineDefId,
            sideDef->id,
            sideDef->side};
    const game::TopologySurfaceEditTarget wallTarget{
            game::TopologySurfaceEditTargetKind::SideDefWall,
            sideDef->sectorId,
            sideDef->lineDefId,
            sideDef->id,
            sideDef->side};
    const std::string wallLabel =
            BuildEditorSurface3DTargetLabel(state, documentState, authoringGraph, wallSurface, wallTarget);
    Check(wallLabel.find("Authoring Side") != std::string::npos,
          "mapped 3D wall label identifies authoring side");
    Check(wallLabel.find("derived sideDef") != std::string::npos
                  && wallLabel.find("line") != std::string::npos,
          "mapped 3D wall label keeps derived IDs");

    const game::SectorSurfaceRef floorSurface{
            game::SectorSurfaceKind::Floor,
            200,
            -1,
            -1,
            game::SectorTopologySideKind::Front};
    const game::TopologySurfaceEditTarget floorTarget{
            game::TopologySurfaceEditTargetKind::SectorFloor,
            200,
            -1,
            -1,
            game::SectorTopologySideKind::Front};
    const std::string floorLabel =
            BuildEditorSurface3DTargetLabel(state, documentState, authoringGraph, floorSurface, floorTarget);
    Check(floorLabel.find("Authoring Floor") != std::string::npos,
          "mapped 3D floor label identifies authoring floor");
    Check(floorLabel.find("derived sector 200") != std::string::npos,
          "mapped 3D floor label keeps derived sector ID");
}

void TestEditorSelectedAuthoringLineBlocksPlayerWritesWithoutTopologySelection()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "selected line blocksPlayer direct write setup derives valid topology");
    Check(game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 11),
          "selected line blocksPlayer direct write selects authoring line");

    selectionState.topologySelectionKind = game::TopologySelectionKind::None;
    selectionState.selectedTopologyLineDefId = -1;
    selectionState.selectedTopologySideDefId = -1;

    Check(game::MutateSectorEditorAuthoringLineById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState.selectedAuthoring.lineId,
                  "Updated selected authoring line flags",
                  [](game::SectorAuthoringLine& line) {
                      line.flags.blocksPlayer = true;
                      return true;
                  }),
          "selected line blocksPlayer direct write succeeds without topology selection");

    const game::SectorAuthoringLine* authoringLine =
            game::FindSectorAuthoringLine(authoringGraph, 11);
    const game::SectorTopologyLineDef* projectedLineDef =
            FindDerivedLineDefForAuthoringLine(documentState.derivation.authoringDerivation, 11);
    Check(authoringLine != nullptr && authoringLine->flags.blocksPlayer,
          "selected line blocksPlayer direct write updates authoring source");
    Check(projectedLineDef != nullptr && projectedLineDef->flags.blocksPlayer,
          "selected line blocksPlayer direct write refreshes projected derived linedef");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologyLineDefId == -1
                  && selectionState.selectedTopologySideDefId == -1,
          "selected line blocksPlayer direct write does not create topology selection");
}

void TestEditorSelectedAuthoringLineSideMaterialWritesWithoutTopologySelection()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}, {1, 3}, {2, 4}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "selected side direct material write setup derives valid topology");
    Check(game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 14),
          "selected side direct material write selects source line");

    selectionState.topologySelectionKind = game::TopologySelectionKind::None;
    selectionState.selectedTopologyLineDefId = -1;
    selectionState.selectedTopologySideDefId = -1;

    Check(game::MutateSectorEditorAuthoringSideById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Front},
                  "Updated selected authoring side material",
                  [](game::SectorAuthoringLineSide& side) {
                      side.wall = WallPart("direct_selected_wall", 2.0f, 3.0f, 4.0f, 5.0f);
                      return true;
                  }),
          "selected side direct material write succeeds without topology selection");

    const game::SectorAuthoringLineSide* authoringSide =
            game::FindSectorAuthoringLineSide(
                    authoringGraph,
                    game::SectorAuthoringSideId{14, game::SectorTopologySideKind::Front});
    Check(authoringSide != nullptr && authoringSide->wall.textureId == "direct_selected_wall",
          "selected side direct material write updates authoring side metadata");

    int projectedCount = 0;
    for (const game::SectorAuthoringDerivedSideMapping& mapping : documentState.derivation.authoringDerivation.mapping.sides) {
        if (mapping.authoringLineId != 14
                || mapping.authoringSide != game::SectorTopologySideKind::Front) {
            continue;
        }
        const game::SectorTopologySideDef* sideDef =
                game::FindSectorTopologySideDef(documentState.map.topologyMap, mapping.topologySideDefId);
        Check(sideDef != nullptr && sideDef->wall.textureId == "direct_selected_wall",
              "selected side direct material write projects to split derived sidedef");
        ++projectedCount;
    }
    Check(projectedCount == 2,
          "selected side direct material write preserves split-line projection");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologyLineDefId == -1
                  && selectionState.selectedTopologySideDefId == -1,
          "selected side direct material write does not create topology selection");
}

void TestEditorSelectedFaceAnchorPropertiesWriteWithoutTopologySelection()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "selected face direct property write setup derives valid topology");
    Check(game::SelectSectorEditorAuthoringFaceAnchor(authoringGraph, selectionState, 200),
          "selected face direct property write selects face anchor");

    selectionState.topologySelectionKind = game::TopologySelectionKind::None;
    selectionState.selectedTopologySectorId = -1;

    Check(game::MutateSectorEditorAuthoringFaceAnchorById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState.selectedAuthoring.faceAnchorId,
                  "Updated selected authoring face properties",
                  [](game::SectorAuthoringFaceAnchor& anchor) {
                      anchor.floorZ = -4.0f;
                      anchor.ceilingZ = 48.0f;
                      anchor.ceilingSky = true;
                      anchor.ambientIntensity = 0.25f;
                      anchor.ambientColor = Color{10, 20, 30, 255};
                      return true;
                  }),
          "selected face direct property write succeeds without topology sector selection");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(anchor != nullptr && anchor->floorZ == -4.0f && anchor->ceilingSky,
          "selected face direct property write updates authoring source");
    Check(sector != nullptr
                  && sector->floorZ == -4.0f
                  && sector->ceilingZ == 48.0f
                  && sector->ceilingSky
                  && sector->ambientColor.g == 20
                  && sector->ambientIntensity == 0.25f,
          "selected face direct property write refreshes derived sector");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologySectorId == -1,
          "selected face direct property write does not create topology sector selection");
}

void TestEditorSelectedFaceAnchorVoidToggleWritesAuthoringOnly()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {128, 0}, {128, 64}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 5}, {5, 6}, {6, 1}, {2, 3}, {3, 4}, {4, 5}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "left-room");
    AddFaceAnchor(authoringGraph, 201, 96, 32, "right-room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "selected face void toggle setup derives valid topology");
    Check(game::SelectSectorEditorAuthoringFaceAnchor(authoringGraph, selectionState, 201),
          "selected face void toggle selects face anchor");
    Check(documentState.map.topologyMap.sectors.size() == 2,
          "selected face void toggle starts with two sectors");

    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;
    selectionState.topologySelectionKind = game::TopologySelectionKind::None;
    selectionState.selectedTopologySectorId = -1;

    Check(game::MutateSectorEditorAuthoringFaceAnchorById(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState.selectedAuthoring.faceAnchorId,
                  "Updated selected authoring face void state",
                  [](game::SectorAuthoringFaceAnchor& anchor) {
                      anchor.isVoid = true;
                      return true;
                  }),
          "selected face void toggle succeeds through authoring helper");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 201);
    Check(anchor != nullptr && anchor->isVoid,
          "selected face void toggle updates authoring anchor");
    Check(game::FindSectorTopologySector(documentState.map.topologyMap, 201) == nullptr,
          "selected face void toggle removes void sector through derivation");
    Check(documentState.map.topologyMap.sectors.size() == 1,
          "selected face void toggle leaves non-void sector only");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "selected face void toggle marks document dirty");
    Check(!state.topologyRenderCache.valid,
          "selected face void toggle invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "selected face void toggle bumps topology render revision");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologySectorId == -1,
          "selected face void toggle does not create topology selection");
}

void TestEditorAuthoringFacePointSelectionRequiresCurrentUnambiguousMapping()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "point face-anchor selection setup derives valid topology");

    game::SectorAuthoringSelectionTarget target;
    std::string status;
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), game::SectorCoordToVisibleAuthoring(32)},
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "point inside derived face resolves authoring selection target");
    Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && target.faceAnchorId == 200,
          "point inside derived face resolves expected face anchor ID");

    game::SectorEditorState staleState = state;
    game::SectorEditorDocumentState staleDocumentState = documentState;
    game::MarkSectorEditorAuthoringGraphEdited(
            staleState,
            game::MakeSectorEditorDocumentLifecycleAccess(staleDocumentState.lifecycle),
            game::MakeSectorEditorDerivationDocumentAccess(staleDocumentState.derivation),
            "authoring graph changed before face selection");
    target = game::SectorAuthoringSelectionTarget{};
    status.clear();
    Check(!FindEditorAuthoringSelectionAtMapPoint(
                  staleState,
                  staleDocumentState,
                  authoringGraph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), game::SectorCoordToVisibleAuthoring(32)},
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "point inside derived face selection fails closed when derivation is stale");
    Check(target.kind == game::SectorAuthoringSelectionKind::None,
          "stale point face selection leaves no authoring target");
    Check(status.find("requires current valid derived topology") != std::string::npos,
          "stale point face selection reports current-derivation requirement");

    game::SectorEditorState missingState = state;
    game::SectorEditorDocumentState missingDocumentState = documentState;
    missingDocumentState.derivation.authoringDerivation.mapping.sectors.clear();
    missingDocumentState.derivation.authoringDerivation.mapping.resolvedFaces.clear();
    status.clear();
    Check(!FindEditorAuthoringSelectionAtMapPoint(
                  missingState,
                  missingDocumentState,
                  authoringGraph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), game::SectorCoordToVisibleAuthoring(32)},
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "point inside derived face selection fails closed with missing mapping");
    Check(status.find("no face anchor mapping") != std::string::npos,
          "missing point face selection reports missing mapping");

    game::SectorEditorState ambiguousState = state;
    game::SectorEditorDocumentState ambiguousDocumentState = documentState;
    ambiguousDocumentState.derivation.authoringDerivation.mapping.sectors.push_back(
            ambiguousDocumentState.derivation.authoringDerivation.mapping.sectors.front());
    ambiguousDocumentState.derivation.authoringDerivation.mapping.resolvedFaces.push_back(
            ambiguousDocumentState.derivation.authoringDerivation.mapping.resolvedFaces.front());
    status.clear();
    Check(!FindEditorAuthoringSelectionAtMapPoint(
                  ambiguousState,
                  ambiguousDocumentState,
                  authoringGraph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), game::SectorCoordToVisibleAuthoring(32)},
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "point inside derived face selection fails closed with ambiguous mapping");
    Check(status.find("ambiguous face anchor mapping") != std::string::npos,
          "ambiguous point face selection reports ambiguous mapping");
}

void TestEditorAuthoringNestedFacePointSelectionChoosesDeepestAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeNestedRectangleGraph(4);
    AddFaceAnchor(authoringGraph, 200, 16, 16, "outer");
    AddFaceAnchor(authoringGraph, 201, 48, 48, "middle");
    AddFaceAnchor(authoringGraph, 202, 80, 80, "inner");
    AddFaceAnchor(authoringGraph, 203, 128, 128, "deepest");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "nested point face selection setup derives valid topology");

    const auto expectFace = [&](game::SectorCoord x, game::SectorCoord y, int expectedAnchorId, const char* description) {
        game::SectorAuthoringSelectionTarget target;
        std::string status;
        Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                      VisibleAuthoringPoint(x, y),
                      0.25f,
                      0.25f,
                      &target,
                      nullptr,
                      &status),
              description);
        Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                      && target.faceAnchorId == expectedAnchorId,
              TextFormat("%s returns expected face anchor", description));
        Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None,
              TextFormat("%s does not use topology selection", description));
    };

    expectFace(16, 16, 200, "point in outer nested ring selects outer anchor");
    expectFace(48, 48, 201, "point in first nested face selects middle anchor");
    expectFace(80, 80, 202, "point in second nested face selects inner anchor");
    expectFace(128, 128, 203, "point in deepest nested face selects deepest anchor");

    game::SectorAuthoringSelectionTarget target;
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  VisibleAuthoringPoint(0, 0),
                  0.25f,
                  0.25f,
                  &target),
          "nested point face selection keeps vertex priority over face");
    Check(target.kind == game::SectorAuthoringSelectionKind::Vertex
                  && target.vertexId == 1,
          "nested point face selection returns vertex before face");
}

void TestEditorAuthoringSiblingNestedFacePointSelectionChoosesIndependentAnchors()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {256, 0}, {256, 256}, {0, 256},
             {32, 32}, {96, 32}, {96, 96}, {32, 96},
             {160, 32}, {224, 32}, {224, 96}, {160, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}, {10, 11}, {11, 12}, {12, 9}});
    AddFaceAnchor(authoringGraph, 200, 16, 16, "outer");
    AddFaceAnchor(authoringGraph, 201, 64, 64, "left");
    AddFaceAnchor(authoringGraph, 202, 192, 64, "right");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "sibling point face selection setup derives valid topology");

    const auto expectFace = [&](game::SectorCoord x, game::SectorCoord y, int expectedAnchorId, const char* description) {
        game::SectorAuthoringSelectionTarget target;
        std::string status;
        Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                      VisibleAuthoringPoint(x, y),
                      0.25f,
                      0.25f,
                      &target,
                      nullptr,
                      &status),
              description);
        Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                      && target.faceAnchorId == expectedAnchorId,
              TextFormat("%s returns expected face anchor", description));
    };

    expectFace(16, 16, 200, "point in sibling parent area selects outer anchor");
    expectFace(64, 64, 201, "point in left sibling nested face selects left anchor");
    expectFace(192, 64, 202, "point in right sibling nested face selects right anchor");
}

void TestEditorAuthoringRefreshSynthesizesMissingNestedFaceAnchors()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128},
             {32, 32}, {96, 32}, {96, 96}, {32, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5}});
    AddFaceAnchor(authoringGraph, 200, 16, 16, "outer");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "missing nested face anchor setup reconciles and derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 2,
          "missing nested face anchor reconciliation adds exactly one anchor");
    Check(AllDerivedSectorsHaveExactlyOneValidFaceAnchorMapping(
                  authoringGraph,
                  documentState.derivation.authoringDerivation),
          "missing nested face anchor reconciliation maps every visible sector once");
    Check(documentState.lifecycle.topologyDocumentDirty,
          "missing nested face anchor reconciliation marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges,
          "missing nested face anchor reconciliation marks unsaved changes");

    game::SectorAuthoringSelectionTarget target;
    std::string status;
    Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                  VisibleAuthoringPoint(64, 64),
                  0.25f,
                  0.25f,
                  &target,
                  nullptr,
                  &status),
          "point inside reconciled nested face selects synthesized anchor");
    Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && target.faceAnchorId != 200,
          "reconciled nested face selection returns synthesized face anchor");
}

void TestEditorAuthoringRefreshSynthesizesSiblingFaceAnchorsWithUniqueLabels()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {256, 0}, {256, 256}, {0, 256},
             {32, 32}, {96, 32}, {96, 96}, {32, 96},
             {160, 32}, {224, 32}, {224, 96}, {160, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}, {10, 11}, {11, 12}, {12, 9}});

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "sibling missing face anchor refresh reconciles and derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 3,
          "sibling missing face anchor reconciliation adds one anchor per visible face");
    Check(AllDerivedSectorsHaveExactlyOneValidFaceAnchorMapping(
                  authoringGraph,
                  documentState.derivation.authoringDerivation),
          "sibling missing face anchor reconciliation maps every visible sector once");

    std::set<std::string> labels;
    for (const game::SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
        Check(labels.insert(anchor.name).second,
              "sibling synthesized face anchors have unique labels");
    }
    Check(labels.find("Sector 1") != labels.end()
                  && labels.find("Sector 2") != labels.end()
                  && labels.find("Sector 3") != labels.end(),
          "sibling synthesized face anchors use deterministic generated labels");

    std::set<int> selectedAnchorIds;
    const auto expectFace = [&](game::SectorCoord x, game::SectorCoord y, const char* description) {
        game::SectorAuthoringSelectionTarget target;
        Check(FindEditorAuthoringSelectionAtMapPoint(
                  state,
                  documentState,
                  authoringGraph,
                      VisibleAuthoringPoint(x, y),
                      0.25f,
                      0.25f,
                      &target),
              description);
        Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor,
              TextFormat("%s returns face anchor selection", description));
        if (target.kind == game::SectorAuthoringSelectionKind::FaceAnchor) {
            selectedAnchorIds.insert(target.faceAnchorId);
        }
    };
    expectFace(16, 16, "reconciled sibling parent area selects synthesized anchor");
    expectFace(64, 64, "reconciled left sibling area selects synthesized anchor");
    expectFace(192, 64, "reconciled right sibling area selects synthesized anchor");
    Check(selectedAnchorIds.size() == 3,
          "reconciled sibling face selection resolves each visible face to a distinct anchor");
}

void TestEditorAuthoringRefreshDoesNotSynthesizeForInvalidDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}},
            {{1, 2}, {2, 3}});
    const std::size_t originalAnchorCount = authoringGraph.faceAnchors.size();

    Check(!game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "invalid authoring graph refresh fails");
    Check(authoringGraph.faceAnchors.size() == originalAnchorCount,
          "invalid authoring graph refresh does not synthesize face anchors");
}

void TestEditorAuthoringRefreshPreservesUnresolvedExistingAnchors()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "custom-room");
    AddFaceAnchor(authoringGraph, 201, 512, 512, "custom-unresolved");
    const std::size_t originalAnchorCount = authoringGraph.faceAnchors.size();

    Check(!game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "unresolved existing anchor refresh fails");
    Check(authoringGraph.faceAnchors.size() == originalAnchorCount,
          "unresolved existing anchor refresh does not delete or synthesize anchors");
    const game::SectorAuthoringFaceAnchor* unresolved =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 201);
    Check(unresolved != nullptr
                  && unresolved->name == "custom-unresolved"
                  && unresolved->x == 512
                  && unresolved->y == 512,
          "unresolved existing anchor is preserved unchanged");
}

void TestDeriveGeneratedFaceLabelsAreUnique()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(graph, 200, 16, 16, "Sector 1");
    AddFaceAnchor(graph, 201, 48, 48, "Sector 1");
    AddFaceAnchor(graph, 202, 96, 96, "custom");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "duplicate generated label derivation is valid");
    std::set<std::string> labels;
    for (const game::SectorTopologySector& sector : result.topology.sectors) {
        Check(labels.insert(sector.name).second,
              "derived generated/default sector labels are unique");
    }
    const game::SectorTopologySector* outer = game::FindSectorTopologySector(result.topology, 200);
    const game::SectorTopologySector* middle = game::FindSectorTopologySector(result.topology, 201);
    const game::SectorTopologySector* inner = game::FindSectorTopologySector(result.topology, 202);
    Check(outer != nullptr && outer->name == "Sector 1",
          "first generated/default sector label is preserved");
    Check(middle != nullptr && middle->name == "Sector 2",
          "duplicate generated/default sector label is repaired deterministically");
    Check(inner != nullptr && inner->name == "custom",
          "custom face label is preserved");
}

void TestDeriveGeneratedFallbackLabelsSkipExistingNames()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(2);
    AddFaceAnchor(graph, 200, 16, 16, "Sector 1");

    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);

    CheckDerivedTopologyIsValid(result, "generated fallback label derivation is valid");
    const game::SectorTopologySector* outer = game::FindSectorTopologySector(result.topology, 200);
    Check(outer != nullptr && outer->name == "Sector 1",
          "existing generated/default label is preserved");

    bool foundSkippedFallback = false;
    for (const game::SectorTopologySector& sector : result.topology.sectors) {
        if (sector.id != 200 && sector.name == "Sector 2") {
            foundSkippedFallback = true;
        }
    }
    Check(foundSkippedFallback,
          "generated fallback labels skip existing generated/default names");
}

void TestEditorAuthoringRefreshPreservesCustomAndImportedLabels()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeNestedRectangleGraph(4);
    AddFaceAnchor(authoringGraph, 200, 16, 16, "Kitchen");
    AddFaceAnchor(authoringGraph, 201, 48, 48, "Sector 1");
    AddFaceAnchor(authoringGraph, 202, 80, 80, "Sector 7");

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "custom/imported label reconciliation derives valid topology");
    Check(authoringGraph.faceAnchors.size() == 4,
          "custom/imported label reconciliation synthesizes one missing anchor");

    const game::SectorAuthoringFaceAnchor* kitchen =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorAuthoringFaceAnchor* importedSector1 =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 201);
    const game::SectorAuthoringFaceAnchor* importedSector7 =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 202);
    Check(kitchen != nullptr && kitchen->name == "Kitchen",
          "custom label Kitchen is preserved");
    Check(importedSector1 != nullptr && importedSector1->name == "Sector 1",
          "existing imported/generated-looking label Sector 1 is preserved");
    Check(importedSector7 != nullptr && importedSector7->name == "Sector 7",
          "existing imported/generated-looking label Sector 7 is preserved");

    bool foundSector2 = false;
    bool renamedExisting = false;
    for (const game::SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
        if (anchor.id == 200 || anchor.id == 201 || anchor.id == 202) {
            renamedExisting = renamedExisting
                    || (anchor.id == 200 && anchor.name != "Kitchen")
                    || (anchor.id == 201 && anchor.name != "Sector 1")
                    || (anchor.id == 202 && anchor.name != "Sector 7");
            continue;
        }
        foundSector2 = foundSector2 || anchor.name == "Sector 2";
    }
    Check(!renamedExisting,
          "custom/imported label reconciliation does not broadly rename existing anchors");
    Check(foundSector2,
          "new generated label skips existing Sector 1 and Sector 7 labels");
}

void TestEditorAuthoringTexturePickerDirectTargetsFailClosedWhenMappingUnavailable()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_wall", game::SectorTextureDefinition{"old_wall", "old_wall.png"});
    documentState.map.topologyMap.texturesById.emplace("new_wall", game::SectorTextureDefinition{"new_wall", "new_wall.png"});
    documentState.map.topologyMap.texturesById.emplace("old_floor", game::SectorTextureDefinition{"old_floor", "old_floor.png"});
    documentState.map.topologyMap.texturesById.emplace("new_floor", game::SectorTextureDefinition{"new_floor", "new_floor.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    if (anchor != nullptr) {
        anchor->floorTextureId = "old_floor";
    }
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "old_wall";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "direct authoring picker fail-closed setup derives valid topology");

    Check(game::OpenSectorEditorMaterialPickerForAuthoringSide(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front},
                  game::TopologyWallPart::Wall,
                  game::TopologyMaterialLayer::Base),
          "direct authoring side picker opens while mapping is current");
    SelectTextureInPicker(state.texturePicker, "new_wall");
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed after direct side picker open");
    const game::SectorEditorTexturePickerApplyResult staleSideResult =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    const game::SectorAuthoringLineSide* afterStaleSide =
            game::FindSectorAuthoringLineSide(
                    authoringGraph,
                    game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    Check(!staleSideResult.changed,
          "direct authoring side picker apply fails closed when derivation becomes stale");
    Check(staleSideResult.status.find("derived topology is not current") != std::string::npos,
          "direct stale side picker reports current mapping requirement");
    Check(afterStaleSide != nullptr && afterStaleSide->wall.textureId == "old_wall",
          "direct stale side picker does not mutate authoring side");

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "direct picker fail-closed setup restores current derivation");
    documentState.derivation.authoringDerivation.mapping.sectors.clear();
    Check(!game::OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "direct authoring face picker fails closed with missing mapping");
    Check(!state.texturePicker.open,
          "direct authoring face picker missing mapping leaves picker closed");

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "direct picker fail-closed setup restores current derivation again");
    documentState.derivation.authoringDerivation.mapping.sectors.push_back(documentState.derivation.authoringDerivation.mapping.sectors.front());
    Check(!game::OpenSectorEditorMaterialPickerForAuthoringFaceAnchor(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "direct authoring face picker fails closed with ambiguous mapping");
    Check(!state.texturePicker.open,
          "direct authoring face picker ambiguous mapping leaves picker closed");
}

void TestEditorAuthoringSurfaceMappingResolvesFlatSurfaceToFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D flat surface mapping setup derives valid topology");

    game::SectorEditorAuthoringSurfaceTarget target;
    std::string status;
    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;

    Check(ResolveEditorAuthoringSurfaceTarget(state, documentState, authoringGraph, surface, target, &status),
          "3D flat surface resolves to authoring face anchor");
    Check(target.kind == game::SectorEditorAuthoringSurfaceTargetKind::FaceAnchor
                  && target.faceAnchorId == 200,
          "3D flat surface mapping returns selected face anchor ID");
    const game::SectorAuthoringSelectionTarget selection =
            game::MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(target);
    Check(selection.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && selection.faceAnchorId == 200,
          "3D flat surface mapping converts to authoring face-anchor selection");
    Check(status.empty(), "successful 3D flat surface mapping leaves status empty");

    surface.kind = game::SectorSurfaceKind::Ceiling;
    Check(ResolveEditorAuthoringSurfaceTarget(state, documentState, authoringGraph, surface, target, &status),
          "3D ceiling surface resolves to authoring face anchor");
    const game::SectorAuthoringSelectionTarget ceilingSelection =
            game::MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(target);
    Check(ceilingSelection.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                  && ceilingSelection.faceAnchorId == 200,
          "3D ceiling surface mapping converts to authoring face-anchor selection");
}

void TestEditorAuthoringSurfaceMappingResolvesWallSurfaceToAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D wall surface mapping setup derives valid topology");

    const game::SectorTopologySideDef* sideDef = FindDerivedSideDefForAuthoringSide(
            documentState.derivation.authoringDerivation,
            10,
            game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "3D wall surface mapping setup has mapped side");
    if (sideDef == nullptr) {
        return;
    }

    game::SectorEditorAuthoringSurfaceTarget target;
    std::string status;
    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Wall;
    surface.topologySectorId = sideDef->sectorId;
    surface.topologyLineDefId = sideDef->lineDefId;
    surface.topologySideDefId = sideDef->id;
    surface.topologySide = sideDef->side;

    Check(ResolveEditorAuthoringSurfaceTarget(state, documentState, authoringGraph, surface, target, &status),
          "3D wall surface resolves to authoring side");
    Check(target.kind == game::SectorEditorAuthoringSurfaceTargetKind::Side
                  && target.side.lineId == 10
                  && target.side.side == game::SectorTopologySideKind::Front,
          "3D wall surface mapping returns authoring line side identity");
    const game::SectorAuthoringSelectionTarget selection =
            game::MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(target);
    Check(selection.kind == game::SectorAuthoringSelectionKind::Line
                  && selection.lineId == 10
                  && selection.faceAnchorId == -1,
          "3D wall surface mapping converts to authoring line selection");
    Check(status.empty(), "successful 3D wall surface mapping leaves status empty");
}

void TestEditorAuthoringSurfaceMappingBlocksStaleDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale 3D surface mapping setup derives valid topology");
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed");

    game::SectorEditorAuthoringSurfaceTarget target;
    std::string status;
    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;

    Check(!ResolveEditorAuthoringSurfaceTarget(state, documentState, authoringGraph, surface, target, &status),
          "stale 3D surface mapping blocks inspector edits");
    Check(target.kind == game::SectorEditorAuthoringSurfaceTargetKind::None,
          "stale 3D surface mapping leaves no authoring target");
    Check(status.find("derived topology is not current") != std::string::npos,
          "stale 3D surface mapping reports unavailable status");
}

void TestEditorAuthoringSurfaceMappingBlocksMissingMapping()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "missing 3D surface mapping setup derives valid topology");
    documentState.derivation.authoringDerivation.mapping.sectors.clear();

    game::SectorEditorAuthoringSurfaceTarget target;
    std::string status;
    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;

    Check(!ResolveEditorAuthoringSurfaceTarget(state, documentState, authoringGraph, surface, target, &status),
          "missing 3D surface mapping blocks inspector edits");
    Check(target.kind == game::SectorEditorAuthoringSurfaceTargetKind::None,
          "missing 3D surface mapping leaves no authoring target");
    Check(status.find("no face anchor mapping") != std::string::npos,
          "missing 3D surface mapping reports unavailable status");
}

void TestEditorAuthoringSelectedSurfaceClearsWhenMappingBecomesStaleBeforeEdit()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "already-selected 3D surface stale mapping setup derives valid topology");

    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(sector != nullptr, "already-selected 3D surface stale mapping setup has sector");
    if (sector == nullptr) {
        return;
    }

    TestPreviewSelectionState().selectedSurface3D.kind = game::SectorSurfaceKind::Floor;
    TestPreviewSelectionState().selectedSurface3D.topologySectorId = 200;
    TestPreviewSelectionState().selectedTopologySurface3D.kind = game::TopologySurfaceEditTargetKind::SectorFloor;
    TestPreviewSelectionState().selectedTopologySurface3D.sectorId = 200;
    const float originalScaleU = sector->floorUv.scale.x;

    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed after 3D selection");

    std::string status;
    const bool mappingCurrent =
            ClearSelectedEditorSurface3DIfAuthoringMappingUnavailable(state, documentState, authoringGraph, TestPreviewSelectionState(), &status);
    if (mappingCurrent) {
        game::SectorTopologySector* mutableSector =
                game::FindSectorTopologySector(documentState.map.topologyMap, 200);
        if (mutableSector != nullptr) {
            mutableSector->floorUv.scale.x = 3.0f;
        }
    }

    const game::SectorTopologySector* afterSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(!mappingCurrent, "already-selected 3D surface stale mapping blocks attempted edit");
    Check(TestPreviewSelectionState().selectedSurface3D.kind == game::SectorSurfaceKind::None,
          "already-selected 3D surface stale mapping clears selected surface");
    Check(TestPreviewSelectionState().selectedTopologySurface3D.kind == game::TopologySurfaceEditTargetKind::None,
          "already-selected 3D surface stale mapping clears topology edit target");
    Check(status.find("derived topology is not current") != std::string::npos,
          "already-selected 3D surface stale mapping reports unavailable status");
    Check(afterSector != nullptr && afterSector->floorUv.scale.x == originalScaleU,
          "already-selected 3D surface stale mapping skips UV mutation");
}

void TestEditorAuthoringFlatSurfaceFloorUvWritesThroughFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D floor UV face-anchor write setup derives valid topology");

    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;
    game::TopologySurfaceEditTarget target;
    target.kind = game::TopologySurfaceEditTargetKind::SectorFloor;
    target.sectorId = 200;
    TestPreviewSelectionState().selectedSurface3D = surface;
    TestPreviewSelectionState().selectedTopologySurface3D = target;

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ApplySurfaceUvValue(
                  target,
                  game::TopologyMaterialLayer::Base,
                  0,
                  2.5f,
                  game::SectorSurfaceKind::Floor,
                  assets),
          "service 3D floor UV edit handles graph-authored flat target");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(anchor != nullptr && anchor->floorUv.scale.x == 2.5f,
          "service 3D floor UV edit writes to face anchor floor UV");
    Check(sector != nullptr && sector->floorUv.scale.x == 2.5f,
          "service 3D floor UV edit refreshes derived sector projection");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "service 3D floor UV edit marks authoring document dirty");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent
                  && !documentState.derivation.authoringDerivedTopologyStale,
          "service 3D floor UV edit leaves refreshed authoring derivation current");
}

void TestEditorAuthoringFlatSurfaceCeilingUvWritesThroughFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D ceiling UV face-anchor write setup derives valid topology");

    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Ceiling;
    surface.topologySectorId = 200;
    game::TopologySurfaceEditTarget target;
    target.kind = game::TopologySurfaceEditTargetKind::SectorCeiling;
    target.sectorId = 200;
    TestPreviewSelectionState().selectedSurface3D = surface;
    TestPreviewSelectionState().selectedTopologySurface3D = target;

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ApplySurfaceUvValue(
                  target,
                  game::TopologyMaterialLayer::Base,
                  3,
                  1.25f,
                  game::SectorSurfaceKind::Ceiling,
                  assets),
          "service 3D ceiling UV edit handles graph-authored flat target");

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(anchor != nullptr && anchor->ceilingUv.offset.y == 1.25f,
          "service 3D ceiling UV edit writes to face anchor ceiling UV");
    Check(sector != nullptr && sector->ceilingUv.offset.y == 1.25f,
          "service 3D ceiling UV edit refreshes derived sector projection");
}

void TestEditorAuthoringFlatSurfaceTextureWritesThroughFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("floor_tiles", game::SectorTextureDefinition{"floor_tiles", "floor_tiles.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D flat texture face-anchor write setup derives valid topology");

    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;
    game::TopologySurfaceEditTarget target;
    target.kind = game::TopologySurfaceEditTargetKind::SectorFloor;
    target.sectorId = 200;
    TestPreviewSelectionState().selectedSurface3D = surface;
    TestPreviewSelectionState().selectedTopologySurface3D = target;

    game::SectorEditorUiState uiState;
    std::string statusText;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.OpenMaterialPickerForDerivedSector(
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "service flat texture picker opens for graph-authored flat target");
    for (size_t i = 0; i < state.texturePicker.textureIds.size(); ++i) {
        if (state.texturePicker.textureIds[i] == "floor_tiles") {
            state.texturePicker.selectedTextureIndex = static_cast<int>(i);
            break;
        }
    }
    const game::SectorEditorTexturePickerApplyResult result =
            service.ApplyTexturePickerSelection(nullptr);

    const game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(result.changed, "service flat texture picker reports changed edit");
    Check(anchor != nullptr && anchor->floorTextureId == "floor_tiles",
          "service flat texture picker writes to face anchor floor texture");
    Check(sector != nullptr && sector->floorTextureId == "floor_tiles",
          "service flat texture picker refreshes derived sector projection");
}

void TestEditorAuthoringFlatSurfaceStaleMappingBlocksMaterialEdits()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "3D flat stale material edit setup derives valid topology");

    game::SectorSurfaceRef surface;
    surface.kind = game::SectorSurfaceKind::Floor;
    surface.topologySectorId = 200;
    game::TopologySurfaceEditTarget target;
    target.kind = game::TopologySurfaceEditTargetKind::SectorFloor;
    target.sectorId = 200;
    TestPreviewSelectionState().selectedSurface3D = surface;
    TestPreviewSelectionState().selectedTopologySurface3D = target;

    const game::SectorAuthoringFaceAnchor* beforeAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* beforeSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    const float originalAnchorScale = beforeAnchor != nullptr ? beforeAnchor->floorUv.scale.x : -1.0f;
    const float originalSectorScale = beforeSector != nullptr ? beforeSector->floorUv.scale.x : -1.0f;

    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed before flat edit");

    game::SectorEditorUiState uiState;
    std::string statusText;
    engine::AssetManager assets;
    game::SectorEditorMaterialEditingService service =
            MakeMaterialEditingService(state, documentState, authoringGraph, uiState, statusText);

    Check(service.ApplySurfaceUvValue(
                  target,
                  game::TopologyMaterialLayer::Base,
                  0,
                  3.0f,
                  game::SectorSurfaceKind::Floor,
                  assets),
          "stale service 3D flat material edit fails");

    const game::SectorAuthoringFaceAnchor* afterAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* afterSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(statusText.find("derived topology is not current") != std::string::npos,
          "stale service 3D flat material edit reports stale mapping");
    Check(TestPreviewSelectionState().selectedSurface3D.kind == game::SectorSurfaceKind::None
                  && TestPreviewSelectionState().selectedTopologySurface3D.kind == game::TopologySurfaceEditTargetKind::None,
          "stale 3D flat material edit clears selected 3D surface");
    Check(afterAnchor != nullptr && afterAnchor->floorUv.scale.x == originalAnchorScale,
          "stale 3D flat material edit does not mutate face anchor");
    Check(afterSector != nullptr && afterSector->floorUv.scale.x == originalSectorScale,
          "stale 3D flat material edit does not mutate derived topology");
}

void SelectTextureInPicker(game::TexturePickerState& picker, const std::string& textureId)
{
    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        if (picker.textureIds[i] == textureId) {
            picker.selectedTextureIndex = static_cast<int>(i);
            return;
        }
    }
    picker.selectedTextureIndex = -1;
}

void TestEditorAuthoringFaceTexturePickerWritesThroughFaceAnchor()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_floor", game::SectorTextureDefinition{"old_floor", "old_floor.png"});
    documentState.map.topologyMap.texturesById.emplace("new_floor", game::SectorTextureDefinition{"new_floor", "new_floor.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    Check(anchor != nullptr, "authoring face picker setup has face anchor");
    if (anchor != nullptr) {
        anchor->floorTextureId = "old_floor";
    }
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring face picker setup derives valid topology");

    Check(game::OpenSectorEditorMaterialPickerForDerivedSector(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "authoring face picker opens for mapped derived sector");
    Check(state.texturePicker.topologyTargetKind == game::TopologyTexturePickerTargetKind::AuthoringFaceAnchor,
          "authoring face picker records authoring target kind");
    state.texturePicker.authoringFaceAnchorId = 200;
    Check(game::CurrentTextureForPickerTarget(state, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorConstDerivationDocumentAccess(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation))) == "old_floor",
          "authoring face picker reads current texture from face anchor");

    SelectTextureInPicker(state.texturePicker, "new_floor");
    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));

    const game::SectorAuthoringFaceAnchor* editedAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* projectedSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(result.changed, "authoring face picker reports texture change");
    Check(editedAnchor != nullptr && editedAnchor->floorTextureId == "new_floor",
          "authoring face picker writes selected texture to face anchor");
    Check(projectedSector != nullptr && projectedSector->floorTextureId == "new_floor",
          "authoring face picker refreshes projected derived sector texture");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "authoring face picker marks document dirty");
    Check(!state.topologyRenderCache.valid,
          "authoring face picker invalidates cached topology rendering through graph edit");
}

void TestEditorAuthoringSideTexturePickerWritesThroughAuthoringSide()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_wall", game::SectorTextureDefinition{"old_wall", "old_wall.png"});
    documentState.map.topologyMap.texturesById.emplace("new_wall", game::SectorTextureDefinition{"new_wall", "new_wall.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "old_wall";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "authoring side picker setup derives valid topology");

    const game::SectorTopologySideDef* sideDef =
            FindDerivedSideDefForAuthoringSide(
                    documentState.derivation.authoringDerivation,
                    10,
                    game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "authoring side picker setup has mapped sidedef");
    if (sideDef == nullptr) {
        return;
    }

    Check(game::OpenSectorEditorMaterialPickerForDerivedSideDef(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  sideDef->id,
                  game::TopologyWallPart::Wall,
                  game::TopologyMaterialLayer::Base),
          "authoring side picker opens for mapped derived sidedef");
    Check(state.texturePicker.topologyTargetKind == game::TopologyTexturePickerTargetKind::AuthoringSide,
          "authoring side picker records authoring target kind");
    state.texturePicker.authoringLineId = 10;
    state.texturePicker.authoringSide = game::SectorTopologySideKind::Front;
    Check(game::CurrentTextureForPickerTarget(state, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorConstDerivationDocumentAccess(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation))) == "old_wall",
          "authoring side picker reads current texture from authoring side");

    SelectTextureInPicker(state.texturePicker, "new_wall");
    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));

    const game::SectorAuthoringLineSide* editedSide =
            game::FindSectorAuthoringLineSide(
                    authoringGraph,
                    game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* projectedSideDef =
            FindDerivedSideDefForAuthoringSide(
                    documentState.derivation.authoringDerivation,
                    10,
                    game::SectorTopologySideKind::Front);
    Check(result.changed, "authoring side picker reports texture change");
    Check(editedSide != nullptr && editedSide->wall.textureId == "new_wall",
          "authoring side picker writes selected texture to authoring side");
    Check(projectedSideDef != nullptr && projectedSideDef->wall.textureId == "new_wall",
          "authoring side picker refreshes projected derived sidedef texture");
}

void TestEditorAuthoringFaceTexturePickerRejectsStaleMappingAfterOpen()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_floor", game::SectorTextureDefinition{"old_floor", "old_floor.png"});
    documentState.map.topologyMap.texturesById.emplace("new_floor", game::SectorTextureDefinition{"new_floor", "new_floor.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringFaceAnchor* anchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    Check(anchor != nullptr, "stale face picker setup has face anchor");
    if (anchor != nullptr) {
        anchor->floorTextureId = "old_floor";
    }
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale face picker setup derives valid topology");

    Check(game::OpenSectorEditorMaterialPickerForDerivedSector(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "stale face picker opens before derivation becomes stale");
    SelectTextureInPicker(state.texturePicker, "new_floor");

    const game::SectorTopologySector* beforeSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    const std::string beforeSectorTexture = beforeSector != nullptr
            ? beforeSector->floorTextureId
            : std::string{};
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed after picker open");

    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));

    const game::SectorAuthoringFaceAnchor* afterAnchor =
            game::FindSectorAuthoringFaceAnchor(authoringGraph, 200);
    const game::SectorTopologySector* afterSector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(!result.changed, "stale face picker apply reports no texture change");
    Check(result.status.find("derived topology is not current") != std::string::npos,
          "stale face picker apply reports stale mapping");
    Check(!state.texturePicker.open, "stale face picker apply closes picker");
    Check(afterAnchor != nullptr && afterAnchor->floorTextureId == "old_floor",
          "stale face picker apply does not mutate face anchor");
    Check(afterSector != nullptr && afterSector->floorTextureId == beforeSectorTexture,
          "stale face picker apply does not directly mutate live derived sector");
}

void TestEditorAuthoringSideTexturePickerRejectsStaleMappingAfterOpen()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_wall", game::SectorTextureDefinition{"old_wall", "old_wall.png"});
    documentState.map.topologyMap.texturesById.emplace("new_wall", game::SectorTextureDefinition{"new_wall", "new_wall.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall.textureId = "old_wall";
    authoringGraph.lineSides.push_back(side);
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale side picker setup derives valid topology");

    const game::SectorTopologySideDef* sideDef =
            FindDerivedSideDefForAuthoringSide(
                    documentState.derivation.authoringDerivation,
                    10,
                    game::SectorTopologySideKind::Front);
    Check(sideDef != nullptr, "stale side picker setup has mapped sidedef");
    if (sideDef == nullptr) {
        return;
    }
    const int sideDefId = sideDef->id;

    Check(game::OpenSectorEditorMaterialPickerForDerivedSideDef(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  sideDefId,
                  game::TopologyWallPart::Wall,
                  game::TopologyMaterialLayer::Base),
          "stale side picker opens before derivation becomes stale");
    SelectTextureInPicker(state.texturePicker, "new_wall");

    const game::SectorTopologySideDef* beforeSideDef =
            game::FindSectorTopologySideDef(documentState.map.topologyMap, sideDefId);
    const std::string beforeSideTexture = beforeSideDef != nullptr
            ? beforeSideDef->wall.textureId
            : std::string{};
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed after picker open");

    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplySectorEditorMaterialTexturePickerSelection(state.texturePicker, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), state.topologyRenderRevision, state.topologyRenderCache, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));

    const game::SectorAuthoringLineSide* afterSide =
            game::FindSectorAuthoringLineSide(
                    authoringGraph,
                    game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    const game::SectorTopologySideDef* afterSideDef =
            game::FindSectorTopologySideDef(documentState.map.topologyMap, sideDefId);
    Check(!result.changed, "stale side picker apply reports no texture change");
    Check(result.status.find("derived topology is not current") != std::string::npos,
          "stale side picker apply reports stale mapping");
    Check(!state.texturePicker.open, "stale side picker apply closes picker");
    Check(afterSide != nullptr && afterSide->wall.textureId == "old_wall",
          "stale side picker apply does not mutate authoring side");
    Check(afterSideDef != nullptr && afterSideDef->wall.textureId == beforeSideTexture,
          "stale side picker apply does not directly mutate live derived sidedef");
}

void TestEditorAuthoringTexturePickerRejectsStaleMapping()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("wall", game::SectorTextureDefinition{"wall", "wall.png"});
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(authoringGraph, 200, 32, 32, "room");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale authoring picker setup derives valid topology");
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph changed before picker open");

    Check(!game::OpenSectorEditorMaterialPickerForDerivedSector(
                  state.texturePicker,
                  documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  200,
                  game::TopologySectorTextureField::Floor,
                  game::TopologyMaterialLayer::Base),
          "authoring face picker refuses stale derived mapping");
    Check(!state.texturePicker.open,
          "authoring face picker closes state when mapping is stale");
}

void TestEditorNoAuthoringTexturePickerApplyFailsWithoutMutatingTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap = MakeSingleSectorSquareMap();
    documentState.map.topologyMap.texturesById.emplace("room_floor", game::SectorTextureDefinition{"room_floor", "room_floor.png"});
    documentState.map.topologyMap.texturesById.emplace("new_floor", game::SectorTextureDefinition{"new_floor", "new_floor.png"});

    state.texturePicker.open = true;
    state.texturePicker.topologyTargetKind = game::TopologyTexturePickerTargetKind::Sector;
    state.texturePicker.topologyLayer = game::TopologyMaterialLayer::Base;
    state.texturePicker.topologySectorId = 200;
    state.texturePicker.topologyField = game::TopologySectorTextureField::Floor;
    state.texturePicker.textureIds.push_back("room_floor");
    state.texturePicker.textureIds.push_back("new_floor");
    SelectTextureInPicker(state.texturePicker, "new_floor");

    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplyTexturePickerSelection(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(documentState.map.topologyMap, 200);
    Check(!result.changed, "invalid no-authoring texture picker reports no material change");
    Check(result.status.find("authoring data is required") != std::string::npos,
          "invalid no-authoring texture picker reports authoring requirement");
    Check(sector != nullptr && sector->floorTextureId == "room_floor",
          "invalid no-authoring texture picker does not mutate derived topology");
    Check(!state.texturePicker.open, "invalid no-authoring texture picker closes after apply");
}

void TestEditorRuntimeDoorTexturePickerWritesAuthoredDoorTexture()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace("old_door", game::SectorTextureDefinition{"old_door", "old_door.png"});
    documentState.map.topologyMap.texturesById.emplace("new_door", game::SectorTextureDefinition{"new_door", "new_door.png"});

    game::SectorPlacedRuntimeObject object;
    object.id = 77;
    object.kind = "door";
    object.door.textureId = "old_door";
    documentState.map.topologyMap.runtimeObjects.push_back(object);

    game::TextureCatalogState textureCatalogState;
    game::SectorEditorTextureCatalogService catalog{
            game::SectorEditorTextureCatalogServiceContext{
                    documentState.map.topologyMap,
                    textureCatalogState,
                    state.defaultFloorTextureId,
                    state.defaultCeilingTextureId,
                    state.defaultWallTextureId,
                    state.defaultLowerWallTextureId,
                    state.defaultUpperWallTextureId}};

    Check(game::OpenRuntimeDoorTexturePicker(state, documentState.map.topologyMap, authoringGraph, catalog, 77),
          "runtime door texture picker opens for authored door");
    Check(state.texturePicker.topologyTargetKind == game::TopologyTexturePickerTargetKind::RuntimeDoor,
          "runtime door texture picker records door target kind");
    Check(state.texturePicker.runtimeObjectId == 77,
          "runtime door texture picker records target object id");
    Check(game::CurrentTextureForPickerTarget(state, documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorConstDerivationDocumentAccess(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation))) == "old_door",
          "runtime door texture picker reads current authored door texture");

    SelectTextureInPicker(state.texturePicker, "new_door");
    const game::SectorEditorTexturePickerApplyResult result =
            game::ApplyTexturePickerSelection(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation));
    const game::SectorPlacedRuntimeObject* editedObject =
            game::FindSectorPlacedRuntimeObject(documentState.map.topologyMap, 77);
    Check(result.changed, "runtime door texture picker reports texture change");
    Check(editedObject != nullptr && editedObject->door.textureId == "new_door",
          "runtime door texture picker writes selected texture to authored door");
    Check(!state.texturePicker.open, "runtime door texture picker closes after apply");
}

void TestEditorMapTextureImportPreservesMapLevelRegistryOnly()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.texturesById.emplace(
            "z_existing",
            game::SectorTextureDefinition{"z_existing", "assets/images/z_existing.png"});
    documentState.map.topologyMap.texturesById.emplace(
            "a_existing",
            game::SectorTextureDefinition{"a_existing", "assets/images/a_existing.png"});
    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;
    state.addMapTexture.paths.push_back("assets/images/imported_wall.png");
    state.addMapTexture.selectedPathIndex = 0;
    std::snprintf(
            state.addMapTexture.textureIdBuffer,
            sizeof(state.addMapTexture.textureIdBuffer),
            "%s",
            "imported_wall");
    state.addMapTexture.filter = game::SectorTextureFilter::Point;

    game::TextureCatalogState textureCatalogState;
    game::SectorEditorTextureCatalogService catalog{
            game::SectorEditorTextureCatalogServiceContext{
                    documentState.map.topologyMap,
                    textureCatalogState,
                    state.defaultFloorTextureId,
                    state.defaultCeilingTextureId,
                    state.defaultWallTextureId,
                    state.defaultLowerWallTextureId,
                    state.defaultUpperWallTextureId}};
    const game::SectorEditorAddTextureResult result =
            catalog.RegisterSelectedMapTexture(state.addMapTexture);

    const auto textureIt = documentState.map.topologyMap.texturesById.find("imported_wall");
    Check(result.success, "texture catalog registers valid map texture id");
    Check(textureIt != documentState.map.topologyMap.texturesById.end()
                  && textureIt->second.path == "assets/images/imported_wall.png"
                  && textureIt->second.filter == game::SectorTextureFilter::Point,
          "texture catalog writes only the map-level texture registry");
    Check(catalog.HasTexture("imported_wall"), "texture catalog finds registered texture id");
    Check(catalog.FindTexture("missing_texture") == nullptr, "texture catalog reports missing texture id");
    Check(catalog.TextureHandleForId("missing_texture") == engine::NullTextureHandle(),
          "texture catalog missing handle lookup returns null texture handle");
    Check(state.topologyRenderCache.valid,
          "texture catalog registration does not invalidate cached topology rendering by itself");
    Check(state.topologyRenderRevision == originalRevision,
          "texture catalog registration does not bump topology render revision by itself");

    state.addMapTexture.paths[0] = "assets/images/imported_wall_replacement.png";
    state.addMapTexture.filter = game::SectorTextureFilter::Bilinear;
    const game::SectorEditorAddTextureResult replacement =
            catalog.RegisterSelectedMapTexture(state.addMapTexture);
    const game::SectorTextureDefinition* replaced = catalog.FindTexture("imported_wall");
    Check(replacement.success && replacement.replacing,
          "texture catalog preserves duplicate id replacement reporting");
    Check(replaced != nullptr
                  && replaced->path == "assets/images/imported_wall_replacement.png"
                  && replaced->filter == game::SectorTextureFilter::Bilinear,
          "texture catalog preserves duplicate id replacement behavior");

    game::TexturePickerState picker;
    catalog.PopulatePickerOptions(picker, "imported_wall");
    Check(picker.textureIds.size() == 3
                  && picker.textureIds[0] == "a_existing"
                  && picker.textureIds[1] == "imported_wall"
                  && picker.textureIds[2] == "z_existing",
          "texture catalog picker options remain sorted by texture id");
    Check(picker.selectedTextureIndex == 1,
          "texture catalog picker options preserve current texture selection");
}

void TestSpriteMetadataScannerFindsNestedJsonAndFrameTagClips()
{
    const std::filesystem::path assetsRoot = TempDirectoryPath("sector_sprite_scan_tags_assets");
    RecreateTempDirectory(assetsRoot / "sprites" / "effects");
    WriteTextFile(
            assetsRoot / "sprites" / "effects" / "torch.json",
            R"json({
  "frames": {
    "torch_0001": {},
    "torch_0002": {}
  },
  "meta": {
    "image": "torch.png",
    "frameTags": [
      { "name": "Idle", "from": 0, "to": 0 },
      { "name": "Burn", "from": 1, "to": 1 }
    ]
  }
})json");
    WriteTextFile(
            assetsRoot / "sprites" / "effects" / "not_sprite.json",
            R"json({ "meta": { "image": "ignored.png" } })json");
    WriteTextFile(assetsRoot / "sprites" / "effects" / "notes.txt", "ignored");

    std::string message;
    const std::vector<game::SectorSpriteMetadata> sprites =
            game::ScanAssetSpriteAsepriteJsons(assetsRoot, message);

    Check(sprites.size() == 1, "sprite metadata scan finds only valid nested Aseprite JSON");
    if (!sprites.empty()) {
        Check(sprites[0].spriteAnimationPath == "assets/sprites/effects/torch.json",
              "sprite metadata scan returns normalized sprite JSON asset path");
        Check(sprites[0].atlasImagePath == "assets/sprites/effects/torch.png",
              "sprite metadata scan resolves relative atlas image as an asset path");
        Check(sprites[0].clipNames == std::vector<std::string>({"Idle", "Burn"}),
              "sprite metadata scan reads clip names from frameTags");
    }
    Check(message.empty(), "sprite metadata scan leaves message empty when candidates are found");

    std::error_code ec;
    std::filesystem::remove_all(assetsRoot, ec);
}

void TestSpriteMetadataScannerFallsBackToFrameNamesAndDefaultClip()
{
    const std::filesystem::path assetsRoot = TempDirectoryPath("sector_sprite_scan_fallback_assets");
    RecreateTempDirectory(assetsRoot / "sprites" / "characters");
    WriteTextFile(
            assetsRoot / "sprites" / "characters" / "hero.json",
            R"json({
  "frames": {
    "hero#Walk 0": {},
    "hero#Walk 1": {},
    "hero#Run 0": {}
  },
  "meta": {
    "image": "../atlases/hero.png"
  }
})json");
    WriteTextFile(
            assetsRoot / "sprites" / "characters" / "marker.json",
            R"json({
  "frames": {
    "0": {}
  },
  "meta": {
    "image": "marker.png"
  }
})json");

    std::string message;
    const std::vector<game::SectorSpriteMetadata> sprites =
            game::ScanAssetSpriteAsepriteJsons(assetsRoot, message);

    Check(sprites.size() == 2, "sprite metadata fallback scan finds both valid sprites");
    if (sprites.size() == 2) {
        Check(sprites[0].spriteAnimationPath == "assets/sprites/characters/hero.json",
              "sprite metadata fallback scan sorts by asset path");
        Check(sprites[0].atlasImagePath == "assets/sprites/atlases/hero.png",
              "sprite metadata fallback scan resolves parent-relative atlas path");
        Check(sprites[0].clipNames == std::vector<std::string>({"Walk", "Run"}),
              "sprite metadata fallback scan derives clip names from frame names");
        Check(sprites[1].spriteAnimationPath == "assets/sprites/characters/marker.json",
              "sprite metadata fallback scan keeps second sorted sprite");
        Check(sprites[1].clipNames == std::vector<std::string>({"Default"}),
              "sprite metadata fallback scan provides Default when no clip name can be derived");
    }
    Check(message.empty(), "sprite metadata fallback scan leaves message empty when candidates are found");

    std::error_code ec;
    std::filesystem::remove_all(assetsRoot, ec);
}

void TestSpriteMetadataScannerRejectsMissingSpritesDirectory()
{
    const std::filesystem::path assetsRoot = TempDirectoryPath("sector_sprite_scan_missing_assets");
    RecreateTempDirectory(assetsRoot);

    std::string message;
    const std::vector<game::SectorSpriteMetadata> sprites =
            game::ScanAssetSpriteAsepriteJsons(assetsRoot, message);

    Check(sprites.empty(), "sprite metadata scan returns no candidates without assets/sprites");
    Check(message == "assets/sprites was not found",
          "sprite metadata scan reports missing assets/sprites directory");

    std::error_code ec;
    std::filesystem::remove_all(assetsRoot, ec);
}

void TestSpritePickerSelectsSpriteMetadata()
{
    game::SpritePickerState picker;
    picker.sprites.push_back(game::SectorSpriteMetadata{
            "assets/sprites/effects/torch.json",
            "assets/sprites/effects/torch.png",
            {"Idle", "Burn"}});
    picker.sprites.push_back(game::SectorSpriteMetadata{
            "assets/sprites/props/barrel.json",
            "assets/sprites/props/barrel.png",
            {"Default"}});

    game::SelectSpritePickerSprite(picker, 1);
    const game::SectorEditorSpritePickerResult selected =
            game::SelectedSpritePickerResult(picker);

    Check(picker.selectedSpriteIndex == 1, "sprite picker selects requested sprite index");
    Check(selected.valid
                  && selected.spriteAnimationPath == "assets/sprites/props/barrel.json"
                  && selected.atlasImagePath == "assets/sprites/props/barrel.png"
                  && selected.clipNames == std::vector<std::string>({"Default"}),
          "sprite picker result returns selected sprite, atlas, and clip metadata");

    game::SelectSpritePickerSprite(picker, -1);
    const game::SectorEditorSpritePickerResult cleared =
            game::SelectedSpritePickerResult(picker);
    Check(!cleared.valid, "sprite picker result is invalid after clearing selection");
}

void TestBillboardSpritePickerApplyRepairsSingleClips()
{
    game::SectorPlacedBillboard billboard;
    billboard.spriteAnimationPath.clear();
    billboard.clip.clear();

    game::SectorEditorSpritePickerResult result;
    result.valid = true;
    result.spriteAnimationPath = "assets/sprites/effects/torch.json";
    result.atlasImagePath = "assets/sprites/effects/torch.png";
    result.clipNames = {"Idle", "Burn"};

    Check(game::ApplySpritePickerResultToBillboard(
                  billboard,
                  result),
          "billboard sprite picker apply reports sprite change");
    Check(billboard.spriteAnimationPath == "assets/sprites/effects/torch.json",
          "billboard sprite picker apply writes sprite animation path");
    Check(billboard.clip == "Idle",
          "billboard sprite picker apply defaults empty single clip to first metadata clip");
    Check(billboard.frontClip == "Idle"
                  && billboard.backClip == "Idle"
                  && billboard.leftClip == "Idle"
                  && billboard.rightClip == "Idle",
          "billboard sprite picker apply defaults empty directional clips to metadata");
}

void TestBillboardSpritePickerApplyRepairsDirectionalClips()
{
    game::SectorPlacedBillboard billboard;
    billboard.spriteAnimationPath = "assets/sprites/old.json";
    billboard.directional = true;
    billboard.frontClip = "North";
    billboard.backClip = "";
    billboard.leftClip = "Left";
    billboard.rightClip = "Missing";

    game::SectorEditorSpritePickerResult result;
    result.valid = true;
    result.spriteAnimationPath = "assets/sprites/characters/guard.json";
    result.atlasImagePath = "assets/sprites/characters/guard.png";
    result.clipNames = {"Front", "Back", "Left", "Right", "Attack"};

    Check(game::ApplySpritePickerResultToBillboard(
                  billboard,
                  result),
          "billboard sprite picker apply reports directional change");
    Check(billboard.spriteAnimationPath == "assets/sprites/characters/guard.json",
          "billboard directional picker apply writes sprite animation path");
    Check(billboard.frontClip == "Front"
                  && billboard.backClip == "Back"
                  && billboard.leftClip == "Left"
                  && billboard.rightClip == "Right",
          "billboard directional picker apply repairs invalid directional clips from selected metadata");
}

void TestBillboardClipRepairPreservesNonEmptyOnInitialMetadataResolve()
{
    game::SectorPlacedBillboard billboard;
    billboard.spriteAnimationPath = "assets/sprites/characters/guard.json";
    billboard.clip.clear();
    billboard.frontClip = "Missing";
    billboard.backClip.clear();
    billboard.leftClip = "Left";
    billboard.rightClip = "AlsoMissing";

    const game::SectorSpriteMetadata metadata{
            "assets/sprites/characters/guard.json",
            "assets/sprites/characters/guard.png",
            {"Front", "Back", "Left", "Right"}};

    Check(game::RepairBillboardClipsForSpriteMetadata(billboard, metadata, false),
          "initial metadata repair fills empty clip fields");
    Check(billboard.clip == "Front"
                  && billboard.frontClip == "Missing"
                  && billboard.backClip == "Back"
                  && billboard.leftClip == "Left"
                  && billboard.rightClip == "AlsoMissing",
          "initial metadata repair preserves non-empty authored stale clips");
}

void TestBillboardClipRepairOverwritesInvalidOnSpriteChange()
{
    game::SectorPlacedBillboard billboard;
    billboard.spriteAnimationPath = "assets/sprites/characters/guard.json";
    billboard.clip = "Missing";
    billboard.frontClip = "Missing";
    billboard.backClip.clear();
    billboard.leftClip = "Left";
    billboard.rightClip = "AlsoMissing";

    const game::SectorSpriteMetadata metadata{
            "assets/sprites/characters/guard.json",
            "assets/sprites/characters/guard.png",
            {"Front", "Back", "Left", "Right"}};

    Check(game::RepairBillboardClipsForSpriteMetadata(billboard, metadata, true),
          "sprite-change metadata repair overwrites invalid clip fields");
    Check(billboard.clip == "Front"
                  && billboard.frontClip == "Front"
                  && billboard.backClip == "Back"
                  && billboard.leftClip == "Left"
                  && billboard.rightClip == "Right",
          "sprite-change metadata repair chooses directional defaults and first single clip");
}

void TestEditorAuthoringFailedDerivationKeepsGraphAndDiagnostics()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed editor derivation setup creates last valid topology");
    const game::SectorTopologyMap lastValid = documentState.map.topologyMap;

    const int addedLineId = 99;
    AddAuthoringVertexWithId(authoringGraph, 5, 128, 0);
    AddAuthoringLineWithId(authoringGraph, addedLineId, 2, 5);
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "dangling authoring line added");

    Check(!game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed editor derivation returns false");
    Check(!documentState.derivation.authoringDerivation.success, "failed editor derivation stores failed result");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed editor derivation keeps invalid/last-valid state");
    Check(documentState.derivation.authoringDerivedTopologyStale, "failed editor derivation leaves stale flag set");
    Check(!documentState.derivation.authoringDerivation.diagnostics.empty(),
          "failed editor derivation records diagnostics");
    Check(game::FindSectorAuthoringLine(authoringGraph, addedLineId) != nullptr,
          "failed editor derivation keeps edited graph data");
    Check(documentState.map.topologyMap.sectors.size() == lastValid.sectors.size(),
          "failed editor derivation keeps current topology unchanged");
    Check(documentState.derivation.lastValidAuthoringDerivedTopology.has_value(),
          "failed editor derivation keeps memory-only last-valid topology");
}

void TestEditorAuthoringPreviewAndBakeGateAllowsCurrentDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "preview gate setup derives a valid topology");

    std::string previewMessage = "not cleared";
    Check(game::CanUseCurrentAuthoringDerivedTopologyForPreview(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &previewMessage),
          "preview gate allows current valid derived topology");
    Check(previewMessage.empty(), "preview gate clears message when allowed");

    std::string bakeMessage = "not cleared";
    Check(game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &bakeMessage),
          "bake gate allows current valid derived topology");
    Check(bakeMessage.empty(), "bake gate clears message when allowed");
    Check(!documentState.map.topologyMap.sectors.empty(), "allowed gate still uses derived topology map");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsInvalidNoDerived()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &previewMessage),
          "preview gate rejects invalid/no-derived graph");
    Check(previewMessage.find("no valid derived topology") != std::string::npos,
          "preview gate reports missing derived topology");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &bakeMessage),
          "bake gate rejects invalid/no-derived graph");
    Check(bakeMessage.find("no valid derived topology") != std::string::npos,
          "bake gate reports missing derived topology");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsStaleDerivedTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "stale gate setup derives a valid topology");
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "authoring graph edited for stale gate test");

    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &previewMessage),
          "preview gate rejects stale derived topology");
    Check(previewMessage.find("re-derive") != std::string::npos,
          "preview gate reports stale derivation");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &bakeMessage),
          "bake gate rejects stale derived topology");
    Check(bakeMessage.find("re-derive") != std::string::npos,
          "bake gate reports stale derivation");
}

void TestEditorAuthoringPreviewAndBakeGateRejectsFailedDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed gate setup creates last valid topology");
    AddAuthoringVertexWithId(authoringGraph, 5, 128, 0);
    AddAuthoringLineWithId(authoringGraph, 99, 2, 5);
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "dangling authoring line added");
    Check(!game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "failed gate setup records failed derivation");

    std::string previewMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForPreview(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &previewMessage),
          "preview gate rejects failed derivation without crashing");
    Check(previewMessage.find("derivation failed") != std::string::npos,
          "preview gate reports failed derivation");

    std::string bakeMessage;
    Check(!game::CanUseCurrentAuthoringDerivedTopologyForLightmapBake(game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), &bakeMessage),
          "bake gate rejects failed derivation without crashing");
    Check(bakeMessage.find("derivation failed") != std::string::npos,
          "bake gate reports failed derivation");
}

void TestEditorAuthoringSuccessfulDerivationPreservesBakedLightmapMetadata()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    documentState.map.topologyMap.bakedLightmap.path = "assets/levels/test/lightmap.png";
    documentState.map.topologyMap.bakedLightmap.width = 128;
    documentState.map.topologyMap.bakedLightmap.height = 128;
    documentState.map.topologyMap.bakedLightmap.sourceHash = "old-hash";
    documentState.map.topologyMap.bakedLightmap.objectProbes.path =
            "assets/levels/test/lightmap.object_probes.bin";
    documentState.map.topologyMap.bakedLightmap.objectProbes.version =
            game::kSectorBakedObjectLightProbeSidecarVersion;
    documentState.map.topologyMap.bakedLightmap.objectProbes.sourceHash = "old-hash";
    documentState.map.topologyMap.bakedLightmap.objectProbes.count = 5;
    documentState.map.topologyMap.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    documentState.map.topologyMap.bakedLightmap.objectProbes.probeHeightWorld = 1.2f;
    documentState.map.topologyMap.bakedLightmap.objectProbes.format =
            game::kSectorBakedObjectLightProbeSidecarFormat;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});

    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "lightmap stale setup derives valid topology");
    Check(documentState.map.topologyMap.bakedLightmap.path == "assets/levels/test/lightmap.png",
          "successful derivation preserves baked lightmap path");
    Check(documentState.map.topologyMap.bakedLightmap.width == 128 && documentState.map.topologyMap.bakedLightmap.height == 128,
          "successful derivation preserves baked lightmap dimensions");
    Check(documentState.map.topologyMap.bakedLightmap.sourceHash == "old-hash",
          "successful derivation preserves baked lightmap source hash");
    Check(documentState.map.topologyMap.bakedLightmap.objectProbes.path
                  == "assets/levels/test/lightmap.object_probes.bin"
                  && documentState.map.topologyMap.bakedLightmap.objectProbes.count == 5,
          "successful derivation preserves baked object probe metadata");
    Check(game::GetSectorLightmapStatus(documentState.map.topologyMap) == game::SectorLightmapStatus::Stale,
          "preserved baked lightmap metadata becomes stale when source hash no longer matches");
    Check(game::GetSectorBakedObjectLightProbeStatus(documentState.map.topologyMap) == game::SectorLightmapStatus::Stale,
          "preserved object probe metadata becomes stale when source hash no longer matches");
}

void TestEditorAuthoringSelectionTargetsRepresentLineAndVertex()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);

    const game::SectorAuthoringSelectionTarget lineTarget =
            game::MakeSectorAuthoringLineSelectionTarget(10);
    const game::SectorAuthoringSelectionTarget vertexTarget =
            game::MakeSectorAuthoringVertexSelectionTarget(1);

    Check(lineTarget.kind == game::SectorAuthoringSelectionKind::Line,
          "authoring selection target can represent a line");
    Check(lineTarget.lineId == 10 && lineTarget.vertexId == -1,
          "authoring line selection target stores only the line ID");
    Check(vertexTarget.kind == game::SectorAuthoringSelectionKind::Vertex,
          "authoring selection target can represent a vertex");
    Check(vertexTarget.vertexId == 1 && vertexTarget.lineId == -1,
          "authoring vertex selection target stores only the vertex ID");
    Check(game::IsSectorAuthoringSelectionTargetValid(authoringGraph, lineTarget),
          "authoring line target validates against the graph");
    Check(game::IsSectorAuthoringSelectionTargetValid(authoringGraph, vertexTarget),
          "authoring vertex target validates against the graph");
    Check(documentState.map.topologyMap.vertices.empty() && documentState.map.topologyMap.lineDefs.empty(),
          "authoring selection target helpers do not mutate derived topology");
}

void TestEditorAuthoringSelectionHelpersSetClearAndRejectMissingTargets()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);

    Check(game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 10),
          "select authoring line helper accepts an existing line");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line,
          "select authoring line helper records line selection");
    Check(!game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 99),
          "select authoring line helper rejects missing line");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && selectionState.selectedAuthoring.lineId == 10,
          "rejected authoring line selection leaves previous selection intact");
    Check(!game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 0),
          "select authoring line helper rejects zero line ID");
    Check(!game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, -1),
          "select authoring line helper rejects negative line ID");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && selectionState.selectedAuthoring.lineId == 10,
          "invalid authoring line selection leaves previous selection intact");

    Check(game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 1),
          "select authoring vertex helper accepts an existing vertex");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex,
          "select authoring vertex helper records vertex selection");
    Check(!game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 99),
          "select authoring vertex helper rejects missing vertex");
    Check(!game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 0),
          "select authoring vertex helper rejects zero vertex ID");
    Check(!game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, -1),
          "select authoring vertex helper rejects negative vertex ID");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && selectionState.selectedAuthoring.vertexId == 1,
          "invalid authoring vertex selection leaves previous selection intact");

    game::ClearSectorEditorAuthoringSelection(selectionState);
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "clear authoring selection helper clears selection kind");
    Check(documentState.map.topologyMap.vertices.empty() && documentState.map.topologyMap.lineDefs.empty(),
          "authoring selection helpers do not mutate derived topology");
}

void TestEditorAuthoringHoverAndPruneUseGraphValidity()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);

    Check(game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 10),
          "prune setup selects authoring line");
    Check(game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, 2),
          "hover authoring vertex helper accepts existing vertex");
    Check(!game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, 0),
          "hover authoring vertex helper rejects zero vertex ID");
    Check(!game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, -1),
          "hover authoring vertex helper rejects negative vertex ID");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && selectionState.hoveredAuthoring.vertexId == 2,
          "invalid authoring vertex hover leaves previous hover intact");
    Check(game::SetHoveredSectorEditorAuthoringLine(authoringGraph, selectionState, 10),
          "hover authoring line helper accepts existing line");
    Check(!game::SetHoveredSectorEditorAuthoringLine(authoringGraph, selectionState, 0),
          "hover authoring line helper rejects zero line ID");
    Check(!game::SetHoveredSectorEditorAuthoringLine(authoringGraph, selectionState, -1),
          "hover authoring line helper rejects negative line ID");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Line
                  && selectionState.hoveredAuthoring.lineId == 10,
          "invalid authoring line hover leaves previous hover intact");
    Check(game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, 2),
          "prune setup restores authoring vertex hover");

    authoringGraph.lines.clear();
    authoringGraph.vertices.erase(
            std::remove_if(
                    authoringGraph.vertices.begin(),
                    authoringGraph.vertices.end(),
                    [](const game::SectorAuthoringVertex& vertex) {
                        return vertex.id == 2;
                    }),
            authoringGraph.vertices.end());

    game::PruneSectorEditorAuthoringSelectionToGraph(authoringGraph, selectionState);
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection prune clears deleted line selection");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection prune clears deleted vertex hover");
    Check(documentState.map.topologyMap.vertices.empty() && documentState.map.topologyMap.lineDefs.empty(),
          "authoring hover/prune helpers do not mutate derived topology");
}

void TestEditorAuthoringLineDrawHelperCreatesLooseLineAndMarksDirty()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = documentState.map.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "authoring line draw helper creates a loose line");

    Check(game::IsValidSectorAuthoringId(lineId), "authoring line draw helper returns a valid line ID");
    Check(authoringGraph.vertices.size() == 2,
          "authoring line draw helper creates endpoint vertices");
    Check(authoringGraph.lines.size() == 1,
          "authoring line draw helper creates one authoring line");
    const game::SectorAuthoringLine* line =
            game::FindSectorAuthoringLine(authoringGraph, lineId);
    Check(line != nullptr && line->startVertexId != line->endVertexId,
          "authoring line draw helper connects distinct endpoint vertices");
    Check(documentState.lifecycle.topologyDocumentDirty, "authoring line draw helper marks document dirty");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "authoring line draw helper marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "authoring line draw helper invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "authoring line draw helper bumps topology render revision");
    Check(documentState.map.topologyMap.vertices.size() == originalTopologyVertexCount
                  && documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "authoring line draw helper does not directly mutate derived topology");
}

void TestEditorAuthoringLineDrawHelperReusesVerticesAndRejectsZeroLength()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 7, 0, 0);
    const uint64_t originalRevision = state.topologyRenderRevision;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "authoring line draw helper creates a line from an existing endpoint");
    Check(authoringGraph.vertices.size() == 2,
          "authoring line draw helper reuses exact-coordinate endpoint vertex");
    const game::SectorAuthoringLine* line =
            game::FindSectorAuthoringLine(authoringGraph, lineId);
    Check(line != nullptr && line->startVertexId == 7,
          "authoring line draw helper connects reused start vertex");

    const std::size_t vertexCount = authoringGraph.vertices.size();
    const std::size_t lineCount = authoringGraph.lines.size();
    Check(!game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 0}),
          "authoring line draw helper rejects zero-length lines");
    Check(authoringGraph.vertices.size() == vertexCount
                  && authoringGraph.lines.size() == lineCount,
          "rejected zero-length authoring line leaves graph unchanged");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "rejected zero-length authoring line does not invalidate cache again");
    Check(documentState.map.topologyMap.vertices.empty() && documentState.map.topologyMap.lineDefs.empty(),
          "authoring line draw helper zero-length rejection does not mutate derived topology");
}

const game::SectorAuthoringLine* FindAuthoringLineWithEndpoints(
        const game::SectorAuthoringGraph& graph,
        int startVertexId,
        int endVertexId);

int FindAuthoringVertexIdAt(
        const game::SectorAuthoringGraph& graph,
        game::SectorCoord x,
        game::SectorCoord y);

void TestEditorAuthoringLineDrawHelperAutoSplitsEndpointOnLine()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 96, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    game::SectorAuthoringLine* originalLine =
            game::FindSectorAuthoringLine(authoringGraph, 10);
    Check(originalLine != nullptr, "auto-split line draw setup has original line");
    originalLine->flags.blocksPlayer = true;
    originalLine->special.type = 7;
    originalLine->special.tag = "split-source";

    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.middle = WallPart("split_middle", 1.0f, 2.0f, 3.0f, 4.0f);
    authoringGraph.lineSides.push_back(side);

    int lineId = -1;
    game::SectorEditorAuthoringLineSegmentResult result;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{32, 0},
                  game::SectorTopologyCoordPoint{32, 64},
                  &lineId,
                  &result),
          "authoring line draw helper auto-splits an existing line endpoint");

    const int splitVertexId = FindAuthoringVertexIdAt(authoringGraph, 32, 0);
    const int newVertexId = FindAuthoringVertexIdAt(authoringGraph, 32, 64);
    Check(splitVertexId > 0 && newVertexId > 0,
          "auto-split line draw creates/reuses expected endpoint vertices");
    Check(result.startVertexId == splitVertexId && result.endVertexId == newVertexId,
          "auto-split line draw returns materialized endpoint IDs");
    Check(game::FindSectorAuthoringLine(authoringGraph, 10) == nullptr,
          "auto-split line draw removes the original containing line");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, 1, splitVertexId) != nullptr
                  && FindAuthoringLineWithEndpoints(authoringGraph, splitVertexId, 2) != nullptr,
          "auto-split line draw creates child source-line segments");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, splitVertexId, newVertexId) != nullptr,
          "auto-split line draw creates the requested new line from the split vertex");

    for (const game::SectorAuthoringLine& line : authoringGraph.lines) {
        if (line.id == lineId) {
            continue;
        }
        Check(line.flags.blocksPlayer
                      && line.special.type == 7
                      && line.special.tag == "split-source",
              "auto-split source child line preserves source metadata");
        const game::SectorAuthoringLineSide* childSide =
                game::FindSectorAuthoringLineSide(
                        authoringGraph,
                        game::SectorAuthoringSideId{line.id, game::SectorTopologySideKind::Front});
        Check(childSide != nullptr
                      && childSide->middle.textureId == "split_middle",
              "auto-split source child line preserves side material metadata");
    }
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "auto-split line draw marks document dirty and unsaved");
    Check(!state.topologyRenderCache.valid,
          "auto-split line draw invalidates topology render cache");
}

void TestEditorAuthoringLineDrawHelperAutoSplitsTwoEndpointsOnSameBoundary()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 128, 0);
    AddAuthoringVertexWithId(authoringGraph, 3, 128, 128);
    AddAuthoringVertexWithId(authoringGraph, 4, 0, 128);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    AddAuthoringLineWithId(authoringGraph, 11, 2, 3);
    AddAuthoringLineWithId(authoringGraph, 12, 3, 4);
    AddAuthoringLineWithId(authoringGraph, 13, 4, 1);

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{32, 0},
                  game::SectorTopologyCoordPoint{32, 64},
                  &lineId),
          "authoring line draw helper auto-splits first room endpoint on boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{32, 64},
                  game::SectorTopologyCoordPoint{96, 64},
                  &lineId),
          "authoring line draw helper creates room side away from boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{96, 64},
                  game::SectorTopologyCoordPoint{96, 0},
                  &lineId),
          "authoring line draw helper auto-splits second room endpoint on boundary");

    const int firstVertexId = FindAuthoringVertexIdAt(authoringGraph, 32, 0);
    const int secondVertexId = FindAuthoringVertexIdAt(authoringGraph, 96, 0);
    Check(firstVertexId > 0 && secondVertexId > 0,
          "two-endpoint auto-split creates both boundary vertices");
    Check(game::FindSectorAuthoringLine(authoringGraph, 10) == nullptr,
          "two-endpoint auto-split removes original boundary line");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, 1, firstVertexId) != nullptr,
          "two-endpoint auto-split keeps first boundary child");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, firstVertexId, secondVertexId) != nullptr,
          "two-endpoint auto-split materializes selectable middle boundary child");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, secondVertexId, 2) != nullptr,
          "two-endpoint auto-split keeps final boundary child");
    const game::SectorAuthoringLine* middleBoundary =
            FindAuthoringLineWithEndpoints(authoringGraph, firstVertexId, secondVertexId);
    Check(middleBoundary != nullptr && middleBoundary->id != lineId,
          "two-endpoint auto-split middle boundary child is distinct from final drawn room side");
    Check(documentState.derivation.authoringDerivation.success,
          "two-endpoint auto-split room workflow derives valid topology");
}

const game::SectorAuthoringLine* FindAuthoringLineWithEndpoints(
        const game::SectorAuthoringGraph& graph,
        int startVertexId,
        int endVertexId);

int FindAuthoringVertexIdAt(
        const game::SectorAuthoringGraph& graph,
        game::SectorCoord x,
        game::SectorCoord y);

void TestEditorAuthoringLineToolChainCommitsConnectedSegments()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});
    const std::size_t originalTopologyVertexCount = documentState.map.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();
    const uint64_t originalRevision = state.topologyRenderRevision;

    game::SectorEditorAuthoringLineToolClickResult first =
            game::ClickSectorEditorAuthoringLineTool(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState, game::SectorTopologyCoordPoint{0, 0});
    Check(first.status == game::SectorEditorAuthoringLineToolClickStatus::StartedChain,
          "line chain first click starts pending chain");
    Check(state.pendingAuthoringLine.active
                  && state.pendingAuthoringLine.startPoint.x == 0
                  && state.pendingAuthoringLine.startPoint.y == 0,
          "line chain starts at first point");

    game::SectorEditorAuthoringLineToolClickResult second =
            game::ClickSectorEditorAuthoringLineTool(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState, game::SectorTopologyCoordPoint{64, 0});
    Check(second.status == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "line chain second click creates first segment");
    Check(state.pendingAuthoringLine.active
                  && state.pendingAuthoringLine.startPoint.x == 64
                  && state.pendingAuthoringLine.startPoint.y == 0,
          "line chain advances to first segment endpoint");

    game::SectorEditorAuthoringLineToolClickResult third =
            game::ClickSectorEditorAuthoringLineTool(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState, game::SectorTopologyCoordPoint{64, 64});
    Check(third.status == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "line chain third click creates second segment");
    Check(state.pendingAuthoringLine.active
                  && state.pendingAuthoringLine.startPoint.x == 64
                  && state.pendingAuthoringLine.startPoint.y == 64,
          "line chain advances to second segment endpoint");

    const int a = FindAuthoringVertexIdAt(authoringGraph, 0, 0);
    const int b = FindAuthoringVertexIdAt(authoringGraph, 64, 0);
    const int c = FindAuthoringVertexIdAt(authoringGraph, 64, 64);
    Check(a > 0 && b > 0 && c > 0, "line chain creates expected vertices");
    Check(authoringGraph.lines.size() == 2, "line chain creates two authoring lines");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, a, b) != nullptr,
          "line chain first line connects A to B");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, b, c) != nullptr,
          "line chain second line connects B to C");
    Check(second.segment.endVertexId == b && third.segment.startVertexId == b,
          "line chain reuses B as the shared endpoint vertex");
    Check(documentState.lifecycle.topologyDocumentDirty, "line chain marks document dirty through authoring edit path");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "line chain leaves open graph derivation stale after failed refresh");
    Check(state.topologyRenderRevision == originalRevision + 2,
          "line chain invalidates render cache once per committed segment");
    Check(documentState.map.topologyMap.vertices.size() == originalTopologyVertexCount
                  && documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "line chain does not directly mutate derived topology");
}

void TestEditorAuthoringLineToolCancelStartsNewChain()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    state.currentTool = game::SectorEditorTool::AuthoringLine;

    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{0, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::StartedChain,
          "line chain cancel setup starts chain");
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{64, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "line chain cancel setup commits first segment");

    game::CancelSectorEditorAuthoringLineToolChain(state);
    Check(!state.pendingAuthoringLine.active, "line chain cancel clears pending chain");
    Check(state.currentTool == game::SectorEditorTool::AuthoringLine,
          "line chain cancel keeps authoring line tool selected");

    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{128, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::StartedChain,
          "line chain click after cancel starts a new chain");
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{192, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "line chain new chain commits separate segment");

    const int a = FindAuthoringVertexIdAt(authoringGraph, 0, 0);
    const int b = FindAuthoringVertexIdAt(authoringGraph, 64, 0);
    const int c = FindAuthoringVertexIdAt(authoringGraph, 128, 0);
    const int d = FindAuthoringVertexIdAt(authoringGraph, 192, 0);
    Check(authoringGraph.lines.size() == 2,
          "line chain after cancel has only two committed segments");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, a, b) != nullptr,
          "line chain before cancel creates A to B");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, c, d) != nullptr,
          "line chain after cancel creates C to D");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, b, c) == nullptr,
          "line chain cancel prevents accidental B to C segment");
}

void TestEditorAuthoringLineToolRejectsZeroLengthWithoutEndingChain()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{0, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::StartedChain,
          "zero-length line chain setup starts chain");

    const game::SectorEditorAuthoringLineToolClickResult zero =
            game::ClickSectorEditorAuthoringLineTool(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState, game::SectorTopologyCoordPoint{0, 0});
    Check(zero.status == game::SectorEditorAuthoringLineToolClickStatus::ZeroLength,
          "line chain rejects zero-length segment");
    Check(authoringGraph.lines.empty(), "zero-length line chain creates no line");
    Check(state.pendingAuthoringLine.active
                  && state.pendingAuthoringLine.startPoint.x == 0
                  && state.pendingAuthoringLine.startPoint.y == 0,
          "zero-length line chain remains active at previous point");
    Check(TextContains(state.pendingAuthoringLine.errorMessage.c_str(), "non-zero"),
          "zero-length line chain stores clear error message");

    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{64, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "line chain continues after zero-length rejection");
    const int a = FindAuthoringVertexIdAt(authoringGraph, 0, 0);
    const int b = FindAuthoringVertexIdAt(authoringGraph, 64, 0);
    Check(authoringGraph.lines.size() == 1
                  && FindAuthoringLineWithEndpoints(authoringGraph, a, b) != nullptr,
          "line chain creates A to B after zero-length rejection");
}

void TestEditorAuthoringLineToolLoopClosureRemainsActive()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{0, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::StartedChain,
          "loop closure setup starts chain");
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{64, 0}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "loop closure creates first segment");
    Check(game::ClickSectorEditorAuthoringLineTool(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  selectionState,
                  game::SectorTopologyCoordPoint{0, 64}).status
                  == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "loop closure creates second segment");
    const game::SectorEditorAuthoringLineToolClickResult close =
            game::ClickSectorEditorAuthoringLineTool(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState, game::SectorTopologyCoordPoint{0, 0});
    Check(close.status == game::SectorEditorAuthoringLineToolClickStatus::CreatedSegment,
          "loop closure creates final segment back to first point");

    const int a = FindAuthoringVertexIdAt(authoringGraph, 0, 0);
    const int b = FindAuthoringVertexIdAt(authoringGraph, 64, 0);
    const int c = FindAuthoringVertexIdAt(authoringGraph, 0, 64);
    Check(authoringGraph.lines.size() == 3, "loop closure creates three lines");
    Check(FindAuthoringLineWithEndpoints(authoringGraph, c, a) != nullptr,
          "loop closure final line connects C to A");
    Check(state.pendingAuthoringLine.active
                  && state.pendingAuthoringLine.startVertexId == a
                  && state.pendingAuthoringLine.startPoint.x == 0
                  && state.pendingAuthoringLine.startPoint.y == 0,
          "loop closure leaves chain active from closed point");

    const game::SectorAuthoringDerivationResult derivation =
            game::DeriveSectorTopologyMapFromAuthoringGraph(authoringGraph);
    Check(derivation.success && !derivation.topology.sectors.empty(),
          "loop closure graph derives a valid face");
    Check(b > 0, "loop closure keeps middle vertex valid");
}

const game::SectorAuthoringLine* FindAuthoringLineWithEndpoints(
        const game::SectorAuthoringGraph& graph,
        int startVertexId,
        int endVertexId)
{
    for (const game::SectorAuthoringLine& line : graph.lines) {
        if (line.startVertexId == startVertexId && line.endVertexId == endVertexId) {
            return &line;
        }
    }
    return nullptr;
}

int CountAuthoringVerticesAt(
        const game::SectorAuthoringGraph& graph,
        game::SectorCoord x,
        game::SectorCoord y)
{
    int count = 0;
    for (const game::SectorAuthoringVertex& vertex : graph.vertices) {
        if (vertex.x == x && vertex.y == y) {
            ++count;
        }
    }
    return count;
}

int FindAuthoringVertexIdAt(
        const game::SectorAuthoringGraph& graph,
        game::SectorCoord x,
        game::SectorCoord y)
{
    for (const game::SectorAuthoringVertex& vertex : graph.vertices) {
        if (vertex.x == x && vertex.y == y) {
            return vertex.id;
        }
    }
    return -1;
}

void CheckSplitLineAtPoint(
        game::SectorCoord ax,
        game::SectorCoord ay,
        game::SectorCoord bx,
        game::SectorCoord by,
        game::SectorCoord vx,
        game::SectorCoord vy,
        const char* description)
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, ax, ay);
    AddAuthoringVertexWithId(graph, 2, bx, by);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    game::SectorAuthoringInsertVertexResult result;
    Check(game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{vx, vy},
                  &result),
          description);
    Check(result.status == game::SectorAuthoringInsertVertexStatus::Inserted,
          "split authoring line reports inserted status");
    Check(result.vertexId > 0 && result.firstLineId > 0 && result.secondLineId > 0,
          "split authoring line returns stable positive IDs");
    Check(game::FindSectorAuthoringLine(graph, 10) == nullptr,
          "split authoring line removes original line");

    const game::SectorAuthoringVertex* vertex =
            game::FindSectorAuthoringVertex(graph, result.vertexId);
    Check(vertex != nullptr && vertex->x == vx && vertex->y == vy,
          "split authoring line creates vertex at expected point");
    Check(FindAuthoringLineWithEndpoints(graph, 1, result.vertexId) != nullptr,
          "split authoring line creates first child preserving orientation");
    Check(FindAuthoringLineWithEndpoints(graph, result.vertexId, 2) != nullptr,
          "split authoring line creates second child preserving orientation");
    Check(!game::HasSectorAuthoringValidationErrors(
                  game::ValidateSectorAuthoringGraphReferences(graph)),
          "split authoring line leaves graph references valid");
}

void TestInsertVertexSplitsHorizontalVerticalAndDiagonalLines()
{
    CheckSplitLineAtPoint(0, 0, 10, 0, 4, 0, "insert vertex splits horizontal authoring line");
    CheckSplitLineAtPoint(2, 0, 2, 10, 2, 6, "insert vertex splits vertical authoring line");
    CheckSplitLineAtPoint(0, 0, 10, 10, 4, 4, "insert vertex splits diagonal authoring line at exact grid point");
}

void TestInsertVertexRejectsOffLineAndEndpoints()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 10, 10);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    const game::SectorAuthoringGraph original = graph;
    game::SectorAuthoringInsertVertexResult result;
    Check(!game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{4, 5},
                  &result),
          "insert vertex rejects off-line point");
    Check(result.status == game::SectorAuthoringInsertVertexStatus::OffLine,
          "off-line insert reports off-line status");
    Check(graph.vertices.size() == original.vertices.size()
                  && graph.lines.size() == original.lines.size()
                  && CountAuthoringVerticesAt(graph, 4, 5) == 0,
          "off-line insert leaves graph unchanged and creates no off-line vertex");

    Check(!game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{0, 0},
                  &result),
          "insert vertex rejects start endpoint");
    Check(result.status == game::SectorAuthoringInsertVertexStatus::Endpoint,
          "start endpoint insert reports endpoint status");
    Check(!game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{10, 10},
                  &result),
          "insert vertex rejects end endpoint");
    Check(result.status == game::SectorAuthoringInsertVertexStatus::Endpoint,
          "end endpoint insert reports endpoint status");
    Check(graph.vertices.size() == original.vertices.size()
                  && graph.lines.size() == original.lines.size(),
          "endpoint insert leaves graph unchanged");
}

void TestInsertVertexReusesExistingInteriorVertex()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 10, 0);
    AddAuthoringVertexWithId(graph, 7, 4, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    game::SectorAuthoringInsertVertexResult result;
    Check(game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{4, 0},
                  &result),
          "insert vertex reuses existing interior vertex");
    Check(result.vertexId == 7 && result.reusedExistingVertex,
          "insert vertex reports reused existing vertex ID");
    Check(CountAuthoringVerticesAt(graph, 4, 0) == 1,
          "insert vertex does not create duplicate vertex at reused coordinate");
    Check(FindAuthoringLineWithEndpoints(graph, 1, 7) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, 7, 2) != nullptr,
          "insert vertex splits through reused vertex");
}

void TestInsertVertexCopiesLineFlagsSpecialAndSideMaterials()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 16, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    game::SectorAuthoringLine* line = game::FindSectorAuthoringLine(graph, 10);
    Check(line != nullptr, "line exists for metadata split test");
    line->flags.blocksPlayer = true;
    line->special.type = 12;
    line->special.tag = "door";

    game::SectorAuthoringLineSide front;
    front.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    front.wall = WallPart("front_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    front.lower = WallPart("front_lower", 2.0f, 3.0f, 4.0f, 5.0f);
    front.upper = WallPart("front_upper", 3.0f, 4.0f, 5.0f, 6.0f);
    front.middle = WallPart("front_middle", 4.0f, 5.0f, 6.0f, 7.0f);
    game::SectorAuthoringLineSide back;
    back.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Back};
    back.wall = WallPart("back_wall", 1.5f, 2.5f, 3.5f, 4.5f);
    back.lower = WallPart("back_lower", 2.5f, 3.5f, 4.5f, 5.5f);
    back.upper = WallPart("back_upper", 3.5f, 4.5f, 5.5f, 6.5f);
    back.middle = WallPart("back_middle", 4.5f, 5.5f, 6.5f, 7.5f);
    graph.lineSides.push_back(front);
    graph.lineSides.push_back(back);

    game::SectorAuthoringInsertVertexResult result;
    Check(game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  10,
                  game::SectorTopologyCoordPoint{8, 0},
                  &result),
          "insert vertex copies line metadata and side materials");
    Check(game::FindSectorAuthoringLineSide(
                  graph,
                  game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front}) == nullptr
                  && game::FindSectorAuthoringLineSide(
                          graph,
                          game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Back}) == nullptr,
          "insert vertex removes active side metadata for old line");

    const int childIds[2] = {result.firstLineId, result.secondLineId};
    for (int childId : childIds) {
        const game::SectorAuthoringLine* child = game::FindSectorAuthoringLine(graph, childId);
        Check(child != nullptr && child->flags.blocksPlayer,
              "insert vertex copies blocksPlayer flag to child line");
        Check(child != nullptr && child->special.type == 12 && child->special.tag == "door",
              "insert vertex copies line special metadata to child line");
        const game::SectorAuthoringLineSide* childFront = game::FindSectorAuthoringLineSide(
                graph,
                game::SectorAuthoringSideId{childId, game::SectorTopologySideKind::Front});
        const game::SectorAuthoringLineSide* childBack = game::FindSectorAuthoringLineSide(
                graph,
                game::SectorAuthoringSideId{childId, game::SectorTopologySideKind::Back});
        Check(childFront != nullptr && childBack != nullptr,
              "insert vertex copies both child side metadata records");
        if (childFront != nullptr) {
            CheckWallPartCopied(childFront->wall, front.wall, "insert vertex copies front wall material");
            CheckWallPartCopied(childFront->lower, front.lower, "insert vertex copies front lower material");
            CheckWallPartCopied(childFront->upper, front.upper, "insert vertex copies front upper material");
            CheckWallPartCopied(childFront->middle, front.middle, "insert vertex copies front middle material");
        }
        if (childBack != nullptr) {
            CheckWallPartCopied(childBack->wall, back.wall, "insert vertex copies back wall material");
            CheckWallPartCopied(childBack->lower, back.lower, "insert vertex copies back lower material");
            CheckWallPartCopied(childBack->upper, back.upper, "insert vertex copies back upper material");
            CheckWallPartCopied(childBack->middle, back.middle, "insert vertex copies back middle material");
        }
    }
}

void TestInsertVertexDerivationStillWorksForRectangle()
{
    game::SectorAuthoringGraph graph;
    game::SectorEditorAuthoringRectangleResult rectangle;
    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &rectangle),
          "insert vertex derivation test creates rectangle");

    game::SectorAuthoringInsertVertexResult insert;
    Check(game::InsertSectorAuthoringVertexOnLine(
                  graph,
                  rectangle.lineIds[0],
                  game::SectorTopologyCoordPoint{32, 0},
                  &insert),
          "insert vertex splits rectangle edge");
    const game::SectorAuthoringDerivationResult result =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(result.success, "rectangle with inserted edge vertex derives successfully");
    Check(!game::HasSectorTopologyValidationErrors(
                  game::ValidateSectorTopologyMap(result.topology)),
          "rectangle with inserted edge vertex validates");
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(result.topology, geometry, &error),
          "rectangle with inserted edge vertex builds generated geometry");
}

void TestEditorAuthoringInsertVertexMarksDirtyInvalidatesAndSelectsVertex()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});
    game::SectorEditorAuthoringRectangleResult rectangle;
    Check(game::AddSectorEditorAuthoringRectangle(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &rectangle),
          "editor insert vertex wrapper setup creates rectangle");
    documentState.lifecycle.topologyDocumentDirty = false;
    documentState.lifecycle.hasUnsavedChanges = false;
    state.topologyRenderCache.valid = true;
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();

    game::SectorAuthoringInsertVertexResult result;
    Check(game::InsertSectorEditorAuthoringVertexOnLine(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            rectangle.lineIds[0],
                  game::SectorTopologyCoordPoint{32, 0},
                  &result),
          "editor insert vertex wrapper splits selected authoring line");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && selectionState.selectedAuthoring.vertexId == result.vertexId,
          "editor insert vertex wrapper selects inserted vertex");
    Check(documentState.lifecycle.topologyDocumentDirty && documentState.lifecycle.hasUnsavedChanges,
          "editor insert vertex wrapper marks document dirty and unsaved");
    Check(!state.topologyRenderCache.valid
                  && state.topologyRenderRevision == originalRevision + 1,
          "editor insert vertex wrapper invalidates topology render cache");
    Check(documentState.derivation.authoringDerivation.success
                  && documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent
                  && !documentState.derivation.authoringDerivedTopologyStale,
          "editor insert vertex wrapper refreshes derivation through authoring path");
    Check(documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount + 1,
          "editor insert vertex wrapper updates derived topology only through derivation refresh");
}

void TestAuthoringRectangleHelperCreatesClosedLoop()
{
    game::SectorAuthoringGraph graph;
    game::SectorEditorAuthoringRectangleResult result;

    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{10, 6},
                  &result),
          "authoring rectangle helper creates a rectangle");
    Check(graph.vertices.size() == 4, "authoring rectangle helper creates four vertices");
    Check(graph.lines.size() == 4, "authoring rectangle helper creates four lines");

    const game::SectorCoord expectedX[4] = {0, 10, 10, 0};
    const game::SectorCoord expectedY[4] = {0, 0, 6, 6};
    for (int index = 0; index < 4; ++index) {
        const game::SectorAuthoringVertex* vertex =
                game::FindSectorAuthoringVertex(graph, result.vertexIds[index]);
        Check(vertex != nullptr && vertex->id > 0,
              "authoring rectangle helper returns stable positive vertex IDs");
        Check(vertex != nullptr && vertex->x == expectedX[index] && vertex->y == expectedY[index],
              "authoring rectangle helper uses deterministic min/max corner order");

        const game::SectorAuthoringLine* line =
                game::FindSectorAuthoringLine(graph, result.lineIds[index]);
        Check(line != nullptr && line->id > 0,
              "authoring rectangle helper returns stable positive line IDs");
        Check(line != nullptr
                      && line->startVertexId == result.vertexIds[index]
                      && line->endVertexId == result.vertexIds[(index + 1) % 4],
              "authoring rectangle helper connects a closed loop in deterministic order");
    }

    Check(!game::HasSectorAuthoringValidationErrors(
                  game::ValidateSectorAuthoringGraphReferences(graph)),
          "authoring rectangle helper leaves graph references valid");
}

void TestAuthoringRectangleHelperReversedDragIsDeterministic()
{
    game::SectorAuthoringGraph graph;
    game::SectorEditorAuthoringRectangleResult result;

    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{10, 6},
                  game::SectorTopologyCoordPoint{0, 0},
                  &result),
          "authoring rectangle helper accepts reversed drag direction");

    const game::SectorCoord expectedX[4] = {0, 10, 10, 0};
    const game::SectorCoord expectedY[4] = {0, 0, 6, 6};
    for (int index = 0; index < 4; ++index) {
        const game::SectorAuthoringVertex* vertex =
                game::FindSectorAuthoringVertex(graph, result.vertexIds[index]);
        Check(vertex != nullptr && vertex->x == expectedX[index] && vertex->y == expectedY[index],
              "authoring rectangle helper normalizes reversed rectangle coordinates");
        const game::SectorAuthoringLine* line =
                game::FindSectorAuthoringLine(graph, result.lineIds[index]);
        Check(line != nullptr
                      && line->startVertexId == result.vertexIds[index]
                      && line->endVertexId == result.vertexIds[(index + 1) % 4],
              "authoring rectangle helper preserves deterministic line orientation after reversed drag");
    }
}

void TestAuthoringRectangleHelperRejectsZeroSize()
{
    const game::SectorTopologyCoordPoint cases[3][2] = {
            {{0, 0}, {0, 0}},
            {{0, 0}, {0, 6}},
            {{0, 0}, {10, 0}}
    };

    for (const auto& testCase : cases) {
        game::SectorAuthoringGraph graph;
        Check(!game::CreateSectorAuthoringRectangle(graph, testCase[0], testCase[1]),
              "authoring rectangle helper rejects zero-width or zero-height rectangles");
        Check(graph.vertices.empty() && graph.lines.empty(),
              "rejected zero-size authoring rectangle leaves graph unchanged");
    }
}

void TestAuthoringRectangleHelperDerivesValidTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});

    Check(game::AddSectorEditorAuthoringRectangle(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64}),
          "authoring rectangle editor helper commits rectangle");
    Check(documentState.derivation.authoringDerivation.success,
          "authoring rectangle derives valid topology through editor refresh");
    Check(!game::HasSectorTopologyValidationErrors(
                  game::ValidateSectorTopologyMap(documentState.map.topologyMap)),
          "authoring rectangle derived topology validates");
    Check(authoringGraph.faceAnchors.size() == 1,
          "authoring rectangle reconciliation synthesizes a face anchor");
    const game::SectorAuthoringFaceAnchor* anchor = authoringGraph.faceAnchors.empty()
            ? nullptr
            : &authoringGraph.faceAnchors.front();
    Check(anchor != nullptr
                  && anchor->floorTextureId == "floor"
                  && anchor->ceilingTextureId == "ceiling"
                  && anchor->defaultWall.textureId == "wall",
          "authoring rectangle synthesized face anchor gets normal default materials");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(documentState.map.topologyMap, geometry, &error),
          "authoring rectangle derived topology builds generated geometry");
    Check(!geometry.surfaces.empty(),
          "authoring rectangle derived topology produces generated surfaces");
}

void TestAuthoringNestedRectanglesDeriveThroughExistingBehavior()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    game::InitializeSectorEditorAuthoringStateFromTopology(authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), game::SectorTopologyMap{});

    Check(game::AddSectorEditorAuthoringRectangle(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{128, 128}),
          "nested rectangle test commits outer rectangle");
    Check(game::AddSectorEditorAuthoringRectangle(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorTopologyCoordPoint{32, 32},
                  game::SectorTopologyCoordPoint{96, 96}),
          "nested rectangle test commits inner rectangle");

    Check(documentState.derivation.authoringDerivation.success,
          "nested rectangles derive through existing nested-loop behavior");
    Check(!game::HasSectorTopologyValidationErrors(
                  game::ValidateSectorTopologyMap(documentState.map.topologyMap)),
          "nested rectangle derived topology validates");
    Check(authoringGraph.faceAnchors.size() == 2,
          "nested rectangle reconciliation synthesizes expected face anchors");
    Check(documentState.map.topologyMap.sectors.size() == 2,
          "nested rectangles produce expected visible faces");
}

void TestEditorAuthoringRectangleCommitMarksDirtyAndReusesVertices()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 7, 0, 0);
    const uint64_t originalRevision = state.topologyRenderRevision;

    game::SectorEditorAuthoringRectangleResult result;
    Check(game::AddSectorEditorAuthoringRectangle(
                  state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
                  authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &result),
          "authoring rectangle editor helper commits a rectangle");

    Check(authoringGraph.vertices.size() == 4,
          "authoring rectangle editor helper reuses an existing corner vertex");
    Check(result.vertexIds[0] == 7,
          "authoring rectangle editor helper records reused first corner vertex");
    Check(authoringGraph.lines.size() == 4,
          "authoring rectangle editor helper creates four authoring lines");
    Check(documentState.lifecycle.topologyDocumentDirty,
          "authoring rectangle editor helper marks document dirty");
    Check(!documentState.derivation.authoringDerivedTopologyStale
                  && documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "authoring rectangle editor helper refreshes derivation through existing edit path");
    Check(!state.topologyRenderCache.valid,
          "authoring rectangle editor helper invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision > originalRevision,
          "authoring rectangle editor helper bumps topology render revision");
}

void TestAuthoringRectangleSplitsCrossedAuthoringLineAndOwnEdges()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 32, -16);
    AddAuthoringVertexWithId(graph, 2, 32, 80);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    game::SectorAuthoringLine* source = game::FindSectorAuthoringLine(graph, 10);
    Check(source != nullptr, "rectangle split metadata setup has source line");
    source->flags.blocksPlayer = true;
    source->special.type = 4;
    source->special.tag = "portal";

    game::SectorAuthoringLineSide sourceSide;
    sourceSide.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    sourceSide.middle = WallPart("source_middle", 1.0f, 2.0f, 3.0f, 4.0f);
    graph.lineSides.push_back(sourceSide);

    game::SectorEditorAuthoringRectangleResult result;
    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &result),
          "rectangle crossing authoring line inserts successfully");

    const int topSplit = FindAuthoringVertexIdAt(graph, 32, 0);
    const int bottomSplit = FindAuthoringVertexIdAt(graph, 32, 64);
    Check(topSplit > 0 && bottomSplit > 0,
          "rectangle crossing creates shared authoring intersection vertices");
    Check(game::FindSectorAuthoringLine(graph, 10) == nullptr,
          "rectangle crossing removes original crossed source line");
    Check(FindAuthoringLineWithEndpoints(graph, 1, topSplit) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, topSplit, bottomSplit) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, bottomSplit, 2) != nullptr,
          "rectangle crossing splits existing source line into authoring children");
    Check(FindAuthoringLineWithEndpoints(graph, result.vertexIds[0], topSplit) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, topSplit, result.vertexIds[1]) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, result.vertexIds[2], bottomSplit) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, bottomSplit, result.vertexIds[3]) != nullptr,
          "rectangle crossing splits rectangle edges into authoring subsegments");
    Check(result.insertedLineIds.size() == 6,
          "rectangle crossing reports all inserted rectangle subsegment line IDs");

    for (const game::SectorAuthoringLine& line : graph.lines) {
        const bool sourceChild =
                (line.startVertexId == 1 && line.endVertexId == topSplit)
                || (line.startVertexId == topSplit && line.endVertexId == bottomSplit)
                || (line.startVertexId == bottomSplit && line.endVertexId == 2);
        if (!sourceChild) {
            continue;
        }
        Check(line.flags.blocksPlayer
                      && line.special.type == 4
                      && line.special.tag == "portal",
              "rectangle split source child preserves line flags and special data");
        const game::SectorAuthoringLineSide* childSide =
                game::FindSectorAuthoringLineSide(
                        graph,
                        game::SectorAuthoringSideId{line.id, game::SectorTopologySideKind::Front});
        Check(childSide != nullptr
                      && childSide->middle.textureId == "source_middle",
              "rectangle split source child preserves side material metadata");
    }

    Check(!game::HasSectorAuthoringValidationErrors(
                  game::ValidateSectorAuthoringGraphReferences(graph)),
          "rectangle crossing leaves split authoring graph references valid");
}

void TestAuthoringRectangleSortsMultipleIntersectionsOnOneEdge()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 16, -16);
    AddAuthoringVertexWithId(graph, 2, 16, 80);
    AddAuthoringVertexWithId(graph, 3, 48, -16);
    AddAuthoringVertexWithId(graph, 4, 48, 80);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    AddAuthoringLineWithId(graph, 11, 3, 4);

    game::SectorEditorAuthoringRectangleResult result;
    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &result),
          "rectangle with multiple crossings inserts successfully");

    const int a = FindAuthoringVertexIdAt(graph, 16, 0);
    const int b = FindAuthoringVertexIdAt(graph, 48, 0);
    Check(a > 0 && b > 0, "multiple rectangle crossings create top-edge split vertices");
    Check(FindAuthoringLineWithEndpoints(graph, result.vertexIds[0], a) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, a, b) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, b, result.vertexIds[1]) != nullptr,
          "multiple rectangle crossings sort top-edge subsegments without overlap");
}

void TestAuthoringRectangleCornerOnLineSplitsExistingLine()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, -16, -16);
    AddAuthoringVertexWithId(graph, 2, 16, 16);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    game::SectorEditorAuthoringRectangleResult result;
    Check(game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &result),
          "rectangle corner touching existing line inserts successfully");

    const int corner = FindAuthoringVertexIdAt(graph, 0, 0);
    Check(corner == result.vertexIds[0],
          "rectangle corner touching existing line reuses the corner authoring vertex");
    Check(game::FindSectorAuthoringLine(graph, 10) == nullptr,
          "rectangle corner touching line removes original crossed line");
    Check(FindAuthoringLineWithEndpoints(graph, 1, corner) != nullptr
                  && FindAuthoringLineWithEndpoints(graph, corner, 2) != nullptr,
          "rectangle corner touching line splits source line at the corner vertex");
}

void TestAuthoringRectangleRejectsOverlapAtomically()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    const game::SectorAuthoringGraph original = graph;

    game::SectorEditorAuthoringRectangleResult result;
    Check(!game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &result),
          "rectangle overlapping an existing line is rejected");
    Check(TextContains(result.errorMessage.c_str(), "overlaps"),
          "rectangle overlap rejection reports clear status");
    Check(graph.vertices.size() == original.vertices.size()
                  && graph.lines.size() == original.lines.size()
                  && game::FindSectorAuthoringLine(graph, 10) != nullptr,
          "rejected overlapping rectangle leaves graph unchanged atomically");
}

void TestAuthoringRectangleRejectsNonGridIntersectionAtomically()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 32);
    AddAuthoringLineWithId(graph, 10, 1, 2);
    const game::SectorAuthoringGraph original = graph;

    game::SectorEditorAuthoringRectangleResult result;
    Check(!game::CreateSectorAuthoringRectangle(
                  graph,
                  game::SectorTopologyCoordPoint{33, -10},
                  game::SectorTopologyCoordPoint{80, 50},
                  &result),
          "rectangle with non-grid intersection is rejected");
    Check(TextContains(result.errorMessage.c_str(), "not representable"),
          "rectangle non-grid rejection reports clear status");
    Check(graph.vertices.size() == original.vertices.size()
                  && graph.lines.size() == original.lines.size()
                  && CountAuthoringVerticesAt(graph, 33, 16) == 0,
          "rejected non-grid rectangle leaves graph unchanged atomically");
}

void TestEditorAuthoringLinePickingFindsNearestValidLine()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringVertexWithId(graph, 3, 0, 64);
    AddAuthoringVertexWithId(graph, 4, 64, 64);
    AddAuthoringLineWithId(graph, 20, 1, 2);
    AddAuthoringLineWithId(graph, 10, 3, 4);
    AddAuthoringLineWithId(graph, 30, 1, 99);

    int lineId = -1;
    Check(game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 0.125f},
                  0.25f,
                  &lineId),
          "authoring line picking finds a nearby valid line");
    Check(lineId == 20, "authoring line picking returns the nearest line ID");

    lineId = -1;
    Check(!game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 8.0f},
                  0.25f,
                  &lineId),
          "authoring line picking rejects points outside the threshold");
    Check(lineId == -1, "authoring line picking leaves output unchanged on miss");

    Check(!game::FindSectorEditorAuthoringLineNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  -1.0f),
          "authoring line picking rejects negative thresholds");
}

void TestEditorAuthoringDeleteSelectedLineOnlyMutatesGraphAndInvalidates()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringVertexWithId(authoringGraph, 3, 64, 64);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    AddAuthoringLineWithId(authoringGraph, 20, 2, 3);

    game::SectorAuthoringLineSide frontSide;
    frontSide.id.lineId = 10;
    frontSide.id.side = game::SectorTopologySideKind::Front;
    authoringGraph.lineSides.push_back(frontSide);
    game::SectorAuthoringLineSide otherSide;
    otherSide.id.lineId = 20;
    otherSide.id.side = game::SectorTopologySideKind::Back;
    authoringGraph.lineSides.push_back(otherSide);

    Check(game::SelectSectorEditorAuthoringLine(authoringGraph, selectionState, 10),
          "delete selected authoring line setup selects line");
    Check(game::SetHoveredSectorEditorAuthoringLine(authoringGraph, selectionState, 10),
          "delete selected authoring line setup hovers line");
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = documentState.map.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();

    Check(game::DeleteSectorEditorSelectedAuthoringLine(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState),
          "delete selected authoring line helper deletes the selected line");
    Check(game::FindSectorAuthoringLine(authoringGraph, 10) == nullptr,
          "delete selected authoring line removes the line from the graph");
    Check(game::FindSectorAuthoringLine(authoringGraph, 20) != nullptr,
          "delete selected authoring line preserves other lines");
    Check(authoringGraph.vertices.size() == 3,
          "delete selected authoring line leaves endpoint vertices for vertex pass");
    Check(authoringGraph.lineSides.size() == 1
                  && authoringGraph.lineSides.front().id.lineId == 20,
          "delete selected authoring line removes side metadata for the deleted line");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete selected authoring line prunes deleted selection");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete selected authoring line prunes deleted hover");
    Check(documentState.lifecycle.topologyDocumentDirty, "delete selected authoring line marks document dirty");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "delete selected authoring line marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "delete selected authoring line invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "delete selected authoring line bumps topology render revision");
    Check(documentState.map.topologyMap.vertices.size() == originalTopologyVertexCount
                  && documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "delete selected authoring line does not directly mutate derived topology");

    Check(!game::DeleteSectorEditorSelectedAuthoringLine(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState),
          "delete selected authoring line helper rejects missing selection");
}

void TestEditorAuthoringVertexPickingFindsNearestValidVertex()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 20, 0, 0);
    AddAuthoringVertexWithId(graph, 10, 64, 0);

    constexpr float testViewZoom = 48.0f;
    const float practicalPickDistance = game::SectorWorldToAuthoringDistance(
            game::ScreenVertexSnapPixels / testViewZoom);

    int vertexId = -1;
    game::SectorTopologyCoordPoint point{};
    Check(game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(64), 0.125f},
                  0.25f,
                  &vertexId,
                  &point),
          "authoring vertex picking finds a nearby vertex");
    Check(vertexId == 10 && point.x == 64 && point.y == 0,
          "authoring vertex picking returns nearest vertex ID and point");

    vertexId = -1;
    Check(game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(64), 1.0f},
                  practicalPickDistance,
                  &vertexId,
                  &point),
          "authoring vertex picking succeeds within practical screen-space radius");
    Check(vertexId == 10 && point.x == 64 && point.y == 0,
          "authoring practical radius picking returns nearest vertex ID and point");

    vertexId = -1;
    Check(!game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 4.0f},
                  practicalPickDistance,
                  &vertexId),
          "authoring vertex picking rejects points outside threshold");
    Check(vertexId == -1, "authoring vertex picking leaves output unchanged on miss");

    Check(!game::FindSectorEditorAuthoringVertexNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  -1.0f),
          "authoring vertex picking rejects negative thresholds");
}

void TestEditorAuthoringSelectionPickingPrefersVerticesThenLines()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringVertexWithId(graph, 2, 64, 0);
    AddAuthoringLineWithId(graph, 10, 1, 2);

    constexpr float testViewZoom = 48.0f;
    const float practicalPickDistance = game::SectorWorldToAuthoringDistance(
            game::ScreenVertexSnapPixels / testViewZoom);

    game::SectorAuthoringSelectionTarget target;
    game::SectorTopologyCoordPoint vertexPoint{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{0.0f, 0.0f},
                  0.25f,
                  0.25f,
                  &target,
                  &vertexPoint),
          "authoring selection picking finds an authoring target");
    Check(target.kind == game::SectorAuthoringSelectionKind::Vertex
                  && target.vertexId == 1,
          "authoring selection picking prefers vertex over overlapping line");
    Check(vertexPoint.x == 0 && vertexPoint.y == 0,
          "authoring selection picking returns the picked vertex point");

    target = game::SectorAuthoringSelectionTarget{};
    vertexPoint = game::SectorTopologyCoordPoint{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{1.0f, 0.0f},
                  practicalPickDistance,
                  practicalPickDistance,
                  &target,
                  &vertexPoint),
          "authoring selection practical radius finds an authoring target");
    Check(target.kind == game::SectorAuthoringSelectionKind::Vertex
                  && target.vertexId == 1,
          "authoring selection practical radius prefers vertex over nearby line");
    Check(vertexPoint.x == 0 && vertexPoint.y == 0,
          "authoring selection practical radius returns picked vertex point");

    target = game::SectorAuthoringSelectionTarget{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 0.0f},
                  0.25f,
                  0.25f,
                  &target),
          "authoring selection picking finds an authoring line");
    Check(target.kind == game::SectorAuthoringSelectionKind::Line
                  && target.lineId == 10
                  && target.vertexId == -1,
          "authoring selection picking returns only authoring line targets");

    target = game::SectorAuthoringSelectionTarget{};
    Check(game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 1.0f},
                  0.25f,
                  practicalPickDistance,
                  &target),
          "authoring selection picking finds line outside vertex tolerance");
    Check(target.kind == game::SectorAuthoringSelectionKind::Line
                  && target.lineId == 10,
          "authoring selection line fallback works outside vertex tolerance");

    target = game::MakeSectorAuthoringLineSelectionTarget(10);
    Check(!game::FindSectorEditorAuthoringSelectionNearMapPoint(
                  graph,
                  Vector2{game::SectorCoordToVisibleAuthoring(32), 4.0f},
                  practicalPickDistance,
                  practicalPickDistance,
                  &target),
          "authoring selection picking misses empty space");
    Check(target.kind == game::SectorAuthoringSelectionKind::None,
          "authoring selection picking clears target on miss");
}

void TestEditorAuthoringMoveVertexUpdatesConnectedLinesAndInvalidates()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringVertexWithId(authoringGraph, 3, 0, 64);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    AddAuthoringLineWithId(authoringGraph, 20, 3, 1);
    Check(game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 1),
          "move authoring vertex setup selects vertex");
    Check(game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, 1),
          "move authoring vertex setup hovers vertex");
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = documentState.map.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();

    Check(game::MoveSectorEditorAuthoringVertex(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            1,
                  game::SectorTopologyCoordPoint{16, 16}),
          "move authoring vertex helper moves the selected vertex");

    const game::SectorAuthoringVertex* moved =
            game::FindSectorAuthoringVertex(authoringGraph, 1);
    Check(moved != nullptr && moved->x == 16 && moved->y == 16,
          "move authoring vertex updates vertex coordinates");
    const game::SectorAuthoringLine* firstLine =
            game::FindSectorAuthoringLine(authoringGraph, 10);
    const game::SectorAuthoringLine* secondLine =
            game::FindSectorAuthoringLine(authoringGraph, 20);
    Check(firstLine != nullptr && firstLine->startVertexId == 1 && firstLine->endVertexId == 2,
          "move authoring vertex preserves first connected line endpoint IDs");
    Check(secondLine != nullptr && secondLine->startVertexId == 3 && secondLine->endVertexId == 1,
          "move authoring vertex preserves second connected line endpoint IDs");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && selectionState.selectedAuthoring.vertexId == 1,
          "move authoring vertex preserves valid selection");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::Vertex
                  && selectionState.hoveredAuthoring.vertexId == 1,
          "move authoring vertex preserves valid hover");
    Check(documentState.lifecycle.topologyDocumentDirty, "move authoring vertex marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "move authoring vertex marks unsaved changes");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "move authoring vertex marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "move authoring vertex invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "move authoring vertex bumps topology render revision");
    Check(documentState.map.topologyMap.vertices.size() == originalTopologyVertexCount
                  && documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "move authoring vertex does not directly mutate derived topology");

    const uint64_t afterMoveRevision = state.topologyRenderRevision;
    Check(!game::MoveSectorEditorAuthoringVertex(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            1,
                  game::SectorTopologyCoordPoint{16, 16}),
          "move authoring vertex helper treats unchanged coordinates as no-op");
    Check(state.topologyRenderRevision == afterMoveRevision,
          "unchanged authoring vertex move does not invalidate again");
}

void TestEditorAuthoringMoveNestedLoopVertexRederivesValidTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(authoringGraph, 200, 16, 16, "outer");
    AddFaceAnchor(authoringGraph, 201, 48, 48, "middle");
    AddFaceAnchor(authoringGraph, 202, 96, 96, "inner");
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "nested authoring move setup derives valid topology");
    Check(documentState.derivation.authoringDerivation.success, "nested authoring move setup stores successful derivation");
    Check(documentState.map.topologyMap.sectors.size() == 3, "nested authoring move setup derives three sectors");
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(game::MoveSectorEditorAuthoringVertex(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            6,
                  game::SectorTopologyCoordPoint{152, 40}),
          "nested authoring move helper moves a nested-loop vertex");

    const game::SectorAuthoringVertex* moved =
            game::FindSectorAuthoringVertex(authoringGraph, 6);
    Check(moved != nullptr && moved->x == 152 && moved->y == 40,
          "nested authoring move updates nested-loop vertex coordinates");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "nested authoring move re-derives current topology");
    Check(!documentState.derivation.authoringDerivedTopologyStale,
          "nested authoring move leaves derived topology current after refresh");
    Check(documentState.derivation.authoringDerivation.success,
          "nested authoring move stores successful recursive derivation");
    Check(documentState.map.topologyMap.sectors.size() == 3,
          "nested authoring move keeps three derived sectors");
    Check(CountSectorHoles(documentState.map.topologyMap, 200, "moved nested outer sector extracts loops") == 1,
          "moved nested outer sector keeps one direct hole");
    Check(CountSectorHoles(documentState.map.topologyMap, 201, "moved nested middle sector extracts loops") == 1,
          "moved nested middle sector keeps one direct hole");
    Check(CountSectorHoles(documentState.map.topologyMap, 202, "moved nested inner sector extracts loops") == 0,
          "moved nested inner sector keeps no holes");
    Check(documentState.lifecycle.topologyDocumentDirty, "nested authoring move marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "nested authoring move marks unsaved changes");
    Check(!state.topologyRenderCache.valid,
          "nested authoring move invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "nested authoring move bumps topology render revision once");
}

void TestEditorAuthoringDeleteConnectedVertexIsExplicitlyRejected()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    Check(game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 1),
          "delete connected authoring vertex setup selects vertex");
    const uint64_t originalRevision = state.topologyRenderRevision;

    Check(!game::DeleteSectorEditorSelectedAuthoringVertex(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState),
          "delete selected authoring vertex rejects connected vertices");
    Check(game::FindSectorAuthoringVertex(authoringGraph, 1) != nullptr,
          "delete connected authoring vertex leaves vertex in graph");
    Check(game::FindSectorAuthoringLine(authoringGraph, 10) != nullptr,
          "delete connected authoring vertex leaves connected line in graph");
    Check(!documentState.lifecycle.topologyDocumentDirty,
          "delete connected authoring vertex does not mark document dirty");
    Check(!documentState.lifecycle.hasUnsavedChanges,
          "delete connected authoring vertex does not mark unsaved changes");
    Check(state.topologyRenderRevision == originalRevision,
          "delete connected authoring vertex does not invalidate cache");
    Check(documentState.derivation.authoringDerivationStatus == "Authoring vertex is connected; delete its lines first.",
          "delete connected authoring vertex records explicit safe-delete status");
    Check(documentState.map.topologyMap.vertices.empty() && documentState.map.topologyMap.lineDefs.empty(),
          "delete connected authoring vertex does not directly mutate derived topology");
}

void TestEditorAuthoringDeleteIsolatedVertexOnlyMutatesGraphAndInvalidates()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    AddAuthoringVertexWithId(authoringGraph, 1, 0, 0);
    AddAuthoringVertexWithId(authoringGraph, 2, 64, 0);
    AddAuthoringLineWithId(authoringGraph, 10, 1, 2);
    Check(game::SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, 2),
          "delete isolated authoring vertex setup selects vertex");
    Check(game::SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, 2),
          "delete isolated authoring vertex setup hovers vertex");
    Check(game::DeleteSectorEditorSelectedAuthoringLine(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState) == false,
          "delete isolated authoring vertex setup keeps line because vertex is selected");
    authoringGraph.lines.clear();
    const uint64_t originalRevision = state.topologyRenderRevision;
    const std::size_t originalTopologyVertexCount = documentState.map.topologyMap.vertices.size();
    const std::size_t originalTopologyLineCount = documentState.map.topologyMap.lineDefs.size();

    Check(game::DeleteSectorEditorSelectedAuthoringVertex(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), selectionState),
          "delete selected authoring vertex deletes isolated vertex");
    Check(game::FindSectorAuthoringVertex(authoringGraph, 2) == nullptr,
          "delete isolated authoring vertex removes vertex from graph");
    Check(game::FindSectorAuthoringVertex(authoringGraph, 1) != nullptr,
          "delete isolated authoring vertex preserves other vertices");
    Check(selectionState.selectedAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete isolated authoring vertex prunes deleted selection");
    Check(selectionState.hoveredAuthoring.kind == game::SectorAuthoringSelectionKind::None,
          "delete isolated authoring vertex prunes deleted hover");
    Check(documentState.lifecycle.topologyDocumentDirty, "delete isolated authoring vertex marks document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "delete isolated authoring vertex marks unsaved changes");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "delete isolated authoring vertex marks derived topology stale");
    Check(!state.topologyRenderCache.valid,
          "delete isolated authoring vertex invalidates cached editor topology rendering");
    Check(state.topologyRenderRevision == originalRevision + 1,
          "delete isolated authoring vertex bumps topology render revision");
    Check(documentState.map.topologyMap.vertices.size() == originalTopologyVertexCount
                  && documentState.map.topologyMap.lineDefs.size() == originalTopologyLineCount,
          "delete isolated authoring vertex does not directly mutate derived topology");
}

void TestEditorAuthoringEditsRefreshValidCrossingDerivation()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "crossing refresh setup adds south boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "crossing refresh setup adds east boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 64},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "crossing refresh setup adds north boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 64},
                  game::SectorTopologyCoordPoint{0, 0},
                  &lineId),
          "crossing refresh setup adds west boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "crossing refresh setup adds first diagonal");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "crossing refresh setup adds second diagonal");

    Check(documentState.derivation.authoringDerivation.success,
          "completed crossing graph edit refreshes successful derivation");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent,
          "completed crossing graph edit marks derivation current");
    Check(!documentState.derivation.authoringDerivedTopologyStale,
          "completed crossing graph edit clears stale flag");
    Check(documentState.map.topologyMap.sectors.size() == 4,
          "completed crossing graph edit derives four sectors");
    Check(documentState.map.topologyMap.vertices.size() == 5,
          "completed crossing graph edit derives inserted intersection vertex");
    Check(documentState.derivation.lastValidAuthoringDerivedTopology.has_value(),
          "completed crossing graph edit records last-valid topology");
}

void TestEditorAuthoringFailedRefreshDoesNotReplaceLastValidTopology()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;

    int lineId = -1;
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 0},
                  game::SectorTopologyCoordPoint{64, 0},
                  &lineId),
          "failed refresh setup adds south boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{64, 64},
                  &lineId),
          "failed refresh setup adds east boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 64},
                  game::SectorTopologyCoordPoint{0, 64},
                  &lineId),
          "failed refresh setup adds north boundary");
    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{0, 64},
                  game::SectorTopologyCoordPoint{0, 0},
                  &lineId),
          "failed refresh setup adds west boundary");

    Check(documentState.derivation.authoringDerivation.success,
          "failed refresh setup creates current derived topology");
    const game::SectorTopologyMap lastValid = documentState.map.topologyMap;

    Check(game::AddSectorEditorAuthoringLineSegment(
            state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap,
            authoringGraph,
                  game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation),
            selectionState,
            game::SectorTopologyCoordPoint{64, 0},
                  game::SectorTopologyCoordPoint{128, 0},
                  &lineId),
          "failed refresh test adds dangling line");

    Check(!documentState.derivation.authoringDerivation.success,
          "failed refresh records failed derivation");
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::InvalidLastValid,
          "failed refresh keeps invalid/last-valid state");
    Check(documentState.derivation.authoringDerivedTopologyStale,
          "failed refresh keeps derived topology stale");
    Check(!documentState.derivation.authoringDerivation.diagnostics.empty(),
          "failed refresh records diagnostics");
    Check(documentState.map.topologyMap.sectors.size() == lastValid.sectors.size()
                  && documentState.map.topologyMap.lineDefs.size() == lastValid.lineDefs.size()
                  && documentState.map.topologyMap.vertices.size() == lastValid.vertices.size(),
          "failed refresh does not replace current derived topology");
    Check(documentState.derivation.lastValidAuthoringDerivedTopology.has_value()
                  && documentState.derivation.lastValidAuthoringDerivedTopology->sectors.size() == lastValid.sectors.size(),
          "failed refresh keeps memory-only last-valid topology");
}

void TestEditorAuthoringToolPaneNamingAndHelpDistinguishGraphAndLegacyTools()
{
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringLine), "Authoring"),
          "authoring line tool name is exposed as an authoring graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringLine), "continuous line chain"),
          "authoring line tool help describes continuous line-chain creation");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Select), "cycle"),
          "select tool help describes unified click cycling");
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringMove), "Move"),
          "authoring move tool name is exposed as a graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringMove), "authoring vertices"),
          "authoring move tool help describes graph vertex movement");
    Check(!game::IsGraphAuthoringTool(game::SectorEditorTool::Select),
          "select is not classified as a graph-only authoring tool");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringLine),
          "authoring line is classified as a graph-authoring tool");
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringRectangle), "Rectangle"),
          "rectangle tool name is exposed as a graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringRectangle), "opposite corner"),
          "rectangle tool help describes corner-driven rectangle creation");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringRectangle),
          "rectangle is classified as a graph-authoring tool");
    Check(TextContains(game::ToolName(game::SectorEditorTool::AuthoringInsertVertex), "Insert Vertex"),
          "insert vertex tool name is exposed as a graph tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::AuthoringInsertVertex), "authoring line"),
          "insert vertex tool help describes splitting authoring lines");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringInsertVertex),
          "insert vertex is classified as a graph-authoring tool");
    Check(game::IsGraphAuthoringTool(game::SectorEditorTool::AuthoringMove),
          "authoring move is classified as a graph-authoring tool");

    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Move), "unavailable"),
          "topology move help says it is unavailable in graph-authoritative mode");
    Check(!game::IsGraphAuthoringTool(game::SectorEditorTool::Move),
          "legacy topology move is not classified as a graph-authoring tool");
    Check(game::IsLegacyTopologyMutationTool(game::SectorEditorTool::Move),
          "remaining unavailable move tool is classified as legacy topology mutation");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::Select),
          "select remains available in graph-authoritative mode");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::AuthoringLine),
          "authoring line remains available in graph-authoritative mode");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::AuthoringRectangle),
          "rectangle remains available in graph-authoritative mode");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::AuthoringMove),
          "authoring move remains available in graph-authoritative mode");
    Check(TextContains(game::ToolName(game::SectorEditorTool::RuntimeObject), "Billboard"),
          "runtime object tool label is exposed as the billboard tool");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::RuntimeObject), "billboard"),
          "runtime object tool help describes billboard placement");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::RuntimeObject),
          "runtime object placement remains available in graph-authoritative mode");
    Check(TextContains(game::ToolName(game::SectorEditorTool::Door), "Door"),
          "door tool label is exposed");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::Door), "two-sided portal"),
          "door tool help describes portal placement");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::Door),
          "door placement remains available in graph-authoritative mode");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::StaticLight),
          "static light placement remains available in graph-authoritative mode");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::StaticLight), "place"),
          "static light tool help describes placement-only workflow");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::StaticSpotLight),
          "static spot placement remains available in graph-authoritative mode");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::StaticSpotLight), "place"),
          "static spot tool help describes placement-only workflow");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::DynamicLight),
          "dynamic light placement remains available in graph-authoritative mode");
    Check(TextContains(game::ToolName(game::SectorEditorTool::DynamicLight), "Dynamic Light"),
          "dynamic light tool label is separate from static light");
    Check(game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::DynamicSpotLight),
          "dynamic spot placement remains available in graph-authoritative mode");
    Check(TextContains(game::ToolName(game::SectorEditorTool::DynamicSpotLight), "Dynamic Spot"),
          "dynamic spot tool label is separate from dynamic point light");
    Check(TextContains(game::ToolHelpText(game::SectorEditorTool::DynamicSpotLight), "place"),
          "dynamic spot tool help describes placement-only workflow");
    Check(game::IsSectorEditorGraphAuthoritativeMode(),
          "sector editor runs in graph-authoritative mode");
    Check(!game::IsToolAvailableInGraphAuthoritativeMode(game::SectorEditorTool::Move),
          "legacy topology move tool is blocked in graph-authoritative mode");
    Check(TextContains(game::LegacyTopologyMutationUnavailableMessage(), "unavailable"),
          "legacy topology retirement has a status message");
}

void TestEditorBillboardPlacementCreatesGenericAuthoredObject()
{
    game::SectorTopologyMap map = MakeSingleSectorSquareMap();
    const game::SectorEditorAddBillboardResult result =
            game::AddBillboardToSector(map, 200, Vector2{2.0f, 2.0f});

    Check(result.changed && result.objectId > 0, "billboard placement helper reports a new object");
    Check(map.runtimeObjects.size() == 1, "billboard placement helper appends one runtime object");
    if (map.runtimeObjects.empty()) {
        return;
    }

    const game::SectorPlacedRuntimeObject& object = map.runtimeObjects.front();
    Check(object.id == result.objectId, "billboard placement helper assigns the reported object ID");
    Check(object.kind == "billboard" && object.definitionId.empty(),
          "billboard placement helper writes generic billboard authored data");
    Check(object.position.x == 2.0f && object.position.y == -2.0f && object.position.z == 2.0f,
          "billboard placement helper places the object at the sector floor");
    Check(object.yawRadians == 0.0f, "billboard placement helper defaults yaw to zero");
    Check(object.billboard.spriteAnimationPath.empty(),
          "billboard placement helper leaves sprite path empty until the inspector selects one");
    Check(object.billboard.sizeWorld.x == 1.0f && object.billboard.sizeWorld.y == 1.0f,
          "billboard placement helper defaults size to one world unit square");
    Check(object.billboard.keepAspectRatio && object.billboard.playing && !object.billboard.directional,
          "billboard placement helper defaults aspect, playback, and directional state");
    Check(object.billboard.originNormalized.x == 0.5f && object.billboard.originNormalized.y == 1.0f,
          "billboard placement helper defaults to bottom-center origin");

    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    map,
                    game::SectorAuthoringGraph{},
                    game::SectorAuthoringDerivationResult{},
                    1);
    Check(cache.runtimeObjects.size() == 1
                  && cache.runtimeObjects[0].definitionKnown
                  && cache.runtimeObjects[0].definitionId == "billboard",
          "2D render cache treats generic billboard markers as known drawable objects");
}

void TestEditorDoorPlacementCreatesPortalAnchoredObject()
{
    game::SectorTopologyMap map = MakeAdjacentSectorMap();
    const game::SectorEditorAddDoorResult result =
            game::AddDoorToPortal(map, 11);

    Check(result.changed && result.objectId > 0, "door placement helper reports a new object");
    Check(map.runtimeObjects.size() == 1, "door placement helper appends one runtime object");
    if (map.runtimeObjects.empty()) {
        return;
    }

    const game::SectorPlacedRuntimeObject& object = map.runtimeObjects.front();
    Check(object.id == result.objectId, "door placement helper assigns the reported object ID");
    Check(object.kind == "door" && object.definitionId.empty(),
          "door placement helper writes generic door authored data");
    Check(object.door.anchor.lineDefId == 11
                  && object.door.anchor.frontSideDefId == 101
                  && object.door.anchor.backSideDefId == 104
                  && object.door.anchor.frontSectorId == 200
                  && object.door.anchor.backSectorId == 201,
          "door placement helper stores portal anchor IDs");
    Check(object.door.anchor.endpointAX == 64
                  && object.door.anchor.endpointAY == 0
                  && object.door.anchor.endpointBX == 64
                  && object.door.anchor.endpointBY == 64,
          "door placement helper stores exact endpoint diagnostics");
    Check(object.door.width > 0.0f && object.door.height > 0.0f,
          "door placement helper writes explicit default dimensions from portal opening");
    Check(Near(object.door.openDistance, object.door.height),
          "door placement helper defaults vertical open distance to door height");
    Check(object.door.thickness == 0.25f
                  && object.door.motion == game::SectorDoorMotionType::SlideVertical
                  && object.door.initialOpenFraction == 0.0f
                  && !object.door.autoOpen,
          "door placement helper preserves authored door defaults");

    map.runtimeObjects[0].door.normalOffset = 0.125f;
    const game::SectorEditorTopologyRenderCache cache =
            game::BuildSectorEditorTopologyRenderCache(
                    map,
                    game::SectorAuthoringGraph{},
                    game::SectorAuthoringDerivationResult{},
                    1);
    Check(cache.runtimeObjects.size() == 1
                  && cache.runtimeObjects[0].definitionKnown
                  && cache.runtimeObjects[0].isDoor
                  && cache.runtimeObjects[0].doorFootprintValid,
          "2D render cache builds a drawable door footprint");
    Check(Near(cache.runtimeObjects[0].map.x,
               game::SectorCoordToVisibleAuthoring(64)
                       + game::SectorWorldToAuthoringDistance(0.125f))
                  && Near(cache.runtimeObjects[0].map.y, game::SectorCoordToVisibleAuthoring(32)),
          "2D render cache offsets door footprint along the resolved front-to-back normal");
}

void TestEditorDoorPlacementRejectsOneSidedWall()
{
    game::SectorTopologyMap map = MakeSingleSectorSquareMap();
    const game::SectorEditorAddDoorResult result =
            game::AddDoorToPortal(map, 10);

    Check(!result.changed, "door placement rejects one-sided walls");
    Check(map.runtimeObjects.empty(), "rejected door placement leaves runtime objects unchanged");
    Check(result.status.find("two-sided portal") != std::string::npos,
          "rejected door placement explains the portal requirement");
}

void TestEditorUnifiedSelectPickOrderingCyclingAndDragGate()
{
    std::vector<game::SectorEditorPickCandidate> candidates{
            {game::SectorEditorPickTarget{game::SectorEditorPickKind::StaticLight, 7}, 1.0f},
            {game::SectorEditorPickTarget{game::SectorEditorPickKind::RuntimeObject, 4}, 30.0f},
            {game::SectorEditorPickTarget{game::SectorEditorPickKind::DynamicLight, 2}, 2.0f},
            {game::SectorEditorPickTarget{game::SectorEditorPickKind::AuthoringVertex, 9}, 0.0f},
    };

    const std::vector<game::SectorEditorPickCandidate> sorted =
            game::SortSectorEditorPickCandidates(candidates);
    Check(sorted.size() == 4
                  && sorted[0].target.kind == game::SectorEditorPickKind::RuntimeObject
                  && sorted[1].target.kind == game::SectorEditorPickKind::DynamicLight
                  && sorted[2].target.kind == game::SectorEditorPickKind::StaticLight
                  && sorted[3].target.kind == game::SectorEditorPickKind::AuthoringVertex,
          "unified select pick candidates use stable primitive priority");

    int cycleIndex = -1;
    int cycleCount = 0;
    game::SectorEditorPickTarget chosen = game::ChooseSectorEditorPickTarget(
            candidates,
            game::SectorEditorPickTarget{},
            &cycleIndex,
            &cycleCount);
    Check(chosen.kind == game::SectorEditorPickKind::RuntimeObject
                  && chosen.id == 4
                  && cycleIndex == 0
                  && cycleCount == 4,
          "unified select chooses the top candidate when current selection is not under cursor");

    chosen = game::ChooseSectorEditorPickTarget(
            candidates,
            game::SectorEditorPickTarget{game::SectorEditorPickKind::RuntimeObject, 4},
            &cycleIndex,
            &cycleCount);
    Check(chosen.kind == game::SectorEditorPickKind::DynamicLight
                  && chosen.id == 2
                  && cycleIndex == 1
                  && cycleCount == 4,
          "unified select cycles to the next candidate when current selection is under cursor");

    Check(game::IsSectorEditorPickTargetMovable(
                  game::SectorEditorPickTarget{game::SectorEditorPickKind::RuntimeObject, 4}),
          "runtime object pick target is movable");
    Check(!game::IsSectorEditorPickTargetMovable(
                  game::SectorEditorPickTarget{game::SectorEditorPickKind::AuthoringLine, 9}),
          "authoring line pick target is selectable but not movable");
    Check(!game::ShouldStartSectorEditorSelectDrag(Vector2{10.0f, 10.0f}, Vector2{12.0f, 12.0f}),
          "select drag gate ignores small hand jitter");
    Check(game::ShouldStartSectorEditorSelectDrag(Vector2{10.0f, 10.0f}, Vector2{15.0f, 10.0f}),
          "select drag gate starts after deliberate movement");
}

void TestEditorAuthoringLastValidTopologyIsNotPersisted()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    game::SelectionState selectionState;
    authoringGraph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    Check(game::RefreshSectorEditorAuthoringDerivation(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), documentState.map.topologyMap, authoringGraph, game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation)),
          "last-valid persistence setup derives topology");

    game::SectorAuthoringDocument document;
    document.graph = authoringGraph;
    document.mapData = documentState.map.topologyMap;
    document.derivation = documentState.derivation.authoringDerivation;

    std::string json;
    std::string error;
    Check(game::SaveSectorAuthoringDocumentToJsonString(document, json, &error),
          "authoring document serializes without editor last-valid state");
    Check(json.find("lastValid") == std::string::npos,
          "serialized authoring document omits editor last-valid topology");
    Check(json.find("authoringGraph") != std::string::npos,
          "serialized authoring document keeps authoring graph source");
}

void TestEditorResetBlankMapClearsLifecyclePathAndDirtyState()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorEditorPreviewControllerState previewControllerState;
    documentState.lifecycle.currentLevelName = "loaded_level";
    documentState.lifecycle.currentLevelPath = "assets/levels/loaded_level/loaded_level.json";
    documentState.lifecycle.hasCurrentLevelPath = true;
    documentState.lifecycle.hasUnsavedChanges = true;
    documentState.lifecycle.topologyDocumentDirty = true;
    documentState.lifecycle.topologyDocumentStatus = "Loaded level";

    game::ResetEditorTopologyDocumentState(documentState, previewControllerState);

    Check(documentState.lifecycle.currentLevelName.empty(),
          "blank reset clears current level name");
    Check(documentState.lifecycle.currentLevelPath.empty(),
          "blank reset clears current level path");
    Check(!documentState.lifecycle.hasCurrentLevelPath,
          "blank reset clears current level path availability");
    Check(!documentState.lifecycle.hasUnsavedChanges,
          "blank reset clears unsaved changes");
    Check(!documentState.lifecycle.topologyDocumentDirty,
          "blank reset clears document dirty state");
    Check(documentState.lifecycle.topologyDocumentStatus == "Topology document: empty",
          "blank reset reports empty topology document status");
}

void TestEditorAuthoringDocumentSaveWritesGraphNativeAndReloadsValidCurrent()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(graph, 200, 32, 32, "room");
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    Check(documentState.derivation.authoringDerivationState == game::SectorEditorAuthoringDerivationState::ValidCurrent
                  && !documentState.derivation.authoringDerivedTopologyStale,
          "valid graph-authored save setup is current");

    std::string savedText;
    const Json saved = SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_graph_native_valid_save_test.json",
            &savedText);
    Check(saved["formatVersion"] == 3, "editor save writes graph-native format version");
    Check(saved["topology"] == "authoringGraph", "editor save writes graph-native topology marker");
    Check(saved.contains("authoringGraph"), "editor save writes authoring graph source");
    Check(saved["authoringGraph"]["faceAnchors"].size() == 1,
          "editor save writes authoring face anchors");
    Check(!saved.contains("vertices")
                  && !saved.contains("linedefs")
                  && !saved.contains("sidedefs")
                  && !saved.contains("sectors"),
          "editor save does not write topology-v2 root arrays");

    const std::filesystem::path path =
            TempJsonPath("sector_editor_graph_native_valid_reload_test.json");
    WriteTextFile(path, savedText);
    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "graph-native editor document reloads");
    Check(loaded.format == game::SectorEditorDocumentFormat::AuthoringGraph,
          "graph-native reload reports authoring route");
    bool current = false;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(current, "valid graph-native reload derives valid/current");
    Check(loadedAuthoringGraph.faceAnchors.size() == 1
                  && loadedAuthoringGraph.faceAnchors[0].name == "room",
          "valid graph-native reload preserves face anchor label");
    Check(game::GetSectorLightmapStatus(loadedDocumentState.map.topologyMap) == game::SectorLightmapStatus::None,
          "graph-native reload without baked metadata reports no lightmap metadata");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void TestEditorAuthoringDocumentSavePreservesInvalidGraphAndReloadDiagnostics()
{
    game::SectorAuthoringGraph graph;
    AddAuthoringVertexWithId(graph, 1, 0, 0);
    AddAuthoringLineWithId(graph, 10, 1, 99);
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph, false);
    game::MarkSectorEditorAuthoringGraphEdited(state, game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle), game::MakeSectorEditorDerivationDocumentAccess(documentState.derivation), "invalid authoring graph for save test");

    std::string savedText;
    const Json saved = SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_graph_native_invalid_save_test.json",
            &savedText);
    Check(saved["formatVersion"] == 3, "invalid editor graph save writes graph-native version");
    Check(saved["topology"] == "authoringGraph", "invalid editor graph save writes graph-native marker");
    Check(saved["authoringGraph"]["lines"].size() == 1,
          "invalid editor graph save preserves dangling line");
    Check(saved["authoringGraph"]["lines"][0]["endVertexId"] == 99,
          "invalid editor graph save preserves invalid reference");

    const std::filesystem::path path =
            TempJsonPath("sector_editor_graph_native_invalid_reload_test.json");
    WriteTextFile(path, savedText);
    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "invalid graph-native editor document reloads as source data");
    Check(loaded.format == game::SectorEditorDocumentFormat::AuthoringGraph,
          "invalid graph-native reload reports authoring route");
    Check(loaded.authoringGraph.lines.size() == 1
                  && loaded.authoringGraph.lines[0].endVertexId == 99,
          "invalid graph-native reload preserves dangling authoring line");

    bool current = true;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(!current, "invalid graph-native reload is not valid/current");
    Check(loadedDocumentState.derivation.authoringDerivationState
                      == game::SectorEditorAuthoringDerivationState::InvalidNoDerived
                  && loadedDocumentState.derivation.authoringDerivedTopologyStale
                  && !loadedDocumentState.derivation.authoringDerivation.diagnostics.empty(),
          "invalid graph-native reload reports derivation diagnostics");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void TestEditorLegacyTopologyImportThenSaveWritesGraphNative()
{
    const game::SectorTopologyMap source = MakeSingleSectorSquareMap();
    std::string topologyText;
    std::string error;
    Check(game::SaveSectorTopologyMapToJsonString(source, topologyText, &error),
          "legacy topology fixture serializes");
    const std::filesystem::path path =
            TempJsonPath("sector_editor_legacy_topology_import_test.json");
    WriteTextFile(path, topologyText);

    game::SectorEditorLoadedDocument loaded;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "legacy topology-v2 editor load succeeds");
    Check(loaded.format == game::SectorEditorDocumentFormat::TopologyV2Import,
          "legacy topology-v2 load reports import route");
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    const game::SectorEditorState state = MakeEditorStateFromLoadedDocument(loaded, documentState, authoringGraph);
    Check(game::HasAuthoringGraphData(authoringGraph), "legacy topology import synthesizes authoring graph");
    Check(documentState.derivation.authoringDerivation.success, "legacy topology import derives authoring graph");

    const Json saved = SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_legacy_import_save_graph_native_test.json");
    Check(saved["formatVersion"] == 3, "save after topology-v2 import writes graph-native version");
    Check(saved["topology"] == "authoringGraph",
          "save after topology-v2 import writes graph-native marker");
    Check(saved.contains("authoringGraph"), "save after topology-v2 import writes authoring graph");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void TestEditorGraphNativeNestedRoundTripPreservesAnchorsMaterialsAndSelection()
{
    game::SectorAuthoringGraph graph = MakeNestedRectangleGraph(3);
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 48, 48, "middle");
    AddFaceAnchor(graph, 202, 96, 96, "inner");
    if (game::SectorAuthoringFaceAnchor* outer = game::FindSectorAuthoringFaceAnchor(graph, 200)) {
        outer->defaultWall = WallPart("outer_wall", 1.0f, 2.0f, 3.0f, 4.0f);
    }
    game::SectorAuthoringLineSide side;
    side.id = game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front};
    side.wall = WallPart("line_wall", 2.0f, 3.0f, 4.0f, 5.0f);
    graph.lineSides.push_back(side);

    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    std::string savedText;
    SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_nested_graph_native_save_test.json",
            &savedText);
    const std::filesystem::path path =
            TempJsonPath("sector_editor_nested_graph_native_reload_test.json");
    WriteTextFile(path, savedText);

    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "nested graph-native document loads");
    bool current = false;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(current, "nested graph-native reload derives valid/current");
    Check(!game::HasSectorTopologyValidationErrors(
                  game::ValidateSectorTopologyMap(loadedDocumentState.map.topologyMap)),
          "nested graph-native reload validates derived topology");

    game::SectorGeneratedGeometry geometry;
    Check(game::BuildSectorGeneratedGeometry(loadedDocumentState.map.topologyMap, geometry, &error),
          "nested graph-native reload builds generated geometry");
    Check(loadedAuthoringGraph.faceAnchors.size() == 3,
          "nested graph-native reload preserves face anchors");
    std::set<std::string> labels;
    for (const game::SectorAuthoringFaceAnchor& anchor : loadedAuthoringGraph.faceAnchors) {
        Check(labels.insert(anchor.name).second, "nested graph-native labels remain unique");
    }
    const game::SectorAuthoringFaceAnchor* outer =
            game::FindSectorAuthoringFaceAnchor(loadedAuthoringGraph, 200);
    Check(outer != nullptr && outer->defaultWall.textureId == "outer_wall",
          "nested graph-native reload preserves face material");
    const game::SectorAuthoringLineSide* loadedSide =
            game::FindSectorAuthoringLineSide(
                    loadedAuthoringGraph,
                    game::SectorAuthoringSideId{10, game::SectorTopologySideKind::Front});
    Check(loadedSide != nullptr && loadedSide->wall.textureId == "line_wall",
          "nested graph-native reload preserves side material");

    const auto expectFace = [&](game::SectorCoord x, game::SectorCoord y, int expectedAnchorId, const char* description) {
        game::SectorAuthoringSelectionTarget target;
        Check(FindEditorAuthoringSelectionAtMapPoint(
                      loadedState,
                      loadedDocumentState,
                      loadedAuthoringGraph,
                      VisibleAuthoringPoint(x, y),
                      0.25f,
                      0.25f,
                      &target),
              description);
        Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                      && target.faceAnchorId == expectedAnchorId,
              TextFormat("%s resolves expected anchor", description));
    };
    expectFace(16, 16, 200, "nested outer face selection after reload");
    expectFace(48, 48, 201, "nested middle face selection after reload");
    expectFace(96, 96, 202, "nested inner face selection after reload");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void TestEditorGraphNativeSiblingHolesRoundTripPreservesSelection()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {256, 0}, {256, 256}, {0, 256},
             {32, 32}, {96, 32}, {96, 96}, {32, 96},
             {160, 32}, {224, 32}, {224, 96}, {160, 96}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1},
             {5, 6}, {6, 7}, {7, 8}, {8, 5},
             {9, 10}, {10, 11}, {11, 12}, {12, 9}});
    AddFaceAnchor(graph, 200, 16, 16, "outer");
    AddFaceAnchor(graph, 201, 64, 64, "left");
    AddFaceAnchor(graph, 202, 192, 64, "right");
    if (game::SectorAuthoringFaceAnchor* left = game::FindSectorAuthoringFaceAnchor(graph, 201)) {
        left->defaultWall = WallPart("left_wall", 1.0f, 1.0f, 0.0f, 0.0f);
    }

    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    std::string savedText;
    SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_sibling_graph_native_save_test.json",
            &savedText);
    const std::filesystem::path path =
            TempJsonPath("sector_editor_sibling_graph_native_reload_test.json");
    WriteTextFile(path, savedText);

    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "sibling holes graph-native document loads");
    bool current = false;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(current, "sibling holes graph-native reload derives valid/current");
    Check(AllDerivedSectorsHaveExactlyOneValidFaceAnchorMapping(
                  loadedAuthoringGraph,
                  loadedDocumentState.derivation.authoringDerivation),
          "sibling holes graph-native reload maps each visible face to one anchor");

    std::set<std::string> labels;
    for (const game::SectorAuthoringFaceAnchor& anchor : loadedAuthoringGraph.faceAnchors) {
        Check(labels.insert(anchor.name).second, "sibling holes labels remain unique");
    }
    const game::SectorAuthoringFaceAnchor* left =
            game::FindSectorAuthoringFaceAnchor(loadedAuthoringGraph, 201);
    Check(left != nullptr && left->defaultWall.textureId == "left_wall",
          "sibling holes graph-native reload preserves material");

    const auto expectFace = [&](game::SectorCoord x, game::SectorCoord y, int expectedAnchorId, const char* description) {
        game::SectorAuthoringSelectionTarget target;
        Check(FindEditorAuthoringSelectionAtMapPoint(
                      loadedState,
                      loadedDocumentState,
                      loadedAuthoringGraph,
                      VisibleAuthoringPoint(x, y),
                      0.25f,
                      0.25f,
                      &target),
              description);
        Check(target.kind == game::SectorAuthoringSelectionKind::FaceAnchor
                      && target.faceAnchorId == expectedAnchorId,
              TextFormat("%s resolves expected anchor", description));
    };
    expectFace(16, 16, 200, "sibling outer face selection after reload");
    expectFace(64, 64, 201, "sibling left face selection after reload");
    expectFace(192, 64, 202, "sibling right face selection after reload");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void TestEditorGraphNativeMapLevelDataRoundTrip()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {64, 0}, {64, 64}, {0, 64}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(graph, 200, 32, 32, "room");
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    documentState.map.topologyMap.texturesById.emplace("sky", game::SectorTextureDefinition{
            "sky", "textures/sky.png", game::SectorTextureFilter::Trilinear});
    documentState.map.topologyMap.staticLights.push_back(game::SectorTopologyStaticPointLight{
            9,
            Vector3{1.0f, 2.0f, 3.0f},
            Color{10, 20, 30, 255},
            2.0f,
            32.0f,
            1.0f
    });
    documentState.map.topologyMap.previewSettings.walkSpeed = 9.0f;
    documentState.map.topologyMap.skySettings.textureId = "sky";
    documentState.map.topologyMap.skySettings.yawOffsetDegrees = 17.0f;
    documentState.map.topologyMap.directionalLight.enabled = true;
    documentState.map.topologyMap.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
    documentState.map.topologyMap.directionalLight.intensity = 1.5f;
    documentState.map.topologyMap.lightmapSettings.ambientOcclusionStrength = 0.25f;
    documentState.map.topologyMap.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            44,
            "assets/sprites/torch/torch.json",
            Vector3{12.0f, 0.0f, 20.0f},
            1.57079632679f));
    const std::filesystem::path lightmapPath =
            TempJsonPath("sector_editor_map_level_graph_native.lightmap.png");
    WriteTextFile(lightmapPath, "fake-lightmap");
    documentState.map.topologyMap.bakedLightmap.path = lightmapPath.string();
    documentState.map.topologyMap.bakedLightmap.width = 2048;
    documentState.map.topologyMap.bakedLightmap.height = 2048;
    documentState.map.topologyMap.bakedLightmap.sourceHash =
            game::ComputeSectorLightmapSourceHash(documentState.map.topologyMap);

    std::string savedText;
    const Json saved = SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_map_level_graph_native_save_test.json",
            &savedText);
    Check(saved["textures"].contains("sky"), "editor graph-native save persists texture registry");
    Check(saved["staticLights"][0]["id"] == 9, "editor graph-native save persists static light");
    Check(saved["previewSettings"]["walkSpeed"] == 9.0f,
          "editor graph-native save persists preview settings");
    Check(saved["skySettings"]["textureId"] == "sky", "editor graph-native save persists sky settings");
    Check(saved["directionalLight"]["enabled"] == true,
          "editor graph-native save persists directional light");
    Check(saved["lightmapSettings"]["ambientOcclusionStrength"] == 0.25f,
          "editor graph-native save persists lightmap settings");
    Check(saved["bakedLightmap"]["path"] == lightmapPath.string(),
          "editor graph-native save persists baked lightmap path");
    Check(saved["runtimeObjects"][0]["id"] == 44
                  && saved["runtimeObjects"][0]["kind"] == "billboard"
                  && saved["runtimeObjects"][0]["billboard"]["spriteAnimationPath"]
                             == "assets/sprites/torch/torch.json",
          "editor graph-native save persists runtime objects");
    Check(saved["bakedLightmap"]["width"] == 2048
                  && saved["bakedLightmap"]["height"] == 2048,
          "editor graph-native save persists baked lightmap dimensions");
    Check(saved["bakedLightmap"]["sourceHash"] == documentState.map.topologyMap.bakedLightmap.sourceHash,
          "editor graph-native save persists baked lightmap source hash");
    Check(!saved["bakedLightmap"].contains("objectProbes"),
          "editor graph-native save omits absent object probe metadata");

    const std::filesystem::path path =
            TempJsonPath("sector_editor_map_level_graph_native_reload_test.json");
    WriteTextFile(path, savedText);
    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "editor graph-native map-level document reloads");
    Check(loaded.mapData.texturesById.count("sky") == 1
                  && loaded.mapData.staticLights.size() == 1
                  && Near(loaded.mapData.previewSettings.walkSpeed, 9.0f)
                  && loaded.mapData.skySettings.textureId == "sky"
                  && loaded.mapData.directionalLight.enabled
                  && Near(loaded.mapData.lightmapSettings.ambientOcclusionStrength, 0.25f)
                  && loaded.mapData.bakedLightmap.path == lightmapPath.string()
                  && loaded.mapData.bakedLightmap.width == 2048
                  && loaded.mapData.bakedLightmap.height == 2048
                  && loaded.mapData.bakedLightmap.sourceHash == documentState.map.topologyMap.bakedLightmap.sourceHash
                  && loaded.mapData.runtimeObjects.size() == 1
                  && loaded.mapData.runtimeObjects[0].id == 44,
          "editor graph-native load preserves map-level fields");
    bool current = false;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(current
                  && loadedDocumentState.map.topologyMap.texturesById.count("sky") == 1
                  && loadedDocumentState.map.topologyMap.staticLights.size() == 1
                  && loadedDocumentState.map.topologyMap.skySettings.textureId == "sky"
                  && loadedDocumentState.map.topologyMap.bakedLightmap.path == lightmapPath.string()
                  && loadedDocumentState.map.topologyMap.runtimeObjects.size() == 1
                  && loadedDocumentState.map.topologyMap.runtimeObjects[0].id == 44,
          "editor graph-native derivation receives map-level fields after load");
    Check(game::GetSectorLightmapStatus(loadedDocumentState.map.topologyMap) == game::SectorLightmapStatus::Valid,
          "editor graph-native reload reports matching baked lightmap metadata as valid");
    Check(game::GetSectorBakedObjectLightProbeStatus(loadedDocumentState.map.topologyMap)
                  == game::SectorLightmapStatus::None,
          "editor graph-native reload treats missing object probe metadata as none");

    game::SectorTopologyMap staleMap = loadedDocumentState.map.topologyMap;
    staleMap.bakedLightmap.sourceHash = "mismatched-source-hash";
    Check(game::GetSectorLightmapStatus(staleMap) == game::SectorLightmapStatus::Stale,
          "editor graph-native reload uses existing stale policy for mismatched source hash");

    const Json resaved = SaveEditorStateToJson(
            loadedState,
            loadedDocumentState,
            loadedAuthoringGraph,
            "sector_editor_map_level_graph_native_resave_test.json");
    Check(resaved["formatVersion"] == 3 && resaved["topology"] == "authoringGraph",
          "editor graph-native resave remains graph-native");
    Check(resaved["bakedLightmap"]["path"] == saved["bakedLightmap"]["path"]
                  && resaved["bakedLightmap"]["width"] == saved["bakedLightmap"]["width"]
                  && resaved["bakedLightmap"]["height"] == saved["bakedLightmap"]["height"]
                  && resaved["bakedLightmap"]["sourceHash"] == saved["bakedLightmap"]["sourceHash"],
          "editor graph-native save/load/save preserves baked lightmap metadata");
    Check(resaved["runtimeObjects"] == saved["runtimeObjects"],
          "editor graph-native save/load/save preserves runtime objects");

    game::SectorEditorDocumentState probeDocumentState = loadedDocumentState;
    game::SectorTopologyMap& probeMap = probeDocumentState.map.topologyMap;
    const std::filesystem::path probePath =
            TempJsonPath("sector_editor_map_level_graph_native.object_probes.bin");
    WriteTextFile(probePath, "fake-probe-sidecar");
    probeMap.bakedLightmap.objectProbes.path = probePath.string();
    probeMap.bakedLightmap.objectProbes.version =
            game::kSectorBakedObjectLightProbeSidecarVersion;
    probeMap.bakedLightmap.objectProbes.sourceHash =
            probeMap.bakedLightmap.sourceHash;
    probeMap.bakedLightmap.objectProbes.count = 3;
    probeMap.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    probeMap.bakedLightmap.objectProbes.probeHeightWorld = 1.2f;
    probeMap.bakedLightmap.objectProbes.format =
            game::kSectorBakedObjectLightProbeSidecarFormat;

    const Json savedWithProbes = SaveEditorStateToJson(
            loadedState,
            probeDocumentState,
            loadedAuthoringGraph,
            "sector_editor_map_level_graph_native_object_probes_save_test.json");
    const Json& savedObjectProbes = savedWithProbes["bakedLightmap"]["objectProbes"];
    Check(savedObjectProbes["path"] == probePath.string()
                  && savedObjectProbes["version"] == game::kSectorBakedObjectLightProbeSidecarVersion
                  && savedObjectProbes["sourceHash"] == probeMap.bakedLightmap.sourceHash
                  && savedObjectProbes["count"] == 3
                  && savedObjectProbes["probeSpacingWorld"] == 4.0f
                  && savedObjectProbes["probeHeightWorld"] == 1.2f
                  && savedObjectProbes["format"] == game::kSectorBakedObjectLightProbeSidecarFormat,
          "editor graph-native save persists compact object probe metadata");
    Check(!savedObjectProbes.contains("probes") && !savedObjectProbes.contains("payload"),
          "editor graph-native save does not embed object probe payload");
    Check(game::GetSectorLightmapStatus(probeMap) == game::SectorLightmapStatus::Valid,
          "object probe metadata does not invalidate valid surface lightmap status");
    Check(game::GetSectorBakedObjectLightProbeStatus(probeMap)
                  == game::SectorLightmapStatus::Valid,
          "object probe status reports valid metadata and existing sidecar");

    const std::filesystem::path probeDocumentPath =
            TempJsonPath("sector_editor_map_level_graph_native_object_probes_reload_test.json");
    WriteTextFile(probeDocumentPath, savedWithProbes.dump(2));
    game::SectorEditorLoadedDocument loadedWithProbes;
    Check(game::LoadSectorEditorDocumentFromAsset(probeDocumentPath.string(), loadedWithProbes, error),
          "editor graph-native object probe metadata document reloads");
    Check(loadedWithProbes.mapData.bakedLightmap.objectProbes.path == probePath.string()
                  && loadedWithProbes.mapData.bakedLightmap.objectProbes.version
                             == game::kSectorBakedObjectLightProbeSidecarVersion
                  && loadedWithProbes.mapData.bakedLightmap.objectProbes.sourceHash
                             == probeMap.bakedLightmap.sourceHash
                  && loadedWithProbes.mapData.bakedLightmap.objectProbes.count == 3
                  && Near(loadedWithProbes.mapData.bakedLightmap.objectProbes.probeSpacingWorld, 4.0f)
                  && Near(loadedWithProbes.mapData.bakedLightmap.objectProbes.probeHeightWorld, 1.2f)
                  && loadedWithProbes.mapData.bakedLightmap.objectProbes.format
                             == game::kSectorBakedObjectLightProbeSidecarFormat,
          "editor graph-native load preserves object probe metadata");

    std::error_code removeError;
    std::filesystem::remove(probePath, removeError);
    Check(game::GetSectorLightmapStatus(probeMap) == game::SectorLightmapStatus::Valid,
          "missing object probe sidecar does not invalidate surface lightmap status");
    Check(game::GetSectorBakedObjectLightProbeStatus(probeMap)
                  == game::SectorLightmapStatus::Stale,
          "object probe status reports missing sidecar as stale");

    std::filesystem::remove(path, removeError);
    std::filesystem::remove(lightmapPath, removeError);
    std::filesystem::remove(probeDocumentPath, removeError);
}

void TestEditorGraphNativeRuntimeObjectsSurviveLoadDerivation()
{
    game::SectorAuthoringGraph graph = MakeGraphFromConnectedLines(
            {{0, 0}, {128, 0}, {128, 128}, {0, 128}},
            {{1, 2}, {2, 3}, {3, 4}, {4, 1}});
    AddFaceAnchor(graph, 200, 64, 64, "room");
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorAuthoringGraph& authoringGraph = documentState.authoring.authoringGraph;
    InitializeEditorStateWithAuthoringGraph(state, documentState, authoringGraph, graph);
    documentState.map.topologyMap.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            1,
            "assets/sprites/torch/torch.json",
            Vector3{64.0f, 0.0f, 112.0f},
            0.0f));
    documentState.map.topologyMap.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            7,
            "assets/sprites/crate/crate.json",
            Vector3{32.0f, 0.0f, 48.0f},
            1.57079632679f));

    std::string savedText;
    SaveEditorStateToJson(
            state,
            documentState,
            authoringGraph,
            "sector_editor_runtime_objects_graph_native_save_test.json",
            &savedText);
    const std::filesystem::path path =
            TempJsonPath("sector_editor_runtime_objects_graph_native_reload_test.json");
    WriteTextFile(path, savedText);

    game::SectorEditorLoadedDocument loaded;
    std::string error;
    Check(game::LoadSectorEditorDocumentFromAsset(path.string(), loaded, error),
          "graph-native runtime object fixture loads");
    Check(loaded.format == game::SectorEditorDocumentFormat::AuthoringGraph,
          "runtime object fixture uses graph-native load path");
    Check(loaded.mapData.runtimeObjects.size() == 2,
          "runtime object fixture loaded map data includes saved runtime objects");

    bool current = false;
    game::SectorEditorDocumentState loadedDocumentState;
    game::SectorAuthoringGraph& loadedAuthoringGraph =
            loadedDocumentState.authoring.authoringGraph;
    const game::SectorEditorState loadedState =
            MakeEditorStateFromLoadedDocument(loaded, loadedDocumentState, loadedAuthoringGraph, &current);
    Check(current, "runtime object fixture rederives as current after editor load");
    Check(loadedDocumentState.map.topologyMap.runtimeObjects.size() == loaded.mapData.runtimeObjects.size(),
          "runtime objects survive editor authoring rederivation");
    const game::SectorPlacedRuntimeObject* first =
            game::FindSectorPlacedRuntimeObject(loadedDocumentState.map.topologyMap, 1);
    const game::SectorPlacedRuntimeObject* second =
            game::FindSectorPlacedRuntimeObject(loadedDocumentState.map.topologyMap, 7);
    Check(first != nullptr
                  && first->kind == "billboard"
                  && first->definitionId.empty()
                  && first->billboard.spriteAnimationPath == "assets/sprites/torch/torch.json"
                  && Near(first->position, Vector3{64.0f, 0.0f, 112.0f})
                  && Near(first->yawRadians, 0.0f),
          "first generated runtime object survives load with stable authored data");
    Check(second != nullptr
                  && second->kind == "billboard"
                  && second->definitionId.empty()
                  && second->billboard.spriteAnimationPath == "assets/sprites/crate/crate.json"
                  && Near(second->position, Vector3{32.0f, 0.0f, 48.0f})
                  && Near(second->yawRadians, 1.57079632679f),
          "second generated runtime object survives load with stable authored data");

    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

} // namespace

int main()
{
    TestEmptyGraph();
    TestStablePositiveIdAllocation();
    TestAddVerticesAndLines();
    TestInvalidEndpointDiagnosticsAndRejection();
    TestValidateAuthoringGraphRejectsDuplicateVertexIds();
    TestValidateAuthoringGraphRejectsDuplicateLineIds();
    TestValidateAuthoringGraphRejectsDuplicateFaceAnchorIds();
    TestValidateAuthoringGraphRejectsDuplicateSideIdentity();
    TestSideIdentityHelpers();
    TestFaceAnchorDefaultsMatchTopologyDefaults();
    TestImportEmptyTopologyMap();
    TestImportSingleSectorSquare();
    TestImportAdjacentSectors();
    TestPlanarizeCrossingLines();
    TestPlanarizeTJunction();
    TestPlanarizeEndpointOnSegment();
    TestPlanarizeNonCollinearCoincidentEndpointDiagnostic();
    TestPlanarizeMultipleCrossingsOnOneLine();
    TestPlanarizeNonIntersectingLines();
    TestPlanarizeDuplicateLineDiagnostic();
    TestPlanarizeCollinearOverlapDiagnostic();
    TestPlanarizeZeroLengthLineDiagnostic();
    TestPlanarizeNearMissDiagnostic();
    TestPlanarizeMetadataMapping();
    TestExtractFacesSingleSquare();
    TestExtractFacesAdjacentSquares();
    TestExtractFacesRectangleCutByLine();
    TestExtractFacesCrossingDiagonals();
    TestExtractFacesOpenChainDiagnostics();
    TestExtractFacesDanglingLineDoesNotCorruptSquare();
    TestExtractFacesRejectsOuterFace();
    TestExtractFacesSliverThreshold();
    TestExtractFacesNestedDisconnectedLoopsProducesContainedFaces();
    TestExtractDisconnectedClosedLoopsProducesFaces();
    TestExtractDisconnectedClosedLoopsWithDanglingLineReportsOnlyDanglingLine();
    TestDeriveSingleSquare();
    TestDeriveAdjacentSquares();
    TestVoidFaceExcludedFromDerivedTopologyAndJson();
    TestAdjacentVoidFaceCreatesOneSidedWallAndProjectsNonVoidMaterial();
    TestEnclosedVoidFaceCreatesHoleAndSolidInnerWalls();
    TestCenterVoidPreventsAccidentalSameHeightPortals();
    TestDeriveCrossingDiagonals();
    TestDeriveDisconnectedClosedLoopsProducesSectors();
    TestDeriveNestedLoopCreatesHoleTopology();
    TestDeriveNestedLoopProjectsInnerBoundaryProperties();
    TestDeriveNestedLoopFaceAnchorsResolveThroughHoles();
    TestNestedLoopsDepthTwoDerivesValidTopology();
    TestNestedLoopsDepthThreeDerivesValidTopology();
    TestNestedLoopFaceAnchorsResolveToDeepestContainingSector();
    TestNestedLoopDirectChildHolesOnly();
    TestNestedLoopDepthLimitReportsDiagnostic();
    TestDeriveOverlappingNestedLoopsProduceDiagnostics();
    TestDeriveNonIntegerIntersectionFails();
    TestDeriveSquareWithMixedLineDirections();
    TestDeriveOpenGraphFails();
    TestDeriveDuplicateLinesFail();
    TestDeriveDiagnosticsUseSpecificKindsAndSources();
    TestDeriveDanglingLineDiagnosticUsesAuthoringLineId();
    TestDeriveDisconnectedClosedLoopsWithDanglingLineFailsCleanly();
    TestDeriveSplitLineMapping();
    TestDeriveFaceAnchorMapping();
    TestDeriveFaceAnchorDiagnostics();
    TestDeriveInvalidSideProjectionDiagnostic();
    TestDeriveProjectsFaceAnchorProperties();
    TestDeriveProjectsSideMaterialsAndLineFlags();
    TestDeriveSplitLineDuplicatesProjectedProperties();
    TestDeriveUnresolvedAnchorPreservesAuthoringProperties();
    TestDerivedTopologyBuildsGeometry();
    TestFreshDerivedTopologyUsesDefaultMaterials();
    TestEditorAuthoringRefreshSynthesizedOuterSectorGetsDefaultMaterials();
    TestEditorAuthoringRefreshAddingInnerSectorPreservesOuterAnchor();
    TestEditorAuthoringGraphMutationMarksDirtyAndStale();
    TestAuthoringOverlayRenderCacheIncludesLooseGraph();
    TestAuthoringDiagnosticRenderCacheDoesNotRequireDerivedTopology();
    TestAuthoringOverlayRenderCacheIncludesReferenceDiagnostics();
    TestAuthoringOverlaySuppressesLegacyTopologySelectionHighlights();
    TestAuthoringOverlaySelectionHighlightDecisions();
    TestAuthoringOverlayFaceAnchorHighlightResolvesVisibleBoundary();
    TestAuthoringOverlayFaceAnchorHighlightFailsClosedWithoutCurrentMapping();
    TestEditorAuthoringSuccessfulDerivationUpdatesState();
    TestEditorAuthoringFaceAnchorInspectorWritesProjectAfterDerivation();
    TestEditorAuthoringFaceAnchorInspectorWriteDoesNotDirectlyMutateDerivedTopology();
    TestEditorAuthoringSideMaterialInspectorWritesProjectAfterDerivation();
    TestEditorAuthoringSideMaterialInspectorWritesProjectToSplitDerivedSideDefs();
    TestEditorAuthoringSideMaterialInspectorWriteDoesNotDirectlyMutateDerivedTopology();
    TestEditorLightEditingServiceStaticEditMarksDirtyAndInvalidatesCache();
    TestEditorLightEditingServiceDynamicEditPreservesDirtyBehavior();
    TestEditorLightEditingServiceNoOpDoesNotDirtyOrUpdateStatus();
    TestEditorMaterialEditingServiceSideDefBaseUvWritesThroughAuthoringSide();
    TestEditorMaterialEditingServiceNoAuthoringMaterialEditFailsWithoutMutatingTopology();
    TestEditorMaterialEditingServiceSideDefDecalUvWritesThroughAuthoringSide();
    TestEditorMaterialEditingServiceSideDefBaseUvResetWritesThroughAuthoringSide();
    TestEditorMaterialEditingServiceSideDefDecalUvResetWritesThroughAuthoringSide();
    TestEditorMaterialEditingServiceSideDefUvMissingMappingDoesNotMutateTopology();
    TestEditorMaterialEditingServiceSideDecalOpacityWritesThroughAuthoringSide();
    TestEditorMaterialEditingServiceFlatDecalFitWritesThroughFaceAnchor();
    TestEditorMaterialEditingServiceDerivedSidePickerWritesAuthoringSideDirectly();
    TestEditorAuthoringLineFlagInspectorWritesProjectAfterDerivation();
    TestEditorNoAuthoringBlocksPlayerEditFailsWithoutMutatingTopology();
    TestEditorAuthoringBlocksPlayerEditWritesAuthoringLine();
    TestEditorNoAuthoringSectorPropertyEditFailsWithoutMutatingTopology();
    TestEditorAuthoringLineFlagInspectorWritesProjectToSplitDerivedLineDefs();
    TestEditorAuthoringLineFlagInspectorWriteDoesNotDirectlyMutateDerivedTopology();
    TestEditorSelectedAuthoringLineInspectorTargetDoesNotNeedTopologySelection();
    TestEditorMappedTopologySideSelectionUsesAuthoringInspectorTarget();
    TestEditorMappedTopologySectorSelectionUsesAuthoringInspectorTarget();
    TestEditorMappedTopologyMissingOrStaleMappingDoesNotUseLegacyInspectorTarget();
    TestEditorAuthoringSideClearMiddleAndDecalProjectsAfterDerivation();
    TestEditorAuthoringFaceClearDecalProjectsAfterDerivation();
    TestEditorAuthoringSideDecalPropertyEditProjectsAfterDerivation();
    TestEditorAuthoringFaceDecalPropertyEditProjectsAfterDerivation();
    TestEditorAuthoringFaceDefaultDecalTexturePickerWritesThroughAnchor();
    TestEditorSurface3DMappedPanelLabelMentionsAuthoringAndDerivedIds();
    TestEditorSelectedAuthoringLineBlocksPlayerWritesWithoutTopologySelection();
    TestEditorSelectedAuthoringLineSideMaterialWritesWithoutTopologySelection();
    TestEditorSelectedFaceAnchorPropertiesWriteWithoutTopologySelection();
    TestEditorSelectedFaceAnchorVoidToggleWritesAuthoringOnly();
    TestEditorAuthoringFacePointSelectionRequiresCurrentUnambiguousMapping();
    TestEditorAuthoringNestedFacePointSelectionChoosesDeepestAnchor();
    TestEditorAuthoringSiblingNestedFacePointSelectionChoosesIndependentAnchors();
    TestEditorAuthoringRefreshSynthesizesMissingNestedFaceAnchors();
    TestEditorAuthoringRefreshSynthesizesSiblingFaceAnchorsWithUniqueLabels();
    TestEditorAuthoringRefreshDoesNotSynthesizeForInvalidDerivation();
    TestEditorAuthoringRefreshPreservesUnresolvedExistingAnchors();
    TestDeriveGeneratedFaceLabelsAreUnique();
    TestDeriveGeneratedFallbackLabelsSkipExistingNames();
    TestEditorAuthoringRefreshPreservesCustomAndImportedLabels();
    TestEditorAuthoringTexturePickerDirectTargetsFailClosedWhenMappingUnavailable();
    TestEditorAuthoringSurfaceMappingResolvesFlatSurfaceToFaceAnchor();
    TestEditorAuthoringSurfaceMappingResolvesWallSurfaceToAuthoringSide();
    TestEditorAuthoringSurfaceMappingBlocksStaleDerivedTopology();
    TestEditorAuthoringSurfaceMappingBlocksMissingMapping();
    TestEditorAuthoringSelectedSurfaceClearsWhenMappingBecomesStaleBeforeEdit();
    TestEditorAuthoringFlatSurfaceFloorUvWritesThroughFaceAnchor();
    TestEditorAuthoringFlatSurfaceCeilingUvWritesThroughFaceAnchor();
    TestEditorAuthoringFlatSurfaceTextureWritesThroughFaceAnchor();
    TestEditorAuthoringFlatSurfaceStaleMappingBlocksMaterialEdits();
    TestEditorAuthoringFaceTexturePickerWritesThroughFaceAnchor();
    TestEditorAuthoringSideTexturePickerWritesThroughAuthoringSide();
    TestEditorAuthoringFaceTexturePickerRejectsStaleMappingAfterOpen();
    TestEditorAuthoringSideTexturePickerRejectsStaleMappingAfterOpen();
    TestEditorAuthoringTexturePickerRejectsStaleMapping();
    TestEditorNoAuthoringTexturePickerApplyFailsWithoutMutatingTopology();
    TestEditorRuntimeDoorTexturePickerWritesAuthoredDoorTexture();
    TestEditorMapTextureImportPreservesMapLevelRegistryOnly();
    TestSpriteMetadataScannerFindsNestedJsonAndFrameTagClips();
    TestSpriteMetadataScannerFallsBackToFrameNamesAndDefaultClip();
    TestSpriteMetadataScannerRejectsMissingSpritesDirectory();
    TestSpritePickerSelectsSpriteMetadata();
    TestBillboardSpritePickerApplyRepairsSingleClips();
    TestBillboardSpritePickerApplyRepairsDirectionalClips();
    TestBillboardClipRepairPreservesNonEmptyOnInitialMetadataResolve();
    TestBillboardClipRepairOverwritesInvalidOnSpriteChange();
    TestEditorAuthoringFailedDerivationKeepsGraphAndDiagnostics();
    TestEditorAuthoringPreviewAndBakeGateAllowsCurrentDerivedTopology();
    TestEditorAuthoringPreviewAndBakeGateRejectsInvalidNoDerived();
    TestEditorAuthoringPreviewAndBakeGateRejectsStaleDerivedTopology();
    TestEditorAuthoringPreviewAndBakeGateRejectsFailedDerivation();
    TestEditorAuthoringSuccessfulDerivationPreservesBakedLightmapMetadata();
    TestEditorAuthoringSelectionTargetsRepresentLineAndVertex();
    TestEditorAuthoringSelectionHelpersSetClearAndRejectMissingTargets();
    TestEditorAuthoringHoverAndPruneUseGraphValidity();
    TestEditorAuthoringLineDrawHelperCreatesLooseLineAndMarksDirty();
    TestEditorAuthoringLineDrawHelperReusesVerticesAndRejectsZeroLength();
    TestEditorAuthoringLineDrawHelperAutoSplitsEndpointOnLine();
    TestEditorAuthoringLineDrawHelperAutoSplitsTwoEndpointsOnSameBoundary();
    TestEditorAuthoringLineToolChainCommitsConnectedSegments();
    TestEditorAuthoringLineToolCancelStartsNewChain();
    TestEditorAuthoringLineToolRejectsZeroLengthWithoutEndingChain();
    TestEditorAuthoringLineToolLoopClosureRemainsActive();
    TestInsertVertexSplitsHorizontalVerticalAndDiagonalLines();
    TestInsertVertexRejectsOffLineAndEndpoints();
    TestInsertVertexReusesExistingInteriorVertex();
    TestInsertVertexCopiesLineFlagsSpecialAndSideMaterials();
    TestInsertVertexDerivationStillWorksForRectangle();
    TestEditorAuthoringInsertVertexMarksDirtyInvalidatesAndSelectsVertex();
    TestAuthoringRectangleHelperCreatesClosedLoop();
    TestAuthoringRectangleHelperReversedDragIsDeterministic();
    TestAuthoringRectangleHelperRejectsZeroSize();
    TestAuthoringRectangleHelperDerivesValidTopology();
    TestAuthoringNestedRectanglesDeriveThroughExistingBehavior();
    TestEditorAuthoringRectangleCommitMarksDirtyAndReusesVertices();
    TestAuthoringRectangleSplitsCrossedAuthoringLineAndOwnEdges();
    TestAuthoringRectangleSortsMultipleIntersectionsOnOneEdge();
    TestAuthoringRectangleCornerOnLineSplitsExistingLine();
    TestAuthoringRectangleRejectsOverlapAtomically();
    TestAuthoringRectangleRejectsNonGridIntersectionAtomically();
    TestEditorAuthoringLinePickingFindsNearestValidLine();
    TestEditorAuthoringDeleteSelectedLineOnlyMutatesGraphAndInvalidates();
    TestEditorAuthoringVertexPickingFindsNearestValidVertex();
    TestEditorAuthoringSelectionPickingPrefersVerticesThenLines();
    TestEditorAuthoringMoveVertexUpdatesConnectedLinesAndInvalidates();
    TestEditorAuthoringMoveNestedLoopVertexRederivesValidTopology();
    TestEditorAuthoringDeleteConnectedVertexIsExplicitlyRejected();
    TestEditorAuthoringDeleteIsolatedVertexOnlyMutatesGraphAndInvalidates();
    TestEditorAuthoringEditsRefreshValidCrossingDerivation();
    TestEditorAuthoringFailedRefreshDoesNotReplaceLastValidTopology();
    TestEditorAuthoringToolPaneNamingAndHelpDistinguishGraphAndLegacyTools();
    TestEditorBillboardPlacementCreatesGenericAuthoredObject();
    TestEditorDoorPlacementCreatesPortalAnchoredObject();
    TestEditorDoorPlacementRejectsOneSidedWall();
    TestEditorUnifiedSelectPickOrderingCyclingAndDragGate();
    TestEditorAuthoringLastValidTopologyIsNotPersisted();
    TestEditorResetBlankMapClearsLifecyclePathAndDirtyState();
    TestEditorAuthoringDocumentSaveWritesGraphNativeAndReloadsValidCurrent();
    TestEditorAuthoringDocumentSavePreservesInvalidGraphAndReloadDiagnostics();
    TestEditorLegacyTopologyImportThenSaveWritesGraphNative();
    TestEditorGraphNativeNestedRoundTripPreservesAnchorsMaterialsAndSelection();
    TestEditorGraphNativeSiblingHolesRoundTripPreservesSelection();
    TestEditorGraphNativeMapLevelDataRoundTrip();
    TestEditorGraphNativeRuntimeObjectsSurviveLoadDerivation();

    if (failures != 0) {
        std::cerr << failures << " authoring graph test(s) failed\n";
        return 1;
    }

    std::cout << "Sector authoring graph tests passed\n";
    return 0;
}
