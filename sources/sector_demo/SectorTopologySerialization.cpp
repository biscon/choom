#include "sector_demo/SectorTopologySerialization.h"

#include "util/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace game {

namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void Fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void ClearError(std::string* outError)
{
    if (outError != nullptr) {
        outError->clear();
    }
}

void SetError(std::string* outError, const std::string& message)
{
    if (outError != nullptr) {
        *outError = message;
    }
}

const Json& RequireField(const Json& object, const char* field, const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        Fail(context + " is missing required field '" + field + "'");
    }
    return *it;
}

const Json& RequireObjectField(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_object()) {
        Fail(context + "." + field + " must be an object");
    }
    return value;
}

const Json& RequireArrayField(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_array()) {
        Fail(context + "." + field + " must be an array");
    }
    return value;
}

int ReadInt(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        Fail(context + "." + field + " must be a JSON integer");
    }
    if (value.is_number_unsigned()) {
        const uint64_t number = value.get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
            Fail(context + "." + field + " is outside the supported integer range");
        }
        return static_cast<int>(number);
    }
    const int64_t number = value.get<int64_t>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        Fail(context + "." + field + " is outside the supported integer range");
    }
    return static_cast<int>(number);
}

SectorCoord ReadCoord(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        Fail(context + "." + field + " must be a JSON integer");
    }
    if (value.is_number_unsigned()) {
        const uint64_t number = value.get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<SectorCoord>::max())) {
            Fail(context + "." + field + " is outside the SectorCoord range");
        }
        return static_cast<SectorCoord>(number);
    }
    const int64_t number = value.get<int64_t>();
    if (number < std::numeric_limits<SectorCoord>::min()
            || number > std::numeric_limits<SectorCoord>::max()) {
        Fail(context + "." + field + " is outside the SectorCoord range");
    }
    return static_cast<SectorCoord>(number);
}

float ReadFloat(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_number()) {
        Fail(context + "." + field + " must be a number");
    }
    const double number = value.get<double>();
    if (!std::isfinite(number)
            || number < -std::numeric_limits<float>::max()
            || number > std::numeric_limits<float>::max()) {
        Fail(context + "." + field + " must be a finite float");
    }
    return static_cast<float>(number);
}

std::string ReadString(const Json& object, const char* field, const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_string()) {
        Fail(context + "." + field + " must be a string");
    }
    return value.get<std::string>();
}

Vector2 ReadVector2(const Json& value, const std::string& context)
{
    if (!value.is_array() || value.size() != 2
            || !value[0].is_number() || !value[1].is_number()) {
        Fail(context + " must be an array of two numbers");
    }
    const double x = value[0].get<double>();
    const double y = value[1].get<double>();
    if (!std::isfinite(x) || !std::isfinite(y)
            || std::abs(x) > std::numeric_limits<float>::max()
            || std::abs(y) > std::numeric_limits<float>::max()) {
        Fail(context + " values must be finite floats");
    }
    return Vector2{static_cast<float>(x), static_cast<float>(y)};
}

SectorTopologyUvSettings ReadUv(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorTopologyUvSettings uv;
    uv.scale = ReadVector2(RequireField(value, "scale", context), context + ".scale");
    uv.offset = ReadVector2(RequireField(value, "offset", context), context + ".offset");
    return uv;
}

SectorTopologyWallPartSettings ReadWallPart(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorTopologyWallPartSettings part;
    part.textureId = ReadString(value, "textureId", context);
    part.uv = ReadUv(RequireField(value, "uv", context), context + ".uv");
    return part;
}

unsigned char ReadColorChannel(const Json& object, const char* field, const std::string& context)
{
    const int value = ReadInt(object, field, context);
    if (value < 0 || value > 255) {
        Fail(context + "." + field + " must be between 0 and 255");
    }
    return static_cast<unsigned char>(value);
}

Color ReadColor(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    return Color{
            ReadColorChannel(value, "r", context),
            ReadColorChannel(value, "g", context),
            ReadColorChannel(value, "b", context),
            ReadColorChannel(value, "a", context)
    };
}

SectorTextureFilter ReadTextureFilter(const std::string& value, const std::string& context)
{
    if (value == "point") {
        return SectorTextureFilter::Point;
    }
    if (value == "bilinear") {
        return SectorTextureFilter::Bilinear;
    }
    Fail(context + ".filter must be 'point' or 'bilinear'");
}

const char* WriteTextureFilter(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return "point";
        case SectorTextureFilter::Bilinear:
            return "bilinear";
    }
    Fail("texture definition has an invalid filter value");
}

Json WriteVector2(Vector2 value, const std::string& context)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        Fail(context + " contains a non-finite value");
    }
    return Json::array({value.x, value.y});
}

Json WriteUv(const SectorTopologyUvSettings& uv, const std::string& context)
{
    return Json{
            {"scale", WriteVector2(uv.scale, context + ".scale")},
            {"offset", WriteVector2(uv.offset, context + ".offset")}
    };
}

Json WriteWallPart(const SectorTopologyWallPartSettings& part, const std::string& context)
{
    return Json{
            {"textureId", part.textureId},
            {"uv", WriteUv(part.uv, context + ".uv")}
    };
}

Json WriteColor(Color color)
{
    return Json{
            {"r", static_cast<int>(color.r)},
            {"g", static_cast<int>(color.g)},
            {"b", static_cast<int>(color.b)},
            {"a", static_cast<int>(color.a)}
    };
}

void RequireFinite(float value, const std::string& context)
{
    if (!std::isfinite(value)) {
        Fail(context + " must be finite");
    }
}

template<typename T>
std::vector<const T*> SortedById(const std::vector<T>& values)
{
    std::vector<const T*> sorted;
    sorted.reserve(values.size());
    for (const T& value : values) {
        sorted.push_back(&value);
    }
    std::sort(sorted.begin(), sorted.end(), [](const T* left, const T* right) {
        return left->id < right->id;
    });
    return sorted;
}

void ValidateForSerialization(const SectorTopologyMap& map)
{
    const auto issues = ValidateSectorTopologyMap(map);
    const auto error = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    if (error != issues.end()) {
        Fail("Topology validation failed: " + FormatSectorTopologyValidationIssue(*error));
    }
}

SectorTopologyMap ParseMap(const Json& root)
{
    if (!root.is_object()) {
        Fail("Topology JSON root must be an object");
    }

    if (ReadInt(root, "formatVersion", "root") != 2) {
        Fail("root.formatVersion must be 2");
    }
    if (ReadString(root, "topology", "root") != "linedef") {
        Fail("root.topology must be 'linedef'");
    }
    if (ReadInt(root, "coordSubdivisions", "root") != SectorCoordSubdivisions) {
        Fail("root.coordSubdivisions must equal " + std::to_string(SectorCoordSubdivisions));
    }

    SectorTopologyMap map;
    const Json& textures = RequireObjectField(root, "textures", "root");
    for (const auto& entry : textures.items()) {
        const std::string context = "root.textures." + entry.key();
        if (entry.key().empty()) {
            Fail("root.textures contains an empty texture ID");
        }
        if (!entry.value().is_object()) {
            Fail(context + " must be an object");
        }
        SectorTextureDefinition texture;
        texture.id = entry.key();
        texture.path = ReadString(entry.value(), "path", context);
        if (texture.path.empty()) {
            Fail(context + ".path must not be empty");
        }
        texture.filter = ReadTextureFilter(ReadString(entry.value(), "filter", context), context);
        map.texturesById.emplace(texture.id, std::move(texture));
    }

    const Json& vertices = RequireArrayField(root, "vertices", "root");
    for (size_t i = 0; i < vertices.size(); ++i) {
        const std::string context = "root.vertices[" + std::to_string(i) + "]";
        if (!vertices[i].is_object()) {
            Fail(context + " must be an object");
        }
        map.vertices.push_back(SectorTopologyVertex{
                ReadInt(vertices[i], "id", context),
                ReadCoord(vertices[i], "x", context),
                ReadCoord(vertices[i], "y", context)
        });
    }

    const Json& lineDefs = RequireArrayField(root, "linedefs", "root");
    for (size_t i = 0; i < lineDefs.size(); ++i) {
        const std::string context = "root.linedefs[" + std::to_string(i) + "]";
        if (!lineDefs[i].is_object()) {
            Fail(context + " must be an object");
        }
        map.lineDefs.push_back(SectorTopologyLineDef{
                ReadInt(lineDefs[i], "id", context),
                ReadInt(lineDefs[i], "startVertexId", context),
                ReadInt(lineDefs[i], "endVertexId", context),
                ReadInt(lineDefs[i], "frontSideDefId", context),
                ReadInt(lineDefs[i], "backSideDefId", context)
        });
    }

    const Json& sideDefs = RequireArrayField(root, "sidedefs", "root");
    for (size_t i = 0; i < sideDefs.size(); ++i) {
        const std::string context = "root.sidedefs[" + std::to_string(i) + "]";
        const Json& value = sideDefs[i];
        if (!value.is_object()) {
            Fail(context + " must be an object");
        }
        const std::string sideName = ReadString(value, "side", context);
        SectorTopologySideKind side;
        if (sideName == "front") {
            side = SectorTopologySideKind::Front;
        } else if (sideName == "back") {
            side = SectorTopologySideKind::Back;
        } else {
            Fail(context + ".side must be 'front' or 'back'");
        }
        SectorTopologySideDef sideDef;
        sideDef.id = ReadInt(value, "id", context);
        sideDef.lineDefId = ReadInt(value, "lineDefId", context);
        sideDef.side = side;
        sideDef.sectorId = ReadInt(value, "sectorId", context);
        sideDef.wall = ReadWallPart(RequireField(value, "wall", context), context + ".wall");
        sideDef.lower = ReadWallPart(RequireField(value, "lower", context), context + ".lower");
        sideDef.upper = ReadWallPart(RequireField(value, "upper", context), context + ".upper");
        map.sideDefs.push_back(std::move(sideDef));
    }

    const Json& sectors = RequireArrayField(root, "sectors", "root");
    for (size_t i = 0; i < sectors.size(); ++i) {
        const std::string context = "root.sectors[" + std::to_string(i) + "]";
        const Json& value = sectors[i];
        if (!value.is_object()) {
            Fail(context + " must be an object");
        }
        SectorTopologySector sector;
        sector.id = ReadInt(value, "id", context);
        sector.name = ReadString(value, "name", context);
        sector.floorZ = ReadFloat(value, "floorZ", context);
        sector.ceilingZ = ReadFloat(value, "ceilingZ", context);
        sector.floorTextureId = ReadString(value, "floorTextureId", context);
        sector.ceilingTextureId = ReadString(value, "ceilingTextureId", context);
        sector.floorUv = ReadUv(RequireField(value, "floorUv", context), context + ".floorUv");
        sector.ceilingUv = ReadUv(RequireField(value, "ceilingUv", context), context + ".ceilingUv");
        sector.ambientColor = ReadColor(
                RequireField(value, "ambientColor", context), context + ".ambientColor");
        sector.ambientIntensity = ReadFloat(value, "ambientIntensity", context);
        sector.defaultWall = ReadWallPart(
                RequireField(value, "defaultWall", context), context + ".defaultWall");
        sector.defaultLower = ReadWallPart(
                RequireField(value, "defaultLower", context), context + ".defaultLower");
        sector.defaultUpper = ReadWallPart(
                RequireField(value, "defaultUpper", context), context + ".defaultUpper");
        map.sectors.push_back(std::move(sector));
    }

    ValidateForSerialization(map);
    return map;
}

Json SerializeMap(const SectorTopologyMap& map)
{
    ValidateForSerialization(map);

    Json root;
    root["formatVersion"] = 2;
    root["topology"] = "linedef";
    root["coordSubdivisions"] = SectorCoordSubdivisions;
    root["textures"] = Json::object();

    std::vector<std::string> textureIds;
    textureIds.reserve(map.texturesById.size());
    for (const auto& entry : map.texturesById) {
        textureIds.push_back(entry.first);
    }
    std::sort(textureIds.begin(), textureIds.end());
    for (const std::string& id : textureIds) {
        const SectorTextureDefinition& texture = map.texturesById.at(id);
        if (id.empty() || texture.id != id || texture.path.empty()) {
            Fail("texture '" + id + "' must have a matching non-empty ID and path");
        }
        root["textures"][id] = Json{
                {"path", texture.path},
                {"filter", WriteTextureFilter(texture.filter)}
        };
    }

    root["vertices"] = Json::array();
    for (const SectorTopologyVertex* vertex : SortedById(map.vertices)) {
        root["vertices"].push_back(Json{
                {"id", vertex->id}, {"x", vertex->x}, {"y", vertex->y}
        });
    }

    root["linedefs"] = Json::array();
    for (const SectorTopologyLineDef* lineDef : SortedById(map.lineDefs)) {
        root["linedefs"].push_back(Json{
                {"id", lineDef->id},
                {"startVertexId", lineDef->startVertexId},
                {"endVertexId", lineDef->endVertexId},
                {"frontSideDefId", lineDef->frontSideDefId},
                {"backSideDefId", lineDef->backSideDefId}
        });
    }

    root["sidedefs"] = Json::array();
    for (const SectorTopologySideDef* sideDef : SortedById(map.sideDefs)) {
        std::string sideName;
        if (sideDef->side == SectorTopologySideKind::Front) {
            sideName = "front";
        } else if (sideDef->side == SectorTopologySideKind::Back) {
            sideName = "back";
        } else {
            Fail("sidedef " + std::to_string(sideDef->id) + " has an invalid side value");
        }
        const std::string context = "sidedef " + std::to_string(sideDef->id);
        root["sidedefs"].push_back(Json{
                {"id", sideDef->id},
                {"lineDefId", sideDef->lineDefId},
                {"side", sideName},
                {"sectorId", sideDef->sectorId},
                {"wall", WriteWallPart(sideDef->wall, context + ".wall")},
                {"lower", WriteWallPart(sideDef->lower, context + ".lower")},
                {"upper", WriteWallPart(sideDef->upper, context + ".upper")}
        });
    }

    root["sectors"] = Json::array();
    for (const SectorTopologySector* sector : SortedById(map.sectors)) {
        const std::string context = "sector " + std::to_string(sector->id);
        RequireFinite(sector->floorZ, context + ".floorZ");
        RequireFinite(sector->ceilingZ, context + ".ceilingZ");
        RequireFinite(sector->ambientIntensity, context + ".ambientIntensity");
        root["sectors"].push_back(Json{
                {"id", sector->id},
                {"name", sector->name},
                {"floorZ", sector->floorZ},
                {"ceilingZ", sector->ceilingZ},
                {"floorTextureId", sector->floorTextureId},
                {"ceilingTextureId", sector->ceilingTextureId},
                {"floorUv", WriteUv(sector->floorUv, context + ".floorUv")},
                {"ceilingUv", WriteUv(sector->ceilingUv, context + ".ceilingUv")},
                {"ambientColor", WriteColor(sector->ambientColor)},
                {"ambientIntensity", sector->ambientIntensity},
                {"defaultWall", WriteWallPart(sector->defaultWall, context + ".defaultWall")},
                {"defaultLower", WriteWallPart(sector->defaultLower, context + ".defaultLower")},
                {"defaultUpper", WriteWallPart(sector->defaultUpper, context + ".defaultUpper")}
        });
    }

    return root;
}

} // namespace

bool LoadSectorTopologyMapFromJsonString(
        const std::string& jsonText,
        SectorTopologyMap& outMap,
        std::string* outError)
{
    ClearError(outError);
    try {
        const Json root = Json::parse(jsonText);
        SectorTopologyMap parsed = ParseMap(root);
        outMap = std::move(parsed);
        return true;
    } catch (const std::exception& exception) {
        SetError(outError, exception.what());
        return false;
    }
}

bool SaveSectorTopologyMapToJsonString(
        const SectorTopologyMap& map,
        std::string& outJsonText,
        std::string* outError)
{
    ClearError(outError);
    try {
        std::string serialized = SerializeMap(map).dump(2);
        serialized.push_back('\n');
        outJsonText = std::move(serialized);
        return true;
    } catch (const std::exception& exception) {
        SetError(outError, exception.what());
        return false;
    }
}

bool LoadSectorTopologyMap(
        const char* path,
        SectorTopologyMap& outMap,
        std::string* outError)
{
    ClearError(outError);
    if (path == nullptr || path[0] == '\0') {
        SetError(outError, "Cannot load a topology map without a path");
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(outError, std::string("Failed to open topology map: ") + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        SetError(outError, std::string("Failed to read topology map: ") + path);
        return false;
    }
    return LoadSectorTopologyMapFromJsonString(contents.str(), outMap, outError);
}

bool SaveSectorTopologyMap(
        const char* path,
        const SectorTopologyMap& map,
        std::string* outError)
{
    ClearError(outError);
    if (path == nullptr || path[0] == '\0') {
        SetError(outError, "Cannot save a topology map without a path");
        return false;
    }

    std::string jsonText;
    if (!SaveSectorTopologyMapToJsonString(map, jsonText, outError)) {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        SetError(outError, std::string("Failed to open topology map for writing: ") + path);
        return false;
    }
    file.write(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
    if (!file) {
        SetError(outError, std::string("Failed to write topology map: ") + path);
        return false;
    }
    return true;
}

} // namespace game
