#include "sector_editor/inspector/SectorEditorInspectorPanel.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorLightInspector.h"
#include "sector_editor/SectorEditorSectorInspector.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorVertexInspector.h"
#include "sector_editor/inspector/SectorEditorLevelMarkerInspector.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/tools/doors/SectorEditorDoorModals.h"
#include "sector_editor/tools/materials/SectorEditorMaterialInspector.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace game {
namespace {

constexpr float SectorEditorPanelScrollPaddingPx = 8.0f;

float ScrollAreaContentWidthForVerticalScrollbar(
        float boundsWidth,
        const engine::UIConfig& config,
        float paddingPx,
        bool drawFrame)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - (drawFrame ? config.borderThickness * 2.0f : 0.0f));
    return std::max(0.0f, clientWidth - config.scrollbarSize - paddingPx * 2.0f);
}

void AppendRequest(
        SectorEditorInspectorPanelResult& result,
        SectorEditorInspectorPanelRequestKind kind,
        const char* status = nullptr,
        int lineId = -1)
{
    if (result.requestCount < static_cast<int>(result.requests.size())) {
        SectorEditorInspectorPanelRequest& request = result.requests[static_cast<size_t>(result.requestCount++)];
        request.kind = kind;
        request.status = status == nullptr ? std::string{} : std::string{status};
        request.lineId = lineId;
    }
}

bool TryRenameSelectedDerivedSectorAuthoringNameForInspector(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringGraph& authoringGraph,
        InspectorIdUiState& inspectorIdUiState,
        std::string& statusText,
        SectorTopologySector* sector)
{
    if (sector == nullptr) {
        inspectorIdUiState.idEditError = "No topology sector selected";
        statusText = inspectorIdUiState.idEditError;
        return false;
    }

    const std::string newName = inspectorIdUiState.selectedSectorIdBuffer;
    if (newName == sector->name) {
        inspectorIdUiState.idEditError.clear();
        return true;
    }

    const bool hasAuthoringGraph = HasAuthoringGraphData(authoringGraph);
    if (!hasAuthoringGraph) {
        inspectorIdUiState.idEditError = "Cannot edit sector property: authoring data is required.";
        statusText = inspectorIdUiState.idEditError;
        return true;
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(derivation)) {
        inspectorIdUiState.idEditError = "Sector name edit unavailable: derived topology is not current";
        statusText = inspectorIdUiState.idEditError;
        return true;
    }

    const bool hasFaceAnchorMapping =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    authoringGraph,
                    derivation.authoringDerivation,
                    sector->id) >= 0;
    if (hasFaceAnchorMapping) {
        MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                state,
                lifecycle,
                topologyMap,
                authoringGraph,
                derivation,
                sector->id,
                TextFormat("Renamed authoring face anchor %d", sector->id),
                [&newName](SectorAuthoringFaceAnchor& anchor) {
                    if (anchor.name == newName) {
                        return false;
                    }
                    anchor.name = newName;
                    return true;
                });
        inspectorIdUiState.idEditError.clear();
        return true;
    }
    inspectorIdUiState.idEditError = "Sector name edit unavailable: selected sector has no face anchor mapping";
    statusText = inspectorIdUiState.idEditError;
    return true;
}

bool SetAuthoringLineDefBlocksPlayerForInspector(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringGraph& authoringGraph,
        std::string& statusText,
        SectorEditorInspectorPanelResult& result,
        int lineDefId,
        bool blocksPlayer)
{
    std::string status;
    const bool changed = SetSectorEditorAuthoringLineDefBlocksPlayer(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            lineDefId,
            blocksPlayer,
            &status);
    if (!status.empty()) {
        statusText = status;
    }
    if (changed && status.empty()) {
        return true;
    }
    if (changed) {
        AppendRequest(result, SectorEditorInspectorPanelRequestKind::RebuildSectorCollisionWorld);
    }
    return changed;
}

float AuthoringInspectorTextureRowTotalHeight(float gap)
{
    return SectorEditorInspectorTextureRowHeight() + gap;
}

float AuthoringInspectorAssignedDecalControlsHeight(bool emissive, float rowH, float gap, bool includeTintAndFit)
{
    float height = 0.0f;
    height += rowH + gap;
    height += 36.0f + gap;
    if (emissive) {
        height += rowH + gap;
    }
    if (includeTintAndFit) {
        height += rowH + gap;
        height += 36.0f + gap;
    }
    return height;
}

float AuthoringInspectorDecalBlockHeight(
        const SectorTopologyDecalLayer& decal,
        float rowH,
        float gap,
        bool includeTintAndFit)
{
    float height = AuthoringInspectorTextureRowTotalHeight(gap);
    if (!decal.textureId.empty()) {
        height += AuthoringInspectorAssignedDecalControlsHeight(
                decal.emissive,
                rowH,
                gap,
                includeTintAndFit);
    }
    return height;
}

float AuthoringLineInspectorContentHeight(
        const SectorAuthoringLine& line,
        const SectorAuthoringGraph& graph,
        float rowH,
        float gap,
        float endpointSummaryHeight)
{
    float height = 0.0f;
    height += 38.0f;
    height += endpointSummaryHeight;
    height += 36.0f + gap;

    const auto addSideSection = [&](SectorTopologySideKind sideKind) {
        height += 18.0f;
        height += 30.0f;
        height += AuthoringInspectorTextureRowTotalHeight(gap) * 4.0f;

        const SectorAuthoringLineSide* side =
                FindSectorAuthoringLineSide(graph, SectorAuthoringSideId{line.id, sideKind});
        const SectorTopologyDecalLayer emptyDecal;
        const SectorTopologyDecalLayer& wallDecal =
                side != nullptr ? side->wall.decal : emptyDecal;
        const SectorTopologyDecalLayer& lowerDecal =
                side != nullptr ? side->lower.decal : emptyDecal;
        const SectorTopologyDecalLayer& upperDecal =
                side != nullptr ? side->upper.decal : emptyDecal;
        height += AuthoringInspectorDecalBlockHeight(wallDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(lowerDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(upperDecal, rowH, gap, true);
    };

    addSideSection(SectorTopologySideKind::Front);
    addSideSection(SectorTopologySideKind::Back);
    height += rowH + gap;
    height += rowH + gap;
    return height;
}

bool DrawTopologySideDefInspector(
        SectorEditorInspectorPanelContext& context,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorEditorInspectorPanelResult& result)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    const engine::FontHandle smallFont = context.smallFont;
    SelectionState& selectionState = context.selectionState;
    SectorEditorUiState& uiState = context.uiState;
    MaterialEditingUiState& materialUiState = context.materialUiState;
    std::string& statusText = context.statusText;
    SectorEditorTextureCatalogService& textureCatalog = context.textureCatalog;

    const SectorEditorMaterialInspectorCallbacks callbacks{
            [&](int sideDefId, TopologyWallPart wallPart) {
                SelectSectorEditorTopologySideDef(context.selection, sideDefId, wallPart);
            },
            [&](int lineDefId, bool blocksPlayer) {
                return SetAuthoringLineDefBlocksPlayerForInspector(
                        context.state,
                        context.lifecycle,
                        context.topologyMap,
                        context.derivation,
                        context.authoringGraph,
                        context.statusText,
                        result,
                        lineDefId,
                        blocksPlayer);
            }};
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;
    SectorEditorMaterialInspectorContext materialContext{
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            scroll,
            contentW,
            rowH,
            gap,
            context.topologyMap,
            context.authoringGraph,
            context.selectionState,
            uiState.inspectorScroll,
            materialUiState,
            statusText,
            callbacks,
            materialEditing,
            textureCatalog};
    return DrawTopologySideDefMaterialInspector(materialContext);
}

} // namespace

SectorEditorInspectorPanelResult DrawSectorEditorInspectorPanel(
        SectorEditorInspectorPanelContext& context)
{
    SectorEditorInspectorPanelResult result;
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    const engine::FontHandle smallFont = context.smallFont;
    SectorEditorState& state = context.state;
    SectorAuthoringGraph& authoringGraph = context.authoringGraph;
    SelectionState& selectionState = context.selectionState;
    SectorEditorUiState& uiState = context.uiState;
    MaterialEditingUiState& materialUiState = context.materialUiState;
    std::string& statusText = context.statusText;
    SectorEditorSelectionServiceContext& selection = context.selection;
    SectorEditorRuntimeObjectEditingService& runtimeObjectEditing =
            context.runtimeObjectEditing;
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;
    SectorEditorTextureCatalogService& textureCatalog = context.textureCatalog;
    SectorEditorLightEditingService& lightEditing = context.lightEditing;
    engine::EngineContext* engineContext = context.engineContext;

    auto selectedTopologySector = [&]() { return SelectedSectorEditorTopologySector(selection); };
    auto selectedTopologyVertex = [&]() { return SelectedSectorEditorTopologyVertex(selection); };
    auto selectedTopologySideDef = [&]() { return SelectedSectorEditorTopologySideDef(selection); };
    auto selectedTopologyLineDef = [&]() { return SelectedSectorEditorTopologyLineDef(selection); };
    auto selectedStaticLight = [&]() { return SelectedSectorEditorTopologyLight(selection); };
    auto selectedStaticSpotLight = [&]() { return SelectedSectorEditorTopologyStaticSpotLight(selection); };
    auto selectedDynamicLight = [&]() { return SelectedSectorEditorTopologyDynamicLight(selection); };
    auto selectedDynamicSpotLight = [&]() { return SelectedSectorEditorTopologyDynamicSpotLight(selection); };
    auto selectedRuntimeObject = [&]() {
        return runtimeObjectEditing.SelectedObject();
    };

    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_sectors",
            context.panelRect,
            font,
            "Inspector"
    );

    ClearStaleSectorEditorTopologySelection(selection);
    SyncSectorEditorSelectedSectorIdBuffer(selection);
    SyncSectorEditorSelectedLightIdBuffer(selection);

    const bool hasSelectedTopologySector = selectedTopologySector() != nullptr;
    const bool hasSelectedTopologyVertex = selectedTopologyVertex() != nullptr;
    const bool hasSelectedTopologySideDef = selectedTopologySideDef() != nullptr;
    const bool hasSelectedTopologyLineDef = selectionState.topologySelectionKind == TopologySelectionKind::LineDef
            && selectedTopologyLineDef() != nullptr;
    const bool hasSelectedLight = selectedStaticLight() != nullptr;
    const bool hasSelectedStaticSpotLight = selectedStaticSpotLight() != nullptr;
    const bool hasSelectedDynamicLight = selectedDynamicLight() != nullptr;
    const bool hasSelectedDynamicSpotLight = selectedDynamicSpotLight() != nullptr;
    const bool hasSelectedRuntimeObject = selectedRuntimeObject() != nullptr;
    const bool authoringDerivationCurrent =
            IsSectorEditorAuthoringDerivationCurrent(context.derivation);
    const SectorEditorInspectorTarget inspectorTarget =
            ResolveSectorEditorInspectorTarget(
                    context.topologyMap,
                    authoringGraph,
                    context.derivation.authoringDerivation,
                    authoringDerivationCurrent,
                    selectionState);
    const bool allowLegacyTopologyInspector =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::LegacyTopology
            || inspectorTarget.kind == SectorEditorInspectorTargetKind::None;
    const SectorAuthoringLine* selectedAuthoringLine =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringLine
            ? FindSectorAuthoringLine(authoringGraph, inspectorTarget.lineId)
            : nullptr;
    const SectorAuthoringFaceAnchor* selectedAuthoringFaceAnchor =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringFaceAnchor
            ? FindSectorAuthoringFaceAnchor(authoringGraph, inspectorTarget.faceAnchorId)
            : nullptr;
    const bool hasMultipleSelectedAuthoringFaces =
            selectionState.selectedAuthoringFaceAnchorIds.size() > 1;
    const SectorAuthoringVertex* selectedAuthoringVertex =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringVertex
            ? FindSectorAuthoringVertex(authoringGraph, inspectorTarget.vertexId)
            : nullptr;
    const SectorAuthoringFogVolume* selectedAuthoringFogVolume =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringFogVolume
            ? FindSectorAuthoringFogVolume(authoringGraph, inspectorTarget.fogVolumeId)
            : nullptr;
    const SectorAuthoringLevelMarker* selectedLevelMarker =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringLevelMarker
            ? FindSectorAuthoringLevelMarker(authoringGraph, inspectorTarget.levelMarkerId)
            : nullptr;
    const SectorTopologyVertex* inspectedVertex = FindSectorTopologyVertex(
            context.topologyMap,
            selectionState.inspectedTopologyVertexId);
    const bool hasInspectedVertex = !hasSelectedTopologySector
            && !hasSelectedTopologyVertex
            && !hasSelectedTopologySideDef
            && !hasSelectedTopologyLineDef
            && !hasSelectedLight
            && !hasSelectedStaticSpotLight
            && !hasSelectedDynamicLight
            && !hasSelectedDynamicSpotLight
            && state.currentTool == SectorEditorTool::Move
            && inspectedVertex != nullptr;

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = ScrollAreaContentWidthForVerticalScrollbar(
            panel.contentRect.width,
            config,
            SectorEditorPanelScrollPaddingPx,
            false);
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
    bool deleteRuntimeObjectRequested = false;
    const auto inspectorContentHeight = [&]() {
        if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
            return 120.0f;
        }
        if (hasSelectedRuntimeObject) {
            const SectorEditorPlacedObjectInspectorMeasureContext runtimeObjectMeasureContext{
                    assets,
                    smallFont,
                    smallConfig,
                    context.topologyMap,
                    context.runtimeObjects,
                    engineContext,
                    runtimeObjectEditing,
                    context.runtimeObjectEditingState,
                    textureCatalog,
                    context.sounds,
                    scrollContentW,
                    rowH,
                    gap
            };
            return MeasureSectorEditorPlacedObjectInspectorContentHeight(runtimeObjectMeasureContext);
        }
        if (hasSelectedLight) {
            return StaticLightInspectorContentHeight(
                    rowH, gap, !context.inspectorIdUiState.idEditError.empty(), selectedStaticLight()->atmosphere);
        }
        if (hasSelectedStaticSpotLight) {
            return StaticSpotLightInspectorContentHeight(
                    rowH, gap, !context.inspectorIdUiState.idEditError.empty(), selectedStaticSpotLight()->atmosphere);
        }
        if (hasSelectedDynamicLight) {
            return DynamicLightInspectorContentHeight(
                    rowH, gap, !context.inspectorIdUiState.idEditError.empty(), selectedDynamicLight()->atmosphere);
        }
        if (hasSelectedDynamicSpotLight) {
            const float shadowNoteHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    TextFormat(
                            "Requests one of %zu shadow slots. Priority decides budget; over-budget spots still light.",
                            MaxDynamicSpotLightShadowCasters),
                    scrollContentW,
                    2);
            return DynamicSpotLightInspectorContentHeight(
                    rowH,
                    gap,
                    !context.inspectorIdUiState.idEditError.empty(),
                    shadowNoteHeight,
                    selectedDynamicSpotLight()->atmosphere);
        }
        if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
            return SectorInspectorContentHeight(rowH, gap, !context.inspectorIdUiState.idEditError.empty());
        }
        if (hasSelectedTopologyVertex && allowLegacyTopologyInspector) {
            return SelectedVertexInspectorContentHeight();
        }
        if (hasSelectedTopologySideDef && allowLegacyTopologyInspector) {
            return 1240.0f;
        }
        if (hasSelectedTopologyLineDef && allowLegacyTopologyInspector) {
            return 218.0f;
        }
        if (hasInspectedVertex) {
            return InspectedVertexInspectorContentHeight();
        }
        if (selectedAuthoringLine != nullptr) {
            const SectorAuthoringVertex* start =
                    FindSectorAuthoringVertex(authoringGraph, selectedAuthoringLine->startVertexId);
            const SectorAuthoringVertex* end =
                    FindSectorAuthoringVertex(authoringGraph, selectedAuthoringLine->endVertexId);
            const float endpointSummaryHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    start != nullptr && end != nullptr
                            ? TextFormat(
                                    "From %.2f, %.2f  To %.2f, %.2f",
                                    SectorCoordToVisibleAuthoring(start->x),
                                    SectorCoordToVisibleAuthoring(start->y),
                                    SectorCoordToVisibleAuthoring(end->x),
                                    SectorCoordToVisibleAuthoring(end->y))
                            : "Line endpoints are invalid",
                    scrollContentW);
            return AuthoringLineInspectorContentHeight(
                    *selectedAuthoringLine,
                    authoringGraph,
                    rowH,
                    gap,
                    endpointSummaryHeight);
        }
        if (selectedAuthoringFaceAnchor != nullptr
                && hasMultipleSelectedAuthoringFaces) {
            std::string selectedIds = "Faces: ";
            for (std::size_t index = 0;
                    index < selectionState.selectedAuthoringFaceAnchorIds.size();
                    ++index) {
                if (index > 0) {
                    selectedIds += ", ";
                }
                selectedIds += std::to_string(
                        selectionState.selectedAuthoringFaceAnchorIds[index]);
            }
            const float idsHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    selectedIds.c_str(),
                    scrollContentW);
            return 38.0f + idsHeight + gap + 36.0f + gap;
        }
        if (selectedAuthoringFaceAnchor != nullptr) {
            const float anchorSummaryHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    TextFormat(
                            "Anchor %.2f, %.2f",
                            SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->x),
                            SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->y)),
                    scrollContentW);
            return MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    *selectedAuthoringFaceAnchor,
                    rowH,
                    gap,
                    anchorSummaryHeight);
        }
        if (selectedAuthoringVertex != nullptr) {
            return 164.0f;
        }
        if (selectedAuthoringFogVolume != nullptr) {
            return 38.0f + (rowH + gap) * 18.0f + 28.0f;
        }
        if (selectedLevelMarker != nullptr) {
            return MeasureSectorEditorLevelMarkerInspectorContentHeight(
                    context.levelMarkerUiState,
                    rowH,
                    gap);
        }
        return 42.0f;
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
            false,
            SectorEditorPanelScrollPaddingPx
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;

    if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Authoring Inspector",
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                inspectorTarget.status.empty()
                        ? "Mapped authoring target is unavailable."
                        : inspectorTarget.status.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (hasSelectedRuntimeObject) {
        SectorEditorPlacedObjectInspectorContext runtimeObjectInspectorContext{
                ui,
                config,
                input,
                assets,
                font,
                smallFont,
                scroll,
                state,
                context.authoringGraph,
                context.topologyMap,
                context.runtimeObjects,
                context.runtimeObjectEditingUiState,
                engineContext,
                runtimeObjectEditing,
                context.runtimeObjectEditingState,
                context.staticModelPicker,
                statusText,
                deleteRuntimeObjectRequested,
                textureCatalog,
                context.sounds,
                contentW,
                rowH,
                gap
        };
        DrawSectorEditorPlacedObjectInspector(runtimeObjectInspectorContext);
        if (deleteRuntimeObjectRequested) {
            AppendRequest(
                    result,
                    SectorEditorInspectorPanelRequestKind::DeleteSelectedRuntimeObject);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
        const auto hasAuthoringGraph = [&]() {
            return !authoringGraph.vertices.empty()
                    || !authoringGraph.lines.empty()
                    || !authoringGraph.lineSides.empty()
                    || !authoringGraph.faceAnchors.empty();
        };
        const auto selectedAuthoringFaceAnchorUnavailable = [&, hasAuthoringGraph]() {
            const SectorTopologySector* selectedSector = selectedTopologySector();
            if (selectedSector == nullptr) {
                return false;
            }
            if (!hasAuthoringGraph()) {
                return true;
            }
            if (!IsSectorEditorAuthoringDerivationCurrent(context.derivation)) {
                return true;
            }
            return FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                           authoringGraph,
                           context.derivation.authoringDerivation,
                           selectedSector->id) < 0;
        };
        const auto reportAuthoringFaceAnchorUnavailable = [&, hasAuthoringGraph]() {
            const char* message = !hasAuthoringGraph()
                    ? "Cannot edit sector property: authoring data is required."
                    : "Sector property edit unavailable: selected sector has no current face anchor mapping";
            statusText = message;
            return true;
        };
        const auto mutateSelectedAuthoringFaceAnchor =
                [&](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    const SectorTopologySector* selectedSector = selectedTopologySector();
                    if (selectedSector == nullptr) {
                        return false;
                    }
                    if (!IsSectorEditorAuthoringDerivationCurrent(context.derivation)) {
                        return false;
                    }
                    if (FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                                authoringGraph,
                                context.derivation.authoringDerivation,
                                selectedSector->id) < 0) {
                        return false;
                    }
                    MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                            state,
                            context.lifecycle,
                            context.topologyMap,
                            authoringGraph,
                            context.derivation,
                            selectedSector->id,
                            status,
                            mutate);
                    return true;
                };
            const SectorEditorSectorInspectorCallbacks callbacks{
                [&]() {
                    return TryRenameSelectedDerivedSectorAuthoringNameForInspector(
                            state,
                            context.lifecycle,
                            context.topologyMap,
                            context.derivation,
                            authoringGraph,
                            context.inspectorIdUiState,
                            statusText,
                            selectedTopologySector());
                },
                [&](const char* status) { statusText = status != nullptr ? status : ""; },
                [&](const char* status) { statusText = status != nullptr ? status : ""; },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        float floorZ,
                        float ceilingZ) {
                    const char* status = "Updated sector height";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [floorZ, ceilingZ](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.floorZ == floorZ && anchor.ceilingZ == ceilingZ) {
                                        return false;
                                    }
                                    anchor.floorZ = floorZ;
                                    anchor.ceilingZ = ceilingZ;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](bool ceilingSky) {
                    const char* status = "Updated sector ceiling sky";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ceilingSky == ceilingSky) {
                                        return false;
                                    }
                                    anchor.ceilingSky = ceilingSky;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](float intensity) {
                    const char* status = "Updated sector ambient intensity";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [intensity](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientIntensity == intensity) {
                                        return false;
                                    }
                                    anchor.ambientIntensity = intensity;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](Color color) {
                    const char* status = "Updated sector ambient color";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [color](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientColor.r == color.r
                                            && anchor.ambientColor.g == color.g
                                            && anchor.ambientColor.b == color.b
                                            && anchor.ambientColor.a == color.a) {
                                        return false;
                                    }
                                    anchor.ambientColor = color;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        TopologySectorTextureField field,
                        const SectorTopologyUvSettings& uv) {
                    const char* status = "Updated sector UV";
                    const auto applyAnchorUv =
                            [field, uv](SectorAuthoringFaceAnchor& anchor) {
                                SectorTopologyUvSettings* target = nullptr;
                                switch (field) {
                                case TopologySectorTextureField::Floor:
                                    target = &anchor.floorUv;
                                    break;
                                case TopologySectorTextureField::Ceiling:
                                    target = &anchor.ceilingUv;
                                    break;
                                case TopologySectorTextureField::DefaultWall:
                                    target = &anchor.defaultWall.uv;
                                    break;
                                case TopologySectorTextureField::DefaultLower:
                                    target = &anchor.defaultLower.uv;
                                    break;
                                case TopologySectorTextureField::DefaultUpper:
                                    target = &anchor.defaultUpper.uv;
                                    break;
                                case TopologySectorTextureField::None:
                                    break;
                                }
                                if (target == nullptr) {
                                    return false;
                                }
                                *target = uv;
                                return true;
                };
                    if (mutateSelectedAuthoringFaceAnchor(status, applyAnchorUv)) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                }
        };
        if (game::DrawTopologySectorInspector(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    scroll,
                    contentW,
                    rowH,
                    gap,
                    *selectedTopologySector(),
                    state,
                    authoringGraph,
                    selectionState,
                    uiState,
                    context.inspectorIdUiState,
                    context.materialUiState,
                    materialEditing,
                    textureCatalog,
                    callbacks)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return result;
        }
    }

    if (allowLegacyTopologyInspector && (hasSelectedTopologySideDef || hasSelectedTopologyLineDef)) {
        if (DrawTopologySideDefInspector(context, scroll, contentW, rowH, gap, result)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return result;
        }
    }

    if (hasSelectedLight) {
        bool deleteRequested = false;
        bool bakeRequested = false;
        bool sourceRefreshRequested = false;
        DrawSelectedStaticLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *selectedStaticLight(),
                uiState,
                context.inspectorIdUiState,
                lightEditing,
                deleteRequested,
                bakeRequested,
                sourceRefreshRequested);
        if (deleteRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLightConfirmation);
        }
        if (bakeRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::BakeLightmaps);
        }
        if (sourceRefreshRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (hasSelectedStaticSpotLight) {
        bool deleteRequested = false;
        bool bakeRequested = false;
        bool sourceRefreshRequested = false;
        DrawSelectedStaticSpotLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *selectedStaticSpotLight(),
                uiState,
                context.inspectorIdUiState,
                lightEditing,
                deleteRequested,
                bakeRequested,
                sourceRefreshRequested);
        if (deleteRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLightConfirmation);
        }
        if (bakeRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::BakeLightmaps);
        }
        if (sourceRefreshRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (hasSelectedDynamicLight) {
        bool deleteRequested = false;
        bool sourceRefreshRequested = false;
        DrawSelectedDynamicLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *selectedDynamicLight(),
                uiState,
                context.inspectorIdUiState,
                lightEditing,
                deleteRequested,
                sourceRefreshRequested);
        if (deleteRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLightConfirmation);
        }
        if (sourceRefreshRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (hasSelectedDynamicSpotLight) {
        bool deleteRequested = false;
        bool sourceRefreshRequested = false;
        DrawSelectedDynamicSpotLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                smallFont,
                scroll,
                contentW,
                rowH,
                gap,
                *selectedDynamicSpotLight(),
                uiState,
                context.inspectorIdUiState,
                lightEditing,
                deleteRequested,
                sourceRefreshRequested);
        if (deleteRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLightConfirmation);
        }
        if (sourceRefreshRequested) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if ((allowLegacyTopologyInspector && hasSelectedTopologyVertex)
            || hasInspectedVertex) {
        const SectorEditorVertexInspectorCallbacks callbacks{
                [&]() { ClearStaleSectorEditorTopologySelection(selection); }
        };
        DrawTopologyVertexInspector(
                ui,
                config,
                input,
                assets,
                font,
                contentW,
                rowH,
                gap,
                inspectedVertex,
                hasSelectedTopologyVertex,
                context.topologyMap,
                selectionState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedAuthoringLine != nullptr) {
        const SectorAuthoringVertex* start =
                FindSectorAuthoringVertex(authoringGraph, selectedAuthoringLine->startVertexId);
        const SectorAuthoringVertex* end =
                FindSectorAuthoringVertex(authoringGraph, selectedAuthoringLine->endVertexId);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Line: %d", selectedAuthoringLine->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;

        if (start != nullptr && end != nullptr) {
            const char* endpointText = TextFormat(
                    "From %.2f, %.2f  To %.2f, %.2f",
                    SectorCoordToVisibleAuthoring(start->x),
                    SectorCoordToVisibleAuthoring(start->y),
                    SectorCoordToVisibleAuthoring(end->x),
                    SectorCoordToVisibleAuthoring(end->y));
            const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    endpointText,
                    contentW);
            engine::Text(
                    ui,
                    smallConfig,
                    assets,
                    Rectangle{0.0f, y, contentW, endpointHeight},
                    smallFont,
                    endpointText,
                    engine::UITextJustify::Left,
                    smallConfig.mutedTextColor,
                    true);
            y += endpointHeight;
        } else {
            const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    "Line endpoints are invalid",
                    contentW);
            engine::Text(ui, smallConfig, assets, Rectangle{0.0f, y, contentW, endpointHeight}, smallFont, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor, true);
            y += endpointHeight;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_insert_vertex",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Insert Vertex")) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::BeginAuthoringInsertVertex, nullptr, selectedAuthoringLine->id);
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return result;
        }
        y += rowH + gap;

        bool blocksPlayer = selectedAuthoringLine->flags.blocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_blocks_player",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            const int lineId = selectedAuthoringLine->id;
            MutateSectorEditorAuthoringLineById(
                    state,
                    context.lifecycle,
                    context.topologyMap,
                    authoringGraph,
                    context.derivation,
                    lineId,
                    "Updated authoring line flags",
                    [blocksPlayer](SectorAuthoringLine& line) {
                        if (line.flags.blocksPlayer == blocksPlayer) {
                            return false;
                        }
                        line.flags.blocksPlayer = blocksPlayer;
                        return true;
                    });
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return result;
        }
        y += 36.0f + gap;

        const auto drawAuthoringSideSection =
                [&](SectorTopologySideKind sideKind, const char* title, const char* idPrefix) {
                    engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
                    y += 18.0f;
                    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
                    y += 30.0f;

                    const SectorAuthoringSideId sideId{selectedAuthoringLine->id, sideKind};
                    const SectorAuthoringLineSide* authoringSide =
                            FindSectorAuthoringLineSide(authoringGraph, sideId);
                    const auto textureForPart = [authoringSide](TopologyWallPart part) -> std::string {
                        if (authoringSide == nullptr) {
                            return std::string{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).textureId;
                    };
                    const auto decalForPart = [authoringSide](TopologyWallPart part) -> SectorTopologyDecalLayer {
                        if (authoringSide == nullptr) {
                            return SectorTopologyDecalLayer{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).decal;
                    };
                    const auto mappedTargetForPart = [&, sideId](TopologyWallPart part, TopologySurfaceEditTarget& outTarget) {
                        if (!IsSectorEditorAuthoringDerivationCurrent(context.derivation)) {
                            return false;
                        }
                        for (const SectorAuthoringDerivedSideMapping& mapping
                                : context.derivation.authoringDerivation.mapping.sides) {
                            if (mapping.authoringLineId != sideId.lineId || mapping.authoringSide != sideId.side) {
                                continue;
                            }
                            const SectorTopologySideDef* sideDef =
                                    FindSectorTopologySideDef(context.topologyMap, mapping.topologySideDefId);
                            if (sideDef == nullptr) {
                                continue;
                            }
                            outTarget.kind = TopologyWallPartEditTargetKind(part);
                            outTarget.sectorId = sideDef->sectorId;
                            outTarget.lineDefId = sideDef->lineDefId;
                            outTarget.sideDefId = sideDef->id;
                            outTarget.side = sideDef->side;
                            return true;
                        }
                        return false;
                    };
                    const auto mutateSide = [&, sideId](const char* status, const std::function<bool(SectorAuthoringLineSide&)>& mutate) {
                        return MutateSectorEditorAuthoringSideById(
                                state,
                                context.lifecycle,
                                context.topologyMap,
                                authoringGraph,
                                context.derivation,
                                sideId,
                                status,
                                mutate);
                    };
                    const auto drawTextureRow =
                            [&](const char* suffix, const char* label, TopologyWallPart part) {
                                const float buttonW = 38.0f;
                                const bool canClear = part == TopologyWallPart::Middle;
                                const float clearW = canClear ? 58.0f : 0.0f;
                                const std::string textureId = textureForPart(part);
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !textureId.empty()
                                        && !textureCatalog.HasTexture(textureId);
                                engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        smallConfig,
                                        assets,
                                        row.valueRect,
                                        smallFont,
                                        textureId.empty() ? "<none>" : textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (canClear
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_clear", idPrefix, suffix),
                                                row.clearButtonRect,
                                                font,
                                                "Clear")) {
                                    mutateSide(
                                            "Cleared authoring middle texture",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyWallPartSettings& settings =
                                                        TopologyWallPartSettingsFor(side, part);
                                                if (IsDefaultWallPartSettings(settings)) {
                                                    return false;
                                                }
                                                settings = SectorTopologyWallPartSettings{};
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!materialEditing.OpenMaterialPickerForAuthoringSide(
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Base)) {
                                        statusText = "Authoring side texture picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;
                            };
                    const auto drawDecalControls =
                            [&](const char* suffix, const char* title, TopologyWallPart part) {
                                const SectorTopologyDecalLayer decal = decalForPart(part);
                                const float buttonW = 38.0f;
                                const float clearW = 92.0f;
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !decal.textureId.empty()
                                        && !textureCatalog.HasTexture(decal.textureId);
                                engine::Text(ui, config, assets, row.labelRect, font, title, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        smallConfig,
                                        assets,
                                        row.valueRect,
                                        smallFont,
                                        decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_clear_decal", idPrefix, suffix),
                                            row.clearButtonRect,
                                            font,
                                            "Clear")) {
                                    mutateSide(
                                            "Cleared authoring side decal",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (IsDefaultDecalLayer(target)) {
                                                    return false;
                                                }
                                                ResetDecalLayer(target);
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_pick_decal", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!materialEditing.OpenMaterialPickerForAuthoringSide(
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Decal)) {
                                        statusText = "Authoring side decal picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;

                                if (decal.textureId.empty()) {
                                    return;
                                }

                                const SectorEditorInspectorNumericRowLayout opacityLayout =
                                        BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                                        ui,
                                        config,
                                        input,
                                        assets,
                                        font,
                                        TextFormat("%s_%s_decal_opacity", idPrefix, suffix),
                                        "Opacity:",
                                        opacityLayout.labelRect,
                                        opacityLayout.inputRect,
                                        engine::UITextJustify::Left,
                                        decal.opacity,
                                        materialUiState.topologySideDefDecalOpacityInput,
                                        0.0f,
                                        1.0f,
                                        3);
                                if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                                    mutateSide(
                                            "Updated authoring side decal opacity",
                                            [part, value = opacityResult.value](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.opacity == value) {
                                                    return false;
                                                }
                                                target.opacity = value;
                                                return true;
                                            });
                                }
                                y += rowH + gap;

                                bool emissive = decal.emissive;
                                if (engine::Checkbox(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_emissive", idPrefix, suffix),
                                            Rectangle{0.0f, y, contentW, 36.0f},
                                            font,
                                            "Emissive",
                                            emissive)) {
                                    mutateSide(
                                            "Updated authoring side decal emissive",
                                            [part, emissive](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.emissive == emissive) {
                                                    return false;
                                                }
                                                target.emissive = emissive;
                                                return true;
                                            });
                                }
                                y += 36.0f + gap;

                                if (decal.emissive) {
                                    const SectorEditorInspectorNumericRowLayout bloomLayout =
                                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                    const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            font,
                                            TextFormat("%s_%s_decal_bloom", idPrefix, suffix),
                                            "Bloom:",
                                            bloomLayout.labelRect,
                                            bloomLayout.inputRect,
                                            engine::UITextJustify::Left,
                                            decal.bloomIntensity,
                                            materialUiState.topologySideDefDecalBloomIntensityInput,
                                            0.0f,
                                            10.0f,
                                            3);
                                    if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                                        mutateSide(
                                                "Updated authoring side decal bloom intensity",
                                                [part, value = bloomResult.value](SectorAuthoringLineSide& side) {
                                                    SectorTopologyDecalLayer& target =
                                                            TopologyWallPartSettingsFor(side, part).decal;
                                                    if (target.textureId.empty() || target.bloomIntensity == value) {
                                                        return false;
                                                    }
                                                    target.bloomIntensity = value;
                                                    return true;
                                                });
                                    }
                                    y += rowH + gap;
                                }

                                engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                                const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_tint", idPrefix, suffix),
                                            swatchLocal,
                                            font,
                                            "")) {
                                    TopologySurfaceEditTarget target;
                                    if (mappedTargetForPart(part, target)) {
                                        materialEditing.OpenDecalTintModal(target);
                                    } else {
                                        statusText = "Authoring side decal tint unavailable: derived mapping is not current";
                                    }
                                }
                                const Rectangle swatchScreen{
                                        scroll.viewport.x + swatchLocal.x,
                                        scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                                        swatchLocal.width,
                                        swatchLocal.height};
                                DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                                y += rowH + gap;

                                TopologySurfaceEditTarget fitTarget;
                                if (mappedTargetForPart(part, fitTarget)
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_fit_decal", idPrefix, suffix),
                                                Rectangle{0.0f, y, contentW, 36.0f},
                                                font,
                                                "Fit Decal")) {
                                    materialEditing.FitSelectedDecal(fitTarget, &assets);
                                }
                                y += 36.0f + gap;
                            };
                    drawTextureRow("wall", "Wall:", TopologyWallPart::Wall);
                    drawTextureRow("lower", "Lower:", TopologyWallPart::Lower);
                    drawTextureRow("upper", "Upper:", TopologyWallPart::Upper);
                    drawTextureRow("middle", "Middle:", TopologyWallPart::Middle);
                    drawDecalControls("wall", "Wall Decal:", TopologyWallPart::Wall);
                    drawDecalControls("lower", "Lower Decal:", TopologyWallPart::Lower);
                    drawDecalControls("upper", "Upper Decal:", TopologyWallPart::Upper);
                };

        drawAuthoringSideSection(SectorTopologySideKind::Front, "Front Side", "sector_editor_authoring_front_side");
        drawAuthoringSideSection(SectorTopologySideKind::Back, "Back Side", "sector_editor_authoring_back_side");
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedAuthoringFaceAnchor != nullptr
            && hasMultipleSelectedAuthoringFaces) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat(
                        "Selected Authoring Faces: %d",
                        static_cast<int>(
                                selectionState.selectedAuthoringFaceAnchorIds.size())),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;

        std::string selectedIds = "Faces: ";
        for (std::size_t index = 0;
                index < selectionState.selectedAuthoringFaceAnchorIds.size();
                ++index) {
            if (index > 0) {
                selectedIds += ", ";
            }
            selectedIds += std::to_string(
                    selectionState.selectedAuthoringFaceAnchorIds[index]);
        }
        const float idsHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                selectedIds.c_str(),
                contentW);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, idsHeight},
                smallFont,
                selectedIds.c_str(),
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        y += idsHeight + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_merge_selected_authoring_faces",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Merge Selected Into...")) {
            context.authoringFaceMerge.BeginTargetPick();
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedAuthoringFaceAnchor != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Face: %d", selectedAuthoringFaceAnchor->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        const char* anchorText = TextFormat(
                "Anchor %.2f, %.2f",
                SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->x),
                SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->y));
        const float anchorHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                anchorText,
                contentW);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, anchorHeight},
                smallFont,
                anchorText,
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        y += anchorHeight;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_merge_selected_authoring_face",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Merge Selected Into...")) {
            if (selectionState.selectedAuthoringFaceAnchorIds.empty()) {
                SelectSectorEditorAuthoringFaceAnchorTarget(
                        context.selection,
                        selectedAuthoringFaceAnchor->id);
            }
            context.authoringFaceMerge.BeginTargetPick();
        }
        y += 36.0f + gap;

        const int faceAnchorId = selectedAuthoringFaceAnchor->id;
        const auto mutateFaceAnchor =
                [&, faceAnchorId](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    return MutateSectorEditorAuthoringFaceAnchorById(
                            state,
                            context.lifecycle,
                            context.topologyMap,
                            authoringGraph,
                            context.derivation,
                            faceAnchorId,
                            status,
                            mutate);
                };

        bool isVoidFace = selectedAuthoringFaceAnchor->isVoid;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_void",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Void Face",
                    isVoidFace)) {
            mutateFaceAnchor(
                    "Updated authoring face void state",
                    [isVoidFace](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.isVoid == isVoidFace) {
                            return false;
                        }
                        anchor.isVoid = isVoidFace;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawHeight = [&](const char* id, const char* label, float current, engine::UIFloatInputState& inputState, bool floorField) {
            const SectorEditorInspectorNumericRowLayout layout =
                    BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    layout.labelRect,
                    layout.inputRect,
                    engine::UITextJustify::Right,
                    current,
                    inputState,
                    -512.0f,
                    512.0f,
                    2);
            if (result.changed && result.value != current) {
                const float nextFloor = floorField ? result.value : selectedAuthoringFaceAnchor->floorZ;
                const float nextCeiling = floorField ? selectedAuthoringFaceAnchor->ceilingZ : result.value;
                if (!std::isfinite(nextFloor) || !std::isfinite(nextCeiling) || nextCeiling <= nextFloor) {
                    statusText = "Invalid authoring face heights: ceiling must be greater than floor";
                } else {
                    mutateFaceAnchor(
                            "Updated authoring face height",
                            [nextFloor, nextCeiling](SectorAuthoringFaceAnchor& anchor) {
                                if (anchor.floorZ == nextFloor && anchor.ceilingZ == nextCeiling) {
                                    return false;
                                }
                                anchor.floorZ = nextFloor;
                                anchor.ceilingZ = nextCeiling;
                                return true;
                            });
                }
            }
            y += rowH + gap;
        };
        drawHeight("sector_editor_authoring_face_floor", "Floor:", selectedAuthoringFaceAnchor->floorZ, uiState.floorInput, true);
        drawHeight("sector_editor_authoring_face_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingZ, uiState.ceilingInput, false);

        bool ceilingSky = selectedAuthoringFaceAnchor->ceilingSky;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_ceiling_sky",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Ceiling Sky",
                    ceilingSky)) {
            mutateFaceAnchor(
                    "Updated authoring face ceiling sky",
                    [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ceilingSky == ceilingSky) {
                            return false;
                        }
                        anchor.ceilingSky = ceilingSky;
                        return true;
                    });
        }
        y += rowH + gap;

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Audio", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        const SectorEditorInspectorTextureRowLayout footstepRow =
                BuildSectorEditorInspectorTextureRowLayout(
                        y,
                        contentW,
                        gap,
                        38.0f,
                        0.0f);
        const std::string effectiveFootstep = context.footsteps.EffectiveSetId(
                selectedAuthoringFaceAnchor->footstepSet);
        engine::Text(ui, config, assets, footstepRow.labelRect, font, "Footsteps:", engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                smallConfig,
                assets,
                footstepRow.valueRect,
                smallFont,
                effectiveFootstep.c_str(),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_pick_footsteps",
                    footstepRow.pickerButtonRect,
                    font,
                    ">")) {
            context.footsteps.OpenForAuthoringFaceAnchor(faceAnchorId);
        }
        y += footstepRow.height + gap;

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Lighting", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;

        const float ambientIntensity = std::clamp(selectedAuthoringFaceAnchor->ambientIntensity, 0.0f, 1.0f);
        const SectorEditorInspectorNumericRowLayout ambientLayout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult ambientResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_authoring_face_ambient_intensity",
                "Intensity:",
                ambientLayout.labelRect,
                ambientLayout.inputRect,
                engine::UITextJustify::Right,
                ambientIntensity,
                uiState.ambientIntensityInput,
                0.0f,
                1.0f,
                3);
        if (ambientResult.changed && ambientResult.value != selectedAuthoringFaceAnchor->ambientIntensity) {
            mutateFaceAnchor(
                    "Updated authoring face ambient intensity",
                    [value = ambientResult.value](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ambientIntensity == value) {
                            return false;
                        }
                        anchor.ambientIntensity = value;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawAmbientChannel = [&](const char* id, const char* label, unsigned char current, engine::UIIntInputState& inputState, int channel) {
            const float colorLabelW = 92.0f;
            const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    Rectangle{0.0f, y, colorLabelW, rowH},
                    Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                    engine::UITextJustify::Right,
                    current,
                    inputState);
            if (result.changed && result.channel != current) {
                mutateFaceAnchor(
                        "Updated authoring face ambient color",
                        [channel, value = result.channel](SectorAuthoringFaceAnchor& anchor) {
                            Color next = anchor.ambientColor;
                            if (channel == 0) {
                                next.r = value;
                            } else if (channel == 1) {
                                next.g = value;
                            } else {
                                next.b = value;
                            }
                            next.a = 255;
                            if (anchor.ambientColor.r == next.r
                                    && anchor.ambientColor.g == next.g
                                    && anchor.ambientColor.b == next.b
                                    && anchor.ambientColor.a == next.a) {
                                return false;
                            }
                            anchor.ambientColor = next;
                            return true;
                        });
            }
            y += rowH + gap;
        };
        drawAmbientChannel("sector_editor_authoring_face_ambient_r", "R:", selectedAuthoringFaceAnchor->ambientColor.r, uiState.ambientRedInput, 0);
        drawAmbientChannel("sector_editor_authoring_face_ambient_g", "G:", selectedAuthoringFaceAnchor->ambientColor.g, uiState.ambientGreenInput, 1);
        drawAmbientChannel("sector_editor_authoring_face_ambient_b", "B:", selectedAuthoringFaceAnchor->ambientColor.b, uiState.ambientBlueInput, 2);

        const auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologySectorTextureField field) {
            const float buttonW = 38.0f;
            const SectorEditorInspectorTextureRowLayout row =
                    BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, 0.0f);
            const bool missing = !textureId.empty() && !textureCatalog.HasTexture(textureId);
            engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
            engine::Text(
                    ui,
                    smallConfig,
                    assets,
                    row.valueRect,
                    smallFont,
                    textureId.empty() ? "<none>" : textureId.c_str(),
                    engine::UITextJustify::Left,
                    missing ? config.invalidColor : config.mutedTextColor);
            if (engine::Button(ui, config, input, assets, id, row.pickerButtonRect, font, ">")) {
                if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                            faceAnchorId,
                            field,
                            TopologyMaterialLayer::Base)) {
                    statusText = "Authoring face texture picker unavailable: derived mapping is not current";
                }
            }
            y += row.height + gap;
        };
        const auto mappedFlatTargetForField = [&, faceAnchorId](TopologySectorTextureField field, TopologySurfaceEditTarget& outTarget) {
            if (!IsSectorEditorAuthoringDerivationCurrent(context.derivation)) {
                return false;
            }
            for (const SectorAuthoringDerivedSectorMapping& mapping
                    : context.derivation.authoringDerivation.mapping.sectors) {
                if (mapping.faceAnchorId != faceAnchorId) {
                    continue;
                }
                if (FindSectorTopologySector(context.topologyMap, mapping.topologySectorId) == nullptr) {
                    continue;
                }
                if (field == TopologySectorTextureField::Floor) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorFloor;
                } else if (field == TopologySectorTextureField::Ceiling) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorCeiling;
                } else {
                    return false;
                }
                outTarget.sectorId = mapping.topologySectorId;
                return true;
            }
            return false;
        };
        const auto drawFlatDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && !textureCatalog.HasTexture(decal.textureId);
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            smallConfig,
                            assets,
                            row.valueRect,
                            smallFont,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring face decal",
                                [field](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = nullptr;
                                    if (field == TopologySectorTextureField::Floor) {
                                        target = &anchor.floorDecal;
                                    } else if (field == TopologySectorTextureField::Ceiling) {
                                        target = &anchor.ceilingDecal;
                                    }
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring face decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            materialUiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring face decal opacity",
                                [field, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring face decal emissive",
                                [field, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                materialUiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring face decal bloom intensity",
                                    [field, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                                ? &anchor.floorDecal
                                                : &anchor.ceilingDecal;
                                        if (target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }

                    engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                    const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_tint", idPrefix),
                                swatchLocal,
                                font,
                                "")) {
                        TopologySurfaceEditTarget target;
                        if (mappedFlatTargetForField(field, target)) {
                            materialEditing.OpenDecalTintModal(target);
                        } else {
                            statusText = "Authoring face decal tint unavailable: derived mapping is not current";
                        }
                    }
                    const Rectangle swatchScreen{
                            scroll.viewport.x + swatchLocal.x,
                            scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                            swatchLocal.width,
                            swatchLocal.height};
                    DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                    y += rowH + gap;

                    TopologySurfaceEditTarget fitTarget;
                    if (mappedFlatTargetForField(field, fitTarget)
                            && engine::Button(
                                    ui,
                                    config,
                                    input,
                                    assets,
                                    TextFormat("%s_fit", idPrefix),
                                    Rectangle{0.0f, y, contentW, 36.0f},
                                    font,
                                    "Fit Decal")) {
                        materialEditing.FitSelectedDecal(fitTarget, &assets);
                    }
                    y += 36.0f + gap;
                };
        const auto drawDefaultDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && !textureCatalog.HasTexture(decal.textureId);
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            smallConfig,
                            assets,
                            row.valueRect,
                            smallFont,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    auto defaultDecalForField = [field](SectorAuthoringFaceAnchor& anchor) -> SectorTopologyDecalLayer* {
                        if (field == TopologySectorTextureField::DefaultWall) {
                            return &anchor.defaultWall.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultLower) {
                            return &anchor.defaultLower.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultUpper) {
                            return &anchor.defaultUpper.decal;
                        }
                        return nullptr;
                    };
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring default decal",
                                [defaultDecalForField](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring default decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            materialUiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring default decal opacity",
                                [defaultDecalForField, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring default decal emissive",
                                [defaultDecalForField, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                materialUiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring default decal bloom intensity",
                                    [defaultDecalForField, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                        if (target == nullptr || target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }
                };

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Materials", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        drawTextureRow("sector_editor_authoring_face_pick_floor", "Floor:", selectedAuthoringFaceAnchor->floorTextureId, TopologySectorTextureField::Floor);
        drawTextureRow("sector_editor_authoring_face_pick_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingTextureId, TopologySectorTextureField::Ceiling);
        drawTextureRow("sector_editor_authoring_face_pick_default_wall", "Wall:", selectedAuthoringFaceAnchor->defaultWall.textureId, TopologySectorTextureField::DefaultWall);
        drawTextureRow("sector_editor_authoring_face_pick_default_lower", "Lower:", selectedAuthoringFaceAnchor->defaultLower.textureId, TopologySectorTextureField::DefaultLower);
        drawTextureRow("sector_editor_authoring_face_pick_default_upper", "Upper:", selectedAuthoringFaceAnchor->defaultUpper.textureId, TopologySectorTextureField::DefaultUpper);
        drawFlatDecalControls("sector_editor_authoring_face_floor_decal", "Floor Decal:", selectedAuthoringFaceAnchor->floorDecal, TopologySectorTextureField::Floor, 0);
        drawFlatDecalControls("sector_editor_authoring_face_ceiling_decal", "Ceiling Decal:", selectedAuthoringFaceAnchor->ceilingDecal, TopologySectorTextureField::Ceiling, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_wall_decal", "Wall Decal:", selectedAuthoringFaceAnchor->defaultWall.decal, TopologySectorTextureField::DefaultWall, 0);
        drawDefaultDecalControls("sector_editor_authoring_face_default_lower_decal", "Lower Decal:", selectedAuthoringFaceAnchor->defaultLower.decal, TopologySectorTextureField::DefaultLower, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_upper_decal", "Upper Decal:", selectedAuthoringFaceAnchor->defaultUpper.decal, TopologySectorTextureField::DefaultUpper, 0);

        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedAuthoringVertex != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Vertex: %d", selectedAuthoringVertex->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat(
                        "%.2f, %.2f",
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->x),
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->y)),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;

        int incidentLineCount = 0;
        for (const SectorAuthoringLine& line : authoringGraph.lines) {
            if (line.startVertexId == selectedAuthoringVertex->id
                    || line.endVertexId == selectedAuthoringVertex->id) {
                ++incidentLineCount;
            }
        }
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat("Incident lines: %d", incidentLineCount),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_vertex_delete_or_dissolve",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    incidentLineCount == 0 ? "Delete Vertex" : "Dissolve Vertex")) {
            AppendRequest(
                    result,
                    SectorEditorInspectorPanelRequestKind::DeleteSelectedAuthoringVertex);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedAuthoringFogVolume != nullptr) {
        SectorEditorAuthoringFogVolumeEditingService& editing = context.fogVolumeEditing;
        FogVolumeEditingUiState& fogUi = context.fogVolumeUiState;
        const int fogVolumeId = selectedAuthoringFogVolume->id;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Fog Volume: %d", fogVolumeId),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;

        bool enabled = selectedAuthoringFogVolume->enabled;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_fog_volume_enabled",
                    Rectangle{0.0f, y, contentW, rowH},
                    font, "Enabled", enabled)) {
            editing.MutateById(fogVolumeId, "Updated authoring fog volume", [enabled](SectorAuthoringFogVolume& volume) {
                if (volume.enabled == enabled) return false;
                volume.enabled = enabled;
                return true;
            });
        }
        y += rowH + gap;

        const auto drawFloat = [&](const char* id,
                                   const char* label,
                                   float current,
                                   size_t inputIndex,
                                   float minimum,
                                   float maximum,
                                   int decimals,
                                   float SectorAuthoringFogVolume::*member) {
            const SectorEditorInspectorNumericRowLayout layout =
                    BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const SectorEditorFloatInputResult value = DrawLabeledFloatInput(
                    ui, config, input, assets, font,
                    id, label,
                    layout.labelRect, layout.inputRect,
                    engine::UITextJustify::Right,
                    current,
                    fogUi.floatInputs[inputIndex],
                    minimum, maximum, decimals);
            if (value.changed && value.value != current) {
                editing.MutateById(fogVolumeId, "Updated authoring fog volume", [member, value](SectorAuthoringFogVolume& volume) {
                    if (volume.*member == value.value) return false;
                    volume.*member = value.value;
                    return true;
                });
            }
            y += rowH + gap;
        };

        const auto drawPosition = [&](const char* id, const char* label, bool xAxis, size_t inputIndex) {
            const float current = xAxis
                    ? SectorCoordToVisibleAuthoring(selectedAuthoringFogVolume->x)
                    : SectorCoordToVisibleAuthoring(selectedAuthoringFogVolume->y);
            const SectorEditorInspectorNumericRowLayout layout =
                    BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const SectorEditorFloatInputResult value = DrawLabeledFloatInput(
                    ui, config, input, assets, font,
                    id, label,
                    layout.labelRect, layout.inputRect,
                    engine::UITextJustify::Right,
                    current,
                    fogUi.floatInputs[inputIndex],
                    -8192.0f, 8192.0f, 3);
            if (value.changed && value.value != current) {
                SectorCoord coord = 0;
                if (!VisibleAuthoringToSectorCoord(value.value, coord)) {
                    statusText = "Fog volume position is outside the authoring coordinate range";
                } else {
                    SectorTopologyCoordPoint point{
                            selectedAuthoringFogVolume->x,
                            selectedAuthoringFogVolume->y};
                    if (xAxis) point.x = coord; else point.y = coord;
                    editing.SetPosition(fogVolumeId, point, "Moved authoring fog volume");
                }
            }
            y += rowH + gap;
        };

        drawPosition("sector_editor_fog_volume_x", "Center X", true, 0);
        drawPosition("sector_editor_fog_volume_z", "Center Z", false, 1);
        drawFloat("sector_editor_fog_volume_bottom", "Bottom offset", selectedAuthoringFogVolume->bottomOffsetWorld, 2, -16.0f, 16.0f, 3, &SectorAuthoringFogVolume::bottomOffsetWorld);
        drawFloat("sector_editor_fog_volume_radius_x", "Radius X", selectedAuthoringFogVolume->radiusXWorld, 3, 0.05f, 64.0f, 3, &SectorAuthoringFogVolume::radiusXWorld);
        drawFloat("sector_editor_fog_volume_radius_z", "Radius Z", selectedAuthoringFogVolume->radiusZWorld, 4, 0.05f, 64.0f, 3, &SectorAuthoringFogVolume::radiusZWorld);
        drawFloat("sector_editor_fog_volume_height", "Height", selectedAuthoringFogVolume->heightWorld, 5, 0.05f, 32.0f, 3, &SectorAuthoringFogVolume::heightWorld);
        drawFloat("sector_editor_fog_volume_density", "Density", selectedAuthoringFogVolume->density, 6, 0.0f, 8.0f, 3, &SectorAuthoringFogVolume::density);
        drawFloat("sector_editor_fog_volume_opacity", "Max opacity", selectedAuthoringFogVolume->maxOpacity, 7, 0.0f, 1.0f, 3, &SectorAuthoringFogVolume::maxOpacity);
        drawFloat("sector_editor_fog_volume_softness", "Edge softness", selectedAuthoringFogVolume->edgeSoftness, 8, 0.0f, 1.0f, 3, &SectorAuthoringFogVolume::edgeSoftness);
        drawFloat("sector_editor_fog_volume_noise_scale", "Noise scale", selectedAuthoringFogVolume->noiseScaleWorld, 9, 0.05f, 64.0f, 3, &SectorAuthoringFogVolume::noiseScaleWorld);
        drawFloat("sector_editor_fog_volume_noise_amount", "Noise amount", selectedAuthoringFogVolume->noiseAmount, 10, 0.0f, 1.0f, 3, &SectorAuthoringFogVolume::noiseAmount);
        drawFloat("sector_editor_fog_volume_flow_direction", "Flow direction", selectedAuthoringFogVolume->flowDirectionDegrees, 11, 0.0f, 360.0f, 2, &SectorAuthoringFogVolume::flowDirectionDegrees);
        drawFloat("sector_editor_fog_volume_flow_speed", "Flow speed", selectedAuthoringFogVolume->flowSpeedWorld, 12, 0.0f, 8.0f, 3, &SectorAuthoringFogVolume::flowSpeedWorld);

        for (int channel = 0; channel < 3; ++channel) {
            const char* labels[] = {"Color R", "Color G", "Color B"};
            const char* ids[] = {
                    "sector_editor_fog_volume_color_r",
                    "sector_editor_fog_volume_color_g",
                    "sector_editor_fog_volume_color_b"};
            const unsigned char channels[] = {
                    selectedAuthoringFogVolume->color.r,
                    selectedAuthoringFogVolume->color.g,
                    selectedAuthoringFogVolume->color.b};
            const SectorEditorInspectorNumericRowLayout layout =
                    BuildSectorEditorInspectorRightIntRowLayout(y, contentW, rowH, gap);
            const SectorEditorIntInputResult value = DrawLabeledIntInput(
                    ui, config, input, assets, font,
                    ids[channel], labels[channel],
                    layout.labelRect, layout.inputRect,
                    engine::UITextJustify::Right,
                    channels[channel],
                    fogUi.colorInputs[static_cast<size_t>(channel)],
                    0, 255, 1);
            if (value.changed && value.value != channels[channel]) {
                editing.MutateById(fogVolumeId, "Updated authoring fog volume color", [channel, value](SectorAuthoringFogVolume& volume) {
                    unsigned char* target = channel == 0 ? &volume.color.r : channel == 1 ? &volume.color.g : &volume.color.b;
                    if (*target == value.value) return false;
                    *target = static_cast<unsigned char>(value.value);
                    return true;
                });
            }
            y += rowH + gap;
        }

        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_fog_volume_delete",
                    Rectangle{0.0f, y, contentW, rowH},
                    font, "Delete Fog Volume")) {
            AppendRequest(result, SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedFogVolumeConfirmation);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    if (selectedLevelMarker != nullptr) {
        const bool deleteRequested = DrawSectorEditorLevelMarkerInspector(
                ui,
                config,
                input,
                assets,
                font,
                contentW,
                rowH,
                gap,
                *selectedLevelMarker,
                context.levelMarkerUiState,
                context.levelMarkerEditing);
        if (deleteRequested) {
            AppendRequest(
                    result,
                    SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLevelMarkerConfirmation);
        }
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return result;
    }

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 42.0f}, font, "Selected: none", engine::UITextJustify::Left, config.mutedTextColor);

    // TODO: Add undo/redo.
    // TODO: Add validation issue highlighting.

    engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
    engine::EndPanel(ui, config, panel);
    return result;
}

} // namespace game
