#include "sector_editor/SectorEditorTopologyRenderCache.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorDoorRuntime.h"
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

float PickTriangleCross(Vector2 a, Vector2 b, Vector2 point)
{
    return (b.x - a.x) * (point.y - a.y)
            - (b.y - a.y) * (point.x - a.x);
}

bool PointInOrOnPickTriangle(Vector2 point, Vector2 a, Vector2 b, Vector2 c)
{
    constexpr float epsilon = 0.0001f;
    const float ab = PickTriangleCross(a, b, point);
    const float bc = PickTriangleCross(b, c, point);
    const float ca = PickTriangleCross(c, a, point);
    const bool hasNegative = ab < -epsilon || bc < -epsilon || ca < -epsilon;
    const bool hasPositive = ab > epsilon || bc > epsilon || ca > epsilon;
    return !(hasNegative && hasPositive);
}

float DistanceSquaredToPickSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length2 = dx * dx + dy * dy;
    if (length2 <= 0.000001f) {
        const float px = point.x - a.x;
        const float py = point.y - a.y;
        return px * px + py * py;
    }

    const float t = std::clamp(
            ((point.x - a.x) * dx + (point.y - a.y) * dy) / length2,
            0.0f,
            1.0f);
    const float px = point.x - (a.x + dx * t);
    const float py = point.y - (a.y + dy * t);
    return px * px + py * py;
}

float DistanceSquaredToPickQuad(Vector2 point, const Vector2 (&corners)[4])
{
    if (PointInOrOnPickTriangle(point, corners[0], corners[1], corners[2])
            || PointInOrOnPickTriangle(point, corners[0], corners[2], corners[3])) {
        return 0.0f;
    }

    float distance2 = DistanceSquaredToPickSegment(point, corners[0], corners[1]);
    for (int i = 1; i < 4; ++i) {
        distance2 = std::min(
                distance2,
                DistanceSquaredToPickSegment(point, corners[i], corners[(i + 1) % 4]));
    }
    return distance2;
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
    const auto findFogVolumePosition = [&]() {
        if (const SectorAuthoringFogVolume* volume =
                    FindSectorAuthoringFogVolume(graph, diagnostic.objectId)) {
            outMap = Vector2{
                    SectorCoordToVisibleAuthoring(volume->x),
                    SectorCoordToVisibleAuthoring(volume->y)};
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
    case SectorAuthoringDerivationDiagnosticKind::UnresolvedFogVolume:
        return findFogVolumePosition();
    case SectorAuthoringDerivationDiagnosticKind::AuthoringReference:
    case SectorAuthoringDerivationDiagnosticKind::FaceExtraction:
    case SectorAuthoringDerivationDiagnosticKind::TinySliverFace:
    case SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex:
    case SectorAuthoringDerivationDiagnosticKind::InvalidTopology:
        break;
    }

    if (findVertexPosition() || findLinePosition() || findAnchorPosition() || findFogVolumePosition()) {
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

void DrawTopologyLightTypeLabel(Vector2 anchor, const char* label)
{
    const Color labelColor = Color{92, 255, 176, 255};
    DrawText(
            label,
            static_cast<int>(anchor.x + 12.0f),
            static_cast<int>(anchor.y - 22.0f),
            18,
            labelColor);
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

Vector2 DoorWorldToCachedMap(Vector2 world)
{
    return Vector2{
            SectorWorldToAuthoringDistance(world.x),
            SectorWorldToAuthoringDistance(world.y)};
}

void SetCachedDoorCorners(
        Vector2 center,
        Vector2 widthAxis,
        Vector2 thicknessAxis,
        float width,
        float thickness,
        Vector2 (&outCorners)[4])
{
    const float halfWidth = SectorWorldToAuthoringDistance(width) * 0.5f;
    const float halfThickness = SectorWorldToAuthoringDistance(thickness) * 0.5f;
    center = DoorWorldToCachedMap(center);
    outCorners[0] = Vector2{
            center.x - widthAxis.x * halfWidth - thicknessAxis.x * halfThickness,
            center.y - widthAxis.y * halfWidth - thicknessAxis.y * halfThickness};
    outCorners[1] = Vector2{
            center.x + widthAxis.x * halfWidth - thicknessAxis.x * halfThickness,
            center.y + widthAxis.y * halfWidth - thicknessAxis.y * halfThickness};
    outCorners[2] = Vector2{
            center.x + widthAxis.x * halfWidth + thicknessAxis.x * halfThickness,
            center.y + widthAxis.y * halfWidth + thicknessAxis.y * halfThickness};
    outCorners[3] = Vector2{
            center.x - widthAxis.x * halfWidth + thicknessAxis.x * halfThickness,
            center.y - widthAxis.y * halfWidth + thicknessAxis.y * halfThickness};
}

void PopulateCachedDoorDraw(
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& object,
        const SectorSwingDoorCatalog* swingDoorCatalog,
        CachedRuntimeObjectDraw& cached)
{
    cached.isDoor = true;
    cached.doorModelMetadataValid = true;
    const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(map, object.door);
    if (!resolved.valid) {
        cached.definitionKnown = false;
        return;
    }

    cached.doorEndpointA = DoorWorldToCachedMap(resolved.endpointA);
    cached.doorEndpointB = DoorWorldToCachedMap(resolved.endpointB);

    float width = resolved.width;
    float height = resolved.height;
    float thickness = object.door.thickness;
    float effectiveScale = 1.0f;
    float leafHingeToFrameCenter = 0.0f;
    float leafBottomOffset = 0.0f;
    bool alignLeafToFrame = false;
    if (object.door.visual == SectorDoorVisualType::Model) {
        cached.doorModelMetadataValid = false;
        SectorSwingDoorCatalogAsset asset;
        if (swingDoorCatalog != nullptr
                && FindSectorSwingDoorCatalogAsset(
                        *swingDoorCatalog, object.door.modelAssetId, asset)) {
            const SectorSwingDoorFitResult fit = ComputeSectorSwingDoorFit(
                    asset,
                    resolved.width,
                    resolved.height,
                    object.door.modelFit,
                    object.door.modelScale);
            if (fit.status != SectorSwingDoorFitStatus::InvalidInput) {
                width = fit.actualWidth;
                height = fit.actualHeight;
                thickness = fit.actualThickness;
                effectiveScale = fit.effectiveScale;
                if (asset.hasFrame) {
                    leafHingeToFrameCenter = asset.leafHingeToFrameCenter
                            * fit.effectiveScale;
                    leafBottomOffset = asset.leafBottomOffset
                            * fit.effectiveScale;
                    alignLeafToFrame = true;
                }
                cached.doorModelMetadataValid = true;
            }
        }
    }

    const SectorDoorResolvedAnchor runtimeAnchor = ToSectorRuntimeDoorAnchor(resolved);
    SectorDoorRender runtimeRender{
            width,
            height,
            thickness,
            object.door.normalOffset,
            object.door.heightOffsetWorld,
            {},
            {},
            WHITE,
            true};
    runtimeRender.leafHingeToFrameCenter = leafHingeToFrameCenter;
    runtimeRender.leafBottomOffset = leafBottomOffset;
    runtimeRender.alignLeafToFrame = alignLeafToFrame;
    if (object.door.motion == SectorDoorMotionType::Swing) {
        const SectorDoorMotion runtimeMotion{
                SectorDoorMotionType::Swing,
                0.0f,
                0.0f,
                object.door.openAngleDegrees * DEG2RAD,
                object.door.angularSpeedDegrees * DEG2RAD,
                object.door.hinge,
                object.door.swingSide};
        const SectorDoorSwingPose closed = BuildSectorDoorSwingPose(
                runtimeAnchor, runtimeMotion, runtimeRender, effectiveScale, 0.0f);
        const SectorDoorSwingPose open = BuildSectorDoorSwingPose(
                runtimeAnchor, runtimeMotion, runtimeRender, effectiveScale, 1.0f);
        SetCachedDoorCorners(
                Vector2{closed.center.x, closed.center.z},
                closed.widthAxis,
                closed.thicknessAxis,
                width,
                thickness,
                cached.doorCorners);
        SetCachedDoorCorners(
                Vector2{open.center.x, open.center.z},
                open.widthAxis,
                open.thicknessAxis,
                width,
                thickness,
                cached.doorOpenCorners);
        cached.map = DoorWorldToCachedMap(Vector2{closed.center.x, closed.center.z});
        cached.doorHinge = DoorWorldToCachedMap(
                Vector2{closed.hingePosition.x, closed.hingePosition.z});
        cached.doorOpenFreeEdge = DoorWorldToCachedMap(Vector2{
                open.hingePosition.x + open.widthAxis.x * width,
                open.hingePosition.z + open.widthAxis.y * width});
        cached.doorSwingArcRadius = SectorWorldToAuthoringDistance(width);
        cached.doorSwingArcStartRadians = std::atan2(
                closed.widthAxis.y, closed.widthAxis.x);
        cached.doorSwingArcSweepRadians = open.angleRadians;
        cached.doorSwingIntoBack = object.door.swingSide == SectorDoorSwingSide::Back;
        cached.doorSwingGuideValid = std::isfinite(cached.doorSwingArcRadius)
                && cached.doorSwingArcRadius > 0.0f
                && std::isfinite(cached.doorSwingArcStartRadians)
                && std::isfinite(cached.doorSwingArcSweepRadians);
    } else {
        const Vector2 midpoint{
                resolved.midpoint.x + resolved.normal.x * object.door.normalOffset,
                resolved.midpoint.y + resolved.normal.y * object.door.normalOffset};
        SetCachedDoorCorners(
                midpoint,
                resolved.tangent,
                resolved.normal,
                width,
                thickness,
                cached.doorCorners);
        cached.map = DoorWorldToCachedMap(midpoint);
    }
    cached.doorFootprintValid = std::isfinite(width)
            && width > 0.0f
            && std::isfinite(thickness)
            && thickness > 0.0f;
}

} // namespace

void UpdateCachedSectorEditorRuntimeObjectDraw(
        SectorEditorTopologyRenderCache& cache,
        const SectorPlacedRuntimeObject& object)
{
    for (CachedRuntimeObjectDraw& cached : cache.runtimeObjects) {
        if (cached.objectId != object.id) {
            continue;
        }
        cached.definitionId = !object.kind.empty()
                ? object.kind
                : object.definitionId;
        cached.map = Vector2{object.position.x, object.position.z};
        cached.yawRadians = object.yawRadians;
        cached.definitionKnown = object.kind == "billboard"
                || object.kind == "door"
                || object.kind == "static_model"
                || object.kind == "dynamic_model"
                || object.kind == "npc";
        cached.isDoor = object.kind == "door";
        cached.isNpc = object.kind == "npc";
        cached.doorFootprintValid = false;
        return;
    }
}

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
    const bool selected = context.selectedAuthoringFaceAnchorIds != nullptr
            ? std::find(
                      context.selectedAuthoringFaceAnchorIds->begin(),
                      context.selectedAuthoringFaceAnchorIds->end(),
                      faceAnchorId)
                    != context.selectedAuthoringFaceAnchorIds->end()
            : context.selectedAuthoring.kind
                            == SectorAuthoringSelectionKind::FaceAnchor
                    && context.selectedAuthoring.faceAnchorId == faceAnchorId;
    return !context.derivedTopologyStale
            && selected
            && FindCachedAuthoringFaceHighlight(cache, faceAnchorId) != nullptr;
}

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        uint64_t revision,
        const SectorSwingDoorCatalog* swingDoorCatalog,
        uint64_t swingDoorCatalogRevision)
{
    SectorEditorTopologyRenderCache cache;
    cache.revision = revision;
    cache.swingDoorCatalogRevision = swingDoorCatalogRevision;

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

    cache.levelMarkers.reserve(authoringGraph.levelMarkers.size());
    for (const SectorAuthoringLevelMarker& marker : authoringGraph.levelMarkers) {
        CachedAuthoringLevelMarkerDraw cached;
        cached.markerId = marker.id;
        cached.referenceId = marker.referenceId;
        cached.map = Vector2{
                SectorCoordToVisibleAuthoring(marker.x),
                SectorCoordToVisibleAuthoring(marker.z)};
        cached.orientationDegrees = marker.orientationDegrees;
        cache.levelMarkers.push_back(std::move(cached));
    }

    cache.triggers.reserve(authoringGraph.triggers.size());
    for (const SectorAuthoringTrigger& trigger : authoringGraph.triggers) {
        CachedAuthoringTriggerDraw cached;
        cached.triggerId = trigger.editorId;
        cached.id = trigger.id;
        cached.enabled = trigger.enabled;
        std::vector<std::vector<std::array<double, 2>>> polygon(1);
        for (SectorTriggerPoint point : trigger.points) {
            const Vector2 map{SectorCoordToVisibleAuthoring(point.x),
                    SectorCoordToVisibleAuthoring(point.z)};
            cached.points.push_back(map);
            polygon[0].push_back({static_cast<double>(map.x), static_cast<double>(map.y)});
        }
        const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
        cached.fillTrianglePoints.reserve(indices.size());
        for (uint32_t index : indices) {
            if (index < cached.points.size()) cached.fillTrianglePoints.push_back(cached.points[index]);
        }
        cache.triggers.push_back(std::move(cached));
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

    cache.runtimeObjects.reserve(map.runtimeObjects.size());
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        CachedRuntimeObjectDraw cached;
        cached.objectId = object.id;
        cached.definitionId = !object.kind.empty() ? object.kind : object.definitionId;
        cached.map = Vector2{object.position.x, object.position.z};
        cached.yawRadians = object.yawRadians;
        cached.definitionKnown = object.kind == "billboard"
                || object.kind == "door"
                || object.kind == "static_model"
                || object.kind == "dynamic_model"
                || object.kind == "npc";
        cached.isDoor = object.kind == "door";
        cached.isNpc = object.kind == "npc";
        if (cached.isDoor) {
            PopulateCachedDoorDraw(map, object, swingDoorCatalog, cached);
        }
        cache.runtimeObjects.push_back(cached);
    }

    cache.valid = true;
    return cache;
}

void AppendCachedRuntimeObjectPickCandidates(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        Vector2 screenPoint,
        float tolerancePixels,
        std::vector<SectorEditorPickCandidate>& outCandidates)
{
    if (!cache.valid || tolerancePixels < 0.0f) {
        return;
    }

    for (const CachedRuntimeObjectDraw& object : cache.runtimeObjects) {
        const Vector2 center = CachedMapToScreen(context, object.map);
        const float centerDx = center.x - screenPoint.x;
        const float centerDy = center.y - screenPoint.y;
        float distance2 = centerDx * centerDx + centerDy * centerDy;

        if (object.isDoor && object.doorFootprintValid) {
            Vector2 corners[4];
            for (int i = 0; i < 4; ++i) {
                corners[i] = CachedMapToScreen(context, object.doorCorners[i]);
            }
            distance2 = DistanceSquaredToPickQuad(screenPoint, corners);
        }

        const float activeTolerance = object.isNpc
                ? tolerancePixels + 18.0f
                : tolerancePixels;
        if (distance2 <= activeTolerance * activeTolerance) {
            outCandidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::RuntimeObject, object.objectId},
                    distance2});
        }
    }
}

void AppendCachedLevelMarkerPickCandidates(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        Vector2 screenPoint,
        float tolerancePixels,
        std::vector<SectorEditorPickCandidate>& outCandidates)
{
    if (!cache.valid || tolerancePixels < 0.0f) {
        return;
    }
    const float tolerance2 = tolerancePixels * tolerancePixels;
    for (const CachedAuthoringLevelMarkerDraw& marker : cache.levelMarkers) {
        const Vector2 center = CachedMapToScreen(context, marker.map);
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 <= tolerance2) {
            outCandidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::LevelMarker, marker.markerId},
                    distance2});
        }
    }
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
        const bool selected = ShouldDrawAuthoringFaceSelectionHighlight(
                cache,
                context,
                highlight.faceAnchorId);
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

    if (!context.derivedTopologyStale) {
        for (const CachedAuthoringFaceHighlightDraw& highlight
                : cache.authoringFaceHighlights) {
            if (!ShouldDrawAuthoringFaceSelectionHighlight(
                        cache,
                        context,
                        highlight.faceAnchorId)) {
                continue;
            }
            const Color selectedFaceColor = highlight.isVoid
                    ? Color{190, 188, 204, 235}
                    : selectedLineColor;
            for (const CachedTopologyOutlineSegment& segment
                    : highlight.outlineSegments) {
                const Vector2 a = CachedMapToScreen(context, segment.a);
                const Vector2 b = CachedMapToScreen(context, segment.b);
                DrawLineEx(a, b, 8.0f, lineShadow);
                DrawLineEx(a, b, 4.0f, selectedFaceColor);
            }
        }

        if (const CachedAuthoringFaceHighlightDraw* target =
                    FindCachedAuthoringFaceHighlight(
                            cache,
                            context.authoringFaceMergeTargetId)) {
            const Color targetColor{100, 226, 150, 255};
            for (const CachedTopologyOutlineSegment& segment
                    : target->outlineSegments) {
                const Vector2 a = CachedMapToScreen(context, segment.a);
                const Vector2 b = CachedMapToScreen(context, segment.b);
                DrawLineEx(a, b, 10.0f, lineShadow);
                DrawLineEx(a, b, 5.0f, targetColor);
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
        DrawTopologyLightTypeLabel(center, "SL");
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
        DrawTopologyLightTypeLabel(center, "DL");
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
        DrawTopologyLightTypeLabel(origin, "SS");
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
        DrawTopologyLightTypeLabel(origin, "DS");
    }
}

void DrawCachedRuntimeObjects(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color outline = Color{20, 24, 32, 255};
    const Color objectFill = Color{238, 204, 96, 235};
    const Color npcFill = Color{245, 90, 190, 255};
    const Color doorFill = Color{72, 220, 128, 64};
    const Color doorLine = Color{72, 220, 128, 235};
    const Color selectedFill = Color{122, 220, 244, 255};
    const Color missingFill = Color{236, 92, 92, 245};
    for (const CachedRuntimeObjectDraw& object : cache.runtimeObjects) {
        const Vector2 center = CachedMapToScreen(context, object.map);
        const bool selected = object.objectId == context.selectedRuntimeObjectId;
        const Color fill = !object.definitionKnown || !object.doorModelMetadataValid
                ? missingFill
                : selected ? selectedFill : object.isNpc ? npcFill : objectFill;

        const float radius = selected ? 8.0f : 6.0f;
        if (object.isDoor && object.doorFootprintValid) {
            const Color guideColor = object.doorSwingIntoBack
                    ? Color{95, 166, 255, 185}
                    : Color{255, 174, 72, 185};
            if (object.doorSwingGuideValid) {
                const Vector2 open0 = CachedMapToScreen(context, object.doorOpenCorners[0]);
                const Vector2 open1 = CachedMapToScreen(context, object.doorOpenCorners[1]);
                const Vector2 open2 = CachedMapToScreen(context, object.doorOpenCorners[2]);
                const Vector2 open3 = CachedMapToScreen(context, object.doorOpenCorners[3]);
                DrawLineEx(open0, open1, 1.5f, guideColor);
                DrawLineEx(open1, open2, 1.5f, guideColor);
                DrawLineEx(open2, open3, 1.5f, guideColor);
                DrawLineEx(open3, open0, 1.5f, guideColor);

                const int arcSegments = std::clamp(
                        static_cast<int>(std::ceil(
                                std::fabs(object.doorSwingArcSweepRadians)
                                / (5.0f * DEG2RAD))),
                        1,
                        34);
                Vector2 previousMap{
                        object.doorHinge.x
                                + std::cos(object.doorSwingArcStartRadians)
                                        * object.doorSwingArcRadius,
                        object.doorHinge.y
                                + std::sin(object.doorSwingArcStartRadians)
                                        * object.doorSwingArcRadius};
                for (int segment = 1; segment <= arcSegments; ++segment) {
                    const float fraction = static_cast<float>(segment)
                            / static_cast<float>(arcSegments);
                    const float angle = object.doorSwingArcStartRadians
                            + object.doorSwingArcSweepRadians * fraction;
                    const Vector2 currentMap{
                            object.doorHinge.x
                                    + std::cos(angle) * object.doorSwingArcRadius,
                            object.doorHinge.y
                                    + std::sin(angle) * object.doorSwingArcRadius};
                    DrawLineEx(
                            CachedMapToScreen(context, previousMap),
                            CachedMapToScreen(context, currentMap),
                            selected ? 2.0f : 1.5f,
                            guideColor);
                    previousMap = currentMap;
                }
            }

            const Vector2 c0 = CachedMapToScreen(context, object.doorCorners[0]);
            const Vector2 c1 = CachedMapToScreen(context, object.doorCorners[1]);
            const Vector2 c2 = CachedMapToScreen(context, object.doorCorners[2]);
            const Vector2 c3 = CachedMapToScreen(context, object.doorCorners[3]);
            const Color activeDoorLine = selected ? selectedFill : doorLine;
            DrawTriangle(c0, c1, c2, selected ? WithAlpha(selectedFill, 76) : doorFill);
            DrawTriangle(c0, c2, c3, selected ? WithAlpha(selectedFill, 76) : doorFill);
            DrawLineEx(c0, c1, selected ? 3.5f : 2.5f, outline);
            DrawLineEx(c1, c2, selected ? 3.5f : 2.5f, outline);
            DrawLineEx(c2, c3, selected ? 3.5f : 2.5f, outline);
            DrawLineEx(c3, c0, selected ? 3.5f : 2.5f, outline);
            DrawLineEx(c0, c1, selected ? 2.0f : 1.5f, activeDoorLine);
            DrawLineEx(c1, c2, selected ? 2.0f : 1.5f, activeDoorLine);
            DrawLineEx(c2, c3, selected ? 2.0f : 1.5f, activeDoorLine);
            DrawLineEx(c3, c0, selected ? 2.0f : 1.5f, activeDoorLine);
            DrawLineEx(
                    CachedMapToScreen(context, object.doorEndpointA),
                    CachedMapToScreen(context, object.doorEndpointB),
                    selected ? 3.0f : 2.0f,
                    activeDoorLine);
            if (object.doorSwingGuideValid) {
                const Vector2 hinge = CachedMapToScreen(context, object.doorHinge);
                DrawCircleV(hinge, selected ? 6.0f : 5.0f, outline);
                DrawCircleV(hinge, selected ? 3.5f : 3.0f, guideColor);
            }
        }

        if (object.isNpc) {
            const Vector2 forward{
                    std::cos(object.yawRadians),
                    std::sin(object.yawRadians)};
            const Vector2 side{-forward.y, forward.x};
            const Vector2 head{
                    center.x + forward.x * 13.0f,
                    center.y + forward.y * 13.0f};
            const Vector2 neck{
                    center.x + forward.x * 7.5f,
                    center.y + forward.y * 7.5f};
            const Vector2 shoulders{
                    center.x + forward.x * 4.0f,
                    center.y + forward.y * 4.0f};
            const Vector2 hip{
                    center.x - forward.x * 5.0f,
                    center.y - forward.y * 5.0f};
            const Vector2 handA{
                    shoulders.x + side.x * 8.0f,
                    shoulders.y + side.y * 8.0f};
            const Vector2 handB{
                    shoulders.x - side.x * 8.0f,
                    shoulders.y - side.y * 8.0f};
            const Vector2 footA{
                    hip.x - forward.x * 8.0f + side.x * 6.0f,
                    hip.y - forward.y * 8.0f + side.y * 6.0f};
            const Vector2 footB{
                    hip.x - forward.x * 8.0f - side.x * 6.0f,
                    hip.y - forward.y * 8.0f - side.y * 6.0f};
            const float bodyWidth = selected ? 3.5f : 2.5f;
            const auto drawLimb = [&](Vector2 start, Vector2 end) {
                DrawLineEx(start, end, bodyWidth + 3.0f, outline);
                DrawLineEx(start, end, bodyWidth, fill);
            };
            drawLimb(neck, hip);
            drawLimb(handA, handB);
            drawLimb(hip, footA);
            drawLimb(hip, footB);
            DrawCircleV(head, selected ? 6.0f : 5.5f, outline);
            DrawCircleV(head, selected ? 3.5f : 3.0f, fill);
        } else {
            DrawCircleV(center, radius + 3.0f, outline);
            DrawCircleV(center, radius, fill);
        }

        if (!object.isDoor && !object.isNpc) {
            const Vector2 direction{
                    std::cos(object.yawRadians),
                    std::sin(object.yawRadians)
            };
            const Vector2 tip{
                    center.x + direction.x * 18.0f,
                    center.y + direction.y * 18.0f
            };
            DrawLineEx(center, tip, selected ? 3.0f : 2.0f, fill);
            DrawCircleV(tip, selected ? 3.5f : 3.0f, fill);
        }

        if (!object.definitionKnown) {
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
    }
}

void DrawCachedLevelMarkers(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        const LevelMarkerDragState* drag)
{
    constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
    const Color outline{28, 18, 40, 255};
    const Color normal{196, 104, 244, 255};
    const Color selectedColor{92, 224, 244, 255};
    const Color hoveredColor{236, 168, 255, 255};
    for (const CachedAuthoringLevelMarkerDraw& marker : cache.levelMarkers) {
        Vector2 map = marker.map;
        if (drag != nullptr && drag->active && drag->markerId == marker.markerId) {
            map = Vector2{
                    SectorCoordToVisibleAuthoring(drag->previewX),
                    SectorCoordToVisibleAuthoring(drag->previewZ)};
        }
        const bool selected = context.selectedAuthoring.kind == SectorAuthoringSelectionKind::LevelMarker
                && context.selectedAuthoring.levelMarkerId == marker.markerId;
        const bool hovered = context.hoveredAuthoring.kind == SectorAuthoringSelectionKind::LevelMarker
                && context.hoveredAuthoring.levelMarkerId == marker.markerId;
        const Color color = selected ? selectedColor : hovered ? hoveredColor : normal;
        const Vector2 center = CachedMapToScreen(context, map);
        const float radius = selected ? 9.0f : 7.0f;
        const Vector2 top{center.x, center.y - radius};
        const Vector2 right{center.x + radius, center.y};
        const Vector2 bottom{center.x, center.y + radius};
        const Vector2 left{center.x - radius, center.y};
        DrawTriangle(top, right, bottom, color);
        DrawTriangle(top, bottom, left, color);
        DrawLineEx(top, right, 2.0f, outline);
        DrawLineEx(right, bottom, 2.0f, outline);
        DrawLineEx(bottom, left, 2.0f, outline);
        DrawLineEx(left, top, 2.0f, outline);
        DrawCircleV(center, selected ? 2.8f : 2.2f, outline);

        const float radians = marker.orientationDegrees * DegreesToRadians;
        const Vector2 direction{std::cos(radians), std::sin(radians)};
        const Vector2 tip{center.x + direction.x * 22.0f, center.y + direction.y * 22.0f};
        DrawLineEx(center, tip, selected ? 3.0f : 2.0f, color);
        DrawCircleV(tip, selected ? 3.5f : 3.0f, color);
        DrawText(marker.referenceId.c_str(), static_cast<int>(center.x + 11.0f),
                static_cast<int>(center.y - 16.0f), 12, color);
    }
}

void DrawCachedTriggers(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        const TriggerDragState* drag)
{
    const Color normal{255, 72, 184, 220};
    const Color disabled{154, 70, 126, 190};
    const Color selected{255, 154, 220, 255};
    const Color hovered{255, 108, 198, 255};
    for (const CachedAuthoringTriggerDraw& trigger : cache.triggers) {
        const bool previewing = drag != nullptr && drag->active
                && drag->triggerId == trigger.triggerId;
        const bool isSelected = context.selectedAuthoring.kind == SectorAuthoringSelectionKind::Trigger
                && context.selectedAuthoring.triggerId == trigger.triggerId;
        const bool isHovered = context.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Trigger
                && context.hoveredAuthoring.triggerId == trigger.triggerId;
        const Color line = isSelected ? selected : isHovered ? hovered : trigger.enabled ? normal : disabled;
        if (!previewing) {
            for (size_t i = 0; i + 2 < trigger.fillTrianglePoints.size(); i += 3) {
                DrawTriangle(CachedMapToScreen(context, trigger.fillTrianglePoints[i]),
                        CachedMapToScreen(context, trigger.fillTrianglePoints[i + 1]),
                        CachedMapToScreen(context, trigger.fillTrianglePoints[i + 2]),
                        Color{line.r, line.g, line.b, static_cast<unsigned char>(isSelected ? 58 : 34)});
            }
        }
        const size_t pointCount = previewing ? drag->previewPoints.size() : trigger.points.size();
        if (pointCount < 2) continue;
        const auto mapPoint = [&](size_t index) {
            if (!previewing) return trigger.points[index];
            const SectorTriggerPoint point = drag->previewPoints[index];
            return Vector2{SectorCoordToVisibleAuthoring(point.x),
                    SectorCoordToVisibleAuthoring(point.z)};
        };
        for (size_t i = 0; i < pointCount; ++i) {
            DrawLineEx(CachedMapToScreen(context, mapPoint(i)),
                    CachedMapToScreen(context, mapPoint((i + 1) % pointCount)),
                    isSelected ? 3.5f : 2.5f, line);
        }
        Vector2 center{};
        for (size_t i = 0; i < pointCount; ++i) {
            const Vector2 point = mapPoint(i);
            center.x += point.x;
            center.y += point.y;
        }
        center.x /= static_cast<float>(pointCount);
        center.y /= static_cast<float>(pointCount);
        const Vector2 screen = CachedMapToScreen(context, center);
        DrawText(trigger.id.c_str(), static_cast<int>(screen.x + 7.0f),
                static_cast<int>(screen.y - 15.0f), 12, line);
    }
}

} // namespace game
