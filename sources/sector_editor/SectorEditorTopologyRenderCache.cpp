#include "sector_editor/SectorEditorTopologyRenderCache.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "util/earcut.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace game {

namespace {

Vector2 CachedMapToScreen(const SectorEditorTopologyDrawContext& context, Vector2 map)
{
    const Vector2 canvasWorld = SectorAuthoringToWorldPosition(map);
    return Vector2{
            context.canvasRect.x + context.canvasRect.width * 0.5f
                    + (canvasWorld.x - context.viewCenter.x) * context.viewZoom,
            context.canvasRect.y + context.canvasRect.height * 0.5f
                    + (canvasWorld.y - context.viewCenter.y) * context.viewZoom
    };
}

Vector2 AuthoringVertexToMap(const SectorAuthoringVertex& vertex)
{
    return Vector2{
            SectorCoordToVisibleAuthoring(vertex.x),
            SectorCoordToVisibleAuthoring(vertex.y)
    };
}

Vector2 AuthoringFaceAnchorToMap(const SectorAuthoringFaceAnchor& anchor)
{
    return Vector2{
            SectorCoordToVisibleAuthoring(anchor.x),
            SectorCoordToVisibleAuthoring(anchor.y)
    };
}

const SectorAuthoringPlanarVertex* FindCachedPlanarVertex(
        const SectorAuthoringPlanarizationResult& planar,
        int vertexId)
{
    for (const SectorAuthoringPlanarVertex& vertex : planar.vertices) {
        if (vertex.id == vertexId) {
            return &vertex;
        }
    }
    return nullptr;
}

bool PlanarVertexToMap(
        const SectorAuthoringPlanarizationResult& planar,
        int vertexId,
        Vector2& outMap)
{
    const SectorAuthoringPlanarVertex* vertex = FindCachedPlanarVertex(planar, vertexId);
    if (vertex == nullptr
            || !SectorAuthoringPlanarRationalIsInteger(vertex->point.x)
            || !SectorAuthoringPlanarRationalIsInteger(vertex->point.y)) {
        return false;
    }

    outMap = Vector2{
            SectorCoordToVisibleAuthoring(SectorAuthoringPlanarRationalToSectorCoord(vertex->point.x)),
            SectorCoordToVisibleAuthoring(SectorAuthoringPlanarRationalToSectorCoord(vertex->point.y))
    };
    return true;
}

bool AppendAuthoringFaceBoundaryHighlight(
        const SectorAuthoringDerivationResult& derivation,
        const SectorAuthoringResolvedFaceMapping& mapping,
        bool isVoid,
        CachedAuthoringFaceHighlightDraw& outHighlight)
{
    const SectorAuthoringExtractedFace* face = nullptr;
    for (const SectorAuthoringExtractedFace& candidate : derivation.faces.faces) {
        if (candidate.id == mapping.extractedFaceId) {
            face = &candidate;
            break;
        }
    }
    if (face == nullptr) {
        return false;
    }

    outHighlight.faceAnchorId = mapping.faceAnchorId;
    outHighlight.topologySectorId = mapping.topologySectorId;
    outHighlight.isVoid = isVoid;
    outHighlight.outlineSegments.reserve(face->boundary.size());
    for (const SectorAuthoringFaceBoundaryEdge& edge : face->boundary) {
        Vector2 a{};
        Vector2 b{};
        if (!PlanarVertexToMap(derivation.planar, edge.startVertexId, a)
                || !PlanarVertexToMap(derivation.planar, edge.endVertexId, b)) {
            continue;
        }

        CachedTopologyOutlineSegment segment;
        segment.a = a;
        segment.b = b;
        segment.hole = isVoid;
        outHighlight.outlineSegments.push_back(segment);
    }
    return !outHighlight.outlineSegments.empty();
}

bool FindAuthoringLineMidpoint(
        const SectorAuthoringGraph& graph,
        int lineId,
        Vector2& outMap)
{
    const SectorAuthoringLine* line = FindSectorAuthoringLine(graph, lineId);
    if (line == nullptr) {
        return false;
    }

    const SectorAuthoringVertex* start = FindSectorAuthoringVertex(graph, line->startVertexId);
    const SectorAuthoringVertex* end = FindSectorAuthoringVertex(graph, line->endVertexId);
    if (start == nullptr && end == nullptr) {
        return false;
    }
    if (start == nullptr || end == nullptr) {
        outMap = AuthoringVertexToMap(start == nullptr ? *end : *start);
        return true;
    }

    const Vector2 a = AuthoringVertexToMap(*start);
    const Vector2 b = AuthoringVertexToMap(*end);
    outMap = Vector2{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    return true;
}

bool FindAuthoringDiagnosticPosition(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationDiagnostic& diagnostic,
        Vector2& outMap)
{
    const auto findLinePosition = [&]() {
        return FindAuthoringLineMidpoint(graph, diagnostic.objectId, outMap)
                || FindAuthoringLineMidpoint(graph, diagnostic.relatedObjectId, outMap);
    };
    const auto findAnchorPosition = [&]() {
        if (const SectorAuthoringFaceAnchor* anchor =
                    FindSectorAuthoringFaceAnchor(graph, diagnostic.objectId)) {
            outMap = AuthoringFaceAnchorToMap(*anchor);
            return true;
        }
        return false;
    };
    const auto findVertexPosition = [&]() {
        if (const SectorAuthoringVertex* vertex =
                    FindSectorAuthoringVertex(graph, diagnostic.objectId)) {
            outMap = AuthoringVertexToMap(*vertex);
            return true;
        }
        return false;
    };

    switch (diagnostic.kind) {
    case SectorAuthoringDerivationDiagnosticKind::DanglingLine:
    case SectorAuthoringDerivationDiagnosticKind::ZeroLengthLine:
    case SectorAuthoringDerivationDiagnosticKind::DuplicateLine:
    case SectorAuthoringDerivationDiagnosticKind::CollinearOverlap:
    case SectorAuthoringDerivationDiagnosticKind::NearMiss:
    case SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection:
    case SectorAuthoringDerivationDiagnosticKind::Planarization:
        return findLinePosition() || findVertexPosition() || findAnchorPosition();
    case SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor:
    case SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor:
        return findAnchorPosition() || findLinePosition() || findVertexPosition();
    case SectorAuthoringDerivationDiagnosticKind::AuthoringReference:
    case SectorAuthoringDerivationDiagnosticKind::FaceExtraction:
    case SectorAuthoringDerivationDiagnosticKind::TinySliverFace:
    case SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex:
    case SectorAuthoringDerivationDiagnosticKind::InvalidTopology:
        break;
    }

    if (findVertexPosition() || findLinePosition() || findAnchorPosition()) {
        return true;
    }
    return false;
}

SectorAuthoringDerivationDiagnosticKind ReferenceIssueKind(
        SectorAuthoringObjectKind objectKind)
{
    return objectKind == SectorAuthoringObjectKind::Side
            ? SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection
            : SectorAuthoringDerivationDiagnosticKind::AuthoringReference;
}

SectorAuthoringDerivationDiagnostic MakeReferenceDiagnostic(
        const SectorAuthoringValidationIssue& issue)
{
    SectorAuthoringDerivationDiagnostic diagnostic;
    diagnostic.severity = issue.severity;
    diagnostic.kind = ReferenceIssueKind(issue.objectKind);
    diagnostic.objectId = issue.objectId;
    diagnostic.message = issue.message;
    return diagnostic;
}

void AppendCachedAuthoringDiagnostic(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationDiagnostic& diagnostic,
        std::vector<CachedAuthoringDiagnosticDraw>& diagnostics)
{
    CachedAuthoringDiagnosticDraw cached;
    cached.kind = diagnostic.kind;
    cached.severity = diagnostic.severity;
    cached.objectId = diagnostic.objectId;
    cached.relatedObjectId = diagnostic.relatedObjectId;
    cached.message = diagnostic.message;
    cached.hasPosition = FindAuthoringDiagnosticPosition(graph, diagnostic, cached.map);
    diagnostics.push_back(std::move(cached));
}

void DrawCachedTopologySector(
        const CachedTopologySectorDraw& sector,
        const SectorEditorTopologyDrawContext& context,
        Color fill,
        Color outline,
        float outlineThickness = 2.0f)
{
    for (size_t i = 0; i + 2 < sector.fillTrianglePoints.size(); i += 3) {
        DrawTriangle(
                CachedMapToScreen(context, sector.fillTrianglePoints[i]),
                CachedMapToScreen(context, sector.fillTrianglePoints[i + 1]),
                CachedMapToScreen(context, sector.fillTrianglePoints[i + 2]),
                fill
        );
    }

    for (const CachedTopologyOutlineSegment& segment : sector.outlineSegments) {
        DrawLineEx(
                CachedMapToScreen(context, segment.a),
                CachedMapToScreen(context, segment.b),
                outlineThickness,
                segment.hole ? Color{164, 187, 220, 245} : outline
        );
    }

    if (context.showSectorIds && !sector.label.empty()) {
        const Vector2 screen = CachedMapToScreen(context, sector.labelCenter);
        DrawText(
                sector.label.c_str(),
                static_cast<int>(screen.x - 18.0f),
                static_cast<int>(screen.y - 10.0f),
                18,
                RAYWHITE);
    }
}

const CachedAuthoringLineDraw* FindCachedAuthoringLine(
        const SectorEditorTopologyRenderCache& cache,
        int lineId)
{
    for (const CachedAuthoringLineDraw& line : cache.authoringLines) {
        if (line.lineId == lineId) {
            return &line;
        }
    }
    return nullptr;
}

const CachedAuthoringVertexDraw* FindCachedAuthoringVertex(
        const SectorEditorTopologyRenderCache& cache,
        int vertexId)
{
    for (const CachedAuthoringVertexDraw& vertex : cache.authoringVertices) {
        if (vertex.vertexId == vertexId) {
            return &vertex;
        }
    }
    return nullptr;
}

} // namespace

bool ShouldDrawLegacyTopologySelectionHighlight(
        bool hasAuthoringGraphData,
        TopologySelectionKind selectionKind)
{
    return !hasAuthoringGraphData && selectionKind != TopologySelectionKind::None;
}

bool ShouldDrawAuthoringLineSelectionHighlight(
        SectorAuthoringSelectionTarget selectedAuthoring,
        int lineId)
{
    return selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
            && selectedAuthoring.lineId == lineId;
}

bool ShouldDrawAuthoringVertexSelectionHighlight(
        SectorAuthoringSelectionTarget selectedAuthoring,
        int vertexId)
{
    return selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
            && selectedAuthoring.vertexId == vertexId;
}

const CachedAuthoringFaceHighlightDraw* FindCachedAuthoringFaceHighlight(
        const SectorEditorTopologyRenderCache& cache,
        int faceAnchorId)
{
    const CachedAuthoringFaceHighlightDraw* found = nullptr;
    for (const CachedAuthoringFaceHighlightDraw& highlight : cache.authoringFaceHighlights) {
        if (highlight.faceAnchorId != faceAnchorId) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &highlight;
    }
    return found;
}

bool ShouldDrawAuthoringFaceSelectionHighlight(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        int faceAnchorId)
{
    return !context.derivedTopologyStale
            && context.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && context.selectedAuthoring.faceAnchorId == faceAnchorId
            && FindCachedAuthoringFaceHighlight(cache, faceAnchorId) != nullptr;
}

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        uint64_t revision)
{
    SectorEditorTopologyRenderCache cache;
    cache.revision = revision;

    cache.authoringVertices.reserve(authoringGraph.vertices.size());
    for (const SectorAuthoringVertex& vertex : authoringGraph.vertices) {
        CachedAuthoringVertexDraw cached;
        cached.vertexId = vertex.id;
        cached.point = SectorTopologyCoordPoint{vertex.x, vertex.y};
        cached.map = AuthoringVertexToMap(vertex);
        cache.authoringVertices.push_back(cached);
    }

    cache.authoringLines.reserve(authoringGraph.lines.size());
    for (const SectorAuthoringLine& line : authoringGraph.lines) {
        CachedAuthoringLineDraw cached;
        cached.lineId = line.id;
        const SectorAuthoringVertex* start = FindSectorAuthoringVertex(authoringGraph, line.startVertexId);
        const SectorAuthoringVertex* end = FindSectorAuthoringVertex(authoringGraph, line.endVertexId);
        cached.validEndpoints = start != nullptr && end != nullptr;
        if (cached.validEndpoints) {
            cached.start = AuthoringVertexToMap(*start);
            cached.end = AuthoringVertexToMap(*end);
        } else if (start != nullptr || end != nullptr) {
            cached.hasPartialEndpoint = true;
            cached.partialEndpoint = AuthoringVertexToMap(start == nullptr ? *end : *start);
        }
        cache.authoringLines.push_back(cached);
    }

    const std::vector<SectorAuthoringValidationIssue> referenceIssues =
            ValidateSectorAuthoringGraphReferences(authoringGraph);
    cache.authoringDiagnostics.reserve(referenceIssues.size() + authoringDerivation.diagnostics.size());
    for (const SectorAuthoringValidationIssue& issue : referenceIssues) {
        AppendCachedAuthoringDiagnostic(
                authoringGraph,
                MakeReferenceDiagnostic(issue),
                cache.authoringDiagnostics);
    }
    for (const SectorAuthoringDerivationDiagnostic& diagnostic : authoringDerivation.diagnostics) {
        AppendCachedAuthoringDiagnostic(authoringGraph, diagnostic, cache.authoringDiagnostics);
    }

    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
    const auto issues = ValidateSectorTopologyMap(map);
    if (!issues.empty()) {
        cache.warning = "Topology render warning: "
                + FormatSectorTopologyValidationIssue(issues.front());
    }

    cache.vertices.reserve(map.vertices.size());
    for (const SectorTopologyVertex& vertex : map.vertices) {
        CachedTopologyVertexDraw cached;
        cached.vertexId = vertex.id;
        cached.point = SectorTopologyCoordPoint{vertex.x, vertex.y};
        cached.map = SectorTopologyVertexToMap(vertex);
        cache.vertices.push_back(cached);
    }

    cache.lineDefs.reserve(map.lineDefs.size());
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        CachedTopologyLineDraw cached;
        cached.lineDefId = lineDef.id;
        cached.frontSideDefId = lineDef.frontSideDefId;
        cached.backSideDefId = lineDef.backSideDefId;

        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        cached.validEndpoints = GetSectorTopologyLineVertices(map, lineDef, start, end);
        if (cached.validEndpoints) {
            cached.start = SectorTopologyVertexToMap(*start);
            cached.end = SectorTopologyVertexToMap(*end);
        } else {
            const SectorTopologyVertex* partial = FindSectorTopologyVertex(map, lineDef.startVertexId);
            if (partial == nullptr) {
                partial = FindSectorTopologyVertex(map, lineDef.endVertexId);
            }
            if (partial != nullptr) {
                cached.hasPartialEndpoint = true;
                cached.partialEndpoint = SectorTopologyVertexToMap(*partial);
            }
        }
        cached.hasFront = lineDef.frontSideDefId >= 0
                && FindSectorTopologySideDef(map, lineDef.frontSideDefId) != nullptr;
        cached.hasBack = lineDef.backSideDefId >= 0
                && FindSectorTopologySideDef(map, lineDef.backSideDefId) != nullptr;
        cache.lineDefs.push_back(cached);
    }

    cache.sectors.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(map, indexes, sector.id, loops, &loopIssues)) {
            if (cache.warning.empty() && !loopIssues.empty()) {
                cache.warning = "Topology render warning: "
                        + FormatSectorTopologyValidationIssue(loopIssues.front());
            }
            continue;
        }

        CachedTopologySectorDraw cached;
        cached.sectorId = sector.id;
        cached.label = sector.name.empty() ? TextFormat("%d", sector.id) : sector.name;

        using DrawEarcutPoint = std::array<double, 2>;
        std::vector<std::vector<DrawEarcutPoint>> polygon;
        std::vector<Vector2> flattened;
        bool missingVertex = false;
        const auto appendLoop = [&](const SectorTopologyLoop& loop) {
            polygon.emplace_back();
            polygon.back().reserve(loop.vertexIds.size());
            for (int vertexId : loop.vertexIds) {
                const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
                if (vertex == nullptr) {
                    missingVertex = true;
                    continue;
                }
                const Vector2 mapPoint = SectorTopologyVertexToMap(*vertex);
                polygon.back().push_back(DrawEarcutPoint{mapPoint.x, mapPoint.y});
                flattened.push_back(mapPoint);
            }
        };
        appendLoop(loops.outer);
        for (const SectorTopologyLoop& hole : loops.holes) {
            appendLoop(hole);
        }

        if (missingVertex) {
            if (cache.warning.empty()) {
                cache.warning = TextFormat(
                        "Topology render warning: sector %d references a missing loop vertex",
                        sector.id
                );
            }
            continue;
        }

        const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
        cached.fillTrianglePoints.reserve(indices.size());
        for (uint32_t index : indices) {
            if (index < flattened.size()) {
                cached.fillTrianglePoints.push_back(flattened[index]);
            }
        }

        const auto appendOutline = [&](const SectorTopologyLoop& loop, bool hole) {
            if (loop.vertexIds.size() < 2) {
                return;
            }
            for (size_t i = 0; i < loop.vertexIds.size(); ++i) {
                const SectorTopologyVertex* a = FindSectorTopologyVertex(map, loop.vertexIds[i]);
                const SectorTopologyVertex* b = FindSectorTopologyVertex(
                        map,
                        loop.vertexIds[(i + 1) % loop.vertexIds.size()]
                );
                if (a == nullptr || b == nullptr) {
                    continue;
                }
                cached.outlineSegments.push_back(CachedTopologyOutlineSegment{
                        SectorTopologyVertexToMap(*a),
                        SectorTopologyVertexToMap(*b),
                        hole
                });
            }
        };
        appendOutline(loops.outer, false);
        for (const SectorTopologyLoop& hole : loops.holes) {
            appendOutline(hole, true);
        }

        if (!loops.outer.vertexIds.empty()) {
            int count = 0;
            for (int vertexId : loops.outer.vertexIds) {
                const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
                if (vertex == nullptr) {
                    continue;
                }
                const Vector2 mapPoint = SectorTopologyVertexToMap(*vertex);
                cached.labelCenter.x += mapPoint.x;
                cached.labelCenter.y += mapPoint.y;
                ++count;
            }
            if (count > 0) {
                cached.labelCenter.x /= static_cast<float>(count);
                cached.labelCenter.y /= static_cast<float>(count);
            }
        }

        cache.sectors.push_back(std::move(cached));
    }

    if (authoringDerivation.success) {
        std::map<int, int> topologySectorIdByFaceAnchorId;
        std::map<int, int> mappingCountByFaceAnchorId;
        std::map<int, const SectorAuthoringResolvedFaceMapping*> voidMappingByFaceAnchorId;
        for (const SectorAuthoringResolvedFaceMapping& mapping
                : authoringDerivation.mapping.resolvedFaces) {
            if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                    || FindSectorAuthoringFaceAnchor(authoringGraph, mapping.faceAnchorId) == nullptr) {
                continue;
            }
            ++mappingCountByFaceAnchorId[mapping.faceAnchorId];
            if (mapping.kind == SectorAuthoringFaceResolutionKind::DerivedSector) {
                topologySectorIdByFaceAnchorId[mapping.faceAnchorId] = mapping.topologySectorId;
            } else if (mapping.kind == SectorAuthoringFaceResolutionKind::VoidNoDerivedSector) {
                voidMappingByFaceAnchorId[mapping.faceAnchorId] = &mapping;
            }
        }

        cache.authoringFaceHighlights.reserve(topologySectorIdByFaceAnchorId.size() + voidMappingByFaceAnchorId.size());
        for (const auto& entry : topologySectorIdByFaceAnchorId) {
            const int faceAnchorId = entry.first;
            if (mappingCountByFaceAnchorId[faceAnchorId] != 1) {
                continue;
            }

            const CachedTopologySectorDraw* sectorDraw = nullptr;
            for (const CachedTopologySectorDraw& sector : cache.sectors) {
                if (sector.sectorId == entry.second) {
                    sectorDraw = &sector;
                    break;
                }
            }
            if (sectorDraw == nullptr || sectorDraw->outlineSegments.empty()) {
                continue;
            }

            CachedAuthoringFaceHighlightDraw highlight;
            highlight.faceAnchorId = faceAnchorId;
            highlight.topologySectorId = sectorDraw->sectorId;
            highlight.outlineSegments = sectorDraw->outlineSegments;
            cache.authoringFaceHighlights.push_back(std::move(highlight));
        }
        for (const auto& entry : voidMappingByFaceAnchorId) {
            const int faceAnchorId = entry.first;
            if (mappingCountByFaceAnchorId[faceAnchorId] != 1 || entry.second == nullptr) {
                continue;
            }
            CachedAuthoringFaceHighlightDraw highlight;
            if (AppendAuthoringFaceBoundaryHighlight(authoringDerivation, *entry.second, true, highlight)) {
                cache.authoringFaceHighlights.push_back(std::move(highlight));
            }
        }
    }

    cache.staticLights.reserve(map.staticLights.size());
    for (const SectorTopologyStaticPointLight& light : map.staticLights) {
        CachedTopologyLightDraw cached;
        cached.lightId = light.id;
        cached.map = Vector2{light.position.x, light.position.z};
        cached.color = light.color;
        cached.radiusPixelsAtZoomOne = SectorAuthoringToWorldDistance(light.radius);
        cached.sourceRadiusPixelsAtZoomOne = SectorAuthoringToWorldDistance(light.sourceRadius);
        cache.staticLights.push_back(cached);
    }
    cache.staticSpotLights.reserve(map.staticSpotLights.size());
    for (const SectorTopologyStaticSpotLight& light : map.staticSpotLights) {
        CachedTopologySpotLightDraw cached;
        cached.lightId = light.id;
        cached.origin = Vector2{light.position.x, light.position.z};
        cached.target = Vector2{light.target.x, light.target.z};
        cached.color = light.color;
        cached.range = light.range;
        cached.innerConeDegrees = light.innerConeDegrees;
        cached.outerConeDegrees = light.outerConeDegrees;
        cache.staticSpotLights.push_back(cached);
    }
    cache.dynamicLights.reserve(map.dynamicPointLights.size());
    for (const SectorTopologyDynamicPointLight& light : map.dynamicPointLights) {
        CachedTopologyLightDraw cached;
        cached.lightId = light.id;
        cached.map = Vector2{light.position.x, light.position.z};
        cached.color = light.enabled ? light.color : Color{120, 128, 140, 255};
        cached.radiusPixelsAtZoomOne = SectorAuthoringToWorldDistance(light.radius);
        cached.sourceRadiusPixelsAtZoomOne = 0.0f;
        cache.dynamicLights.push_back(cached);
    }
    cache.dynamicSpotLights.reserve(map.dynamicSpotLights.size());
    for (const SectorTopologyDynamicSpotLight& light : map.dynamicSpotLights) {
        CachedTopologySpotLightDraw cached;
        cached.lightId = light.id;
        cached.origin = Vector2{light.position.x, light.position.z};
        cached.target = Vector2{light.target.x, light.target.z};
        cached.color = light.enabled ? light.color : Color{120, 128, 140, 255};
        cached.range = light.range;
        cached.innerConeDegrees = light.innerConeDegrees;
        cached.outerConeDegrees = light.outerConeDegrees;
        cache.dynamicSpotLights.push_back(cached);
    }

    cache.valid = true;
    return cache;
}

void DrawCachedTopologySectors(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color fill = context.derivedTopologyStale
            ? Color{214, 152, 58, 26}
            : Color{82, 112, 154, 42};
    const Color outline = context.derivedTopologyStale
            ? Color{214, 152, 58, 150}
            : Color{116, 139, 174, 235};
    const Color selectedFill = Color{72, 220, 128, 38};
    const Color selectedOutline = Color{86, 232, 142, 135};

    const CachedTopologySectorDraw* selectedCachedSector = nullptr;
    for (const CachedTopologySectorDraw& sector : cache.sectors) {
        if (context.selectionKind == TopologySelectionKind::Sector
                && sector.sectorId == context.selectedSectorId) {
            selectedCachedSector = &sector;
            continue;
        }
        DrawCachedTopologySector(sector, context, fill, outline);
    }
    if (selectedCachedSector != nullptr) {
        DrawCachedTopologySector(*selectedCachedSector, context, selectedFill, selectedOutline, 10.0f);
    }
}

void DrawCachedAuthoringGraphOverlay(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color lineColor = Color{248, 216, 108, 245};
    const Color selectedLineColor = Color{122, 220, 244, 255};
    const Color hoveredLineColor = Color{122, 220, 244, 255};
    const Color lineShadow = Color{20, 24, 32, 220};
    const Color warningColor = Color{236, 92, 92, 245};
    const Color vertexFill = Color{255, 242, 178, 255};
    const Color selectedVertexFill = Color{122, 220, 244, 255};
    const Color hoveredVertexFill = Color{122, 220, 244, 255};
    const Color vertexOutline = Color{22, 26, 34, 245};

    for (const CachedAuthoringLineDraw& line : cache.authoringLines) {
        if (!line.validEndpoints) {
            if (line.hasPartialEndpoint) {
                const Vector2 point = CachedMapToScreen(context, line.partialEndpoint);
                DrawCircleLines(
                        static_cast<int>(std::round(point.x)),
                        static_cast<int>(std::round(point.y)),
                        13.0f,
                        warningColor);
            }
            continue;
        }

        const Vector2 a = CachedMapToScreen(context, line.start);
        const Vector2 b = CachedMapToScreen(context, line.end);
        const bool hovered =
                context.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Line
                && context.hoveredAuthoring.lineId == line.lineId;
        const Color activeColor = hovered ? hoveredLineColor : lineColor;
        DrawLineEx(a, b, 5.0f, lineShadow);
        DrawLineEx(a, b, 2.5f, activeColor);
    }

    for (const CachedAuthoringVertexDraw& vertex : cache.authoringVertices) {
        const Vector2 screen = CachedMapToScreen(context, vertex.map);
        const bool hovered =
                context.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Vertex
                && context.hoveredAuthoring.vertexId == vertex.vertexId;
        const Color fill = hovered ? hoveredVertexFill : vertexFill;
        DrawCircleV(screen, hovered ? 7.0f : 6.0f, vertexOutline);
        DrawCircleV(screen, hovered ? 4.5f : 3.5f, fill);
    }

    const Color voidFaceColor = Color{162, 160, 174, 160};
    for (const CachedAuthoringFaceHighlightDraw& highlight : cache.authoringFaceHighlights) {
        if (!highlight.isVoid) {
            continue;
        }
        const bool selected =
                context.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
                && context.selectedAuthoring.faceAnchorId == highlight.faceAnchorId;
        if (selected) {
            continue;
        }
        for (const CachedTopologyOutlineSegment& segment : highlight.outlineSegments) {
            const Vector2 a = CachedMapToScreen(context, segment.a);
            const Vector2 b = CachedMapToScreen(context, segment.b);
            DrawLineEx(a, b, 5.0f, lineShadow);
            DrawLineEx(a, b, 2.0f, voidFaceColor);
        }
    }

    if (context.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && !context.derivedTopologyStale) {
        if (const CachedAuthoringFaceHighlightDraw* highlight = FindCachedAuthoringFaceHighlight(
                    cache,
                    context.selectedAuthoring.faceAnchorId)) {
            const Color selectedFaceColor = highlight->isVoid
                    ? Color{190, 188, 204, 235}
                    : selectedLineColor;
            for (const CachedTopologyOutlineSegment& segment : highlight->outlineSegments) {
                const Vector2 a = CachedMapToScreen(context, segment.a);
                const Vector2 b = CachedMapToScreen(context, segment.b);
                DrawLineEx(a, b, 8.0f, lineShadow);
                DrawLineEx(a, b, 4.0f, selectedFaceColor);
            }
        }
    }

    if (const CachedAuthoringLineDraw* selectedLine = FindCachedAuthoringLine(
                cache,
                context.selectedAuthoring.lineId)) {
        if (ShouldDrawAuthoringLineSelectionHighlight(context.selectedAuthoring, selectedLine->lineId)
                && selectedLine->validEndpoints) {
            const Vector2 a = CachedMapToScreen(context, selectedLine->start);
            const Vector2 b = CachedMapToScreen(context, selectedLine->end);
            DrawLineEx(a, b, 8.0f, lineShadow);
            DrawLineEx(a, b, 4.0f, selectedLineColor);
        }
    }

    if (const CachedAuthoringVertexDraw* selectedVertex = FindCachedAuthoringVertex(
                cache,
                context.selectedAuthoring.vertexId)) {
        if (ShouldDrawAuthoringVertexSelectionHighlight(
                    context.selectedAuthoring,
                    selectedVertex->vertexId)) {
            const Vector2 screen = CachedMapToScreen(context, selectedVertex->map);
            DrawCircleV(screen, 7.5f, vertexOutline);
            DrawCircleV(screen, 4.5f, selectedVertexFill);
        }
    }
}

void DrawCachedAuthoringDiagnostics(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color errorFill = Color{238, 84, 84, 230};
    const Color warningFill = Color{236, 196, 92, 230};
    const Color outline = Color{22, 26, 34, 250};

    int unpositionedCount = 0;
    for (const CachedAuthoringDiagnosticDraw& diagnostic : cache.authoringDiagnostics) {
        if (!diagnostic.hasPosition) {
            ++unpositionedCount;
            continue;
        }

        const Vector2 center = CachedMapToScreen(context, diagnostic.map);
        const Color fill = diagnostic.severity == SectorAuthoringValidationSeverity::Warning
                ? warningFill
                : errorFill;
        DrawCircleV(center, 8.0f, outline);
        DrawCircleV(center, 5.0f, fill);
        DrawLineEx(
                Vector2{center.x - 5.0f, center.y - 5.0f},
                Vector2{center.x + 5.0f, center.y + 5.0f},
                2.0f,
                outline);
        DrawLineEx(
                Vector2{center.x + 5.0f, center.y - 5.0f},
                Vector2{center.x - 5.0f, center.y + 5.0f},
                2.0f,
                outline);
    }

    if (unpositionedCount > 0) {
        DrawText(
                TextFormat("%d authoring diagnostic(s) have no map position", unpositionedCount),
                static_cast<int>(context.canvasRect.x + 16.0f),
                static_cast<int>(context.canvasRect.y + 40.0f),
                18,
                warningFill);
    }
}

void DrawCachedTopologyLineDefs(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color oneSidedColor = Color{142, 184, 230, 255};
    const Color twoSidedColor = Color{122, 220, 244, 255};
    const Color warningColor = Color{230, 82, 82, 255};
    const Color frontColor = Color{196, 244, 255, 230};
    const Color backColor = Color{236, 154, 214, 220};
    const Color arrowColor = Color{236, 196, 92, 240};

    for (const CachedTopologyLineDraw& lineDef : cache.lineDefs) {
        if (!lineDef.validEndpoints) {
            if (lineDef.hasPartialEndpoint) {
                const Vector2 point = CachedMapToScreen(context, lineDef.partialEndpoint);
                DrawCircleLines(
                        static_cast<int>(std::round(point.x)),
                        static_cast<int>(std::round(point.y)),
                        11.0f,
                        warningColor);
            }
            continue;
        }

        const bool twoSided = lineDef.hasFront && lineDef.hasBack;
        const Color lineColor = twoSided ? twoSidedColor : oneSidedColor;

        const Vector2 a = CachedMapToScreen(context, lineDef.start);
        const Vector2 b = CachedMapToScreen(context, lineDef.end);
        DrawLineEx(a, b, twoSided ? 3.5f : 3.0f, lineColor);

        Vector2 dir{b.x - a.x, b.y - a.y};
        const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length <= GeometryEpsilon) {
            continue;
        }
        dir.x /= length;
        dir.y /= length;
        const Vector2 normal{-dir.y, dir.x};
        const Vector2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};

        const Vector2 arrowStart{mid.x - dir.x * 10.0f, mid.y - dir.y * 10.0f};
        const Vector2 arrowEnd{mid.x + dir.x * 10.0f, mid.y + dir.y * 10.0f};
        DrawLineEx(arrowStart, arrowEnd, 2.0f, arrowColor);
        DrawLineEx(
                arrowEnd,
                Vector2{arrowEnd.x - dir.x * 6.0f + normal.x * 4.0f,
                        arrowEnd.y - dir.y * 6.0f + normal.y * 4.0f},
                2.0f,
                arrowColor);
        DrawLineEx(
                arrowEnd,
                Vector2{arrowEnd.x - dir.x * 6.0f - normal.x * 4.0f,
                        arrowEnd.y - dir.y * 6.0f - normal.y * 4.0f},
                2.0f,
                arrowColor);

        if (lineDef.hasFront) {
            const Vector2 frontEnd{mid.x + normal.x * 15.0f, mid.y + normal.y * 15.0f};
            DrawLineEx(mid, frontEnd, 2.0f, frontColor);
            DrawCircleV(frontEnd, 3.0f, frontColor);
        }
        if (lineDef.hasBack) {
            const Vector2 backEnd{mid.x - normal.x * 15.0f, mid.y - normal.y * 15.0f};
            DrawLineEx(mid, backEnd, 2.0f, backColor);
            DrawCircleV(backEnd, 3.0f, backColor);
        }
    }
}

void DrawCachedTopologyVertices(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color pointColor = Color{245, 226, 154, 255};
    const Color outlineColor = Color{20, 24, 32, 255};
    const Color selectedFill = Color{72, 220, 128, 92};
    const Color selectedOutline = Color{86, 232, 142, 245};
    const Color hoverOutline = Color{122, 220, 244, 245};
    for (const CachedTopologyVertexDraw& vertex : cache.vertices) {
        const Vector2 screen = CachedMapToScreen(context, vertex.map);
        const bool selected = context.selectionKind == TopologySelectionKind::Vertex
                && vertex.vertexId == context.selectedVertexId;
        const bool hovered = context.hasHoveredVertex && vertex.vertexId == context.hoveredVertexId;
        if (selected) {
            DrawCircleV(screen, 12.0f, selectedFill);
            DrawCircleLines(
                    static_cast<int>(std::round(screen.x)),
                    static_cast<int>(std::round(screen.y)),
                    13.0f,
                    selectedOutline);
        }
        if (hovered && !selected) {
            DrawCircleLines(
                    static_cast<int>(std::round(screen.x)),
                    static_cast<int>(std::round(screen.y)),
                    11.0f,
                    hoverOutline);
        }
        DrawCircleV(screen, 4.5f, pointColor);
        DrawCircleLines(
                static_cast<int>(std::round(screen.x)),
                static_cast<int>(std::round(screen.y)),
                7.0f,
                outlineColor);
    }
}

void DrawCachedTopologyStaticLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    for (const CachedTopologyLightDraw& light : cache.staticLights) {
        const Vector2 center = CachedMapToScreen(context, light.map);
        const bool selected = context.selectionKind == TopologySelectionKind::StaticLight
                && light.lightId == context.selectedLightId;
        const bool hovered = light.lightId == context.hoveredLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;
        const float radiusPixels = light.radiusPixelsAtZoomOne * context.viewZoom;

        if (selected || hovered || context.currentTool == SectorEditorTool::StaticLight) {
            DrawCircleLines(
                    static_cast<int>(std::round(center.x)),
                    static_cast<int>(std::round(center.y)),
                    radiusPixels,
                    WithAlpha(color, selected ? 150 : 90)
            );
        }
        if (selected && light.sourceRadiusPixelsAtZoomOne > 0.0f) {
            const float sourceRadiusPixels = light.sourceRadiusPixelsAtZoomOne * context.viewZoom;
            if (sourceRadiusPixels >= 3.0f) {
                DrawCircleLines(
                        static_cast<int>(std::round(center.x)),
                        static_cast<int>(std::round(center.y)),
                        sourceRadiusPixels,
                        WithAlpha(color, 210)
                );
            }
        }

        DrawCircleV(center, selected ? 7.0f : 5.5f, color);
        DrawCircleLines(
                static_cast<int>(std::round(center.x)),
                static_cast<int>(std::round(center.y)),
                selected ? 11.0f : 9.0f,
                Color{20, 24, 32, 255});
        DrawLineEx(Vector2{center.x - 10.0f, center.y}, Vector2{center.x + 10.0f, center.y}, 2.0f, color);
        DrawLineEx(Vector2{center.x, center.y - 10.0f}, Vector2{center.x, center.y + 10.0f}, 2.0f, color);
    }
}

void DrawCachedTopologyDynamicLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    for (const CachedTopologyLightDraw& light : cache.dynamicLights) {
        const Vector2 center = CachedMapToScreen(context, light.map);
        const bool selected = context.selectionKind == TopologySelectionKind::DynamicLight
                && light.lightId == context.selectedDynamicLightId;
        const bool hovered = light.lightId == context.hoveredDynamicLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;
        const float radiusPixels = light.radiusPixelsAtZoomOne * context.viewZoom;

        if (selected || hovered || context.currentTool == SectorEditorTool::DynamicLight) {
            DrawCircleLines(
                    static_cast<int>(std::round(center.x)),
                    static_cast<int>(std::round(center.y)),
                    radiusPixels,
                    WithAlpha(color, selected ? 150 : 90)
            );
        }

        const float diamondRadius = selected ? 8.0f : 6.5f;
        const Vector2 top{center.x, center.y - diamondRadius};
        const Vector2 right{center.x + diamondRadius, center.y};
        const Vector2 bottom{center.x, center.y + diamondRadius};
        const Vector2 left{center.x - diamondRadius, center.y};
        DrawTriangle(top, left, right, color);
        DrawTriangle(bottom, right, left, color);
        DrawLineEx(top, right, 2.0f, Color{20, 24, 32, 255});
        DrawLineEx(right, bottom, 2.0f, Color{20, 24, 32, 255});
        DrawLineEx(bottom, left, 2.0f, Color{20, 24, 32, 255});
        DrawLineEx(left, top, 2.0f, Color{20, 24, 32, 255});
        DrawLineEx(Vector2{center.x - 10.0f, center.y}, Vector2{center.x + 10.0f, center.y}, 2.0f, color);
        DrawLineEx(Vector2{center.x, center.y - 10.0f}, Vector2{center.x, center.y + 10.0f}, 2.0f, color);
    }
}

void DrawCachedTopologyStaticSpotLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color outline = Color{20, 24, 32, 255};
    for (const CachedTopologySpotLightDraw& light : cache.staticSpotLights) {
        const Vector2 origin = CachedMapToScreen(context, light.origin);
        const Vector2 target = CachedMapToScreen(context, light.target);
        const bool selected = context.selectionKind == TopologySelectionKind::StaticSpotLight
                && light.lightId == context.selectedStaticSpotLightId;
        const bool hovered = light.lightId == context.hoveredStaticSpotLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;

        Vector2 direction{
                light.target.x - light.origin.x,
                light.target.y - light.origin.y
        };
        float directionLength = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (directionLength <= GeometryEpsilon) {
            direction = Vector2{1.0f, 0.0f};
            directionLength = 1.0f;
        }
        direction.x /= directionLength;
        direction.y /= directionLength;

        const float outerRadians = light.outerConeDegrees * 0.5f * DEG2RAD;
        const float cosOuter = std::cos(outerRadians);
        const float sinOuter = std::sin(outerRadians);
        const Vector2 leftDir{
                direction.x * cosOuter - direction.y * sinOuter,
                direction.x * sinOuter + direction.y * cosOuter
        };
        const Vector2 rightDir{
                direction.x * cosOuter + direction.y * sinOuter,
                -direction.x * sinOuter + direction.y * cosOuter
        };
        const Vector2 leftMap{
                light.origin.x + leftDir.x * light.range,
                light.origin.y + leftDir.y * light.range
        };
        const Vector2 rightMap{
                light.origin.x + rightDir.x * light.range,
                light.origin.y + rightDir.y * light.range
        };
        const Vector2 left = CachedMapToScreen(context, leftMap);
        const Vector2 right = CachedMapToScreen(context, rightMap);

        const bool drawCone = selected || hovered || context.currentTool == SectorEditorTool::StaticSpotLight;
        if (drawCone) {
            DrawTriangle(origin, left, right, WithAlpha(color, selected ? 42 : 24));
            DrawLineEx(origin, left, selected ? 2.5f : 1.5f, WithAlpha(color, selected ? 210 : 135));
            DrawLineEx(origin, right, selected ? 2.5f : 1.5f, WithAlpha(color, selected ? 210 : 135));
            DrawLineEx(left, right, selected ? 2.0f : 1.0f, WithAlpha(color, selected ? 150 : 90));

            if (selected && light.innerConeDegrees > 0.0f
                    && light.innerConeDegrees < light.outerConeDegrees - GeometryEpsilon) {
                const float innerRadians = light.innerConeDegrees * 0.5f * DEG2RAD;
                const float cosInner = std::cos(innerRadians);
                const float sinInner = std::sin(innerRadians);
                const Vector2 innerLeftMap{
                        light.origin.x + (direction.x * cosInner - direction.y * sinInner) * light.range,
                        light.origin.y + (direction.x * sinInner + direction.y * cosInner) * light.range
                };
                const Vector2 innerRightMap{
                        light.origin.x + (direction.x * cosInner + direction.y * sinInner) * light.range,
                        light.origin.y + (-direction.x * sinInner + direction.y * cosInner) * light.range
                };
                DrawLineEx(origin, CachedMapToScreen(context, innerLeftMap), 1.5f, WithAlpha(color, 150));
                DrawLineEx(origin, CachedMapToScreen(context, innerRightMap), 1.5f, WithAlpha(color, 150));
            }
        }

        DrawLineEx(origin, target, selected ? 3.0f : 2.0f, WithAlpha(color, selected ? 235 : 165));
        const float originRadius = selected ? 8.5f : 7.0f;
        const Vector2 markerTop{origin.x, origin.y - originRadius};
        const Vector2 markerRight{origin.x + originRadius, origin.y};
        const Vector2 markerBottom{origin.x, origin.y + originRadius};
        const Vector2 markerLeft{origin.x - originRadius, origin.y};
        DrawTriangle(markerTop, markerLeft, markerRight, color);
        DrawTriangle(markerBottom, markerRight, markerLeft, color);
        DrawLineEx(markerTop, markerRight, 2.0f, outline);
        DrawLineEx(markerRight, markerBottom, 2.0f, outline);
        DrawLineEx(markerBottom, markerLeft, 2.0f, outline);
        DrawLineEx(markerLeft, markerTop, 2.0f, outline);
        DrawCircleV(target, selected ? 5.5f : 4.5f, WithAlpha(color, selected ? 245 : 205));
        DrawCircleLines(
                static_cast<int>(std::round(target.x)),
                static_cast<int>(std::round(target.y)),
                selected ? 9.0f : 7.0f,
                outline);
    }
}

void DrawCachedTopologyDynamicSpotLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color outline = Color{20, 24, 32, 255};
    for (const CachedTopologySpotLightDraw& light : cache.dynamicSpotLights) {
        const Vector2 origin = CachedMapToScreen(context, light.origin);
        const Vector2 target = CachedMapToScreen(context, light.target);
        const bool selected = context.selectionKind == TopologySelectionKind::DynamicSpotLight
                && light.lightId == context.selectedDynamicSpotLightId;
        const bool hovered = light.lightId == context.hoveredDynamicSpotLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;

        Vector2 direction{
                light.target.x - light.origin.x,
                light.target.y - light.origin.y
        };
        float directionLength = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (directionLength <= GeometryEpsilon) {
            direction = Vector2{1.0f, 0.0f};
            directionLength = 1.0f;
        }
        direction.x /= directionLength;
        direction.y /= directionLength;

        const float outerRadians = light.outerConeDegrees * 0.5f * DEG2RAD;
        const float cosOuter = std::cos(outerRadians);
        const float sinOuter = std::sin(outerRadians);
        const Vector2 leftDir{
                direction.x * cosOuter - direction.y * sinOuter,
                direction.x * sinOuter + direction.y * cosOuter
        };
        const Vector2 rightDir{
                direction.x * cosOuter + direction.y * sinOuter,
                -direction.x * sinOuter + direction.y * cosOuter
        };
        const Vector2 leftMap{
                light.origin.x + leftDir.x * light.range,
                light.origin.y + leftDir.y * light.range
        };
        const Vector2 rightMap{
                light.origin.x + rightDir.x * light.range,
                light.origin.y + rightDir.y * light.range
        };
        const Vector2 left = CachedMapToScreen(context, leftMap);
        const Vector2 right = CachedMapToScreen(context, rightMap);

        const bool drawCone = selected || hovered || context.currentTool == SectorEditorTool::DynamicSpotLight;
        if (drawCone) {
            DrawTriangle(origin, left, right, WithAlpha(color, selected ? 42 : 24));
            DrawLineEx(origin, left, selected ? 2.5f : 1.5f, WithAlpha(color, selected ? 210 : 135));
            DrawLineEx(origin, right, selected ? 2.5f : 1.5f, WithAlpha(color, selected ? 210 : 135));
            DrawLineEx(left, right, selected ? 2.0f : 1.0f, WithAlpha(color, selected ? 150 : 90));

            if (selected && light.innerConeDegrees > 0.0f
                    && light.innerConeDegrees < light.outerConeDegrees - GeometryEpsilon) {
                const float innerRadians = light.innerConeDegrees * 0.5f * DEG2RAD;
                const float cosInner = std::cos(innerRadians);
                const float sinInner = std::sin(innerRadians);
                const Vector2 innerLeftMap{
                        light.origin.x + (direction.x * cosInner - direction.y * sinInner) * light.range,
                        light.origin.y + (direction.x * sinInner + direction.y * cosInner) * light.range
                };
                const Vector2 innerRightMap{
                        light.origin.x + (direction.x * cosInner + direction.y * sinInner) * light.range,
                        light.origin.y + (-direction.x * sinInner + direction.y * cosInner) * light.range
                };
                DrawLineEx(origin, CachedMapToScreen(context, innerLeftMap), 1.5f, WithAlpha(color, 150));
                DrawLineEx(origin, CachedMapToScreen(context, innerRightMap), 1.5f, WithAlpha(color, 150));
            }
        }

        DrawLineEx(origin, target, selected ? 3.0f : 2.0f, WithAlpha(color, selected ? 235 : 165));
        DrawCircleV(origin, selected ? 7.5f : 6.0f, color);
        DrawCircleLines(
                static_cast<int>(std::round(origin.x)),
                static_cast<int>(std::round(origin.y)),
                selected ? 11.0f : 9.0f,
                outline);
        DrawCircleV(target, selected ? 5.5f : 4.5f, WithAlpha(color, selected ? 245 : 205));
        DrawCircleLines(
                static_cast<int>(std::round(target.x)),
                static_cast<int>(std::round(target.y)),
                selected ? 9.0f : 7.0f,
                outline);
    }
}

} // namespace game
