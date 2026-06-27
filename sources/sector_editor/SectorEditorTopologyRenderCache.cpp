#include "sector_editor/SectorEditorTopologyRenderCache.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "util/earcut.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
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

} // namespace

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
        uint64_t revision)
{
    SectorEditorTopologyRenderCache cache;
    cache.revision = revision;

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

    cache.valid = true;
    return cache;
}

void DrawCachedTopologySectors(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context)
{
    const Color fill = Color{82, 112, 154, 42};
    const Color outline = Color{116, 139, 174, 235};
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
        const bool selected = context.selectionKind == TopologySelectionKind::Light
                && light.lightId == context.selectedLightId;
        const bool hovered = light.lightId == context.hoveredLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;
        const float radiusPixels = light.radiusPixelsAtZoomOne * context.viewZoom;

        if (selected || hovered || context.currentTool == SectorEditorTool::Light) {
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

} // namespace game
