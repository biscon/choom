#include "sector_demo/SectorGeneratedGeometry.h"

#include "sector_demo/SectorMap.h"
#include "sector_demo/SectorUnits.h"
#include "util/earcut.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>

namespace game {

namespace {

struct PointKey {
    int x = 0;
    int y = 0;
};

struct EdgeRef {
    SectorBoundaryEdgeRef boundary;
};

constexpr float TextureWorldSize = 2.0f;
constexpr float EdgeEpsilon = 0.001f;

using EarcutPoint = std::array<double, 2>;
using EarcutRing = std::vector<EarcutPoint>;
using EarcutPolygon = std::vector<EarcutRing>;

PointKey MakePointKey(SectorPoint point)
{
    return PointKey{
            static_cast<int>(std::lround(point.x * 1000.0f)),
            static_cast<int>(std::lround(point.y * 1000.0f))
    };
}

std::string MakePointKeyString(PointKey point)
{
    return std::to_string(point.x) + "," + std::to_string(point.y);
}

std::string MakeEdgeKey(SectorPoint a, SectorPoint b)
{
    return MakePointKeyString(MakePointKey(a)) + ">" + MakePointKeyString(MakePointKey(b));
}

Vector3 ToWorld(SectorPoint point, float height)
{
    return SectorAuthoringToWorldPosition(point.x, height, point.y);
}

float EdgeLength(SectorPoint a, SectorPoint b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return SectorAuthoringToWorldDistance(std::sqrt(dx * dx + dy * dy));
}

Vector3 WallNormal(SectorPoint a, SectorPoint b)
{
    const float dx = b.x - a.x;
    const float dz = b.y - a.y;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length <= EdgeEpsilon) {
        return Vector3{0.0f, 0.0f, -1.0f};
    }

    return Vector3{-dz / length, 0.0f, dx / length};
}

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= EdgeEpsilon && std::fabs(a.y - b.y) <= EdgeEpsilon;
}

bool IsCollinear(SectorPoint a, SectorPoint b, SectorPoint c)
{
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float acx = c.x - a.x;
    const float acy = c.y - a.y;
    return std::fabs(abx * acy - aby * acx) <= EdgeEpsilon;
}

bool RangeOverlaps(float a0, float a1, float b0, float b1)
{
    const float minA = std::fmin(a0, a1);
    const float maxA = std::fmax(a0, a1);
    const float minB = std::fmin(b0, b1);
    const float maxB = std::fmax(b0, b1);
    return std::fmax(minA, minB) < std::fmin(maxA, maxB) - EdgeEpsilon;
}

bool EdgesPartiallyOverlap(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    if (!IsCollinear(a0, a1, b0) || !IsCollinear(a0, a1, b1)) {
        return false;
    }

    if (SamePoint(a0, b1) && SamePoint(a1, b0)) {
        return false;
    }

    if (SamePoint(a0, b0) && SamePoint(a1, b1)) {
        return false;
    }

    if (std::fabs(a0.x - a1.x) >= std::fabs(a0.y - a1.y)) {
        return RangeOverlaps(a0.x, a1.x, b0.x, b1.x);
    }

    return RangeOverlaps(a0.y, a1.y, b0.y, b1.y);
}

float TriangleArea2(SectorPoint a, SectorPoint b, SectorPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

EarcutPolygon BuildEarcutPolygon(const SectorDefinition& sector)
{
    EarcutPolygon polygon;
    const auto appendRing = [&polygon](const std::vector<SectorPoint>& ring) {
        polygon.emplace_back();
        polygon.back().reserve(ring.size());
        for (const SectorPoint point : ring) {
            polygon.back().push_back(EarcutPoint{
                    static_cast<double>(point.x),
                    static_cast<double>(point.y)
            });
        }
    };
    appendRing(sector.points);
    for (const std::vector<SectorPoint>& hole : sector.holes) {
        appendRing(hole);
    }

    return polygon;
}

std::vector<SectorPoint> FlattenSectorPoints(const SectorDefinition& sector)
{
    size_t count = sector.points.size();
    for (const std::vector<SectorPoint>& hole : sector.holes) {
        count += hole.size();
    }
    std::vector<SectorPoint> points;
    points.reserve(count);
    points.insert(points.end(), sector.points.begin(), sector.points.end());
    for (const std::vector<SectorPoint>& hole : sector.holes) {
        points.insert(points.end(), hole.begin(), hole.end());
    }
    return points;
}

Vector2 ApplyUvSettings(Vector2 baseUv, Vector2 uvScale, Vector2 uvOffset)
{
    return Vector2{
            baseUv.x * uvScale.x + uvOffset.x,
            baseUv.y * uvScale.y + uvOffset.y
    };
}

unsigned char ClampColorByte(float value)
{
    return static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
}

Color MakeSectorVertexColor(const SectorDefinition& sector)
{
    const float intensity = std::clamp(sector.ambientIntensity, 0.0f, 1.0f);
    return Color{
            ClampColorByte(static_cast<float>(sector.ambientColor.r) * intensity),
            ClampColorByte(static_cast<float>(sector.ambientColor.g) * intensity),
            ClampColorByte(static_cast<float>(sector.ambientColor.b) * intensity),
            255
    };
}

void AddFlatSectorSurfaces(
        SectorGeneratedGeometry& geometry,
        const SectorDefinition& sector,
        int sectorIndex,
        SectorGeneratedSurfaceKind kind,
        float height,
        Vector3 normal,
        bool facePositiveY,
        const std::string& textureId,
        Vector2 uvScale,
        Vector2 uvOffset)
{
    const Color color = MakeSectorVertexColor(sector);
    const EarcutPolygon polygon = BuildEarcutPolygon(sector);
    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    if (indices.size() % 3 != 0) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Earcut returned malformed indices for sector '%s'\n",
                sector.id.c_str()
        );
        return;
    }

    const std::vector<SectorPoint> flattenedPoints = FlattenSectorPoints(sector);
    const size_t pointCount = flattenedPoints.size();
    for (size_t i = 0; i < indices.size(); i += 3) {
        const uint32_t ia = indices[i + 0];
        const uint32_t ib = indices[i + 1];
        const uint32_t ic = indices[i + 2];
        if (ia >= pointCount || ib >= pointCount || ic >= pointCount) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Earcut returned out-of-range index for sector '%s'\n",
                    sector.id.c_str()
            );
            return;
        }

        const SectorPoint pa = flattenedPoints[ia];
        SectorPoint pb = flattenedPoints[ib];
        SectorPoint pc = flattenedPoints[ic];
        const float area2 = TriangleArea2(pa, pb, pc);
        if (std::fabs(area2) <= EdgeEpsilon) {
            continue;
        }

        if ((facePositiveY && area2 > 0.0f) || (!facePositiveY && area2 < 0.0f)) {
            const SectorPoint swap = pb;
            pb = pc;
            pc = swap;
        }

        const Vector2 worldPa = SectorAuthoringToWorldPosition(Vector2{pa.x, pa.y});
        const Vector2 worldPb = SectorAuthoringToWorldPosition(Vector2{pb.x, pb.y});
        const Vector2 worldPc = SectorAuthoringToWorldPosition(Vector2{pc.x, pc.y});
        const float minX = std::fmin(worldPa.x, std::fmin(worldPb.x, worldPc.x));
        const float minY = std::fmin(worldPa.y, std::fmin(worldPb.y, worldPc.y));
        const float maxX = std::fmax(worldPa.x, std::fmax(worldPb.x, worldPc.x));
        const float maxY = std::fmax(worldPa.y, std::fmax(worldPb.y, worldPc.y));

        SectorGeneratedSurface surface;
        surface.ref = SectorGeneratedSurfaceRef{kind, sectorIndex, SectorBoundaryRingKind::Outer, -1, -1};
        surface.textureId = textureId;
        surface.normal = normal;
        surface.chartWidth = std::max(maxX - minX, EdgeEpsilon);
        surface.chartHeight = std::max(maxY - minY, EdgeEpsilon);
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pa, height),
                normal,
                ApplyUvSettings(Vector2{worldPa.x / TextureWorldSize, worldPa.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{worldPa.x - minX, worldPa.y - minY},
                color
        });
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pb, height),
                normal,
                ApplyUvSettings(Vector2{worldPb.x / TextureWorldSize, worldPb.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{worldPb.x - minX, worldPb.y - minY},
                color
        });
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pc, height),
                normal,
                ApplyUvSettings(Vector2{worldPc.x / TextureWorldSize, worldPc.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{worldPc.x - minX, worldPc.y - minY},
                color
        });
        geometry.surfaces.push_back(std::move(surface));
    }
}

void AddWallSurface(
        SectorGeneratedGeometry& geometry,
        SectorGeneratedSurfaceKind kind,
        int sectorIndex,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex,
        const std::string& textureId,
        SectorPoint a,
        SectorPoint b,
        float bottom,
        float top,
        Vector2 uvScale,
        Vector2 uvOffset,
        Color color)
{
    if (top <= bottom + EdgeEpsilon) {
        return;
    }

    const Vector3 normal = WallNormal(a, b);
    const float length = EdgeLength(a, b);
    const float height = SectorAuthoringToWorldDistance(top - bottom);
    const float u1 = length / TextureWorldSize;
    const float v0 = height / TextureWorldSize;
    const float v1 = 0.0f;

    const SectorGeneratedVertex af{ToWorld(a, bottom), normal, ApplyUvSettings(Vector2{0.0f, v0}, uvScale, uvOffset), Vector2{0.0f, height}, color};
    const SectorGeneratedVertex ac{ToWorld(a, top), normal, ApplyUvSettings(Vector2{0.0f, v1}, uvScale, uvOffset), Vector2{0.0f, 0.0f}, color};
    const SectorGeneratedVertex bc{ToWorld(b, top), normal, ApplyUvSettings(Vector2{u1, v1}, uvScale, uvOffset), Vector2{length, 0.0f}, color};
    const SectorGeneratedVertex bf{ToWorld(b, bottom), normal, ApplyUvSettings(Vector2{u1, v0}, uvScale, uvOffset), Vector2{length, height}, color};

    SectorGeneratedSurface surface;
    surface.ref = SectorGeneratedSurfaceRef{kind, sectorIndex, ringKind, holeIndex, edgeIndex};
    surface.textureId = textureId;
    surface.normal = normal;
    surface.chartWidth = std::max(length, EdgeEpsilon);
    surface.chartHeight = std::max(height, EdgeEpsilon);
    surface.vertices.reserve(6);
    surface.vertices.push_back(af);
    surface.vertices.push_back(bc);
    surface.vertices.push_back(ac);
    surface.vertices.push_back(af);
    surface.vertices.push_back(bf);
    surface.vertices.push_back(bc);
    geometry.surfaces.push_back(std::move(surface));
}

void WarnAboutPartialEdges(const SectorMap& map)
{
    for (size_t sectorA = 0; sectorA < map.sectors.size(); ++sectorA) {
        const SectorDefinition& a = map.sectors[sectorA];
        const size_t ringCountA = 1 + a.holes.size();
        for (size_t ringA = 0; ringA < ringCountA; ++ringA) {
            const std::vector<SectorPoint>& pointsA = ringA == 0 ? a.points : a.holes[ringA - 1];
            for (size_t edgeA = 0; edgeA < pointsA.size(); ++edgeA) {
                const SectorPoint a0 = pointsA[edgeA];
                const SectorPoint a1 = pointsA[(edgeA + 1) % pointsA.size()];

                for (size_t sectorB = sectorA + 1; sectorB < map.sectors.size(); ++sectorB) {
                    const SectorDefinition& b = map.sectors[sectorB];
                    const size_t ringCountB = 1 + b.holes.size();
                    for (size_t ringB = 0; ringB < ringCountB; ++ringB) {
                        const std::vector<SectorPoint>& pointsB = ringB == 0 ? b.points : b.holes[ringB - 1];
                        for (size_t edgeB = 0; edgeB < pointsB.size(); ++edgeB) {
                            const SectorPoint b0 = pointsB[edgeB];
                            const SectorPoint b1 = pointsB[(edgeB + 1) % pointsB.size()];
                            if (EdgesPartiallyOverlap(a0, a1, b0, b1)) {
                                std::fprintf(
                                        stderr,
                                        "[SectorDemo WARNING] Sectors '%s' and '%s' have partially matching edges; MVP requires exact reverse endpoints\n",
                                        a.id.c_str(),
                                        b.id.c_str()
                                );
                            }
                        }
                    }
                }
            }
        }
    }
}

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

Color MakeTopologySectorVertexColor(const SectorTopologySector& sector)
{
    const float intensity = std::clamp(sector.ambientIntensity, 0.0f, 1.0f);
    return Color{
            ClampColorByte(static_cast<float>(sector.ambientColor.r) * intensity),
            ClampColorByte(static_cast<float>(sector.ambientColor.g) * intensity),
            ClampColorByte(static_cast<float>(sector.ambientColor.b) * intensity),
            255
    };
}

bool SetTopologyError(
        SectorGeneratedGeometry& outGeometry,
        std::string* outError,
        const std::string& message)
{
    outGeometry = {};
    if (outError != nullptr) {
        *outError = message;
    }
    return false;
}

bool ValidateTopologyGeometryValues(
        const SectorTopologyMap& map,
        SectorGeneratedGeometry& outGeometry,
        std::string* outError)
{
    for (const SectorTopologySector& sector : map.sectors) {
        if (!std::isfinite(sector.floorZ) || !std::isfinite(sector.ceilingZ)
            || !std::isfinite(sector.ambientIntensity)
            || !IsFinite(sector.floorUv.scale) || !IsFinite(sector.floorUv.offset)
            || !IsFinite(sector.ceilingUv.scale) || !IsFinite(sector.ceilingUv.offset)) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Topology sector " + std::to_string(sector.id)
                            + " contains non-finite generated-geometry values");
        }
        if (sector.ceilingZ <= sector.floorZ) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Topology sector " + std::to_string(sector.id)
                            + " must have ceilingZ greater than floorZ");
        }
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        const std::array<const SectorTopologyWallPartSettings*, 3> parts{
                &sideDef.wall, &sideDef.lower, &sideDef.upper};
        for (const SectorTopologyWallPartSettings* part : parts) {
            if (!IsFinite(part->uv.scale) || !IsFinite(part->uv.offset)) {
                return SetTopologyError(
                        outGeometry,
                        outError,
                        "Topology sidedef " + std::to_string(sideDef.id)
                                + " contains non-finite UV values");
            }
        }
    }
    return true;
}

bool BuildTopologyFlatSurface(
        const SectorTopologyMap& map,
        const SectorTopologySector& sector,
        const SectorTopologyLoopSet& loops,
        SectorGeneratedSurfaceKind kind,
        float height,
        Vector3 normal,
        bool facePositiveY,
        const std::string& textureId,
        const SectorTopologyUvSettings& uvSettings,
        SectorGeneratedSurface& outSurface,
        std::string& outError)
{
    EarcutPolygon polygon;
    std::vector<const SectorTopologyVertex*> flattenedVertices;
    const auto appendLoop = [&](const SectorTopologyLoop& loop) -> bool {
        EarcutRing ring;
        ring.reserve(loop.vertexIds.size());
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
            if (vertex == nullptr) {
                outError = "Topology sector " + std::to_string(sector.id)
                        + " loop references missing vertex " + std::to_string(vertexId);
                return false;
            }
            ring.push_back(EarcutPoint{
                    static_cast<double>(vertex->x),
                    static_cast<double>(vertex->y)});
            flattenedVertices.push_back(vertex);
        }
        polygon.push_back(std::move(ring));
        return true;
    };

    size_t pointCount = loops.outer.vertexIds.size();
    for (const SectorTopologyLoop& hole : loops.holes) {
        pointCount += hole.vertexIds.size();
    }
    polygon.reserve(1 + loops.holes.size());
    flattenedVertices.reserve(pointCount);
    if (!appendLoop(loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (!appendLoop(hole)) {
            return false;
        }
    }

    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    if (indices.empty() || indices.size() % 3 != 0) {
        outError = "Earcut failed for topology sector " + std::to_string(sector.id);
        return false;
    }

    float minX = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const SectorTopologyVertex* vertex : flattenedVertices) {
        const Vector2 world = SectorCoordToWorldPosition2(vertex->x, vertex->y);
        minX = std::min(minX, world.x);
        minZ = std::min(minZ, world.y);
        maxX = std::max(maxX, world.x);
        maxZ = std::max(maxZ, world.y);
    }

    SectorGeneratedSurface surface;
    surface.ref.kind = kind;
    surface.ref.topologySectorId = sector.id;
    surface.textureId = textureId;
    surface.normal = normal;
    surface.chartWidth = std::max(maxX - minX, EdgeEpsilon);
    surface.chartHeight = std::max(maxZ - minZ, EdgeEpsilon);
    surface.vertices.reserve(indices.size());
    const Color color = MakeTopologySectorVertexColor(sector);

    for (size_t index = 0; index < indices.size(); index += 3) {
        uint32_t ia = indices[index];
        uint32_t ib = indices[index + 1];
        uint32_t ic = indices[index + 2];
        if (ia >= flattenedVertices.size() || ib >= flattenedVertices.size()
            || ic >= flattenedVertices.size()) {
            outError = "Earcut returned an out-of-range index for topology sector "
                    + std::to_string(sector.id);
            return false;
        }

        const SectorTopologyVertex* a = flattenedVertices[ia];
        const SectorTopologyVertex* b = flattenedVertices[ib];
        const SectorTopologyVertex* c = flattenedVertices[ic];
        const double areaTwice =
                (static_cast<double>(b->x) - static_cast<double>(a->x))
                        * (static_cast<double>(c->y) - static_cast<double>(a->y))
                - (static_cast<double>(b->y) - static_cast<double>(a->y))
                        * (static_cast<double>(c->x) - static_cast<double>(a->x));
        if (areaTwice == 0.0) {
            continue;
        }
        if ((facePositiveY && areaTwice > 0.0)
            || (!facePositiveY && areaTwice < 0.0)) {
            std::swap(b, c);
        }

        const auto appendVertex = [&](const SectorTopologyVertex& vertex) {
            const Vector2 world = SectorCoordToWorldPosition2(vertex.x, vertex.y);
            surface.vertices.push_back(SectorGeneratedVertex{
                    SectorCoordToWorldPosition3(vertex.x, height, vertex.y),
                    normal,
                    ApplyUvSettings(
                            Vector2{world.x / TextureWorldSize, world.y / TextureWorldSize},
                            uvSettings.scale,
                            uvSettings.offset),
                    Vector2{world.x - minX, world.y - minZ},
                    color});
        };
        appendVertex(*a);
        appendVertex(*b);
        appendVertex(*c);
    }

    if (surface.vertices.empty()) {
        outError = "Topology sector " + std::to_string(sector.id)
                + " produced no flat triangles";
        return false;
    }
    outSurface = std::move(surface);
    return true;
}

bool BuildTopologyWallSurface(
        const SectorTopologyVertex& a,
        const SectorTopologyVertex& b,
        const SectorTopologySector& sector,
        const SectorTopologyLineDef& lineDef,
        const SectorTopologySideDef& sideDef,
        SectorGeneratedSurfaceKind kind,
        float bottom,
        float top,
        const SectorTopologyWallPartSettings& settings,
        SectorGeneratedSurface& outSurface,
        std::string& outError)
{
    const double dx = static_cast<double>(b.x) - static_cast<double>(a.x);
    const double dy = static_cast<double>(b.y) - static_cast<double>(a.y);
    const double coordLength = std::sqrt(dx * dx + dy * dy);
    if (!(coordLength > 0.0) || !std::isfinite(coordLength)) {
        outError = "Topology linedef " + std::to_string(lineDef.id)
                + " has zero or invalid length";
        return false;
    }
    if (!(top > bottom) || !std::isfinite(bottom) || !std::isfinite(top)) {
        outError = "Topology sidedef " + std::to_string(sideDef.id)
                + " has an invalid wall span";
        return false;
    }

    const float length = SectorCoordDistanceToWorldDistance(coordLength);
    const float height = SectorAuthoringToWorldDistance(top - bottom);
    const Vector3 normal{
            static_cast<float>(-dy / coordLength),
            0.0f,
            static_cast<float>(dx / coordLength)};
    const float u1 = length / TextureWorldSize;
    const float v0 = height / TextureWorldSize;
    const Color color = MakeTopologySectorVertexColor(sector);
    const Vector3 afPosition = SectorCoordToWorldPosition3(a.x, bottom, a.y);
    const Vector3 acPosition = SectorCoordToWorldPosition3(a.x, top, a.y);
    const Vector3 bcPosition = SectorCoordToWorldPosition3(b.x, top, b.y);
    const Vector3 bfPosition = SectorCoordToWorldPosition3(b.x, bottom, b.y);

    SectorGeneratedSurface surface;
    surface.ref.kind = kind;
    surface.ref.topologySectorId = sector.id;
    surface.ref.topologyLineDefId = lineDef.id;
    surface.ref.topologySideDefId = sideDef.id;
    surface.ref.topologySide = sideDef.side;
    surface.textureId = settings.textureId;
    surface.normal = normal;
    surface.chartWidth = length;
    surface.chartHeight = height;
    surface.vertices.reserve(6);
    const SectorGeneratedVertex af{afPosition, normal,
            ApplyUvSettings(Vector2{0.0f, v0}, settings.uv.scale, settings.uv.offset),
            Vector2{0.0f, height}, color};
    const SectorGeneratedVertex ac{acPosition, normal,
            ApplyUvSettings(Vector2{0.0f, 0.0f}, settings.uv.scale, settings.uv.offset),
            Vector2{0.0f, 0.0f}, color};
    const SectorGeneratedVertex bc{bcPosition, normal,
            ApplyUvSettings(Vector2{u1, 0.0f}, settings.uv.scale, settings.uv.offset),
            Vector2{length, 0.0f}, color};
    const SectorGeneratedVertex bf{bfPosition, normal,
            ApplyUvSettings(Vector2{u1, v0}, settings.uv.scale, settings.uv.offset),
            Vector2{length, height}, color};
    surface.vertices.push_back(af);
    surface.vertices.push_back(bc);
    surface.vertices.push_back(ac);
    surface.vertices.push_back(af);
    surface.vertices.push_back(bf);
    surface.vertices.push_back(bc);
    outSurface = std::move(surface);
    return true;
}

} // namespace

const char* SectorGeneratedSurfaceKindName(SectorGeneratedSurfaceKind kind)
{
    switch (kind) {
        case SectorGeneratedSurfaceKind::Floor: return "Floor";
        case SectorGeneratedSurfaceKind::Ceiling: return "Ceiling";
        case SectorGeneratedSurfaceKind::Wall: return "Wall";
        case SectorGeneratedSurfaceKind::LowerWall: return "Lower";
        case SectorGeneratedSurfaceKind::UpperWall: return "Upper";
    }
    return "Unknown";
}

SectorGeneratedSurfaceHit PickSectorGeneratedGeometry(
        const SectorGeneratedGeometry& geometry,
        Ray ray,
        float minDistance)
{
    SectorGeneratedSurfaceHit best;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
            RayCollision collision = GetRayCollisionTriangle(
                    ray,
                    surface.vertices[i + 0].position,
                    surface.vertices[i + 1].position,
                    surface.vertices[i + 2].position);
            if (!collision.hit) {
                collision = GetRayCollisionTriangle(
                        ray,
                        surface.vertices[i + 2].position,
                        surface.vertices[i + 1].position,
                        surface.vertices[i + 0].position);
            }
            if (!collision.hit || collision.distance <= minDistance) {
                continue;
            }
            if (!best.hit || collision.distance < best.distance) {
                best.hit = true;
                best.ref = surface.ref;
                best.worldPosition = collision.point;
                best.distance = collision.distance;
            }
        }
    }
    return best;
}

std::string FormatSectorGeneratedSurfaceLabel(const SectorGeneratedSurfaceRef& ref)
{
    std::ostringstream label;
    label << SectorGeneratedSurfaceKindName(ref.kind);
    if (ref.topologySectorId >= 0) {
        label << " sector=" << ref.topologySectorId;
        if (ref.topologyLineDefId >= 0) {
            label << " line=" << ref.topologyLineDefId;
        }
        if (ref.topologySideDefId >= 0) {
            label << " sideDef=" << ref.topologySideDefId
                  << " " << (ref.topologySide == SectorTopologySideKind::Front ? "front" : "back");
        }
        return label.str();
    }

    label << " sectorIndex=" << ref.sectorIndex;
    if (ref.kind == SectorGeneratedSurfaceKind::Wall
        || ref.kind == SectorGeneratedSurfaceKind::LowerWall
        || ref.kind == SectorGeneratedSurfaceKind::UpperWall) {
        label << " ring=" << (ref.ringKind == SectorBoundaryRingKind::Outer ? "outer" : "hole")
              << " holeIndex=" << ref.holeIndex
              << " edgeIndex=" << ref.edgeIndex;
    }
    return label.str();
}

bool BuildSectorGeneratedGeometry(const SectorMap& map, SectorGeneratedGeometry& outGeometry)
{
    outGeometry = SectorGeneratedGeometry{};
    std::unordered_map<std::string, EdgeRef> edgeRefs;

    WarnAboutPartialEdges(map);

    for (size_t sectorIndex = 0; sectorIndex < map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = map.sectors[sectorIndex];
        const size_t ringCount = 1 + sector.holes.size();
        for (size_t ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            const std::vector<SectorPoint>& ring = ringIndex == 0 ? sector.points : sector.holes[ringIndex - 1];
            for (size_t edgeIndex = 0; edgeIndex < ring.size(); ++edgeIndex) {
                const SectorPoint a = ring[edgeIndex];
                const SectorPoint b = ring[(edgeIndex + 1) % ring.size()];
                const std::string edgeKey = MakeEdgeKey(a, b);
                if (edgeRefs.find(edgeKey) != edgeRefs.end()) {
                    std::fprintf(
                            stderr,
                            "[SectorDemo WARNING] Duplicate same-direction edge found in sector '%s'\n",
                            sector.id.c_str()
                    );
                }
                edgeRefs[edgeKey] = EdgeRef{SectorBoundaryEdgeRef{
                        static_cast<int>(sectorIndex),
                        ringIndex == 0 ? SectorBoundaryRingKind::Outer : SectorBoundaryRingKind::Hole,
                        ringIndex == 0 ? -1 : static_cast<int>(ringIndex - 1),
                        static_cast<int>(edgeIndex)
                }};
            }
        }
    }

    for (size_t sectorIndex = 0; sectorIndex < map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = map.sectors[sectorIndex];
        const int sectorInt = static_cast<int>(sectorIndex);
        const Color sectorColor = MakeSectorVertexColor(sector);
        AddFlatSectorSurfaces(
                outGeometry,
                sector,
                sectorInt,
                SectorGeneratedSurfaceKind::Floor,
                sector.floorZ,
                Vector3{0.0f, 1.0f, 0.0f},
                true,
                sector.floorTextureId,
                sector.floorUv.hasUvScale ? sector.floorUv.uvScale : Vector2{1.0f, 1.0f},
                sector.floorUv.hasUvOffset ? sector.floorUv.uvOffset : Vector2{0.0f, 0.0f}
        );
        AddFlatSectorSurfaces(
                outGeometry,
                sector,
                sectorInt,
                SectorGeneratedSurfaceKind::Ceiling,
                sector.ceilingZ,
                Vector3{0.0f, -1.0f, 0.0f},
                false,
                sector.ceilingTextureId,
                sector.ceilingUv.hasUvScale ? sector.ceilingUv.uvScale : Vector2{1.0f, 1.0f},
                sector.ceilingUv.hasUvOffset ? sector.ceilingUv.uvOffset : Vector2{0.0f, 0.0f}
        );

        const size_t ringCount = 1 + sector.holes.size();
        for (size_t ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            const std::vector<SectorPoint>& ring = ringIndex == 0 ? sector.points : sector.holes[ringIndex - 1];
            const SectorBoundaryRingKind ringKind = ringIndex == 0
                    ? SectorBoundaryRingKind::Outer : SectorBoundaryRingKind::Hole;
            const int holeIndex = ringIndex == 0 ? -1 : static_cast<int>(ringIndex - 1);
            for (size_t edgeIndex = 0; edgeIndex < ring.size(); ++edgeIndex) {
                const SectorPoint a = ring[edgeIndex];
                const SectorPoint b = ring[(edgeIndex + 1) % ring.size()];
                const int edgeInt = static_cast<int>(edgeIndex);
                const EffectiveEdgeSettings edgeSettings = GetEffectiveEdgeSettings(
                        map,
                        SectorBoundaryEdgeRef{sectorInt, ringKind, holeIndex, edgeInt});
                const auto reverse = edgeRefs.find(MakeEdgeKey(b, a));
                if (reverse == edgeRefs.end() || reverse->second.boundary.sectorIndex == sectorInt) {
                    AddWallSurface(
                            outGeometry,
                            SectorGeneratedSurfaceKind::Wall,
                            sectorInt,
                            ringKind,
                            holeIndex,
                            edgeInt,
                            edgeSettings.wall.textureId,
                            a,
                            b,
                            sector.floorZ,
                            sector.ceilingZ,
                            edgeSettings.wall.uvScale,
                            edgeSettings.wall.uvOffset,
                            sectorColor
                    );
                    continue;
                }

                const SectorDefinition& neighbor = map.sectors[static_cast<size_t>(reverse->second.boundary.sectorIndex)];
                if (neighbor.floorZ > sector.floorZ) {
                    AddWallSurface(
                            outGeometry,
                            SectorGeneratedSurfaceKind::LowerWall,
                            sectorInt,
                            ringKind,
                            holeIndex,
                            edgeInt,
                            edgeSettings.lower.textureId,
                            a,
                            b,
                            sector.floorZ,
                            neighbor.floorZ,
                            edgeSettings.lower.uvScale,
                            edgeSettings.lower.uvOffset,
                            sectorColor
                    );
                }

                if (neighbor.ceilingZ < sector.ceilingZ) {
                    AddWallSurface(
                            outGeometry,
                            SectorGeneratedSurfaceKind::UpperWall,
                            sectorInt,
                            ringKind,
                            holeIndex,
                            edgeInt,
                            edgeSettings.upper.textureId,
                            a,
                            b,
                            neighbor.ceilingZ,
                            sector.ceilingZ,
                            edgeSettings.upper.uvScale,
                            edgeSettings.upper.uvOffset,
                            sectorColor
                    );
                }
            }
        }
    }

    return !outGeometry.surfaces.empty();
}

bool BuildSectorGeneratedGeometry(
        const SectorTopologyMap& map,
        SectorGeneratedGeometry& outGeometry,
        std::string* outError)
{
    outGeometry = {};
    if (outError != nullptr) {
        outError->clear();
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(map);
    const auto firstError = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    if (firstError != issues.end()) {
        return SetTopologyError(
                outGeometry,
                outError,
                "Topology validation failed: "
                        + FormatSectorTopologyValidationIssue(*firstError));
    }
    if (map.sectors.empty()) {
        return SetTopologyError(outGeometry, outError, "Topology map has no sectors");
    }
    if (!ValidateTopologyGeometryValues(map, outGeometry, outError)) {
        return false;
    }

    SectorGeneratedGeometry generated;
    for (const SectorTopologySector& sector : map.sectors) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(map, sector.id, loops, &loopIssues)) {
            const std::string detail = loopIssues.empty()
                    ? "unknown loop extraction failure"
                    : FormatSectorTopologyValidationIssue(loopIssues.front());
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Failed to extract loops for topology sector "
                            + std::to_string(sector.id) + ": " + detail);
        }

        SectorGeneratedSurface floor;
        std::string error;
        if (!BuildTopologyFlatSurface(
                    map, sector, loops, SectorGeneratedSurfaceKind::Floor,
                    sector.floorZ, Vector3{0.0f, 1.0f, 0.0f}, true,
                    sector.floorTextureId, sector.floorUv, floor, error)) {
            return SetTopologyError(outGeometry, outError, error);
        }
        generated.surfaces.push_back(std::move(floor));

        SectorGeneratedSurface ceiling;
        if (!BuildTopologyFlatSurface(
                    map, sector, loops, SectorGeneratedSurfaceKind::Ceiling,
                    sector.ceilingZ, Vector3{0.0f, -1.0f, 0.0f}, false,
                    sector.ceilingTextureId, sector.ceilingUv, ceiling, error)) {
            return SetTopologyError(outGeometry, outError, error);
        }
        generated.surfaces.push_back(std::move(ceiling));
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef.lineDefId);
        const SectorTopologySector* sector = FindSectorTopologySector(map, sideDef.sectorId);
        if (lineDef == nullptr || sector == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Unexpected lookup failure for topology sidedef "
                            + std::to_string(sideDef.id));
        }

        const SectorTopologyVertex* lineStart = nullptr;
        const SectorTopologyVertex* lineEnd = nullptr;
        if (!GetSectorTopologyLineVertices(map, *lineDef, lineStart, lineEnd)
            || lineStart == nullptr || lineEnd == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve vertices for topology linedef "
                            + std::to_string(lineDef->id));
        }
        const SectorTopologyVertex* directedStart = sideDef.side == SectorTopologySideKind::Front
                ? lineStart : lineEnd;
        const SectorTopologyVertex* directedEnd = sideDef.side == SectorTopologySideKind::Front
                ? lineEnd : lineStart;
        const int oppositeId = sideDef.side == SectorTopologySideKind::Front
                ? lineDef->backSideDefId : lineDef->frontSideDefId;

        SectorGeneratedSurface wall;
        std::string error;
        if (oppositeId == -1) {
            if (!BuildTopologyWallSurface(
                        *directedStart, *directedEnd, *sector, *lineDef, sideDef,
                        SectorGeneratedSurfaceKind::Wall, sector->floorZ, sector->ceilingZ,
                        sideDef.wall, wall, error)) {
                return SetTopologyError(outGeometry, outError, error);
            }
            generated.surfaces.push_back(std::move(wall));
            continue;
        }

        const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(map, sideDef.id);
        if (opposite == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve opposite sidedef for topology sidedef "
                            + std::to_string(sideDef.id));
        }
        const SectorTopologySector* oppositeSector = FindSectorTopologySector(map, opposite->sectorId);
        if (oppositeSector == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve opposite sector for topology sidedef "
                            + std::to_string(sideDef.id));
        }

        if (oppositeSector->floorZ > sector->floorZ) {
            if (!BuildTopologyWallSurface(
                        *directedStart, *directedEnd, *sector, *lineDef, sideDef,
                        SectorGeneratedSurfaceKind::LowerWall,
                        sector->floorZ, oppositeSector->floorZ,
                        sideDef.lower, wall, error)) {
                return SetTopologyError(outGeometry, outError, error);
            }
            generated.surfaces.push_back(std::move(wall));
        }
        if (oppositeSector->ceilingZ < sector->ceilingZ) {
            if (!BuildTopologyWallSurface(
                        *directedStart, *directedEnd, *sector, *lineDef, sideDef,
                        SectorGeneratedSurfaceKind::UpperWall,
                        oppositeSector->ceilingZ, sector->ceilingZ,
                        sideDef.upper, wall, error)) {
                return SetTopologyError(outGeometry, outError, error);
            }
            generated.surfaces.push_back(std::move(wall));
        }
    }

    if (generated.surfaces.empty()) {
        return SetTopologyError(outGeometry, outError, "Topology map generated no surfaces");
    }
    outGeometry = std::move(generated);
    return true;
}

} // namespace game
