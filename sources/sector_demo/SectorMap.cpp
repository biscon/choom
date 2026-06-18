#include "sector_demo/SectorMap.h"

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

bool NormalizeSectorWinding(SectorDefinition& sector)
{
    for (size_t i = 0; i < sector.points.size(); ++i) {
        const SectorPoint a = sector.points[i];
        const SectorPoint b = sector.points[(i + 1) % sector.points.size()];
        if (SamePoint(a, b)) {
            std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': duplicate adjacent points\n", sector.id.c_str());
            return false;
        }
    }

    // TODO: Replace this simple O(n^2) check if sector authoring needs richer polygon diagnostics.
    if (HasSelfIntersection(sector.points)) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': self-intersecting polygon\n", sector.id.c_str());
        return false;
    }

    const float area2 = PolygonArea2(sector.points);
    if (std::fabs(area2) <= GeometryEpsilon) {
        std::fprintf(stderr, "[SectorDemo WARNING] Ignoring sector '%s': polygon area is too small\n", sector.id.c_str());
        return false;
    }

    if (area2 < 0.0f) {
        std::reverse(sector.points.begin(), sector.points.end());
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

SectorEdgeOverride* FindEdgeOverride(SectorDefinition& sector, int edgeIndex)
{
    for (SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (edgeOverride.edgeIndex == edgeIndex) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

const SectorEdgeOverride* FindEdgeOverride(const SectorDefinition& sector, int edgeIndex)
{
    for (const SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (edgeOverride.edgeIndex == edgeIndex) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

SectorEdgeOverride& EnsureEdgeOverride(SectorDefinition& sector, int edgeIndex)
{
    if (SectorEdgeOverride* existing = FindEdgeOverride(sector, edgeIndex)) {
        return *existing;
    }

    SectorEdgeOverride edgeOverride;
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

        const int edgeIndex = edgeIt->get<int>();
        if (edgeIndex < 0 || edgeIndex >= static_cast<int>(sector.points.size())) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Ignoring out-of-range edge override %d in sector '%s'\n",
                    edgeIndex,
                    sector.id.c_str()
            );
            continue;
        }

        SectorEdgeOverride& edgeOverride = EnsureEdgeOverride(sector, edgeIndex);
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

void RemapEdgeOverridesAfterReverse(SectorDefinition& sector, const std::vector<SectorPoint>& originalPoints)
{
    for (SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        const int oldIndex = edgeOverride.edgeIndex;
        if (oldIndex < 0 || oldIndex >= static_cast<int>(originalPoints.size())) {
            continue;
        }

        const SectorPoint oldA = originalPoints[static_cast<size_t>(oldIndex)];
        const SectorPoint oldB = originalPoints[(static_cast<size_t>(oldIndex) + 1) % originalPoints.size()];
        for (size_t newIndex = 0; newIndex < sector.points.size(); ++newIndex) {
            const SectorPoint newA = sector.points[newIndex];
            const SectorPoint newB = sector.points[(newIndex + 1) % sector.points.size()];
            if ((SamePoint(oldA, newA) && SamePoint(oldB, newB))
                    || (SamePoint(oldA, newB) && SamePoint(oldB, newA))) {
                edgeOverride.edgeIndex = static_cast<int>(newIndex);
                break;
            }
        }
    }
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

    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(sector, edgeIndex);
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

EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, int sectorIndex, int edgeIndex)
{
    if (sectorIndex < 0 || sectorIndex >= static_cast<int>(map.sectors.size())) {
        return EdgeNeighborInfo{};
    }

    const SectorDefinition& sector = map.sectors[static_cast<size_t>(sectorIndex)];
    if (edgeIndex < 0 || edgeIndex >= static_cast<int>(sector.points.size())) {
        return EdgeNeighborInfo{};
    }

    const SectorPoint a = sector.points[static_cast<size_t>(edgeIndex)];
    const SectorPoint b = sector.points[(static_cast<size_t>(edgeIndex) + 1) % sector.points.size()];
    for (size_t otherSectorIndex = 0; otherSectorIndex < map.sectors.size(); ++otherSectorIndex) {
        if (static_cast<int>(otherSectorIndex) == sectorIndex) {
            continue;
        }
        const SectorDefinition& other = map.sectors[otherSectorIndex];
        for (size_t otherEdgeIndex = 0; otherEdgeIndex < other.points.size(); ++otherEdgeIndex) {
            const SectorPoint otherA = other.points[otherEdgeIndex];
            const SectorPoint otherB = other.points[(otherEdgeIndex + 1) % other.points.size()];
            if (SamePoint(a, otherB) && SamePoint(b, otherA)) {
                return EdgeNeighborInfo{true, static_cast<int>(otherSectorIndex), static_cast<int>(otherEdgeIndex)};
            }
        }
    }

    return EdgeNeighborInfo{};
}

bool LoadSectorMap(const char* path, SectorMap& outMap)
{
    outMap = SectorMap{};

    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to open sector map: %s\n", path);
        return false;
    }

    Json root;
    try {
        file >> root;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to parse sector map '%s': %s\n", path, ex.what());
        return false;
    }

    const auto texturesIt = root.find("textures");
    if (texturesIt == root.end() || !texturesIt->is_object()) {
        std::fprintf(stderr, "[SectorDemo ERROR] Sector map is missing object field 'textures'\n");
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
        return false;
    }

    const std::string defaultWall = DefaultWallTextureId(outMap);
    const auto sectorsIt = root.find("sectors");
    if (sectorsIt == root.end() || !sectorsIt->is_array()) {
        std::fprintf(stderr, "[SectorDemo ERROR] Sector map is missing array field 'sectors'\n");
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

        const bool windingWillReverse = PolygonArea2(sector.points) < 0.0f;
        const std::vector<SectorPoint> originalPoints = sector.points;
        if (!NormalizeSectorWinding(sector)) {
            continue;
        }
        if (windingWillReverse) {
            RemapEdgeOverridesAfterReverse(sector, originalPoints);
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

    const auto playerIt = root.find("playerStart");
    if (playerIt != root.end() && playerIt->is_object()) {
        const Json& player = *playerIt;
        const auto posIt = player.find("pos");
        if (posIt != player.end() && posIt->is_array() && posIt->size() == 2) {
            outMap.playerStartPosition.x = (*posIt)[0].get<float>();
            outMap.playerStartPosition.z = (*posIt)[1].get<float>();
        }

        outMap.playerStartPosition.y = player.value("z", 1.6f);
        outMap.playerStartYawRadians = player.value("yawDegrees", 0.0f) * DegreesToRadians;
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Loaded sector map '%s': %zu sectors, %zu textures\n",
            path,
            outMap.sectors.size(),
            outMap.texturesById.size()
    );

    return !outMap.sectors.empty();
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
