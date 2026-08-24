#include "sector_demo/SectorMaterialRegistry.h"

#include "sector_demo/SectorTopologyMap.h"
#include "util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

bool IsNormalizedAssetImagePath(std::string_view value)
{
    constexpr std::string_view Prefix = "assets/images/";
    if (value.size() <= Prefix.size()
            || value.substr(0, Prefix.size()) != Prefix
            || value.find('\\') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path path{std::string(value)};
    if (path.is_absolute()
            || path.lexically_normal().generic_string() != value) {
        return false;
    }
    for (const std::filesystem::path& segment : path) {
        if (segment == "." || segment == "..") return false;
    }
    std::string extension = path.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".png";
}

SectorMaterialFilter ParseFilter(const std::string& value)
{
    if (value == "point") return SectorMaterialFilter::Point;
    if (value == "linear") return SectorMaterialFilter::Bilinear;
    if (value == "trilinear") return SectorMaterialFilter::Trilinear;
    if (value == "anisotropic8x") return SectorMaterialFilter::Anisotropic8x;
    throw std::runtime_error("unsupported material filter '" + value + "'");
}

const char* SerializeFilter(SectorMaterialFilter filter)
{
    switch (filter) {
        case SectorMaterialFilter::Point: return "point";
        case SectorMaterialFilter::Bilinear: return "linear";
        case SectorMaterialFilter::Trilinear: return "trilinear";
        case SectorMaterialFilter::Anisotropic8x: return "anisotropic8x";
    }
    return "anisotropic8x";
}

float ReadFactor(const Json& value, const char* field, float fallback)
{
    const auto it = value.find(field);
    if (it == value.end()) return fallback;
    if (!it->is_number()) {
        throw std::runtime_error(std::string(field) + " must be a number");
    }
    const double result = it->get<double>();
    if (!std::isfinite(result) || result < 0.0 || result > 1.0) {
        throw std::runtime_error(std::string(field) + " must be between 0 and 1");
    }
    return static_cast<float>(result);
}

void AddMaterialId(std::unordered_set<std::string>& ids, const std::string& id)
{
    if (!id.empty()) ids.insert(id);
}

void AddWallPartMaterialIds(
        std::unordered_set<std::string>& ids,
        const SectorTopologyWallPartSettings& part)
{
    AddMaterialId(ids, part.materialId);
    AddMaterialId(ids, part.decal.materialId);
}

} // namespace

bool IsValidSectorMaterialId(std::string_view id)
{
    if (id.empty() || id.size() > 95) return false;
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
    });
}

bool ValidateSectorMaterialDefinition(
        const SectorMaterialDefinition& material,
        std::string& error)
{
    if (!IsValidSectorMaterialId(material.id)) {
        error = "Material ID must contain 1-95 letters, digits, underscores, or dashes";
        return false;
    }
    if (!IsNormalizedAssetImagePath(material.path)) {
        error = "Material albedo must be a normalized PNG path under assets/images";
        return false;
    }
    if (!std::isfinite(material.metallicFactor)
            || material.metallicFactor < 0.0f
            || material.metallicFactor > 1.0f
            || !std::isfinite(material.roughnessFactor)
            || material.roughnessFactor < 0.0f
            || material.roughnessFactor > 1.0f
            || !std::isfinite(material.normalStrength)
            || material.normalStrength < 0.0f
            || material.normalStrength > 1.0f) {
        error = "Material metallic, roughness, and normal strength must be between 0 and 1";
        return false;
    }
    error.clear();
    return true;
}

const SectorMaterialDefinition* FindSectorMaterial(
        const SectorMaterialRegistry& registry,
        std::string_view id)
{
    const auto it = registry.materialsById.find(std::string(id));
    return it == registry.materialsById.end() ? nullptr : &it->second;
}

std::vector<std::string> SortedSectorMaterialIds(
        const SectorMaterialRegistry& registry)
{
    std::vector<std::string> ids;
    ids.reserve(registry.materialsById.size());
    for (const auto& entry : registry.materialsById) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool ParseSectorMaterialRegistryJson(
        std::string_view jsonText,
        SectorMaterialRegistry& outRegistry,
        std::string& error)
{
    try {
        const Json root = Json::parse(jsonText);
        if (!root.is_object()
                || !root.contains("formatVersion")
                || root.at("formatVersion") != kSectorMaterialRegistryFormatVersion
                || !root.contains("materials")
                || !root.at("materials").is_object()) {
            throw std::runtime_error("Material registry must contain formatVersion 1 and a materials object");
        }
        SectorMaterialRegistry parsed;
        for (const auto& entry : root.at("materials").items()) {
            if (!entry.value().is_object()) {
                throw std::runtime_error("Material '" + entry.key() + "' must be an object");
            }
            const Json& value = entry.value();
            if (!value.contains("albedoTexturePath")
                    || !value.at("albedoTexturePath").is_string()) {
                throw std::runtime_error("Material '" + entry.key() + "' requires albedoTexturePath");
            }
            SectorMaterialDefinition material;
            material.id = entry.key();
            material.path = value.at("albedoTexturePath").get<std::string>();
            if (value.contains("filter")) {
                if (!value.at("filter").is_string()) {
                    throw std::runtime_error("Material '" + entry.key() + "' filter must be a string");
                }
                material.filter = ParseFilter(value.at("filter").get<std::string>());
            }
            material.metallicFactor = ReadFactor(value, "metallicFactor", 0.0f);
            material.roughnessFactor = ReadFactor(value, "roughnessFactor", 0.8f);
            material.normalStrength = ReadFactor(value, "normalStrength", 1.0f);
            std::string validation;
            if (!ValidateSectorMaterialDefinition(material, validation)) {
                throw std::runtime_error("Material '" + entry.key() + "': " + validation);
            }
            parsed.materialsById.emplace(material.id, std::move(material));
        }
        if (parsed.materialsById.empty()) {
            throw std::runtime_error("Material registry must contain at least one material");
        }
        outRegistry = std::move(parsed);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool SerializeSectorMaterialRegistryJson(
        const SectorMaterialRegistry& registry,
        std::string& outJson,
        std::string& error)
{
    try {
        if (registry.materialsById.empty()) {
            throw std::runtime_error("Material registry must contain at least one material");
        }
        Json root;
        root["formatVersion"] = kSectorMaterialRegistryFormatVersion;
        root["materials"] = Json::object();
        for (const std::string& id : SortedSectorMaterialIds(registry)) {
            const SectorMaterialDefinition& material = registry.materialsById.at(id);
            std::string validation;
            if (material.id != id
                    || !ValidateSectorMaterialDefinition(material, validation)) {
                throw std::runtime_error("Material '" + id + "': "
                        + (validation.empty() ? "key and ID must match" : validation));
            }
            root["materials"][id] = Json{
                    {"albedoTexturePath", material.path},
                    {"filter", SerializeFilter(material.filter)},
                    {"metallicFactor", material.metallicFactor},
                    {"roughnessFactor", material.roughnessFactor},
                    {"normalStrength", material.normalStrength}};
        }
        outJson = root.dump(2) + "\n";
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool LoadSectorMaterialRegistry(
        const std::filesystem::path& path,
        SectorMaterialRegistry& outRegistry,
        std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open material registry '" + path.generic_string() + "'";
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        error = "Could not read material registry '" + path.generic_string() + "'";
        return false;
    }
    return ParseSectorMaterialRegistryJson(contents.str(), outRegistry, error);
}

bool SaveSectorMaterialRegistry(
        const std::filesystem::path& path,
        const SectorMaterialRegistry& registry,
        std::string& error)
{
    std::string json;
    if (!SerializeSectorMaterialRegistryJson(registry, json, error)) return false;
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) {
        error = "Could not create material registry directory: " + directoryError.message();
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "Could not open material registry for writing '" + path.generic_string() + "'";
        return false;
    }
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!file) {
        error = "Could not write material registry '" + path.generic_string() + "'";
        return false;
    }
    error.clear();
    return true;
}

std::vector<std::string> ResolveSectorMaterialsForMap(
        SectorTopologyMap& map,
        const SectorMaterialRegistry& registry)
{
    std::unordered_set<std::string> referenced;
    for (const SectorTopologySideDef& side : map.sideDefs) {
        AddWallPartMaterialIds(referenced, side.wall);
        AddWallPartMaterialIds(referenced, side.lower);
        AddWallPartMaterialIds(referenced, side.upper);
        AddWallPartMaterialIds(referenced, side.middle);
    }
    bool hasSky = false;
    for (const SectorTopologySector& sector : map.sectors) {
        AddMaterialId(referenced, sector.floorMaterialId);
        AddMaterialId(referenced, sector.ceilingMaterialId);
        AddMaterialId(referenced, sector.floorDecal.materialId);
        AddMaterialId(referenced, sector.ceilingDecal.materialId);
        hasSky = hasSky || sector.ceilingSky;
    }
    if (hasSky) AddMaterialId(referenced, map.skySettings.materialId);
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "door"
                && object.door.visual == SectorDoorVisualType::Procedural) {
            AddMaterialId(referenced, object.door.materialId);
        }
    }

    map.resolvedMaterialsById.clear();
    std::vector<std::string> missing;
    for (const std::string& id : referenced) {
        const SectorMaterialDefinition* material = FindSectorMaterial(registry, id);
        if (material == nullptr) {
            missing.push_back(id);
            continue;
        }
        map.resolvedMaterialsById.emplace(id, *material);
    }
    std::sort(missing.begin(), missing.end());
    return missing;
}

} // namespace game
