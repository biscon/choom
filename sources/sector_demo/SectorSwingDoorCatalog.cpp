#include "sector_demo/SectorSwingDoorCatalog.h"

#include "util/json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void Fail(const std::string& message)
{
    throw std::runtime_error(message);
}

const Json& RequireField(
        const Json& object,
        const char* field,
        const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        Fail(context + "." + field + " is required");
    }
    return *it;
}

std::string ReadString(
        const Json& object,
        const char* field,
        const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_string()) {
        Fail(context + "." + field + " must be a string");
    }
    return value.get<std::string>();
}

float ReadPositiveFloat(
        const Json& object,
        const char* field,
        const std::string& context)
{
    const Json& value = RequireField(object, field, context);
    if (!value.is_number()) {
        Fail(context + "." + field + " must be a number");
    }
    const double number = value.get<double>();
    if (!std::isfinite(number)
            || number <= 0.0
            || number > std::numeric_limits<float>::max()) {
        Fail(context + "." + field + " must be a finite positive float");
    }
    return static_cast<float>(number);
}

void RejectUnknownFields(
        const Json& object,
        const std::unordered_set<std::string>& allowed,
        const std::string& context)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (allowed.find(it.key()) == allowed.end()) {
            Fail(context + "." + it.key() + " is not supported");
        }
    }
}

bool IsValidModelPath(const std::string& path)
{
    constexpr std::string_view prefix = "assets/models/doors/swing/";
    if (path.size() <= prefix.size()
            || path.compare(0, prefix.size(), prefix) != 0
            || path.find('\\') != std::string::npos) {
        return false;
    }
    const std::filesystem::path parsed(path);
    if (parsed.is_absolute()
            || parsed.extension() != ".gltf"
            || parsed.lexically_normal().generic_string() != path) {
        return false;
    }
    for (const std::filesystem::path& segment : parsed) {
        if (segment == "." || segment == "..") {
            return false;
        }
    }
    return true;
}

bool IsFinitePositive(float value)
{
    return std::isfinite(value) && value > 0.0f;
}

} // namespace

bool ParseSectorSwingDoorCatalogJson(
        std::string_view jsonText,
        SectorSwingDoorCatalog& outCatalog,
        std::string& outError)
{
    try {
        const Json root = Json::parse(jsonText.begin(), jsonText.end());
        if (!root.is_object()) {
            Fail("swing door catalog root must be an object");
        }
        RejectUnknownFields(root, {"formatVersion", "assets"}, "swing door catalog");

        const Json& formatVersion = RequireField(
                root, "formatVersion", "swing door catalog");
        if (!formatVersion.is_number_integer()
                || formatVersion.get<int>() != 1) {
            Fail("swing door catalog.formatVersion must be 1");
        }
        const Json& assets = RequireField(root, "assets", "swing door catalog");
        if (!assets.is_array()) {
            Fail("swing door catalog.assets must be an array");
        }

        SectorSwingDoorCatalog parsed;
        parsed.formatVersion = 1;
        parsed.assets.reserve(assets.size());
        parsed.assetIndexById.reserve(assets.size());
        for (size_t i = 0; i < assets.size(); ++i) {
            const Json& value = assets[i];
            const std::string context =
                    "swing door catalog.assets[" + std::to_string(i) + "]";
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }
            RejectUnknownFields(
                    value,
                    {"id", "displayName", "leafModelPath", "frameModelPath",
                     "nominalWidth", "nominalHeight", "nominalThickness",
                     "frameOuterWidth", "frameOuterHeight", "sourcePack"},
                    context);

            SectorSwingDoorCatalogAsset asset;
            asset.id = ReadString(value, "id", context);
            asset.displayName = ReadString(value, "displayName", context);
            asset.leafModelPath = ReadString(value, "leafModelPath", context);
            asset.sourcePack = ReadString(value, "sourcePack", context);
            asset.nominalWidth = ReadPositiveFloat(value, "nominalWidth", context);
            asset.nominalHeight = ReadPositiveFloat(value, "nominalHeight", context);
            asset.nominalThickness = ReadPositiveFloat(value, "nominalThickness", context);
            if (asset.id.empty()) Fail(context + ".id must be non-empty");
            if (asset.displayName.empty()) Fail(context + ".displayName must be non-empty");
            if (asset.sourcePack.empty()) Fail(context + ".sourcePack must be non-empty");
            if (!IsValidModelPath(asset.leafModelPath)) {
                Fail(context + ".leafModelPath must be a normalized .gltf path inside assets/models/doors/swing/");
            }

            const bool hasFramePath = value.contains("frameModelPath");
            const bool hasFrameWidth = value.contains("frameOuterWidth");
            const bool hasFrameHeight = value.contains("frameOuterHeight");
            if (hasFramePath != hasFrameWidth || hasFramePath != hasFrameHeight) {
                Fail(context + " frameModelPath, frameOuterWidth, and frameOuterHeight must be provided together");
            }
            asset.hasFrame = hasFramePath;
            if (asset.hasFrame) {
                asset.frameModelPath = ReadString(value, "frameModelPath", context);
                asset.frameOuterWidth = ReadPositiveFloat(value, "frameOuterWidth", context);
                asset.frameOuterHeight = ReadPositiveFloat(value, "frameOuterHeight", context);
                if (!IsValidModelPath(asset.frameModelPath)) {
                    Fail(context + ".frameModelPath must be a normalized .gltf path inside assets/models/doors/swing/");
                }
            }

            const size_t index = parsed.assets.size();
            if (!parsed.assetIndexById.emplace(asset.id, index).second) {
                Fail("swing door catalog contains duplicate id '" + asset.id + "'");
            }
            parsed.assets.push_back(std::move(asset));
        }

        outCatalog = std::move(parsed);
        outError.clear();
        return true;
    } catch (const std::exception& exception) {
        outCatalog = SectorSwingDoorCatalog{};
        outError = exception.what();
        return false;
    }
}

bool LoadSectorSwingDoorCatalog(
        const std::string& path,
        SectorSwingDoorCatalog& outCatalog,
        std::string& outError)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outCatalog = SectorSwingDoorCatalog{};
        outError = "could not open swing door catalog: " + path;
        return false;
    }
    std::ostringstream text;
    text << input.rdbuf();
    if (!input.good() && !input.eof()) {
        outCatalog = SectorSwingDoorCatalog{};
        outError = "could not read swing door catalog: " + path;
        return false;
    }
    return ParseSectorSwingDoorCatalogJson(text.str(), outCatalog, outError);
}

bool FindSectorSwingDoorCatalogAsset(
        const SectorSwingDoorCatalog& catalog,
        std::string_view id,
        SectorSwingDoorCatalogAsset& outAsset)
{
    const auto it = catalog.assetIndexById.find(std::string{id});
    if (it == catalog.assetIndexById.end() || it->second >= catalog.assets.size()) {
        return false;
    }
    outAsset = catalog.assets[it->second];
    return true;
}

SectorSwingDoorFitResult ComputeSectorSwingDoorFit(
        const SectorSwingDoorCatalogAsset& asset,
        float targetWidth,
        float targetHeight,
        SectorDoorModelFit fit,
        float modelScale)
{
    SectorSwingDoorFitResult result;
    if (!IsFinitePositive(asset.nominalWidth)
            || !IsFinitePositive(asset.nominalHeight)
            || !IsFinitePositive(asset.nominalThickness)
            || !IsFinitePositive(targetWidth)
            || !IsFinitePositive(targetHeight)
            || !IsFinitePositive(modelScale)) {
        return result;
    }

    switch (fit) {
        case SectorDoorModelFit::Manual:
            result.effectiveScale = modelScale;
            break;
        case SectorDoorModelFit::FitWidth:
            result.effectiveScale = (targetWidth / asset.nominalWidth) * modelScale;
            break;
        case SectorDoorModelFit::FitInside:
            result.effectiveScale = std::min(
                    targetWidth / asset.nominalWidth,
                    targetHeight / asset.nominalHeight) * modelScale;
            break;
        default:
            return result;
    }

    result.actualWidth = asset.nominalWidth * result.effectiveScale;
    result.actualHeight = asset.nominalHeight * result.effectiveScale;
    result.actualThickness = asset.nominalThickness * result.effectiveScale;
    result.widthGap = targetWidth - result.actualWidth;
    result.heightGap = targetHeight - result.actualHeight;
    if (!IsFinitePositive(result.effectiveScale)
            || !IsFinitePositive(result.actualWidth)
            || !IsFinitePositive(result.actualHeight)
            || !IsFinitePositive(result.actualThickness)
            || !std::isfinite(result.widthGap)
            || !std::isfinite(result.heightGap)) {
        return SectorSwingDoorFitResult{};
    }

    constexpr float overflowEpsilon = 0.00001f;
    const bool widthOverflow = result.widthGap < -overflowEpsilon;
    const bool heightOverflow = result.heightGap < -overflowEpsilon;
    result.status = widthOverflow && heightOverflow
            ? SectorSwingDoorFitStatus::WidthAndHeightOverflow
            : widthOverflow
                    ? SectorSwingDoorFitStatus::WidthOverflow
                    : heightOverflow
                            ? SectorSwingDoorFitStatus::HeightOverflow
                            : SectorSwingDoorFitStatus::Valid;
    return result;
}

const char* SectorSwingDoorFitStatusName(SectorSwingDoorFitStatus status)
{
    switch (status) {
        case SectorSwingDoorFitStatus::Valid: return "valid";
        case SectorSwingDoorFitStatus::WidthOverflow: return "width overflow";
        case SectorSwingDoorFitStatus::HeightOverflow: return "height overflow";
        case SectorSwingDoorFitStatus::WidthAndHeightOverflow: return "width and height overflow";
        case SectorSwingDoorFitStatus::InvalidInput: return "invalid input";
    }
    return "invalid input";
}

} // namespace game
