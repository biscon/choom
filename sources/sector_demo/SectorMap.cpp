#include "sector_demo/SectorMap.h"

#include "sector_demo/SectorUnits.h"
#include "util/json.hpp"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <utility>

namespace game {

namespace {

using Json = nlohmann::json;

constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float RadiansToDegrees = 180.0f / 3.14159265358979323846f;
constexpr float GeometryEpsilon = 0.001f;

bool ReadPoint(const Json& value, SectorPoint& point)
{
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        return false;
    }

    point.x = value[0].get<float>();
    point.y = value[1].get<float>();
    return true;
}

bool ReadStringField(const Json& object, const char* field, std::string& out)
{
    const auto it = object.find(field);
    if (it == object.end() || !it->is_string()) {
        return false;
    }

    out = it->get<std::string>();
    return true;
}

std::string LowercaseAscii(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

SectorTextureFilter ParseTextureFilter(const std::string& value, const char* textureId)
{
    const std::string normalized = LowercaseAscii(value);
    if (normalized == "point") {
        return SectorTextureFilter::Point;
    }
    if (normalized == "bilinear") {
        return SectorTextureFilter::Bilinear;
    }

    std::fprintf(
            stderr,
            "[SectorDemo WARNING] Unknown texture filter '%s' for id '%s'; using bilinear\n",
            value.c_str(),
            textureId == nullptr ? "" : textureId
    );
    return SectorTextureFilter::Bilinear;
}

bool ReadTextureDefinition(const std::string& id, const Json& value, SectorTextureDefinition& out)
{
    out = SectorTextureDefinition{};
    out.id = id;

    if (value.is_string()) {
        out.path = value.get<std::string>();
        out.filter = SectorTextureFilter::Bilinear;
        return !out.path.empty();
    }

    if (!value.is_object()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed texture entry for id '%s'\n", id.c_str());
        return false;
    }

    if (!ReadStringField(value, "path", out.path) || out.path.empty()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring texture id '%s' without string path\n", id.c_str());
        return false;
    }

    std::string filter;
    if (ReadStringField(value, "filter", filter)) {
        out.filter = ParseTextureFilter(filter, id.c_str());
    } else {
        out.filter = SectorTextureFilter::Bilinear;
    }

    return true;
}

bool ReadVector2Field(const Json& object, const char* field, Vector2& out)
{
    const auto it = object.find(field);
    if (it == object.end() || !it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number()) {
        return false;
    }

    out = Vector2{(*it)[0].get<float>(), (*it)[1].get<float>()};
    return true;
}

int ClampColorChannel(int value)
{
    return std::clamp(value, 0, 255);
}

float ClampAmbientIntensity(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ClampLightIntensity(float value)
{
    return std::clamp(value, 0.0f, 8.0f);
}

float ClampLightRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f));
}

float ClampLightSourceRadius(float value, float lightRadius)
{
    const float clamped = std::clamp(value, 0.0f, SectorWorldToAuthoringDistance(8.0f));
    return std::min(clamped, std::max(0.0f, lightRadius * 0.5f));
}

float ClampAmbientOcclusionRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(16.0f));
}

float ClampAmbientOcclusionStrength(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ClampIndirectBounceRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(16.0f));
}

float ClampIndirectBounceStrength(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

void ReadAmbientColorField(const Json& sectorJson, SectorDefinition& sector)
{
    const auto it = sectorJson.find("ambientColor");
    if (it == sectorJson.end()) {
        return;
    }

    if (!it->is_array() || it->size() != 3 || !(*it)[0].is_number() || !(*it)[1].is_number() || !(*it)[2].is_number()) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Ignoring malformed ambientColor in sector '%s'\n",
                sector.id.c_str()
        );
        return;
    }

    sector.ambientColor = Color{
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround((*it)[0].get<float>())))),
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround((*it)[1].get<float>())))),
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround((*it)[2].get<float>())))),
            255
    };
}

void ReadAmbientIntensityField(const Json& sectorJson, SectorDefinition& sector)
{
    const auto it = sectorJson.find("ambientIntensity");
    if (it == sectorJson.end()) {
        return;
    }

    if (!it->is_number()) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Ignoring malformed ambientIntensity in sector '%s'\n",
                sector.id.c_str()
        );
        return;
    }

    sector.ambientIntensity = ClampAmbientIntensity(it->get<float>());
}

bool ReadVector3Array(const Json& value, Vector3& out)
{
    if (!value.is_array()
            || value.size() != 3
            || !value[0].is_number()
            || !value[1].is_number()
            || !value[2].is_number()) {
        return false;
    }

    out = Vector3{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
    return true;
}

bool ReadColorRgbArray(const Json& value, Color& out)
{
    if (!value.is_array()
            || value.size() != 3
            || !value[0].is_number()
            || !value[1].is_number()
            || !value[2].is_number()) {
        return false;
    }

    out = Color{
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround(value[0].get<float>())))),
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround(value[1].get<float>())))),
            static_cast<unsigned char>(ClampColorChannel(static_cast<int>(std::lround(value[2].get<float>())))),
            255
    };
    return true;
}

void ReadStaticLights(const Json& root, SectorMap& outMap)
{
    const auto lightsIt = root.find("staticLights");
    if (lightsIt == root.end()) {
        return;
    }
    if (!lightsIt->is_array()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed staticLights field\n");
        return;
    }

    for (const Json& lightJson : *lightsIt) {
        if (!lightJson.is_object()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed static light entry\n");
            continue;
        }

        SectorStaticPointLight light;
        if (!ReadStringField(lightJson, "id", light.id) || light.id.empty()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring static light without string id\n");
            continue;
        }

        const auto positionIt = lightJson.find("position");
        if (positionIt == lightJson.end() || !ReadVector3Array(*positionIt, light.position)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring static light '%s' with malformed position\n", light.id.c_str());
            continue;
        }

        const auto colorIt = lightJson.find("color");
        if (colorIt == lightJson.end() || !ReadColorRgbArray(*colorIt, light.color)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring static light '%s' with malformed color\n", light.id.c_str());
            continue;
        }

        const auto intensityIt = lightJson.find("intensity");
        const auto radiusIt = lightJson.find("radius");
        if (intensityIt == lightJson.end()
                || radiusIt == lightJson.end()
                || !intensityIt->is_number()
                || !radiusIt->is_number()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring static light '%s' with malformed intensity/radius\n", light.id.c_str());
            continue;
        }

        light.intensity = ClampLightIntensity(intensityIt->get<float>());
        light.radius = ClampLightRadius(radiusIt->get<float>());
        const auto sourceRadiusIt = lightJson.find("sourceRadius");
        if (sourceRadiusIt != lightJson.end() && sourceRadiusIt->is_number()) {
            light.sourceRadius = ClampLightSourceRadius(sourceRadiusIt->get<float>(), light.radius);
        } else {
            light.sourceRadius = 0.0f;
        }
        outMap.staticLights.push_back(std::move(light));
    }
}

void ReadLightmapSettings(const Json& root, SectorMap& outMap)
{
    const auto settingsIt = root.find("lightmapSettings");
    if (settingsIt == root.end()) {
        return;
    }
    if (!settingsIt->is_object()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed lightmapSettings field\n");
        return;
    }

    SectorLightmapBakeSettings settings = outMap.lightmapSettings;
    const auto radiusIt = settingsIt->find("ambientOcclusionRadius");
    if (radiusIt != settingsIt->end()) {
        if (radiusIt->is_number()) {
            settings.ambientOcclusionRadius = ClampAmbientOcclusionRadius(radiusIt->get<float>());
        } else {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed ambientOcclusionRadius\n");
        }
    }

    const auto strengthIt = settingsIt->find("ambientOcclusionStrength");
    if (strengthIt != settingsIt->end()) {
        if (strengthIt->is_number()) {
            settings.ambientOcclusionStrength = ClampAmbientOcclusionStrength(strengthIt->get<float>());
        } else {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed ambientOcclusionStrength\n");
        }
    }

    const auto bounceRadiusIt = settingsIt->find("indirectBounceRadius");
    if (bounceRadiusIt != settingsIt->end()) {
        if (bounceRadiusIt->is_number()) {
            settings.indirectBounceRadius = ClampIndirectBounceRadius(bounceRadiusIt->get<float>());
        } else {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed indirectBounceRadius\n");
        }
    }

    const auto bounceStrengthIt = settingsIt->find("indirectBounceStrength");
    if (bounceStrengthIt != settingsIt->end()) {
        if (bounceStrengthIt->is_number()) {
            settings.indirectBounceStrength = ClampIndirectBounceStrength(bounceStrengthIt->get<float>());
        } else {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed indirectBounceStrength\n");
        }
    }

    outMap.lightmapSettings = settings;
}

void ReadBakedLightmapMetadata(const Json& root, SectorMap& outMap)
{
    const auto metadataIt = root.find("bakedLightmap");
    if (metadataIt == root.end()) {
        return;
    }
    if (!metadataIt->is_object()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed bakedLightmap field\n");
        return;
    }

    SectorLightmapMetadata metadata;
    if (!ReadStringField(*metadataIt, "path", metadata.path) || metadata.path.empty()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring bakedLightmap without path\n");
        return;
    }
    if (!ReadStringField(*metadataIt, "sourceHash", metadata.sourceHash) || metadata.sourceHash.empty()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring bakedLightmap without sourceHash\n");
        return;
    }

    const auto widthIt = metadataIt->find("width");
    const auto heightIt = metadataIt->find("height");
    if (widthIt == metadataIt->end()
            || heightIt == metadataIt->end()
            || !widthIt->is_number_integer()
            || !heightIt->is_number_integer()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring bakedLightmap with malformed dimensions\n");
        return;
    }

    metadata.width = widthIt->get<int>();
    metadata.height = heightIt->get<int>();
    if (metadata.width <= 0 || metadata.height <= 0) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring bakedLightmap with invalid dimensions\n");
        return;
    }

    outMap.bakedLightmap = std::move(metadata);
}

bool HasTexture(const SectorMap& map, const std::string& id)
{
    return FindSectorTexture(map, id) != nullptr;
}

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= GeometryEpsilon && std::fabs(a.y - b.y) <= GeometryEpsilon;
}

float Cross(SectorPoint a, SectorPoint b, SectorPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
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

bool EdgesAreAdjacent(size_t edgeA, size_t edgeB, size_t edgeCount)
{
    if (edgeA == edgeB) {
        return true;
    }

    return (edgeA + 1) % edgeCount == edgeB || (edgeB + 1) % edgeCount == edgeA;
}

bool HasSelfIntersection(const std::vector<SectorPoint>& points)
{
    const size_t edgeCount = points.size();
    for (size_t edgeA = 0; edgeA < edgeCount; ++edgeA) {
        const SectorPoint a0 = points[edgeA];
        const SectorPoint a1 = points[(edgeA + 1) % edgeCount];
        for (size_t edgeB = edgeA + 1; edgeB < edgeCount; ++edgeB) {
            if (EdgesAreAdjacent(edgeA, edgeB, edgeCount)) {
                continue;
            }

            const SectorPoint b0 = points[edgeB];
            const SectorPoint b1 = points[(edgeB + 1) % edgeCount];
            if (SegmentsIntersect(a0, a1, b0, b1)) {
                return true;
            }
        }
    }

    return false;
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
                && point.x < (b.x - a.x) * (point.y - a.y)
                        / ((b.y - a.y) == 0.0f ? 0.00001f : (b.y - a.y)) + a.x;
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool RingsIntersect(const std::vector<SectorPoint>& a, const std::vector<SectorPoint>& b)
{
    for (size_t edgeA = 0; edgeA < a.size(); ++edgeA) {
        for (size_t edgeB = 0; edgeB < b.size(); ++edgeB) {
            if (SegmentsIntersect(
                        a[edgeA],
                        a[(edgeA + 1) % a.size()],
                        b[edgeB],
                        b[(edgeB + 1) % b.size()])) {
                return true;
            }
        }
    }
    return false;
}

bool NormalizeRingWinding(
        std::vector<SectorPoint>& points,
        bool clockwise,
        const char* sectorId,
        const char* ringLabel)
{
    if (points.size() < 3) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': %s needs at least 3 points\n", sectorId, ringLabel);
        return false;
    }
    for (size_t i = 0; i < points.size(); ++i) {
        const SectorPoint a = points[i];
        const SectorPoint b = points[(i + 1) % points.size()];
        if (SamePoint(a, b)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': duplicate adjacent points in %s\n", sectorId, ringLabel);
            return false;
        }
    }

    if (HasSelfIntersection(points)) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': self-intersecting %s\n", sectorId, ringLabel);
        return false;
    }

    const float area2 = PolygonArea2(points);
    if (std::fabs(area2) <= GeometryEpsilon) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': %s area is too small\n", sectorId, ringLabel);
        return false;
    }

    if ((clockwise && area2 > 0.0f) || (!clockwise && area2 < 0.0f)) {
        std::reverse(points.begin(), points.end());
    }

    return true;
}

bool NormalizeSectorWinding(SectorDefinition& sector)
{
    if (!NormalizeRingWinding(sector.points, false, sector.id.c_str(), "outer ring")) {
        return false;
    }
    for (size_t holeIndex = 0; holeIndex < sector.holes.size(); ++holeIndex) {
        const std::string label = "hole " + std::to_string(holeIndex);
        if (!NormalizeRingWinding(sector.holes[holeIndex], true, sector.id.c_str(), label.c_str())) {
            return false;
        }
        const std::vector<SectorPoint>& hole = sector.holes[holeIndex];
        if (RingsIntersect(hole, sector.points)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': hole touches or crosses outer ring\n", sector.id.c_str());
            return false;
        }
        for (SectorPoint point : hole) {
            if (!StrictPointInPolygon(point, sector.points)) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': hole lies outside outer ring\n", sector.id.c_str());
                return false;
            }
        }
        for (size_t otherHoleIndex = 0; otherHoleIndex < holeIndex; ++otherHoleIndex) {
            const std::vector<SectorPoint>& otherHole = sector.holes[otherHoleIndex];
            if (RingsIntersect(hole, otherHole)
                    || StrictPointInPolygon(hole.front(), otherHole)
                    || StrictPointInPolygon(otherHole.front(), hole)) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': holes overlap or touch\n", sector.id.c_str());
                return false;
            }
        }
    }
    return true;
}

void ValidateTextureReference(const SectorMap& map, const char* sectorId, const char* field, const std::string& id)
{
    if (!HasTexture(map, id)) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Sector '%s' references missing texture id '%s' in field '%s'\n",
                sectorId,
                id.c_str(),
                field
        );
    }
}

std::string DefaultWallTextureId(const SectorMap& map)
{
    const auto it = map.texturesById.find("wall");
    if (it != map.texturesById.end()) {
        return it->first;
    }

    const std::vector<std::string> textureIds = SortedSectorTextureIds(map);
    return textureIds.empty() ? std::string{} : textureIds.front();
}

bool SameBoundaryEdge(const SectorEdgeOverride& edgeOverride, SectorBoundaryRingKind ringKind, int holeIndex, int edgeIndex)
{
    return edgeOverride.ringKind == ringKind
            && edgeOverride.holeIndex == (ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1)
            && edgeOverride.edgeIndex == edgeIndex;
}

SectorEdgeOverride* FindEdgeOverride(
        SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex)
{
    for (SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (SameBoundaryEdge(edgeOverride, ringKind, holeIndex, edgeIndex)) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

const SectorEdgeOverride* FindEdgeOverride(
        const SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex)
{
    for (const SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (SameBoundaryEdge(edgeOverride, ringKind, holeIndex, edgeIndex)) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

SectorEdgeOverride& EnsureEdgeOverride(
        SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex)
{
    if (SectorEdgeOverride* existing = FindEdgeOverride(sector, ringKind, holeIndex, edgeIndex)) {
        return *existing;
    }

    SectorEdgeOverride edgeOverride;
    edgeOverride.ringKind = ringKind;
    edgeOverride.holeIndex = ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1;
    edgeOverride.edgeIndex = edgeIndex;
    sector.edgeOverrides.push_back(std::move(edgeOverride));
    return sector.edgeOverrides.back();
}

void ReadEdgeTextureField(
        const SectorMap& map,
        const char* sectorId,
        const Json& edgeJson,
        const char* field,
        std::string& textureId,
        bool& hasTexture)
{
    const auto it = edgeJson.find(field);
    if (it == edgeJson.end()) {
        return;
    }
    if (!it->is_string()) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Ignoring malformed texture field '%s' on edge override in sector '%s'\n",
                field,
                sectorId
        );
        return;
    }

    const std::string id = it->get<std::string>();
    if (!HasTexture(map, id)) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Sector '%s' edge override references missing texture id '%s' in field '%s'\n",
                sectorId,
                id.c_str(),
                field
        );
    }

    textureId = id;
    hasTexture = true;
}

void ReadEdgeUvField(
        const char* sectorId,
        const Json& edgeJson,
        int edgeIndex,
        const char* field,
        Vector2& uvValue,
        bool& hasUvValue)
{
    const auto it = edgeJson.find(field);
    if (it == edgeJson.end()) {
        return;
    }

    Vector2 parsed{};
    if (ReadVector2Field(edgeJson, field, parsed)) {
        uvValue = parsed;
        hasUvValue = true;
    } else {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed %s on edge %d in sector '%s'\n", field, edgeIndex, sectorId);
    }
}

void ApplyLegacyEdgeUvScale(SectorEdgeOverride& edgeOverride, Vector2 uvScale)
{
    edgeOverride.wallUv.uvScale = uvScale;
    edgeOverride.lowerUv.uvScale = uvScale;
    edgeOverride.upperUv.uvScale = uvScale;
    edgeOverride.wallUv.hasUvScale = true;
    edgeOverride.lowerUv.hasUvScale = true;
    edgeOverride.upperUv.hasUvScale = true;
}

void ApplyLegacyEdgeUvOffset(SectorEdgeOverride& edgeOverride, Vector2 uvOffset)
{
    edgeOverride.wallUv.uvOffset = uvOffset;
    edgeOverride.lowerUv.uvOffset = uvOffset;
    edgeOverride.upperUv.uvOffset = uvOffset;
    edgeOverride.wallUv.hasUvOffset = true;
    edgeOverride.lowerUv.hasUvOffset = true;
    edgeOverride.upperUv.hasUvOffset = true;
}

bool HasAnyUvOverride(const SectorEdgePartUvOverride& uv)
{
    return uv.hasUvScale || uv.hasUvOffset;
}

void ApplyPartUvOverride(EffectiveEdgePartSettings& settings, const SectorEdgePartUvOverride& uv)
{
    if (uv.hasUvScale) {
        settings.uvScale = uv.uvScale;
    }
    if (uv.hasUvOffset) {
        settings.uvOffset = uv.uvOffset;
    }
}

void WriteEdgeUvFields(Json& edgeJson, const char* scaleField, const char* offsetField, const SectorEdgePartUvOverride& uv)
{
    if (uv.hasUvScale) {
        edgeJson[scaleField] = Json::array({uv.uvScale.x, uv.uvScale.y});
    }
    if (uv.hasUvOffset) {
        edgeJson[offsetField] = Json::array({uv.uvOffset.x, uv.uvOffset.y});
    }
}

void ReadSurfaceUvField(
        const char* sectorId,
        const Json& sectorJson,
        const char* field,
        Vector2& uvValue,
        bool& hasUvValue)
{
    const auto it = sectorJson.find(field);
    if (it == sectorJson.end()) {
        return;
    }

    Vector2 parsed{};
    if (ReadVector2Field(sectorJson, field, parsed)) {
        uvValue = parsed;
        hasUvValue = true;
    } else {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed %s in sector '%s'\n", field, sectorId);
    }
}

void WriteSurfaceUvFields(Json& sectorJson, const char* scaleField, const char* offsetField, const SectorSurfaceUvOverride& uv)
{
    if (uv.hasUvScale) {
        sectorJson[scaleField] = Json::array({uv.uvScale.x, uv.uvScale.y});
    }
    if (uv.hasUvOffset) {
        sectorJson[offsetField] = Json::array({uv.uvOffset.x, uv.uvOffset.y});
    }
}

void ReadEdgeOverrides(const SectorMap& map, const Json& sectorJson, SectorDefinition& sector)
{
    const auto edgesIt = sectorJson.find("edges");
    if (edgesIt == sectorJson.end()) {
        return;
    }
    if (!edgesIt->is_array()) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed edges field in sector '%s'\n", sector.id.c_str());
        return;
    }

    for (const Json& edgeJson : *edgesIt) {
        if (!edgeJson.is_object()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed edge override in sector '%s'\n", sector.id.c_str());
            continue;
        }

        const auto edgeIt = edgeJson.find("edge");
        if (edgeIt == edgeJson.end() || !edgeIt->is_number_integer()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring edge override without integer edge in sector '%s'\n", sector.id.c_str());
            continue;
        }

        SectorBoundaryRingKind ringKind = SectorBoundaryRingKind::Outer;
        int holeIndex = -1;
        const auto ringIt = edgeJson.find("ring");
        if (ringIt != edgeJson.end()) {
            if (!ringIt->is_string()) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring edge override with malformed ring in sector '%s'\n", sector.id.c_str());
                continue;
            }
            const std::string ring = ringIt->get<std::string>();
            if (ring == "hole") {
                ringKind = SectorBoundaryRingKind::Hole;
                const auto holeIt = edgeJson.find("hole");
                if (holeIt == edgeJson.end() || !holeIt->is_number_integer()) {
                    std::fprintf(stderr, "[SectorDemo WARNING] Ignoring hole edge override without integer hole in sector '%s'\n", sector.id.c_str());
                    continue;
                }
                holeIndex = holeIt->get<int>();
            } else if (ring != "outer") {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring edge override with unknown ring '%s' in sector '%s'\n", ring.c_str(), sector.id.c_str());
                continue;
            }
        }

        const int edgeIndex = edgeIt->get<int>();
        const std::vector<SectorPoint>* ring = nullptr;
        if (ringKind == SectorBoundaryRingKind::Outer) {
            ring = &sector.points;
        } else if (holeIndex >= 0 && holeIndex < static_cast<int>(sector.holes.size())) {
            ring = &sector.holes[static_cast<size_t>(holeIndex)];
        }
        if (ring == nullptr || edgeIndex < 0 || edgeIndex >= static_cast<int>(ring->size())) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Ignoring out-of-range boundary edge override %d in sector '%s'\n",
                    edgeIndex,
                    sector.id.c_str()
            );
            continue;
        }

        SectorEdgeOverride& edgeOverride = EnsureEdgeOverride(sector, ringKind, holeIndex, edgeIndex);
        ReadEdgeTextureField(map, sector.id.c_str(), edgeJson, "wallTex", edgeOverride.wallTextureId, edgeOverride.hasWallTexture);
        ReadEdgeTextureField(map, sector.id.c_str(), edgeJson, "lowerWallTex", edgeOverride.lowerWallTextureId, edgeOverride.hasLowerWallTexture);
        ReadEdgeTextureField(map, sector.id.c_str(), edgeJson, "upperWallTex", edgeOverride.upperWallTextureId, edgeOverride.hasUpperWallTexture);

        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "wallUvScale", edgeOverride.wallUv.uvScale, edgeOverride.wallUv.hasUvScale);
        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "wallUvOffset", edgeOverride.wallUv.uvOffset, edgeOverride.wallUv.hasUvOffset);
        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "lowerUvScale", edgeOverride.lowerUv.uvScale, edgeOverride.lowerUv.hasUvScale);
        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "lowerUvOffset", edgeOverride.lowerUv.uvOffset, edgeOverride.lowerUv.hasUvOffset);
        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "upperUvScale", edgeOverride.upperUv.uvScale, edgeOverride.upperUv.hasUvScale);
        ReadEdgeUvField(sector.id.c_str(), edgeJson, edgeIndex, "upperUvOffset", edgeOverride.upperUv.uvOffset, edgeOverride.upperUv.hasUvOffset);

        const auto scaleIt = edgeJson.find("uvScale");
        if (scaleIt != edgeJson.end()) {
            Vector2 uvScale{};
            if (ReadVector2Field(edgeJson, "uvScale", uvScale)) {
                ApplyLegacyEdgeUvScale(edgeOverride, uvScale);
            } else {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed uvScale on edge %d in sector '%s'\n", edgeIndex, sector.id.c_str());
            }
        }

        const auto offsetIt = edgeJson.find("uvOffset");
        if (offsetIt != edgeJson.end()) {
            Vector2 uvOffset{};
            if (ReadVector2Field(edgeJson, "uvOffset", uvOffset)) {
                ApplyLegacyEdgeUvOffset(edgeOverride, uvOffset);
            } else {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed uvOffset on edge %d in sector '%s'\n", edgeIndex, sector.id.c_str());
            }
        }
    }
}

bool IsEmptyOverride(const SectorEdgeOverride& edgeOverride)
{
    return !edgeOverride.hasWallTexture
            && !edgeOverride.hasLowerWallTexture
            && !edgeOverride.hasUpperWallTexture
            && !HasAnyUvOverride(edgeOverride.wallUv)
            && !HasAnyUvOverride(edgeOverride.lowerUv)
            && !HasAnyUvOverride(edgeOverride.upperUv);
}

void RemapEdgeOverrides(
        SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        const std::vector<SectorPoint>& originalRing,
        const std::vector<SectorPoint>& normalizedRing)
{
    for (SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (edgeOverride.ringKind != ringKind
                || edgeOverride.holeIndex != (ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1)) {
            continue;
        }
        const int oldIndex = edgeOverride.edgeIndex;
        if (oldIndex < 0 || oldIndex >= static_cast<int>(originalRing.size())) {
            continue;
        }

        const SectorPoint oldA = originalRing[static_cast<size_t>(oldIndex)];
        const SectorPoint oldB = originalRing[(static_cast<size_t>(oldIndex) + 1) % originalRing.size()];
        for (size_t newIndex = 0; newIndex < normalizedRing.size(); ++newIndex) {
            const SectorPoint newA = normalizedRing[newIndex];
            const SectorPoint newB = normalizedRing[(newIndex + 1) % normalizedRing.size()];
            if ((SamePoint(oldA, newA) && SamePoint(oldB, newB))
                    || (SamePoint(oldA, newB) && SamePoint(oldB, newA))) {
                edgeOverride.edgeIndex = static_cast<int>(newIndex);
                break;
            }
        }
    }
}

void InsertSectorEdgeMidpoint(
        SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex,
        SectorPoint midpoint)
{
    std::vector<SectorEdgeOverride> remappedOverrides;
    remappedOverrides.reserve(sector.edgeOverrides.size() + 1);
    for (const SectorEdgeOverride& oldOverride : sector.edgeOverrides) {
        SectorEdgeOverride remapped = oldOverride;
        const bool sameRing = oldOverride.ringKind == ringKind
                && oldOverride.holeIndex == (ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1);
        if (sameRing && oldOverride.edgeIndex > edgeIndex) {
            ++remapped.edgeIndex;
        }
        remappedOverrides.push_back(std::move(remapped));

        if (sameRing && oldOverride.edgeIndex == edgeIndex) {
            SectorEdgeOverride secondHalf = oldOverride;
            secondHalf.edgeIndex = edgeIndex + 1;
            remappedOverrides.push_back(std::move(secondHalf));
        }
    }

    std::vector<SectorPoint>& ring = ringKind == SectorBoundaryRingKind::Outer
            ? sector.points : sector.holes[static_cast<size_t>(holeIndex)];
    ring.insert(ring.begin() + edgeIndex + 1, midpoint);
    sector.edgeOverrides = std::move(remappedOverrides);
}

} // namespace

const SectorTextureDefinition* FindSectorTexture(const SectorMap& map, const std::string& id)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = map.texturesById.find(id);
    return it == map.texturesById.end() ? nullptr : &it->second;
}

std::vector<std::string> SortedSectorTextureIds(const SectorMap& map)
{
    std::vector<std::string> ids;
    ids.reserve(map.texturesById.size());
    for (const auto& texture : map.texturesById) {
        ids.push_back(texture.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return engine::TextureLoad_PointFilter;
        case SectorTextureFilter::Bilinear:
            return engine::TextureLoad_BilinearFilter;
    }
    return engine::TextureLoad_BilinearFilter;
}

const char* SectorTextureFilterName(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return "point";
        case SectorTextureFilter::Bilinear:
            return "bilinear";
    }
    return "bilinear";
}

EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorDefinition& sector, int edgeIndex)
{
    EffectiveEdgeSettings settings;
    settings.wall.textureId = sector.wallTextureId;
    settings.lower.textureId = sector.lowerWallTextureId;
    settings.upper.textureId = sector.upperWallTextureId;

    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(
            sector, SectorBoundaryRingKind::Outer, -1, edgeIndex);
    if (edgeOverride == nullptr) {
        return settings;
    }

    if (edgeOverride->hasWallTexture) {
        settings.wall.textureId = edgeOverride->wallTextureId;
    }
    if (edgeOverride->hasLowerWallTexture) {
        settings.lower.textureId = edgeOverride->lowerWallTextureId;
    }
    if (edgeOverride->hasUpperWallTexture) {
        settings.upper.textureId = edgeOverride->upperWallTextureId;
    }
    ApplyPartUvOverride(settings.wall, edgeOverride->wallUv);
    ApplyPartUvOverride(settings.lower, edgeOverride->lowerUv);
    ApplyPartUvOverride(settings.upper, edgeOverride->upperUv);

    return settings;
}

EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorMap& map, const SectorBoundaryEdgeRef& edge)
{
    EffectiveEdgeSettings settings;
    if (edge.sectorIndex < 0 || edge.sectorIndex >= static_cast<int>(map.sectors.size())) {
        return settings;
    }

    const SectorDefinition& sector = map.sectors[static_cast<size_t>(edge.sectorIndex)];
    settings.wall.textureId = sector.wallTextureId;
    settings.lower.textureId = sector.lowerWallTextureId;
    settings.upper.textureId = sector.upperWallTextureId;

    const std::vector<SectorPoint>* ring = GetSectorBoundaryRing(map, edge);
    if (ring == nullptr || edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(ring->size())) {
        return settings;
    }
    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(
            sector, edge.ringKind, edge.holeIndex, edge.edgeIndex);
    if (edgeOverride == nullptr) {
        return settings;
    }
    if (edgeOverride->hasWallTexture) settings.wall.textureId = edgeOverride->wallTextureId;
    if (edgeOverride->hasLowerWallTexture) settings.lower.textureId = edgeOverride->lowerWallTextureId;
    if (edgeOverride->hasUpperWallTexture) settings.upper.textureId = edgeOverride->upperWallTextureId;
    ApplyPartUvOverride(settings.wall, edgeOverride->wallUv);
    ApplyPartUvOverride(settings.lower, edgeOverride->lowerUv);
    ApplyPartUvOverride(settings.upper, edgeOverride->upperUv);
    return settings;
}

EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, int sectorIndex, int edgeIndex)
{
    return FindReverseEdgeNeighbor(
            map,
            SectorBoundaryEdgeRef{sectorIndex, SectorBoundaryRingKind::Outer, -1, edgeIndex}
    );
}

const std::vector<SectorPoint>* GetSectorBoundaryRing(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge)
{
    if (edge.sectorIndex < 0 || edge.sectorIndex >= static_cast<int>(map.sectors.size())) {
        return nullptr;
    }
    const SectorDefinition& sector = map.sectors[static_cast<size_t>(edge.sectorIndex)];
    if (edge.ringKind == SectorBoundaryRingKind::Outer) {
        return &sector.points;
    }
    if (edge.holeIndex < 0 || edge.holeIndex >= static_cast<int>(sector.holes.size())) {
        return nullptr;
    }
    return &sector.holes[static_cast<size_t>(edge.holeIndex)];
}

std::vector<SectorPoint>* GetSectorBoundaryRing(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge)
{
    return const_cast<std::vector<SectorPoint>*>(GetSectorBoundaryRing(
            static_cast<const SectorMap&>(map), edge));
}

bool GetSectorBoundaryEdge(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        SectorPoint& outA,
        SectorPoint& outB)
{
    const std::vector<SectorPoint>* ring = GetSectorBoundaryRing(map, edge);
    if (ring == nullptr || ring->size() < 2
            || edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(ring->size())) {
        return false;
    }
    outA = (*ring)[static_cast<size_t>(edge.edgeIndex)];
    outB = (*ring)[(static_cast<size_t>(edge.edgeIndex) + 1) % ring->size()];
    return true;
}

EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, const SectorBoundaryEdgeRef& edge)
{
    SectorPoint a{};
    SectorPoint b{};
    if (!GetSectorBoundaryEdge(map, edge, a, b)) {
        return EdgeNeighborInfo{};
    }
    for (size_t otherSectorIndex = 0; otherSectorIndex < map.sectors.size(); ++otherSectorIndex) {
        if (static_cast<int>(otherSectorIndex) == edge.sectorIndex) {
            continue;
        }
        const SectorDefinition& other = map.sectors[otherSectorIndex];
        const size_t ringCount = 1 + other.holes.size();
        for (size_t ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            const std::vector<SectorPoint>& ring = ringIndex == 0
                    ? other.points : other.holes[ringIndex - 1];
            for (size_t otherEdgeIndex = 0; otherEdgeIndex < ring.size(); ++otherEdgeIndex) {
                const SectorPoint otherA = ring[otherEdgeIndex];
                const SectorPoint otherB = ring[(otherEdgeIndex + 1) % ring.size()];
                if (SamePoint(a, otherB) && SamePoint(b, otherA)) {
                    return EdgeNeighborInfo{
                            true,
                            static_cast<int>(otherSectorIndex),
                            static_cast<int>(otherEdgeIndex),
                            ringIndex == 0 ? SectorBoundaryRingKind::Outer : SectorBoundaryRingKind::Hole,
                            ringIndex == 0 ? -1 : static_cast<int>(ringIndex - 1)
                    };
                }
            }
        }
    }

    return EdgeNeighborInfo{};
}

bool SplitSectorEdge(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        int& outNewEdgeIndex,
        std::string& outError)
{
    outNewEdgeIndex = -1;
    outError.clear();
    const std::vector<SectorPoint>* selectedRing = GetSectorBoundaryRing(map, edge);
    if (selectedRing == nullptr || selectedRing->size() < 3) {
        outError = "Cannot split edge: invalid boundary ring";
        return false;
    }
    if (edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(selectedRing->size())) {
        outError = "Cannot split edge: invalid edge index";
        return false;
    }

    const SectorPoint a = (*selectedRing)[static_cast<size_t>(edge.edgeIndex)];
    const SectorPoint b = (*selectedRing)[(static_cast<size_t>(edge.edgeIndex) + 1) % selectedRing->size()];
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    if (dx * dx + dy * dy <= GeometryEpsilon * GeometryEpsilon) {
        outError = "Cannot split edge: edge length is effectively zero";
        return false;
    }
    const SectorPoint midpoint{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    if (SamePoint(midpoint, a) || SamePoint(midpoint, b)) {
        outError = "Cannot split edge: midpoint is too close to an endpoint";
        return false;
    }

    const EdgeNeighborInfo neighbor = FindReverseEdgeNeighbor(map, edge);
    if (neighbor.hasNeighbor) {
        const SectorBoundaryEdgeRef neighborRef{
                neighbor.sectorIndex, neighbor.ringKind, neighbor.holeIndex, neighbor.edgeIndex};
        const std::vector<SectorPoint>* neighborRing = GetSectorBoundaryRing(map, neighborRef);
        if (neighborRing == nullptr || neighbor.edgeIndex < 0
                || neighbor.edgeIndex >= static_cast<int>(neighborRing->size())) {
            outError = "Cannot split edge: invalid reverse boundary edge";
            return false;
        }
    }

    InsertSectorEdgeMidpoint(
            map.sectors[static_cast<size_t>(edge.sectorIndex)],
            edge.ringKind, edge.holeIndex, edge.edgeIndex, midpoint);
    if (neighbor.hasNeighbor) {
        InsertSectorEdgeMidpoint(
                map.sectors[static_cast<size_t>(neighbor.sectorIndex)],
                neighbor.ringKind, neighbor.holeIndex, neighbor.edgeIndex, midpoint);
    }
    outNewEdgeIndex = edge.edgeIndex + 1;
    return true;
}

bool SplitSectorEdge(
        SectorMap& map,
        int sectorIndex,
        int edgeIndex,
        int& outNewEdgeIndex,
        std::string& outError)
{
    return SplitSectorEdge(
            map,
            SectorBoundaryEdgeRef{sectorIndex, SectorBoundaryRingKind::Outer, -1, edgeIndex},
            outNewEdgeIndex,
            outError);
}

bool LoadSectorMap(const char* path, SectorMap& outMap, std::string* outError)
{
    outMap = SectorMap{};
    if (outError != nullptr) {
        outError->clear();
    }
    const auto setError = [outError](const std::string& message) {
        if (outError != nullptr) {
            *outError = message;
        }
    };

    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to open sector map: %s\n", path);
        setError(TextFormat("Failed to open level: %s", path));
        return false;
    }

    Json root;
    try {
        file >> root;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to parse sector map '%s': %s\n", path, ex.what());
        setError(TextFormat("Failed to parse level JSON: %s", ex.what()));
        return false;
    }

    try {

    const auto texturesIt = root.find("textures");
    if (texturesIt == root.end() || !texturesIt->is_object()) {
        std::fprintf(stderr, "[SectorDemo ERROR] Sector map is missing object field 'textures'\n");
        setError("Level JSON is missing the textures object");
        return false;
    }

    for (const auto& item : texturesIt->items()) {
        SectorTextureDefinition definition;
        if (ReadTextureDefinition(item.key(), item.value(), definition)) {
            outMap.texturesById.emplace(item.key(), std::move(definition));
        }
    }

    if (outMap.texturesById.empty()) {
        std::fprintf(stderr, "[SectorDemo ERROR] Sector map has no valid textures\n");
        setError("Level JSON has no valid textures");
        return false;
    }

    const std::string defaultWall = DefaultWallTextureId(outMap);
    const auto sectorsIt = root.find("sectors");
    if (sectorsIt == root.end() || !sectorsIt->is_array()) {
        std::fprintf(stderr, "[SectorDemo ERROR] Sector map is missing array field 'sectors'\n");
        setError("Level JSON is missing the sectors array");
        return false;
    }

    for (const Json& sectorJson : *sectorsIt) {
        if (!sectorJson.is_object()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed sector entry\n");
            continue;
        }

        SectorDefinition sector;
        if (!ReadStringField(sectorJson, "id", sector.id)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector without string id\n");
            continue;
        }

        const auto pointsIt = sectorJson.find("points");
        if (pointsIt == sectorJson.end() || !pointsIt->is_array()) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s' without points array\n", sector.id.c_str());
            continue;
        }

        for (const Json& pointJson : *pointsIt) {
            SectorPoint point;
            if (!ReadPoint(pointJson, point)) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring malformed point in sector '%s'\n", sector.id.c_str());
                continue;
            }
            sector.points.push_back(point);
        }

        const auto holesIt = sectorJson.find("holes");
        if (holesIt != sectorJson.end()) {
            if (!holesIt->is_array()) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': holes must be an array\n", sector.id.c_str());
                continue;
            }
            bool malformedHole = false;
            for (const Json& holeJson : *holesIt) {
                if (!holeJson.is_array()) {
                    malformedHole = true;
                    break;
                }
                std::vector<SectorPoint> hole;
                for (const Json& pointJson : holeJson) {
                    SectorPoint point;
                    if (!ReadPoint(pointJson, point)) {
                        malformedHole = true;
                        break;
                    }
                    hole.push_back(point);
                }
                if (malformedHole) {
                    break;
                }
                sector.holes.push_back(std::move(hole));
            }
            if (malformedHole) {
                std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': malformed hole ring\n", sector.id.c_str());
                continue;
            }
        }

        sector.floorZ = sectorJson.value("floor", 0.0f);
        sector.ceilingZ = sectorJson.value("ceiling", 0.0f);
        ReadStringField(sectorJson, "floorTex", sector.floorTextureId);
        ReadStringField(sectorJson, "ceilingTex", sector.ceilingTextureId);

        sector.wallTextureId = sectorJson.value("wallTex", defaultWall);
        sector.lowerWallTextureId = sectorJson.value("lowerWallTex", sector.wallTextureId);
        sector.upperWallTextureId = sectorJson.value("upperWallTex", sector.wallTextureId);
        ReadAmbientColorField(sectorJson, sector);
        ReadAmbientIntensityField(sectorJson, sector);
        ReadSurfaceUvField(sector.id.c_str(), sectorJson, "floorUvScale", sector.floorUv.uvScale, sector.floorUv.hasUvScale);
        ReadSurfaceUvField(sector.id.c_str(), sectorJson, "floorUvOffset", sector.floorUv.uvOffset, sector.floorUv.hasUvOffset);
        ReadSurfaceUvField(sector.id.c_str(), sectorJson, "ceilingUvScale", sector.ceilingUv.uvScale, sector.ceilingUv.hasUvScale);
        ReadSurfaceUvField(sector.id.c_str(), sectorJson, "ceilingUvOffset", sector.ceilingUv.uvOffset, sector.ceilingUv.hasUvOffset);
        ReadEdgeOverrides(outMap, sectorJson, sector);

        if (sector.points.size() < 3) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': at least 3 points required\n", sector.id.c_str());
            continue;
        }

        if (sector.ceilingZ <= sector.floorZ) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': ceiling must be above floor\n", sector.id.c_str());
            continue;
        }

        const std::vector<SectorPoint> originalPoints = sector.points;
        const std::vector<std::vector<SectorPoint>> originalHoles = sector.holes;
        if (!NormalizeSectorWinding(sector)) {
            continue;
        }
        RemapEdgeOverrides(
                sector,
                SectorBoundaryRingKind::Outer,
                -1,
                originalPoints,
                sector.points);
        for (size_t holeIndex = 0; holeIndex < sector.holes.size(); ++holeIndex) {
            RemapEdgeOverrides(
                    sector,
                    SectorBoundaryRingKind::Hole,
                    static_cast<int>(holeIndex),
                    originalHoles[holeIndex],
                    sector.holes[holeIndex]);
        }
        sector.edgeOverrides.erase(
                std::remove_if(sector.edgeOverrides.begin(), sector.edgeOverrides.end(), IsEmptyOverride),
                sector.edgeOverrides.end()
        );

        ValidateTextureReference(outMap, sector.id.c_str(), "floorTex", sector.floorTextureId);
        ValidateTextureReference(outMap, sector.id.c_str(), "ceilingTex", sector.ceilingTextureId);
        ValidateTextureReference(outMap, sector.id.c_str(), "wallTex", sector.wallTextureId);
        ValidateTextureReference(outMap, sector.id.c_str(), "lowerWallTex", sector.lowerWallTextureId);
        ValidateTextureReference(outMap, sector.id.c_str(), "upperWallTex", sector.upperWallTextureId);

        outMap.sectors.push_back(std::move(sector));
    }

    ReadStaticLights(root, outMap);
    ReadLightmapSettings(root, outMap);
    ReadBakedLightmapMetadata(root, outMap);

    const auto playerIt = root.find("playerStart");
    if (playerIt != root.end() && playerIt->is_object()) {
        const Json& player = *playerIt;
        const auto posIt = player.find("pos");
        if (posIt != player.end() && posIt->is_array() && posIt->size() == 2) {
            outMap.playerStartPosition.x = (*posIt)[0].get<float>();
            outMap.playerStartPosition.z = (*posIt)[1].get<float>();
        }

        outMap.playerStartPosition.y = player.value("z", SectorWorldToAuthoringDistance(1.6f));
        outMap.playerStartYawRadians = player.value("yawDegrees", 0.0f) * DegreesToRadians;
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Loaded sector map '%s': %zu sectors, %zu textures\n",
            path,
            outMap.sectors.size(),
            outMap.texturesById.size()
    );

    return true;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to read sector map '%s': %s\n", path, ex.what());
        setError(TextFormat("Failed to read level JSON: %s", ex.what()));
        outMap = SectorMap{};
        return false;
    }
}

bool SaveSectorMap(const char* path, const SectorMap& map)
{
    if (path == nullptr || path[0] == '\0') {
        std::fprintf(stderr, "[SectorDemo ERROR] Cannot save sector map without a path\n");
        return false;
    }

    Json root;
    root["textures"] = Json::object();
    for (const std::string& id : SortedSectorTextureIds(map)) {
        const SectorTextureDefinition* texture = FindSectorTexture(map, id);
        if (texture == nullptr) {
            continue;
        }

        Json textureJson;
        textureJson["path"] = texture->path;
        textureJson["filter"] = SectorTextureFilterName(texture->filter);
        root["textures"][id] = std::move(textureJson);
    }

    root["sectors"] = Json::array();
    for (const SectorDefinition& sector : map.sectors) {
        Json sectorJson;
        sectorJson["id"] = sector.id;
        sectorJson["points"] = Json::array();
        for (SectorPoint point : sector.points) {
            sectorJson["points"].push_back(Json::array({point.x, point.y}));
        }
        if (!sector.holes.empty()) {
            sectorJson["holes"] = Json::array();
            for (const std::vector<SectorPoint>& hole : sector.holes) {
                Json holeJson = Json::array();
                for (SectorPoint point : hole) {
                    holeJson.push_back(Json::array({point.x, point.y}));
                }
                sectorJson["holes"].push_back(std::move(holeJson));
            }
        }
        sectorJson["floor"] = sector.floorZ;
        sectorJson["ceiling"] = sector.ceilingZ;
        sectorJson["floorTex"] = sector.floorTextureId;
        sectorJson["ceilingTex"] = sector.ceilingTextureId;
        sectorJson["wallTex"] = sector.wallTextureId;
        sectorJson["lowerWallTex"] = sector.lowerWallTextureId;
        sectorJson["upperWallTex"] = sector.upperWallTextureId;
        sectorJson["ambientColor"] = Json::array({
                static_cast<int>(sector.ambientColor.r),
                static_cast<int>(sector.ambientColor.g),
                static_cast<int>(sector.ambientColor.b)
        });
        sectorJson["ambientIntensity"] = ClampAmbientIntensity(sector.ambientIntensity);
        WriteSurfaceUvFields(sectorJson, "floorUvScale", "floorUvOffset", sector.floorUv);
        WriteSurfaceUvFields(sectorJson, "ceilingUvScale", "ceilingUvOffset", sector.ceilingUv);
        if (!sector.edgeOverrides.empty()) {
            Json edges = Json::array();
            for (const SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
                if (IsEmptyOverride(edgeOverride)) {
                    continue;
                }
                Json edgeJson;
                edgeJson["ring"] = edgeOverride.ringKind == SectorBoundaryRingKind::Hole
                        ? "hole" : "outer";
                if (edgeOverride.ringKind == SectorBoundaryRingKind::Hole) {
                    edgeJson["hole"] = edgeOverride.holeIndex;
                }
                edgeJson["edge"] = edgeOverride.edgeIndex;
                if (edgeOverride.hasWallTexture) {
                    edgeJson["wallTex"] = edgeOverride.wallTextureId;
                }
                if (edgeOverride.hasLowerWallTexture) {
                    edgeJson["lowerWallTex"] = edgeOverride.lowerWallTextureId;
                }
                if (edgeOverride.hasUpperWallTexture) {
                    edgeJson["upperWallTex"] = edgeOverride.upperWallTextureId;
                }
                WriteEdgeUvFields(edgeJson, "wallUvScale", "wallUvOffset", edgeOverride.wallUv);
                WriteEdgeUvFields(edgeJson, "lowerUvScale", "lowerUvOffset", edgeOverride.lowerUv);
                WriteEdgeUvFields(edgeJson, "upperUvScale", "upperUvOffset", edgeOverride.upperUv);
                edges.push_back(std::move(edgeJson));
            }
            if (!edges.empty()) {
                sectorJson["edges"] = std::move(edges);
            }
        }
        root["sectors"].push_back(std::move(sectorJson));
    }

    Json player;
    player["pos"] = Json::array({map.playerStartPosition.x, map.playerStartPosition.z});
    player["z"] = map.playerStartPosition.y;
    player["yawDegrees"] = map.playerStartYawRadians * RadiansToDegrees;
    root["playerStart"] = std::move(player);

    if (!map.staticLights.empty()) {
        root["staticLights"] = Json::array();
        for (const SectorStaticPointLight& light : map.staticLights) {
            Json lightJson;
            lightJson["id"] = light.id;
            lightJson["position"] = Json::array({light.position.x, light.position.y, light.position.z});
            lightJson["color"] = Json::array({
                    static_cast<int>(light.color.r),
                    static_cast<int>(light.color.g),
                    static_cast<int>(light.color.b)
            });
            lightJson["intensity"] = ClampLightIntensity(light.intensity);
            lightJson["radius"] = ClampLightRadius(light.radius);
            lightJson["sourceRadius"] = ClampLightSourceRadius(light.sourceRadius, light.radius);
            root["staticLights"].push_back(std::move(lightJson));
        }
    }

    Json lightmapSettings;
    lightmapSettings["ambientOcclusionRadius"] = ClampAmbientOcclusionRadius(map.lightmapSettings.ambientOcclusionRadius);
    lightmapSettings["ambientOcclusionStrength"] = ClampAmbientOcclusionStrength(map.lightmapSettings.ambientOcclusionStrength);
    lightmapSettings["indirectBounceRadius"] = ClampIndirectBounceRadius(map.lightmapSettings.indirectBounceRadius);
    lightmapSettings["indirectBounceStrength"] = ClampIndirectBounceStrength(map.lightmapSettings.indirectBounceStrength);
    root["lightmapSettings"] = std::move(lightmapSettings);

    if (!map.bakedLightmap.path.empty()
            && map.bakedLightmap.width > 0
            && map.bakedLightmap.height > 0
            && !map.bakedLightmap.sourceHash.empty()) {
        Json baked;
        baked["path"] = map.bakedLightmap.path;
        baked["width"] = map.bakedLightmap.width;
        baked["height"] = map.bakedLightmap.height;
        baked["sourceHash"] = map.bakedLightmap.sourceHash;
        root["bakedLightmap"] = std::move(baked);
    }

    std::ofstream file(path);
    if (!file) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to open sector map for save: %s\n", path);
        return false;
    }

    try {
        file << root.dump(2) << '\n';
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to write sector map '%s': %s\n", path, ex.what());
        return false;
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Saved sector map '%s': %zu sectors, %zu textures\n",
            path,
            map.sectors.size(),
            map.texturesById.size()
    );
    return true;
}

} // namespace game
