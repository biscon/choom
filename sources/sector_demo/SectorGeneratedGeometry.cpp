#include "sector_demo/SectorGeneratedGeometry.h"

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

namespace game {

namespace {

constexpr float EdgeEpsilon = 0.001f;
constexpr float PickDistanceTieEpsilon = 0.0001f;

using EarcutPoint = std::array<double, 2>;
using EarcutRing = std::vector<EarcutPoint>;
using EarcutPolygon = std::vector<EarcutRing>;

float Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float DecalBloomIntensityOrDefault(float value)
{
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return std::clamp(value, 0.0f, 10.0f);
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

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsUnitRgb(Vector3 value)
{
    return IsFinite(value)
            && value.x >= 0.0f && value.x <= 1.0f
            && value.y >= 0.0f && value.y <= 1.0f
            && value.z >= 0.0f && value.z <= 1.0f;
}

bool IsUnitFloat(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
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
            || !IsFinite(sector.ceilingUv.scale) || !IsFinite(sector.ceilingUv.offset)
            || !IsFinite(sector.floorDecal.uv.scale) || !IsFinite(sector.floorDecal.uv.offset)
            || !IsUnitFloat(sector.floorDecal.opacity)
            || !IsUnitRgb(sector.floorDecal.tint)
            || !IsFinite(sector.ceilingDecal.uv.scale) || !IsFinite(sector.ceilingDecal.uv.offset)
            || !IsUnitFloat(sector.ceilingDecal.opacity)
            || !IsUnitRgb(sector.ceilingDecal.tint)) {
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
        const std::array<const SectorTopologyWallPartSettings*, 4> parts{
                &sideDef.wall, &sideDef.lower, &sideDef.upper, &sideDef.middle};
        for (const SectorTopologyWallPartSettings* part : parts) {
            if (!IsFinite(part->uv.scale) || !IsFinite(part->uv.offset)
                || !IsFinite(part->decal.uv.scale) || !IsFinite(part->decal.uv.offset)
                || !IsUnitFloat(part->decal.opacity)
                || !IsUnitRgb(part->decal.tint)) {
                return SetTopologyError(
                        outGeometry,
                        outError,
                        "Topology sidedef " + std::to_string(sideDef.id)
                                + " contains non-finite generated-geometry values");
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
        const SectorTopologyDecalLayer& decal,
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
    surface.decalTextureId = decal.textureId;
    surface.decalOpacity = decal.textureId.empty() ? 1.0f : decal.opacity;
    surface.decalEmissive = !decal.textureId.empty() && decal.emissive;
    surface.decalTint = decal.textureId.empty() ? Vector3{1.0f, 1.0f, 1.0f} : decal.tint;
    surface.decalBloomIntensity = decal.textureId.empty()
            ? 1.0f
            : DecalBloomIntensityOrDefault(decal.bloomIntensity);
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
            const Vector2 baseSurfaceUv{
                    world.x / kSectorGeneratedTextureWorldSize,
                    world.y / kSectorGeneratedTextureWorldSize};
            const Vector2 localSurfaceUv{
                    (world.x - minX) / kSectorGeneratedTextureWorldSize,
                    (world.y - minZ) / kSectorGeneratedTextureWorldSize};
            surface.vertices.push_back(SectorGeneratedVertex{
                    SectorCoordToWorldPosition3(vertex.x, height, vertex.y),
                    normal,
                    ApplyUvSettings(baseSurfaceUv, uvSettings.scale, uvSettings.offset),
                    ApplyUvSettings(localSurfaceUv, decal.uv.scale, decal.uv.offset),
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
    const float u1 = length / kSectorGeneratedTextureWorldSize;
    const float v0 = height / kSectorGeneratedTextureWorldSize;
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
    surface.decalTextureId = settings.decal.textureId;
    surface.decalOpacity = settings.decal.textureId.empty() ? 1.0f : settings.decal.opacity;
    surface.decalEmissive = !settings.decal.textureId.empty() && settings.decal.emissive;
    surface.decalTint = settings.decal.textureId.empty()
            ? Vector3{1.0f, 1.0f, 1.0f}
            : settings.decal.tint;
    surface.decalBloomIntensity = settings.decal.textureId.empty()
            ? 1.0f
            : DecalBloomIntensityOrDefault(settings.decal.bloomIntensity);
    surface.normal = normal;
    surface.chartWidth = length;
    surface.chartHeight = height;
    surface.vertices.reserve(6);
    const Vector2 afBaseUv{0.0f, v0};
    const Vector2 acBaseUv{0.0f, 0.0f};
    const Vector2 bcBaseUv{u1, 0.0f};
    const Vector2 bfBaseUv{u1, v0};
    const SectorGeneratedVertex af{afPosition, normal,
            ApplyUvSettings(afBaseUv, settings.uv.scale, settings.uv.offset),
            ApplyUvSettings(afBaseUv, settings.decal.uv.scale, settings.decal.uv.offset),
            Vector2{0.0f, height}, color};
    const SectorGeneratedVertex ac{acPosition, normal,
            ApplyUvSettings(acBaseUv, settings.uv.scale, settings.uv.offset),
            ApplyUvSettings(acBaseUv, settings.decal.uv.scale, settings.decal.uv.offset),
            Vector2{0.0f, 0.0f}, color};
    const SectorGeneratedVertex bc{bcPosition, normal,
            ApplyUvSettings(bcBaseUv, settings.uv.scale, settings.uv.offset),
            ApplyUvSettings(bcBaseUv, settings.decal.uv.scale, settings.decal.uv.offset),
            Vector2{length, 0.0f}, color};
    const SectorGeneratedVertex bf{bfPosition, normal,
            ApplyUvSettings(bfBaseUv, settings.uv.scale, settings.uv.offset),
            ApplyUvSettings(bfBaseUv, settings.decal.uv.scale, settings.decal.uv.offset),
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

bool BuildTopologyMiddleSurface(
        const SectorTopologyVertex& a,
        const SectorTopologyVertex& b,
        const SectorTopologySector& facingSector,
        const SectorTopologyLineDef& lineDef,
        const SectorTopologySideDef& facingSideDef,
        float bottom,
        float top,
        const SectorTopologyWallPartSettings& settings,
        SectorGeneratedSurface& outSurface,
        std::string& outError)
{
    if (!BuildTopologyWallSurface(
                a,
                b,
                facingSector,
                lineDef,
                facingSideDef,
                SectorGeneratedSurfaceKind::Middle,
                bottom,
                top,
                settings,
                outSurface,
                outError)) {
        return false;
    }

    outSurface.decalTextureId.clear();
    outSurface.decalOpacity = 1.0f;
    outSurface.decalEmissive = false;
    outSurface.decalTint = Vector3{1.0f, 1.0f, 1.0f};
    outSurface.decalBloomIntensity = 1.0f;
    outSurface.alphaTest = true;
    outSurface.alphaCutoff = 0.5f;
    outSurface.receivesLightmap = true;
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
        case SectorGeneratedSurfaceKind::Middle: return "Middle";
    }
    return "Unknown";
}

SectorGeneratedSurfaceHit PickSectorGeneratedGeometry(
        const SectorGeneratedGeometry& geometry,
        Ray ray,
        float minDistance)
{
    SectorGeneratedSurfaceHit best;
    bool bestFacesRay = false;
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
            const bool candidateFacesRay = Dot(surface.normal, ray.direction) < 0.0f;
            const bool candidateCloser = !best.hit
                    || collision.distance < best.distance - PickDistanceTieEpsilon;
            const bool candidateTieBreaks = best.hit
                    && std::fabs(collision.distance - best.distance) <= PickDistanceTieEpsilon
                    && candidateFacesRay
                    && !bestFacesRay;
            if (candidateCloser || candidateTieBreaks) {
                best.hit = true;
                best.ref = surface.ref;
                best.worldPosition = collision.point;
                best.distance = collision.distance;
                bestFacesRay = candidateFacesRay;
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

    return label.str();
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
                    sector.floorTextureId, sector.floorUv, sector.floorDecal, floor, error)) {
            return SetTopologyError(outGeometry, outError, error);
        }
        generated.surfaces.push_back(std::move(floor));

        if (!sector.ceilingSky) {
            SectorGeneratedSurface ceiling;
            if (!BuildTopologyFlatSurface(
                        map, sector, loops, SectorGeneratedSurfaceKind::Ceiling,
                        sector.ceilingZ, Vector3{0.0f, -1.0f, 0.0f}, false,
                        sector.ceilingTextureId, sector.ceilingUv, sector.ceilingDecal, ceiling, error)) {
                return SetTopologyError(outGeometry, outError, error);
            }
            generated.surfaces.push_back(std::move(ceiling));
        }
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
        const bool suppressSkyUpperWall = sector->ceilingSky && oppositeSector->ceilingSky;
        if (!suppressSkyUpperWall && oppositeSector->ceilingZ < sector->ceilingZ) {
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

    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.frontSideDefId == -1 || lineDef.backSideDefId == -1) {
            continue;
        }

        const SectorTopologySideDef* frontSideDef = FindSectorTopologySideDef(map, lineDef.frontSideDefId);
        const SectorTopologySideDef* backSideDef = FindSectorTopologySideDef(map, lineDef.backSideDefId);
        if (frontSideDef == nullptr || backSideDef == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve middle texture sidedefs for topology linedef "
                            + std::to_string(lineDef.id));
        }
        if (frontSideDef->middle.textureId.empty() && backSideDef->middle.textureId.empty()) {
            continue;
        }

        const SectorTopologySector* frontSector = FindSectorTopologySector(map, frontSideDef->sectorId);
        const SectorTopologySector* backSector = FindSectorTopologySector(map, backSideDef->sectorId);
        if (frontSector == nullptr || backSector == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve middle texture sectors for topology linedef "
                            + std::to_string(lineDef.id));
        }

        const float bottom = std::max(frontSector->floorZ, backSector->floorZ);
        const float top = std::min(frontSector->ceilingZ, backSector->ceilingZ);
        if (!(top > bottom) || !std::isfinite(bottom) || !std::isfinite(top)) {
            continue;
        }

        const SectorTopologyVertex* lineStart = nullptr;
        const SectorTopologyVertex* lineEnd = nullptr;
        if (!GetSectorTopologyLineVertices(map, lineDef, lineStart, lineEnd)
            || lineStart == nullptr || lineEnd == nullptr) {
            return SetTopologyError(
                    outGeometry,
                    outError,
                    "Could not resolve vertices for topology linedef "
                            + std::to_string(lineDef.id));
        }
        const double dx = static_cast<double>(lineEnd->x) - static_cast<double>(lineStart->x);
        const double dy = static_cast<double>(lineEnd->y) - static_cast<double>(lineStart->y);
        const double coordLength = std::sqrt(dx * dx + dy * dy);
        if (!(coordLength > 0.0) || !std::isfinite(coordLength)) {
            continue;
        }

        const SectorTopologyWallPartSettings* frontSettings = &frontSideDef->middle;
        const SectorTopologyWallPartSettings* backSettings = &backSideDef->middle;
        const SectorTopologySideDef* frontRefSideDef = frontSideDef;
        const SectorTopologySideDef* backRefSideDef = backSideDef;
        const SectorTopologySector* frontRefSector = frontSector;
        const SectorTopologySector* backRefSector = backSector;
        const bool hasFrontMiddle = !frontSettings->textureId.empty();
        const bool hasBackMiddle = !backSettings->textureId.empty();
        if (!hasFrontMiddle && hasBackMiddle) {
            frontSettings = &backSideDef->middle;
            frontRefSideDef = backSideDef;
            frontRefSector = backSector;
        }
        if (!hasBackMiddle && hasFrontMiddle) {
            backSettings = &frontSideDef->middle;
            backRefSideDef = frontSideDef;
            backRefSector = frontSector;
        }

        SectorGeneratedSurface middle;
        std::string error;
        if (!BuildTopologyMiddleSurface(
                    *lineStart, *lineEnd, *frontSector, lineDef, *frontSideDef,
                    bottom, top, *frontSettings, middle, error)) {
            return SetTopologyError(outGeometry, outError, error);
        }
        middle.ref.topologySectorId = frontRefSector->id;
        middle.ref.topologySideDefId = frontRefSideDef->id;
        middle.ref.topologySide = frontRefSideDef->side;
        generated.surfaces.push_back(std::move(middle));

        if (!BuildTopologyMiddleSurface(
                    *lineEnd, *lineStart, *backSector, lineDef, *backSideDef,
                    bottom, top, *backSettings, middle, error)) {
            return SetTopologyError(outGeometry, outError, error);
        }
        middle.ref.topologySectorId = backRefSector->id;
        middle.ref.topologySideDefId = backRefSideDef->id;
        middle.ref.topologySide = backRefSideDef->side;
        generated.surfaces.push_back(std::move(middle));
    }

    if (generated.surfaces.empty()) {
        return SetTopologyError(outGeometry, outError, "Topology map generated no surfaces");
    }
    outGeometry = std::move(generated);
    return true;
}

} // namespace game
