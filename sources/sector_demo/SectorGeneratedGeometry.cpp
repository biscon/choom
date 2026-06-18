#include "sector_demo/SectorGeneratedGeometry.h"

#include "sector_demo/SectorMap.h"
#include "util/earcut.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace game {

namespace {

struct PointKey {
    int x = 0;
    int y = 0;
};

struct EdgeRef {
    int sectorIndex = -1;
    int edgeIndex = -1;
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
    return Vector3{point.x, height, point.y};
}

float EdgeLength(SectorPoint a, SectorPoint b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
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
    polygon.emplace_back();
    polygon.back().reserve(sector.points.size());

    for (const SectorPoint point : sector.points) {
        polygon.back().push_back(EarcutPoint{
                static_cast<double>(point.x),
                static_cast<double>(point.y)
        });
    }

    return polygon;
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

    const size_t pointCount = sector.points.size();
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

        const SectorPoint pa = sector.points[ia];
        SectorPoint pb = sector.points[ib];
        SectorPoint pc = sector.points[ic];
        const float area2 = TriangleArea2(pa, pb, pc);
        if (std::fabs(area2) <= EdgeEpsilon) {
            continue;
        }

        if ((facePositiveY && area2 > 0.0f) || (!facePositiveY && area2 < 0.0f)) {
            const SectorPoint swap = pb;
            pb = pc;
            pc = swap;
        }

        const float minX = std::fmin(pa.x, std::fmin(pb.x, pc.x));
        const float minY = std::fmin(pa.y, std::fmin(pb.y, pc.y));
        const float maxX = std::fmax(pa.x, std::fmax(pb.x, pc.x));
        const float maxY = std::fmax(pa.y, std::fmax(pb.y, pc.y));

        SectorGeneratedSurface surface;
        surface.ref = SectorGeneratedSurfaceRef{kind, sectorIndex, -1};
        surface.textureId = textureId;
        surface.normal = normal;
        surface.chartWidth = std::max(maxX - minX, EdgeEpsilon);
        surface.chartHeight = std::max(maxY - minY, EdgeEpsilon);
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pa, height),
                normal,
                ApplyUvSettings(Vector2{pa.x / TextureWorldSize, pa.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{pa.x - minX, pa.y - minY},
                color
        });
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pb, height),
                normal,
                ApplyUvSettings(Vector2{pb.x / TextureWorldSize, pb.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{pb.x - minX, pb.y - minY},
                color
        });
        surface.vertices.push_back(SectorGeneratedVertex{
                ToWorld(pc, height),
                normal,
                ApplyUvSettings(Vector2{pc.x / TextureWorldSize, pc.y / TextureWorldSize}, uvScale, uvOffset),
                Vector2{pc.x - minX, pc.y - minY},
                color
        });
        geometry.surfaces.push_back(std::move(surface));
    }
}

void AddWallSurface(
        SectorGeneratedGeometry& geometry,
        SectorGeneratedSurfaceKind kind,
        int sectorIndex,
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
    const float height = top - bottom;
    const float u1 = length / TextureWorldSize;
    const float v0 = height / TextureWorldSize;
    const float v1 = 0.0f;

    const SectorGeneratedVertex af{ToWorld(a, bottom), normal, ApplyUvSettings(Vector2{0.0f, v0}, uvScale, uvOffset), Vector2{0.0f, height}, color};
    const SectorGeneratedVertex ac{ToWorld(a, top), normal, ApplyUvSettings(Vector2{0.0f, v1}, uvScale, uvOffset), Vector2{0.0f, 0.0f}, color};
    const SectorGeneratedVertex bc{ToWorld(b, top), normal, ApplyUvSettings(Vector2{u1, v1}, uvScale, uvOffset), Vector2{length, 0.0f}, color};
    const SectorGeneratedVertex bf{ToWorld(b, bottom), normal, ApplyUvSettings(Vector2{u1, v0}, uvScale, uvOffset), Vector2{length, height}, color};

    SectorGeneratedSurface surface;
    surface.ref = SectorGeneratedSurfaceRef{kind, sectorIndex, edgeIndex};
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
        for (size_t edgeA = 0; edgeA < a.points.size(); ++edgeA) {
            const SectorPoint a0 = a.points[edgeA];
            const SectorPoint a1 = a.points[(edgeA + 1) % a.points.size()];

            for (size_t sectorB = sectorA + 1; sectorB < map.sectors.size(); ++sectorB) {
                const SectorDefinition& b = map.sectors[sectorB];
                for (size_t edgeB = 0; edgeB < b.points.size(); ++edgeB) {
                    const SectorPoint b0 = b.points[edgeB];
                    const SectorPoint b1 = b.points[(edgeB + 1) % b.points.size()];
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

bool BuildSectorGeneratedGeometry(const SectorMap& map, SectorGeneratedGeometry& outGeometry)
{
    outGeometry = SectorGeneratedGeometry{};
    std::unordered_map<std::string, EdgeRef> edgeRefs;

    WarnAboutPartialEdges(map);

    for (size_t sectorIndex = 0; sectorIndex < map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = map.sectors[sectorIndex];
        for (size_t edgeIndex = 0; edgeIndex < sector.points.size(); ++edgeIndex) {
            const SectorPoint a = sector.points[edgeIndex];
            const SectorPoint b = sector.points[(edgeIndex + 1) % sector.points.size()];
            const std::string edgeKey = MakeEdgeKey(a, b);
            if (edgeRefs.find(edgeKey) != edgeRefs.end()) {
                std::fprintf(
                        stderr,
                        "[SectorDemo WARNING] Duplicate same-direction edge found in sector '%s'\n",
                        sector.id.c_str()
                );
            }
            edgeRefs[edgeKey] = EdgeRef{static_cast<int>(sectorIndex), static_cast<int>(edgeIndex)};
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

        for (size_t edgeIndex = 0; edgeIndex < sector.points.size(); ++edgeIndex) {
            const SectorPoint a = sector.points[edgeIndex];
            const SectorPoint b = sector.points[(edgeIndex + 1) % sector.points.size()];
            const int edgeInt = static_cast<int>(edgeIndex);
            const EffectiveEdgeSettings edgeSettings = GetEffectiveEdgeSettings(sector, edgeInt);
            const auto reverse = edgeRefs.find(MakeEdgeKey(b, a));
            if (reverse == edgeRefs.end() || reverse->second.sectorIndex == sectorInt) {
                AddWallSurface(
                        outGeometry,
                        SectorGeneratedSurfaceKind::Wall,
                        sectorInt,
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

            const SectorDefinition& neighbor = map.sectors[static_cast<size_t>(reverse->second.sectorIndex)];
            if (neighbor.floorZ > sector.floorZ) {
                AddWallSurface(
                        outGeometry,
                        SectorGeneratedSurfaceKind::LowerWall,
                        sectorInt,
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

    return !outGeometry.surfaces.empty();
}

} // namespace game
