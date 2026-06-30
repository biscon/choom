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

bool ReadOptionalBool(const Json& object, const char* field, const std::string& context, bool defaultValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    if (!it->is_boolean()) {
        Fail(context + "." + field + " must be a boolean");
    }
    return it->get<bool>();
}

int ReadOptionalClampedInt(
        const Json& object,
        const char* field,
        const std::string& context,
        int defaultValue,
        int minValue,
        int maxValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    if (!it->is_number_integer() && !it->is_number_unsigned()) {
        Fail(context + "." + field + " must be a JSON integer");
    }
    int value = 0;
    if (it->is_number_unsigned()) {
        const uint64_t number = it->get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
            Fail(context + "." + field + " is outside the supported integer range");
        }
        value = static_cast<int>(number);
    } else {
        const int64_t number = it->get<int64_t>();
        if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
            Fail(context + "." + field + " is outside the supported integer range");
        }
        value = static_cast<int>(number);
    }
    return std::clamp(value, minValue, maxValue);
}

float ReadOptionalClampedFloat(
        const Json& object,
        const char* field,
        const std::string& context,
        float defaultValue,
        float minValue,
        float maxValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    if (!it->is_number()) {
        Fail(context + "." + field + " must be a number");
    }
    const double number = it->get<double>();
    if (!std::isfinite(number)
            || number < -std::numeric_limits<float>::max()
            || number > std::numeric_limits<float>::max()) {
        Fail(context + "." + field + " must be a finite float");
    }
    return std::clamp(static_cast<float>(number), minValue, maxValue);
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

Vector3 ReadVector3(const Json& value, const std::string& context)
{
    if (!value.is_array() || value.size() != 3
            || !value[0].is_number() || !value[1].is_number() || !value[2].is_number()) {
        Fail(context + " must be an array of three numbers");
    }
    const double x = value[0].get<double>();
    const double y = value[1].get<double>();
    const double z = value[2].get<double>();
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
            || std::abs(x) > std::numeric_limits<float>::max()
            || std::abs(y) > std::numeric_limits<float>::max()
            || std::abs(z) > std::numeric_limits<float>::max()) {
        Fail(context + " values must be finite floats");
    }
    return Vector3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}

Vector3 ReadUnitVector3(const Json& value, const std::string& context)
{
    const Vector3 vector = ReadVector3(value, context);
    if (vector.x < 0.0f || vector.x > 1.0f
            || vector.y < 0.0f || vector.y > 1.0f
            || vector.z < 0.0f || vector.z > 1.0f) {
        Fail(context + " values must be between 0 and 1");
    }
    return vector;
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

SectorTopologyDecalLayer ReadDecal(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorTopologyDecalLayer decal;
    decal.textureId = ReadString(value, "textureId", context);
    if (decal.textureId.empty()) {
        Fail(context + ".textureId must not be empty");
    }
    decal.uv = ReadUv(RequireField(value, "uv", context), context + ".uv");
    const auto opacityIt = value.find("opacity");
    if (opacityIt != value.end()) {
        if (!opacityIt->is_number()) {
            Fail(context + ".opacity must be a number");
        }
        const double opacity = opacityIt->get<double>();
        if (!std::isfinite(opacity)
                || opacity < 0.0
                || opacity > 1.0
                || opacity > std::numeric_limits<float>::max()) {
            Fail(context + ".opacity must be a finite float between 0 and 1");
        }
        decal.opacity = static_cast<float>(opacity);
    }
    const auto emissiveIt = value.find("emissive");
    if (emissiveIt != value.end()) {
        if (!emissiveIt->is_boolean()) {
            Fail(context + ".emissive must be a boolean");
        }
        decal.emissive = emissiveIt->get<bool>();
    }
    const auto tintIt = value.find("tint");
    if (tintIt != value.end()) {
        decal.tint = ReadUnitVector3(*tintIt, context + ".tint");
    }
    const auto bloomIntensityIt = value.find("bloomIntensity");
    if (bloomIntensityIt != value.end()) {
        if (!bloomIntensityIt->is_number()) {
            Fail(context + ".bloomIntensity must be a number");
        }
        const double bloomIntensity = bloomIntensityIt->get<double>();
        if (!std::isfinite(bloomIntensity)
                || bloomIntensity < 0.0
                || bloomIntensity > 10.0
                || bloomIntensity > std::numeric_limits<float>::max()) {
            Fail(context + ".bloomIntensity must be a finite float between 0 and 10");
        }
        decal.bloomIntensity = static_cast<float>(bloomIntensity);
    }
    return decal;
}

void ReadOptionalDecal(
        const Json& object,
        const char* field,
        const std::string& context,
        SectorTopologyDecalLayer& outDecal)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        outDecal = {};
        return;
    }
    outDecal = ReadDecal(*it, context + "." + field);
}

SectorTopologyWallPartSettings ReadWallPart(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorTopologyWallPartSettings part;
    part.textureId = ReadString(value, "textureId", context);
    part.uv = ReadUv(RequireField(value, "uv", context), context + ".uv");
    ReadOptionalDecal(value, "decal", context, part.decal);
    return part;
}

SectorTopologyLineDefFlags ReadLineDefFlags(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorTopologyLineDefFlags flags;
    const auto blocksPlayerIt = value.find("blocksPlayer");
    if (blocksPlayerIt != value.end()) {
        if (!blocksPlayerIt->is_boolean()) {
            Fail(context + ".blocksPlayer must be a boolean");
        }
        flags.blocksPlayer = blocksPlayerIt->get<bool>();
    }
    return flags;
}

SectorTopologySideKind ReadSideKind(const Json& object, const char* field, const std::string& context)
{
    const std::string sideName = ReadString(object, field, context);
    if (sideName == "front") {
        return SectorTopologySideKind::Front;
    }
    if (sideName == "back") {
        return SectorTopologySideKind::Back;
    }
    Fail(context + "." + field + " must be 'front' or 'back'");
}

void ReadOptionalLineDefFlags(
        const Json& object,
        const char* field,
        const std::string& context,
        SectorTopologyLineDefFlags& outFlags)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        outFlags = {};
        return;
    }
    outFlags = ReadLineDefFlags(*it, context + "." + field);
}

void ReadOptionalWallPart(
        const Json& object,
        const char* field,
        const std::string& context,
        SectorTopologyWallPartSettings& outPart)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        outPart = {};
        return;
    }
    outPart = ReadWallPart(*it, context + "." + field);
}

SectorLightmapBakeSettings ReadLightmapSettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorLightmapBakeSettings settings;
    settings.ambientOcclusionRadius = std::clamp(
            ReadFloat(value, "ambientOcclusionRadius", context),
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f));
    settings.ambientOcclusionStrength = std::clamp(
            ReadFloat(value, "ambientOcclusionStrength", context),
            0.0f,
            1.0f);
    settings.indirectBounceRadius = std::clamp(
            ReadFloat(value, "indirectBounceRadius", context),
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f));
    settings.indirectBounceStrength = std::clamp(
            ReadFloat(value, "indirectBounceStrength", context),
            0.0f,
            1.0f);
    return settings;
}

Color ReadColor(const Json& value, const std::string& context);
unsigned char ReadOptionalColorChannel(
        const Json& object,
        const char* field,
        const std::string& context,
        unsigned char defaultValue);

SectorPreviewSettings ReadPreviewSettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPreviewSettings settings = DefaultSectorPreviewSettings();
    const auto walkSpeedIt = value.find("walkSpeed");
    if (walkSpeedIt != value.end()) {
        settings.walkSpeed = ReadFloat(value, "walkSpeed", context);
    }
    const auto runSpeedIt = value.find("runSpeed");
    if (runSpeedIt != value.end()) {
        settings.runSpeed = ReadFloat(value, "runSpeed", context);
    }
    const auto mouseSensitivityIt = value.find("mouseSensitivity");
    if (mouseSensitivityIt != value.end()) {
        settings.mouseSensitivity = ReadFloat(value, "mouseSensitivity", context);
    }
    const auto eyeHeightIt = value.find("eyeHeight");
    if (eyeHeightIt != value.end()) {
        settings.eyeHeight = ReadFloat(value, "eyeHeight", context);
    }
    const auto gravityIt = value.find("gravity");
    if (gravityIt != value.end()) {
        settings.gravity = ReadFloat(value, "gravity", context);
    }
    const auto playerRadiusIt = value.find("playerRadius");
    if (playerRadiusIt != value.end()) {
        settings.playerRadius = ReadFloat(value, "playerRadius", context);
    }
    const auto playerHeightIt = value.find("playerHeight");
    if (playerHeightIt != value.end()) {
        settings.playerHeight = ReadFloat(value, "playerHeight", context);
    }
    const auto stepHeightIt = value.find("stepHeight");
    if (stepHeightIt != value.end()) {
        settings.stepHeight = ReadFloat(value, "stepHeight", context);
    }
    const auto jumpHeightIt = value.find("jumpHeight");
    if (jumpHeightIt != value.end()) {
        settings.jumpHeight = ReadFloat(value, "jumpHeight", context);
    }
    const auto headBobStrengthIt = value.find("headBobStrength");
    if (headBobStrengthIt != value.end()) {
        settings.headBobStrength = ReadFloat(value, "headBobStrength", context);
    }
    const auto headBobFrequencyIt = value.find("headBobFrequency");
    if (headBobFrequencyIt != value.end()) {
        settings.headBobFrequency = ReadFloat(value, "headBobFrequency", context);
    }
    return NormalizeSectorPreviewSettings(settings);
}

SectorTopologySkySettings ReadSkySettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorTopologySkySettings settings = DefaultSectorTopologySkySettings();
    const auto textureIdIt = value.find("textureId");
    if (textureIdIt != value.end()) {
        settings.textureId = ReadString(value, "textureId", context);
    }
    const auto yawIt = value.find("yawOffsetDegrees");
    if (yawIt != value.end()) {
        settings.yawOffsetDegrees = ReadFloat(value, "yawOffsetDegrees", context);
    }
    const auto verticalOffsetIt = value.find("verticalOffset");
    if (verticalOffsetIt != value.end()) {
        settings.verticalOffset = ReadFloat(value, "verticalOffset", context);
    }
    const auto verticalScaleIt = value.find("verticalScale");
    if (verticalScaleIt != value.end()) {
        settings.verticalScale = ReadFloat(value, "verticalScale", context);
    }
    const auto topColorIt = value.find("topColor");
    if (topColorIt != value.end()) {
        settings.topColor = ReadColor(*topColorIt, context + ".topColor");
    }
    return NormalizeSectorTopologySkySettings(settings);
}

SectorTopologyDirectionalLightSettings ReadDirectionalLightSettings(
        const Json& value,
        const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorTopologyDirectionalLightSettings settings = DefaultSectorTopologyDirectionalLightSettings();
    const auto enabledIt = value.find("enabled");
    if (enabledIt != value.end()) {
        if (!enabledIt->is_boolean()) {
            Fail(context + ".enabled must be a boolean");
        }
        settings.enabled = enabledIt->get<bool>();
    }
    const auto directionIt = value.find("directionToLight");
    if (directionIt != value.end()) {
        settings.directionToLight = ReadVector3(*directionIt, context + ".directionToLight");
    }
    const auto colorIt = value.find("color");
    if (colorIt != value.end()) {
        if (!colorIt->is_object()) {
            Fail(context + ".color must be an object");
        }
        const Color defaults = DefaultSectorTopologyDirectionalLightSettings().color;
        settings.color = Color{
                ReadOptionalColorChannel(*colorIt, "r", context + ".color", defaults.r),
                ReadOptionalColorChannel(*colorIt, "g", context + ".color", defaults.g),
                ReadOptionalColorChannel(*colorIt, "b", context + ".color", defaults.b),
                ReadOptionalColorChannel(*colorIt, "a", context + ".color", defaults.a)
        };
    }
    const auto intensityIt = value.find("intensity");
    if (intensityIt != value.end()) {
        settings.intensity = ReadFloat(value, "intensity", context);
    }
    return NormalizeSectorTopologyDirectionalLightSettings(settings);
}

SectorLightmapMetadata ReadBakedLightmap(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorLightmapMetadata metadata;
    metadata.path = ReadString(value, "path", context);
    metadata.width = ReadInt(value, "width", context);
    metadata.height = ReadInt(value, "height", context);
    metadata.sourceHash = ReadString(value, "sourceHash", context);
    if (metadata.path.empty()) {
        Fail(context + ".path must not be empty");
    }
    if (metadata.width <= 0 || metadata.height <= 0) {
        Fail(context + " dimensions must be positive");
    }
    if (metadata.sourceHash.empty()) {
        Fail(context + ".sourceHash must not be empty");
    }
    return metadata;
}

unsigned char ReadColorChannel(const Json& object, const char* field, const std::string& context)
{
    const int value = ReadInt(object, field, context);
    if (value < 0 || value > 255) {
        Fail(context + "." + field + " must be between 0 and 255");
    }
    return static_cast<unsigned char>(value);
}

unsigned char ReadOptionalColorChannel(
        const Json& object,
        const char* field,
        const std::string& context,
        unsigned char defaultValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
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
    if (value == "linear") {
        return SectorTextureFilter::Bilinear;
    }
    if (value == "trilinear") {
        return SectorTextureFilter::Trilinear;
    }
    if (value == "anisotropic8x" || value == "bilinear") {
        return SectorTextureFilter::Anisotropic8x;
    }
    Fail(context + ".filter must be 'point', 'linear', 'trilinear', or 'anisotropic8x'");
}

const char* WriteTextureFilter(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return "point";
        case SectorTextureFilter::Bilinear:
            return "linear";
        case SectorTextureFilter::Trilinear:
            return "trilinear";
        case SectorTextureFilter::Anisotropic8x:
            return "anisotropic8x";
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

Json WriteVector3(Vector3 value, const std::string& context)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        Fail(context + " contains a non-finite value");
    }
    return Json::array({value.x, value.y, value.z});
}

Json WriteUv(const SectorTopologyUvSettings& uv, const std::string& context)
{
    return Json{
            {"scale", WriteVector2(uv.scale, context + ".scale")},
            {"offset", WriteVector2(uv.offset, context + ".offset")}
    };
}

bool HasDecal(const SectorTopologyDecalLayer& decal)
{
    return !decal.textureId.empty();
}

bool IsDefaultUv(const SectorTopologyUvSettings& uv)
{
    return uv.scale.x == 1.0f
            && uv.scale.y == 1.0f
            && uv.offset.x == 0.0f
            && uv.offset.y == 0.0f;
}

bool HasNonDefaultWallPart(const SectorTopologyWallPartSettings& part)
{
    return !part.textureId.empty()
            || !IsDefaultUv(part.uv)
            || HasDecal(part.decal);
}

bool HasNonDefaultLineDefFlags(const SectorTopologyLineDefFlags& flags)
{
    return flags.blocksPlayer;
}

Json WriteLineDefFlags(const SectorTopologyLineDefFlags& flags)
{
    Json value = Json::object();
    if (flags.blocksPlayer) {
        value["blocksPlayer"] = true;
    }
    return value;
}

const char* WriteSideKind(SectorTopologySideKind side, const std::string& context)
{
    switch (side) {
        case SectorTopologySideKind::Front:
            return "front";
        case SectorTopologySideKind::Back:
            return "back";
    }
    Fail(context + " has an invalid side value");
}

Json WriteDecal(const SectorTopologyDecalLayer& decal, const std::string& context)
{
    if (!std::isfinite(decal.opacity)
            || decal.opacity < 0.0f
            || decal.opacity > 1.0f) {
        Fail(context + ".opacity must be a finite float between 0 and 1");
    }
    if (!std::isfinite(decal.tint.x) || !std::isfinite(decal.tint.y) || !std::isfinite(decal.tint.z)
            || decal.tint.x < 0.0f || decal.tint.x > 1.0f
            || decal.tint.y < 0.0f || decal.tint.y > 1.0f
            || decal.tint.z < 0.0f || decal.tint.z > 1.0f) {
        Fail(context + ".tint must contain finite values between 0 and 1");
    }
    if (!std::isfinite(decal.bloomIntensity)
            || decal.bloomIntensity < 0.0f
            || decal.bloomIntensity > 10.0f) {
        Fail(context + ".bloomIntensity must be a finite float between 0 and 10");
    }
    return Json{
            {"textureId", decal.textureId},
            {"uv", WriteUv(decal.uv, context + ".uv")},
            {"opacity", decal.opacity},
            {"emissive", decal.emissive},
            {"tint", WriteVector3(decal.tint, context + ".tint")},
            {"bloomIntensity", decal.bloomIntensity}
    };
}

Json WriteWallPart(const SectorTopologyWallPartSettings& part, const std::string& context)
{
    Json value{
            {"textureId", part.textureId},
            {"uv", WriteUv(part.uv, context + ".uv")}
    };
    if (HasDecal(part.decal)) {
        value["decal"] = WriteDecal(part.decal, context + ".decal");
    }
    return value;
}

void RequireFinite(float value, const std::string& context)
{
    if (!std::isfinite(value)) {
        Fail(context + " must be finite");
    }
}

Json WriteLightmapSettings(const SectorLightmapBakeSettings& settings)
{
    return Json{
            {"ambientOcclusionRadius", std::clamp(
                    settings.ambientOcclusionRadius,
                    SectorWorldToAuthoringDistance(0.05f),
                    SectorWorldToAuthoringDistance(16.0f))},
            {"ambientOcclusionStrength", std::clamp(settings.ambientOcclusionStrength, 0.0f, 1.0f)},
            {"indirectBounceRadius", std::clamp(
                    settings.indirectBounceRadius,
                    SectorWorldToAuthoringDistance(0.05f),
                    SectorWorldToAuthoringDistance(16.0f))},
            {"indirectBounceStrength", std::clamp(settings.indirectBounceStrength, 0.0f, 1.0f)}
    };
}

Json WritePreviewSettings(const SectorPreviewSettings& settings)
{
    RequireFinite(settings.walkSpeed, "previewSettings.walkSpeed");
    RequireFinite(settings.runSpeed, "previewSettings.runSpeed");
    RequireFinite(settings.mouseSensitivity, "previewSettings.mouseSensitivity");
    RequireFinite(settings.eyeHeight, "previewSettings.eyeHeight");
    RequireFinite(settings.gravity, "previewSettings.gravity");
    RequireFinite(settings.playerRadius, "previewSettings.playerRadius");
    RequireFinite(settings.playerHeight, "previewSettings.playerHeight");
    RequireFinite(settings.stepHeight, "previewSettings.stepHeight");
    RequireFinite(settings.jumpHeight, "previewSettings.jumpHeight");
    RequireFinite(settings.headBobStrength, "previewSettings.headBobStrength");
    RequireFinite(settings.headBobFrequency, "previewSettings.headBobFrequency");
    const SectorPreviewSettings normalized = NormalizeSectorPreviewSettings(settings);
    return Json{
            {"walkSpeed", normalized.walkSpeed},
            {"runSpeed", normalized.runSpeed},
            {"mouseSensitivity", normalized.mouseSensitivity},
            {"eyeHeight", normalized.eyeHeight},
            {"gravity", normalized.gravity},
            {"playerRadius", normalized.playerRadius},
            {"playerHeight", normalized.playerHeight},
            {"stepHeight", normalized.stepHeight},
            {"jumpHeight", normalized.jumpHeight},
            {"headBobStrength", normalized.headBobStrength},
            {"headBobFrequency", normalized.headBobFrequency}
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

template<typename T>
void WriteDynamicLightFlickerFields(Json& lightJson, const T& light, const std::string& context)
{
    RequireFinite(light.flickerSpeed, context + ".flickerSpeed");
    RequireFinite(light.flickerAmount, context + ".flickerAmount");
    const float flickerSpeed = ClampDynamicLightFlickerSpeed(light.flickerSpeed);
    const float flickerAmount = ClampDynamicLightFlickerAmount(light.flickerAmount);
    if (light.flicker) {
        lightJson["flicker"] = true;
    }
    if (flickerSpeed != DynamicLightFlickerDefaultSpeed) {
        lightJson["flickerSpeed"] = flickerSpeed;
    }
    if (flickerAmount != DynamicLightFlickerDefaultAmount) {
        lightJson["flickerAmount"] = flickerAmount;
    }
}

float ClampDynamicSpotLightConeDegrees(float value)
{
    return std::clamp(value, 0.0f, 179.0f);
}

Json WriteDynamicSpotLight(const SectorTopologyDynamicSpotLight& light, const std::string& context)
{
    RequireFinite(light.intensity, context + ".intensity");
    RequireFinite(light.range, context + ".range");
    RequireFinite(light.innerConeDegrees, context + ".innerConeDegrees");
    RequireFinite(light.outerConeDegrees, context + ".outerConeDegrees");
    RequireFinite(light.shadowBias, context + ".shadowBias");
    RequireFinite(light.shadowStrength, context + ".shadowStrength");
    RequireFinite(light.shadowSoftness, context + ".shadowSoftness");
    const float innerConeDegrees = ClampDynamicSpotLightConeDegrees(light.innerConeDegrees);
    const float outerConeDegrees = std::max(
            ClampDynamicSpotLightConeDegrees(light.outerConeDegrees),
            innerConeDegrees);
    const int shadowPriority = ClampDynamicSpotLightShadowPriority(light.shadowPriority);
    const float shadowBias = ClampDynamicSpotLightShadowBias(light.shadowBias);
    const float shadowStrength = ClampDynamicSpotLightShadowStrength(light.shadowStrength);
    const float shadowSoftness = ClampDynamicSpotLightShadowSoftness(light.shadowSoftness);
    Json lightJson{
            {"id", light.id},
            {"position", WriteVector3(light.position, context + ".position")},
            {"target", WriteVector3(light.target, context + ".target")},
            {"range", light.range},
            {"intensity", light.intensity},
            {"color", WriteColor(light.color)}
    };
    if (innerConeDegrees != 20.0f) {
        lightJson["innerConeDegrees"] = innerConeDegrees;
    }
    if (outerConeDegrees != 35.0f) {
        lightJson["outerConeDegrees"] = outerConeDegrees;
    }
    if (!light.enabled) {
        lightJson["enabled"] = false;
    }
    WriteDynamicLightFlickerFields(lightJson, light, context);
    if (light.castsShadow) {
        lightJson["castsShadow"] = true;
    }
    if (shadowPriority != DynamicSpotLightDefaultShadowPriority) {
        lightJson["shadowPriority"] = shadowPriority;
    }
    if (shadowBias != DynamicSpotLightDefaultShadowBias) {
        lightJson["shadowBias"] = shadowBias;
    }
    if (shadowStrength != DynamicSpotLightDefaultShadowStrength) {
        lightJson["shadowStrength"] = shadowStrength;
    }
    if (shadowSoftness != DynamicSpotLightDefaultShadowSoftness) {
        lightJson["shadowSoftness"] = shadowSoftness;
    }
    return lightJson;
}

Json WriteStaticSpotLight(const SectorTopologyStaticSpotLight& light, const std::string& context)
{
    RequireFinite(light.intensity, context + ".intensity");
    RequireFinite(light.range, context + ".range");
    RequireFinite(light.innerConeDegrees, context + ".innerConeDegrees");
    RequireFinite(light.outerConeDegrees, context + ".outerConeDegrees");
    RequireFinite(light.sourceRadius, context + ".sourceRadius");
    const float innerConeDegrees = ClampDynamicSpotLightConeDegrees(light.innerConeDegrees);
    const float outerConeDegrees = std::max(
            ClampDynamicSpotLightConeDegrees(light.outerConeDegrees),
            innerConeDegrees);
    Json lightJson{
            {"id", light.id},
            {"position", WriteVector3(light.position, context + ".position")},
            {"target", WriteVector3(light.target, context + ".target")},
            {"range", light.range},
            {"sourceRadius", light.sourceRadius},
            {"intensity", light.intensity},
            {"color", WriteColor(light.color)}
    };
    if (innerConeDegrees != 20.0f) {
        lightJson["innerConeDegrees"] = innerConeDegrees;
    }
    if (outerConeDegrees != 35.0f) {
        lightJson["outerConeDegrees"] = outerConeDegrees;
    }
    return lightJson;
}

Json WriteSkySettings(const SectorTopologySkySettings& settings)
{
    const SectorTopologySkySettings normalized = NormalizeSectorTopologySkySettings(settings);
    return Json{
            {"textureId", normalized.textureId},
            {"yawOffsetDegrees", normalized.yawOffsetDegrees},
            {"verticalOffset", normalized.verticalOffset},
            {"verticalScale", normalized.verticalScale},
            {"topColor", WriteColor(normalized.topColor)}
    };
}

bool IsDefaultSkySettings(const SectorTopologySkySettings& settings)
{
    const SectorTopologySkySettings normalized = NormalizeSectorTopologySkySettings(settings);
    const SectorTopologySkySettings defaults = DefaultSectorTopologySkySettings();
    return normalized.textureId == defaults.textureId
            && normalized.yawOffsetDegrees == defaults.yawOffsetDegrees
            && normalized.verticalOffset == defaults.verticalOffset
            && normalized.verticalScale == defaults.verticalScale
            && normalized.topColor.r == defaults.topColor.r
            && normalized.topColor.g == defaults.topColor.g
            && normalized.topColor.b == defaults.topColor.b
            && normalized.topColor.a == defaults.topColor.a;
}

Json WriteDirectionalLightSettings(const SectorTopologyDirectionalLightSettings& settings)
{
    const SectorTopologyDirectionalLightSettings normalized =
            NormalizeSectorTopologyDirectionalLightSettings(settings);
    return Json{
            {"enabled", normalized.enabled},
            {"directionToLight", WriteVector3(normalized.directionToLight, "directionalLight.directionToLight")},
            {"color", WriteColor(normalized.color)},
            {"intensity", normalized.intensity}
    };
}

bool IsDefaultDirectionalLightSettings(const SectorTopologyDirectionalLightSettings& settings)
{
    const SectorTopologyDirectionalLightSettings normalized =
            NormalizeSectorTopologyDirectionalLightSettings(settings);
    const SectorTopologyDirectionalLightSettings defaults =
            DefaultSectorTopologyDirectionalLightSettings();
    return normalized.enabled == defaults.enabled
            && normalized.directionToLight.x == defaults.directionToLight.x
            && normalized.directionToLight.y == defaults.directionToLight.y
            && normalized.directionToLight.z == defaults.directionToLight.z
            && normalized.color.r == defaults.color.r
            && normalized.color.g == defaults.color.g
            && normalized.color.b == defaults.color.b
            && normalized.color.a == defaults.color.a
            && normalized.intensity == defaults.intensity;
}

Json WriteBakedLightmap(const SectorLightmapMetadata& metadata)
{
    return Json{
            {"path", metadata.path},
            {"width", metadata.width},
            {"height", metadata.height},
            {"sourceHash", metadata.sourceHash}
    };
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

void ValidateAuthoringMapData(const SectorTopologyMap& map)
{
    const auto issues = ValidateSectorTopologyMap(map);
    const auto error = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    if (error != issues.end()) {
        Fail("Authoring map-level validation failed: " + FormatSectorTopologyValidationIssue(*error));
    }
}

void ReadTextures(const Json& root, SectorTopologyMap& map)
{
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
}

void ReadMapLevelFields(const Json& root, SectorTopologyMap& map, bool allowBakedLightmap)
{
    const auto staticLightsIt = root.find("staticLights");
    if (staticLightsIt != root.end()) {
        if (!staticLightsIt->is_array()) {
            Fail("root.staticLights must be an array");
        }
        const Json& staticLights = *staticLightsIt;
        for (size_t i = 0; i < staticLights.size(); ++i) {
            const std::string context = "root.staticLights[" + std::to_string(i) + "]";
            const Json& value = staticLights[i];
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }

            SectorTopologyStaticPointLight light;
            light.id = ReadInt(value, "id", context);
            light.position = ReadVector3(RequireField(value, "position", context), context + ".position");
            light.radius = ReadFloat(value, "radius", context);
            light.sourceRadius = ReadFloat(value, "sourceRadius", context);
            light.intensity = ReadFloat(value, "intensity", context);
            light.color = ReadColor(RequireField(value, "color", context), context + ".color");
            map.staticLights.push_back(light);
        }
    }

    const auto staticSpotLightsIt = root.find("staticSpotLights");
    if (staticSpotLightsIt != root.end()) {
        if (!staticSpotLightsIt->is_array()) {
            Fail("root.staticSpotLights must be an array");
        }
        const Json& staticSpotLights = *staticSpotLightsIt;
        for (size_t i = 0; i < staticSpotLights.size(); ++i) {
            const std::string context = "root.staticSpotLights[" + std::to_string(i) + "]";
            const Json& value = staticSpotLights[i];
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }

            SectorTopologyStaticSpotLight light;
            light.id = ReadInt(value, "id", context);
            light.position = ReadVector3(RequireField(value, "position", context), context + ".position");
            light.target = ReadVector3(RequireField(value, "target", context), context + ".target");
            light.range = ReadFloat(value, "range", context);
            light.sourceRadius = ReadFloat(value, "sourceRadius", context);
            light.intensity = ReadFloat(value, "intensity", context);
            light.color = ReadColor(RequireField(value, "color", context), context + ".color");
            light.innerConeDegrees = ReadOptionalClampedFloat(
                    value,
                    "innerConeDegrees",
                    context,
                    20.0f,
                    0.0f,
                    179.0f);
            light.outerConeDegrees = ReadOptionalClampedFloat(
                    value,
                    "outerConeDegrees",
                    context,
                    35.0f,
                    0.0f,
                    179.0f);
            light.outerConeDegrees = std::max(light.outerConeDegrees, light.innerConeDegrees);
            map.staticSpotLights.push_back(light);
        }
    }

    const auto dynamicLightsIt = root.find("dynamicPointLights");
    if (dynamicLightsIt != root.end()) {
        if (!dynamicLightsIt->is_array()) {
            Fail("root.dynamicPointLights must be an array");
        }
        const Json& dynamicLights = *dynamicLightsIt;
        for (size_t i = 0; i < dynamicLights.size(); ++i) {
            const std::string context = "root.dynamicPointLights[" + std::to_string(i) + "]";
            const Json& value = dynamicLights[i];
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }

            SectorTopologyDynamicPointLight light;
            light.id = ReadInt(value, "id", context);
            light.position = ReadVector3(RequireField(value, "position", context), context + ".position");
            light.radius = ReadFloat(value, "radius", context);
            light.intensity = ReadFloat(value, "intensity", context);
            light.color = ReadColor(RequireField(value, "color", context), context + ".color");
            light.enabled = ReadOptionalBool(value, "enabled", context, true);
            light.flicker = ReadOptionalBool(value, "flicker", context, false);
            light.flickerSpeed = ReadOptionalClampedFloat(
                    value,
                    "flickerSpeed",
                    context,
                    DynamicLightFlickerDefaultSpeed,
                    DynamicLightFlickerMinSpeed,
                    DynamicLightFlickerMaxSpeed);
            light.flickerAmount = ReadOptionalClampedFloat(
                    value,
                    "flickerAmount",
                    context,
                    DynamicLightFlickerDefaultAmount,
                    DynamicLightFlickerMinAmount,
                    DynamicLightFlickerMaxAmount);
            map.dynamicPointLights.push_back(light);
        }
    }

    const auto dynamicSpotLightsIt = root.find("dynamicSpotLights");
    if (dynamicSpotLightsIt != root.end()) {
        if (!dynamicSpotLightsIt->is_array()) {
            Fail("root.dynamicSpotLights must be an array");
        }
        const Json& dynamicSpotLights = *dynamicSpotLightsIt;
        for (size_t i = 0; i < dynamicSpotLights.size(); ++i) {
            const std::string context = "root.dynamicSpotLights[" + std::to_string(i) + "]";
            const Json& value = dynamicSpotLights[i];
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }

            SectorTopologyDynamicSpotLight light;
            light.id = ReadInt(value, "id", context);
            light.position = ReadVector3(RequireField(value, "position", context), context + ".position");
            light.target = ReadVector3(RequireField(value, "target", context), context + ".target");
            light.range = ReadFloat(value, "range", context);
            light.intensity = ReadFloat(value, "intensity", context);
            light.color = ReadColor(RequireField(value, "color", context), context + ".color");
            light.innerConeDegrees = ReadOptionalClampedFloat(
                    value,
                    "innerConeDegrees",
                    context,
                    20.0f,
                    0.0f,
                    179.0f);
            light.outerConeDegrees = ReadOptionalClampedFloat(
                    value,
                    "outerConeDegrees",
                    context,
                    35.0f,
                    0.0f,
                    179.0f);
            light.outerConeDegrees = std::max(light.outerConeDegrees, light.innerConeDegrees);
            light.enabled = ReadOptionalBool(value, "enabled", context, true);
            light.flicker = ReadOptionalBool(value, "flicker", context, false);
            light.flickerSpeed = ReadOptionalClampedFloat(
                    value,
                    "flickerSpeed",
                    context,
                    DynamicLightFlickerDefaultSpeed,
                    DynamicLightFlickerMinSpeed,
                    DynamicLightFlickerMaxSpeed);
            light.flickerAmount = ReadOptionalClampedFloat(
                    value,
                    "flickerAmount",
                    context,
                    DynamicLightFlickerDefaultAmount,
                    DynamicLightFlickerMinAmount,
                    DynamicLightFlickerMaxAmount);
            light.castsShadow = ReadOptionalBool(value, "castsShadow", context, false);
            light.shadowPriority = ReadOptionalClampedInt(
                    value,
                    "shadowPriority",
                    context,
                    DynamicSpotLightDefaultShadowPriority,
                    DynamicSpotLightMinShadowPriority,
                    DynamicSpotLightMaxShadowPriority);
            light.shadowBias = ReadOptionalClampedFloat(
                    value,
                    "shadowBias",
                    context,
                    DynamicSpotLightDefaultShadowBias,
                    DynamicSpotLightMinShadowBias,
                    DynamicSpotLightMaxShadowBias);
            light.shadowStrength = ReadOptionalClampedFloat(
                    value,
                    "shadowStrength",
                    context,
                    DynamicSpotLightDefaultShadowStrength,
                    DynamicSpotLightMinShadowStrength,
                    DynamicSpotLightMaxShadowStrength);
            light.shadowSoftness = ReadOptionalClampedFloat(
                    value,
                    "shadowSoftness",
                    context,
                    DynamicSpotLightDefaultShadowSoftness,
                    DynamicSpotLightMinShadowSoftness,
                    DynamicSpotLightMaxShadowSoftness);
            map.dynamicSpotLights.push_back(light);
        }
    }

    const auto lightmapSettingsIt = root.find("lightmapSettings");
    if (lightmapSettingsIt != root.end()) {
        map.lightmapSettings = ReadLightmapSettings(*lightmapSettingsIt, "root.lightmapSettings");
    }

    const auto previewSettingsIt = root.find("previewSettings");
    if (previewSettingsIt != root.end()) {
        map.previewSettings = ReadPreviewSettings(*previewSettingsIt, "root.previewSettings");
    }

    const auto skySettingsIt = root.find("skySettings");
    if (skySettingsIt != root.end()) {
        map.skySettings = ReadSkySettings(*skySettingsIt, "root.skySettings");
    }

    const auto directionalLightIt = root.find("directionalLight");
    if (directionalLightIt != root.end()) {
        map.directionalLight = ReadDirectionalLightSettings(*directionalLightIt, "root.directionalLight");
    }

    const auto bakedLightmapIt = root.find("bakedLightmap");
    if (bakedLightmapIt != root.end() && allowBakedLightmap) {
        map.bakedLightmap = ReadBakedLightmap(*bakedLightmapIt, "root.bakedLightmap");
    }

    map.previewSettings = NormalizeSectorPreviewSettings(map.previewSettings);
    map.skySettings = NormalizeSectorTopologySkySettings(map.skySettings);
    map.directionalLight = NormalizeSectorTopologyDirectionalLightSettings(map.directionalLight);
}

SectorAuthoringGraph ReadAuthoringGraph(const Json& value)
{
    if (!value.is_object()) {
        Fail("root.authoringGraph must be an object");
    }

    SectorAuthoringGraph graph;
    const Json& vertices = RequireArrayField(value, "vertices", "root.authoringGraph");
    for (size_t i = 0; i < vertices.size(); ++i) {
        const std::string context = "root.authoringGraph.vertices[" + std::to_string(i) + "]";
        if (!vertices[i].is_object()) {
            Fail(context + " must be an object");
        }
        graph.vertices.push_back(SectorAuthoringVertex{
                ReadInt(vertices[i], "id", context),
                ReadCoord(vertices[i], "x", context),
                ReadCoord(vertices[i], "y", context)
        });
    }

    const Json& lines = RequireArrayField(value, "lines", "root.authoringGraph");
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string context = "root.authoringGraph.lines[" + std::to_string(i) + "]";
        if (!lines[i].is_object()) {
            Fail(context + " must be an object");
        }
        SectorAuthoringLine line;
        line.id = ReadInt(lines[i], "id", context);
        line.startVertexId = ReadInt(lines[i], "startVertexId", context);
        line.endVertexId = ReadInt(lines[i], "endVertexId", context);
        ReadOptionalLineDefFlags(lines[i], "flags", context, line.flags);
        const auto specialIt = lines[i].find("special");
        if (specialIt != lines[i].end()) {
            if (!specialIt->is_object()) {
                Fail(context + ".special must be an object");
            }
            line.special.type = ReadInt(*specialIt, "type", context + ".special");
            line.special.tag = ReadString(*specialIt, "tag", context + ".special");
        }
        graph.lines.push_back(std::move(line));
    }

    const Json& lineSides = RequireArrayField(value, "lineSides", "root.authoringGraph");
    for (size_t i = 0; i < lineSides.size(); ++i) {
        const std::string context = "root.authoringGraph.lineSides[" + std::to_string(i) + "]";
        if (!lineSides[i].is_object()) {
            Fail(context + " must be an object");
        }
        SectorAuthoringLineSide side;
        side.id.lineId = ReadInt(lineSides[i], "lineId", context);
        side.id.side = ReadSideKind(lineSides[i], "side", context);
        side.wall = ReadWallPart(RequireField(lineSides[i], "wall", context), context + ".wall");
        side.lower = ReadWallPart(RequireField(lineSides[i], "lower", context), context + ".lower");
        side.upper = ReadWallPart(RequireField(lineSides[i], "upper", context), context + ".upper");
        ReadOptionalWallPart(lineSides[i], "middle", context, side.middle);
        graph.lineSides.push_back(std::move(side));
    }

    const Json& faceAnchors = RequireArrayField(value, "faceAnchors", "root.authoringGraph");
    for (size_t i = 0; i < faceAnchors.size(); ++i) {
        const std::string context = "root.authoringGraph.faceAnchors[" + std::to_string(i) + "]";
        if (!faceAnchors[i].is_object()) {
            Fail(context + " must be an object");
        }
        SectorAuthoringFaceAnchor anchor;
        anchor.id = ReadInt(faceAnchors[i], "id", context);
        anchor.name = ReadString(faceAnchors[i], "name", context);
        anchor.x = ReadCoord(faceAnchors[i], "x", context);
        anchor.y = ReadCoord(faceAnchors[i], "y", context);
        anchor.isVoid = ReadOptionalBool(faceAnchors[i], "isVoid", context, false);
        anchor.floorZ = ReadFloat(faceAnchors[i], "floorZ", context);
        anchor.ceilingZ = ReadFloat(faceAnchors[i], "ceilingZ", context);
        anchor.floorTextureId = ReadString(faceAnchors[i], "floorTextureId", context);
        anchor.ceilingTextureId = ReadString(faceAnchors[i], "ceilingTextureId", context);
        anchor.ceilingSky = ReadOptionalBool(faceAnchors[i], "ceilingSky", context, false);
        anchor.floorUv = ReadUv(RequireField(faceAnchors[i], "floorUv", context), context + ".floorUv");
        anchor.ceilingUv = ReadUv(RequireField(faceAnchors[i], "ceilingUv", context), context + ".ceilingUv");
        ReadOptionalDecal(faceAnchors[i], "floorDecal", context, anchor.floorDecal);
        ReadOptionalDecal(faceAnchors[i], "ceilingDecal", context, anchor.ceilingDecal);
        anchor.ambientColor = ReadColor(
                RequireField(faceAnchors[i], "ambientColor", context), context + ".ambientColor");
        anchor.ambientIntensity = ReadFloat(faceAnchors[i], "ambientIntensity", context);
        anchor.defaultWall = ReadWallPart(
                RequireField(faceAnchors[i], "defaultWall", context), context + ".defaultWall");
        anchor.defaultLower = ReadWallPart(
                RequireField(faceAnchors[i], "defaultLower", context), context + ".defaultLower");
        anchor.defaultUpper = ReadWallPart(
                RequireField(faceAnchors[i], "defaultUpper", context), context + ".defaultUpper");
        graph.faceAnchors.push_back(std::move(anchor));
    }

    return graph;
}

void CopyMapLevelFieldsToDerivedTopology(SectorAuthoringDocument& document)
{
    if (!document.derivation.success) {
        return;
    }

    document.derivation.topology.texturesById = document.mapData.texturesById;
    document.derivation.topology.staticLights = document.mapData.staticLights;
    document.derivation.topology.staticSpotLights = document.mapData.staticSpotLights;
    document.derivation.topology.dynamicPointLights = document.mapData.dynamicPointLights;
    document.derivation.topology.dynamicSpotLights = document.mapData.dynamicSpotLights;
    document.derivation.topology.previewSettings = document.mapData.previewSettings;
    document.derivation.topology.skySettings = document.mapData.skySettings;
    document.derivation.topology.directionalLight = document.mapData.directionalLight;
    document.derivation.topology.lightmapSettings = document.mapData.lightmapSettings;
    document.derivation.topology.bakedLightmap = document.mapData.bakedLightmap;
}

SectorAuthoringDocument ParseAuthoringDocument(const Json& root)
{
    if (!root.is_object()) {
        Fail("Authoring JSON root must be an object");
    }

    if (ReadInt(root, "formatVersion", "root") != 3) {
        Fail("root.formatVersion must be 3");
    }
    if (ReadString(root, "topology", "root") != "authoringGraph") {
        Fail("root.topology must be 'authoringGraph'");
    }
    if (ReadInt(root, "coordSubdivisions", "root") != SectorCoordSubdivisions) {
        Fail("root.coordSubdivisions must equal " + std::to_string(SectorCoordSubdivisions));
    }

    SectorAuthoringDocument document;
    ReadTextures(root, document.mapData);
    ReadMapLevelFields(root, document.mapData, true);
    ValidateAuthoringMapData(document.mapData);
    document.graph = ReadAuthoringGraph(RequireField(root, "authoringGraph", "root"));
    document.derivation = DeriveSectorTopologyMapFromAuthoringGraph(document.graph);
    CopyMapLevelFieldsToDerivedTopology(document);
    return document;
}

void WriteTextureFields(Json& root, const SectorTopologyMap& map)
{
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
}

void WriteMapLevelFields(Json& root, const SectorTopologyMap& map, bool includeBakedLightmap)
{
    root["staticLights"] = Json::array();
    for (const SectorTopologyStaticPointLight* light : SortedById(map.staticLights)) {
        const std::string context = "static light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        RequireFinite(light->sourceRadius, context + ".sourceRadius");
        root["staticLights"].push_back(Json{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"sourceRadius", light->sourceRadius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        });
    }

    root["staticSpotLights"] = Json::array();
    for (const SectorTopologyStaticSpotLight* light : SortedById(map.staticSpotLights)) {
        const std::string context = "static spot light " + std::to_string(light->id);
        root["staticSpotLights"].push_back(WriteStaticSpotLight(*light, context));
    }

    root["dynamicPointLights"] = Json::array();
    for (const SectorTopologyDynamicPointLight* light : SortedById(map.dynamicPointLights)) {
        const std::string context = "dynamic point light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        Json lightJson{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        };
        if (!light->enabled) {
            lightJson["enabled"] = false;
        }
        WriteDynamicLightFlickerFields(lightJson, *light, context);
        root["dynamicPointLights"].push_back(std::move(lightJson));
    }

    root["dynamicSpotLights"] = Json::array();
    for (const SectorTopologyDynamicSpotLight* light : SortedById(map.dynamicSpotLights)) {
        const std::string context = "dynamic spot light " + std::to_string(light->id);
        root["dynamicSpotLights"].push_back(WriteDynamicSpotLight(*light, context));
    }

    root["lightmapSettings"] = WriteLightmapSettings(map.lightmapSettings);
    root["previewSettings"] = WritePreviewSettings(map.previewSettings);
    if (!IsDefaultSkySettings(map.skySettings)) {
        root["skySettings"] = WriteSkySettings(map.skySettings);
    }
    if (!IsDefaultDirectionalLightSettings(map.directionalLight)) {
        root["directionalLight"] = WriteDirectionalLightSettings(map.directionalLight);
    }
    if (includeBakedLightmap
            && !map.bakedLightmap.path.empty()
            && map.bakedLightmap.width > 0
            && map.bakedLightmap.height > 0
            && !map.bakedLightmap.sourceHash.empty()) {
        root["bakedLightmap"] = WriteBakedLightmap(map.bakedLightmap);
    }
}

Json WriteAuthoringGraph(const SectorAuthoringGraph& graph)
{
    Json graphJson;
    graphJson["vertices"] = Json::array();
    for (const SectorAuthoringVertex* vertex : SortedById(graph.vertices)) {
        graphJson["vertices"].push_back(Json{
                {"id", vertex->id}, {"x", vertex->x}, {"y", vertex->y}
        });
    }

    graphJson["lines"] = Json::array();
    for (const SectorAuthoringLine* line : SortedById(graph.lines)) {
        Json lineJson{
                {"id", line->id},
                {"startVertexId", line->startVertexId},
                {"endVertexId", line->endVertexId}
        };
        if (HasNonDefaultLineDefFlags(line->flags)) {
            lineJson["flags"] = WriteLineDefFlags(line->flags);
        }
        if (line->special.type != 0 || !line->special.tag.empty()) {
            lineJson["special"] = Json{
                    {"type", line->special.type},
                    {"tag", line->special.tag}
            };
        }
        graphJson["lines"].push_back(std::move(lineJson));
    }

    graphJson["lineSides"] = Json::array();
    std::vector<const SectorAuthoringLineSide*> sortedSides;
    sortedSides.reserve(graph.lineSides.size());
    for (const SectorAuthoringLineSide& side : graph.lineSides) {
        sortedSides.push_back(&side);
    }
    std::sort(sortedSides.begin(), sortedSides.end(), [](const auto* left, const auto* right) {
        if (left->id.lineId != right->id.lineId) {
            return left->id.lineId < right->id.lineId;
        }
        return static_cast<int>(left->id.side) < static_cast<int>(right->id.side);
    });
    for (const SectorAuthoringLineSide* side : sortedSides) {
        const std::string context = "authoring side " + std::to_string(side->id.lineId);
        Json sideJson{
                {"lineId", side->id.lineId},
                {"side", WriteSideKind(side->id.side, context)},
                {"wall", WriteWallPart(side->wall, context + ".wall")},
                {"lower", WriteWallPart(side->lower, context + ".lower")},
                {"upper", WriteWallPart(side->upper, context + ".upper")}
        };
        if (HasNonDefaultWallPart(side->middle)) {
            sideJson["middle"] = WriteWallPart(side->middle, context + ".middle");
        }
        graphJson["lineSides"].push_back(std::move(sideJson));
    }

    graphJson["faceAnchors"] = Json::array();
    for (const SectorAuthoringFaceAnchor* anchor : SortedById(graph.faceAnchors)) {
        const std::string context = "face anchor " + std::to_string(anchor->id);
        RequireFinite(anchor->floorZ, context + ".floorZ");
        RequireFinite(anchor->ceilingZ, context + ".ceilingZ");
        RequireFinite(anchor->ambientIntensity, context + ".ambientIntensity");
        Json anchorJson{
                {"id", anchor->id},
                {"name", anchor->name},
                {"x", anchor->x},
                {"y", anchor->y},
                {"floorZ", anchor->floorZ},
                {"ceilingZ", anchor->ceilingZ},
                {"floorTextureId", anchor->floorTextureId},
                {"ceilingTextureId", anchor->ceilingTextureId},
                {"floorUv", WriteUv(anchor->floorUv, context + ".floorUv")},
                {"ceilingUv", WriteUv(anchor->ceilingUv, context + ".ceilingUv")},
                {"ambientColor", WriteColor(anchor->ambientColor)},
                {"ambientIntensity", anchor->ambientIntensity},
                {"defaultWall", WriteWallPart(anchor->defaultWall, context + ".defaultWall")},
                {"defaultLower", WriteWallPart(anchor->defaultLower, context + ".defaultLower")},
                {"defaultUpper", WriteWallPart(anchor->defaultUpper, context + ".defaultUpper")}
        };
        if (anchor->ceilingSky) {
            anchorJson["ceilingSky"] = true;
        }
        if (anchor->isVoid) {
            anchorJson["isVoid"] = true;
        }
        if (HasDecal(anchor->floorDecal)) {
            anchorJson["floorDecal"] = WriteDecal(anchor->floorDecal, context + ".floorDecal");
        }
        if (HasDecal(anchor->ceilingDecal)) {
            anchorJson["ceilingDecal"] = WriteDecal(anchor->ceilingDecal, context + ".ceilingDecal");
        }
        graphJson["faceAnchors"].push_back(std::move(anchorJson));
    }

    return graphJson;
}

Json SerializeAuthoringDocument(const SectorAuthoringDocument& document)
{
    ValidateAuthoringMapData(document.mapData);

    Json root;
    root["formatVersion"] = 3;
    root["topology"] = "authoringGraph";
    root["coordSubdivisions"] = SectorCoordSubdivisions;
    WriteTextureFields(root, document.mapData);
    WriteMapLevelFields(root, document.mapData, true);
    root["authoringGraph"] = WriteAuthoringGraph(document.graph);
    return root;
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
    ReadTextures(root, map);

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
        SectorTopologyLineDef lineDef;
        lineDef.id = ReadInt(lineDefs[i], "id", context);
        lineDef.startVertexId = ReadInt(lineDefs[i], "startVertexId", context);
        lineDef.endVertexId = ReadInt(lineDefs[i], "endVertexId", context);
        lineDef.frontSideDefId = ReadInt(lineDefs[i], "frontSideDefId", context);
        lineDef.backSideDefId = ReadInt(lineDefs[i], "backSideDefId", context);
        ReadOptionalLineDefFlags(lineDefs[i], "flags", context, lineDef.flags);
        map.lineDefs.push_back(lineDef);
    }

    const Json& sideDefs = RequireArrayField(root, "sidedefs", "root");
    for (size_t i = 0; i < sideDefs.size(); ++i) {
        const std::string context = "root.sidedefs[" + std::to_string(i) + "]";
        const Json& value = sideDefs[i];
        if (!value.is_object()) {
            Fail(context + " must be an object");
        }
        SectorTopologySideDef sideDef;
        sideDef.id = ReadInt(value, "id", context);
        sideDef.lineDefId = ReadInt(value, "lineDefId", context);
        sideDef.side = ReadSideKind(value, "side", context);
        sideDef.sectorId = ReadInt(value, "sectorId", context);
        sideDef.wall = ReadWallPart(RequireField(value, "wall", context), context + ".wall");
        sideDef.lower = ReadWallPart(RequireField(value, "lower", context), context + ".lower");
        sideDef.upper = ReadWallPart(RequireField(value, "upper", context), context + ".upper");
        ReadOptionalWallPart(value, "middle", context, sideDef.middle);
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
        sector.ceilingSky = ReadOptionalBool(value, "ceilingSky", context, false);
        sector.floorUv = ReadUv(RequireField(value, "floorUv", context), context + ".floorUv");
        sector.ceilingUv = ReadUv(RequireField(value, "ceilingUv", context), context + ".ceilingUv");
        ReadOptionalDecal(value, "floorDecal", context, sector.floorDecal);
        ReadOptionalDecal(value, "ceilingDecal", context, sector.ceilingDecal);
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

    ReadMapLevelFields(root, map, true);
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
        Json lineDefJson{
                {"id", lineDef->id},
                {"startVertexId", lineDef->startVertexId},
                {"endVertexId", lineDef->endVertexId},
                {"frontSideDefId", lineDef->frontSideDefId},
                {"backSideDefId", lineDef->backSideDefId}
        };
        if (HasNonDefaultLineDefFlags(lineDef->flags)) {
            lineDefJson["flags"] = WriteLineDefFlags(lineDef->flags);
        }
        root["linedefs"].push_back(std::move(lineDefJson));
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
        Json sideDefJson{
                {"id", sideDef->id},
                {"lineDefId", sideDef->lineDefId},
                {"side", sideName},
                {"sectorId", sideDef->sectorId},
                {"wall", WriteWallPart(sideDef->wall, context + ".wall")},
                {"lower", WriteWallPart(sideDef->lower, context + ".lower")},
                {"upper", WriteWallPart(sideDef->upper, context + ".upper")}
        };
        if (HasNonDefaultWallPart(sideDef->middle)) {
            sideDefJson["middle"] = WriteWallPart(sideDef->middle, context + ".middle");
        }
        root["sidedefs"].push_back(std::move(sideDefJson));
    }

    root["sectors"] = Json::array();
    for (const SectorTopologySector* sector : SortedById(map.sectors)) {
        const std::string context = "sector " + std::to_string(sector->id);
        RequireFinite(sector->floorZ, context + ".floorZ");
        RequireFinite(sector->ceilingZ, context + ".ceilingZ");
        RequireFinite(sector->ambientIntensity, context + ".ambientIntensity");
        Json sectorJson{
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
        };
        if (HasDecal(sector->floorDecal)) {
            sectorJson["floorDecal"] = WriteDecal(sector->floorDecal, context + ".floorDecal");
        }
        if (HasDecal(sector->ceilingDecal)) {
            sectorJson["ceilingDecal"] = WriteDecal(
                    sector->ceilingDecal, context + ".ceilingDecal");
        }
        if (sector->ceilingSky) {
            sectorJson["ceilingSky"] = true;
        }
        root["sectors"].push_back(std::move(sectorJson));
    }

    root["staticLights"] = Json::array();
    for (const SectorTopologyStaticPointLight* light : SortedById(map.staticLights)) {
        const std::string context = "static light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        RequireFinite(light->sourceRadius, context + ".sourceRadius");
        root["staticLights"].push_back(Json{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"sourceRadius", light->sourceRadius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        });
    }

    root["staticSpotLights"] = Json::array();
    for (const SectorTopologyStaticSpotLight* light : SortedById(map.staticSpotLights)) {
        const std::string context = "static spot light " + std::to_string(light->id);
        root["staticSpotLights"].push_back(WriteStaticSpotLight(*light, context));
    }

    root["dynamicPointLights"] = Json::array();
    for (const SectorTopologyDynamicPointLight* light : SortedById(map.dynamicPointLights)) {
        const std::string context = "dynamic point light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        Json lightJson{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        };
        if (!light->enabled) {
            lightJson["enabled"] = false;
        }
        WriteDynamicLightFlickerFields(lightJson, *light, context);
        root["dynamicPointLights"].push_back(std::move(lightJson));
    }

    root["dynamicSpotLights"] = Json::array();
    for (const SectorTopologyDynamicSpotLight* light : SortedById(map.dynamicSpotLights)) {
        const std::string context = "dynamic spot light " + std::to_string(light->id);
        root["dynamicSpotLights"].push_back(WriteDynamicSpotLight(*light, context));
    }

    root["lightmapSettings"] = WriteLightmapSettings(map.lightmapSettings);
    root["previewSettings"] = WritePreviewSettings(map.previewSettings);
    if (!IsDefaultSkySettings(map.skySettings)) {
        root["skySettings"] = WriteSkySettings(map.skySettings);
    }
    if (!IsDefaultDirectionalLightSettings(map.directionalLight)) {
        root["directionalLight"] = WriteDirectionalLightSettings(map.directionalLight);
    }
    if (!map.bakedLightmap.path.empty()
            && map.bakedLightmap.width > 0
            && map.bakedLightmap.height > 0
            && !map.bakedLightmap.sourceHash.empty()) {
        root["bakedLightmap"] = WriteBakedLightmap(map.bakedLightmap);
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

bool LoadSectorAuthoringDocumentFromJsonString(
        const std::string& jsonText,
        SectorAuthoringDocument& outDocument,
        std::string* outError)
{
    ClearError(outError);
    try {
        const Json root = Json::parse(jsonText);
        SectorAuthoringDocument parsed = ParseAuthoringDocument(root);
        outDocument = std::move(parsed);
        return true;
    } catch (const std::exception& exception) {
        SetError(outError, exception.what());
        return false;
    }
}

bool SaveSectorAuthoringDocumentToJsonString(
        const SectorAuthoringDocument& document,
        std::string& outJsonText,
        std::string* outError)
{
    ClearError(outError);
    try {
        std::string serialized = SerializeAuthoringDocument(document).dump(2);
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

bool LoadSectorAuthoringDocument(
        const char* path,
        SectorAuthoringDocument& outDocument,
        std::string* outError)
{
    ClearError(outError);
    if (path == nullptr || path[0] == '\0') {
        SetError(outError, "Cannot load an authoring document without a path");
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(outError, std::string("Failed to open authoring document: ") + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        SetError(outError, std::string("Failed to read authoring document: ") + path);
        return false;
    }
    return LoadSectorAuthoringDocumentFromJsonString(contents.str(), outDocument, outError);
}

bool SaveSectorAuthoringDocument(
        const char* path,
        const SectorAuthoringDocument& document,
        std::string* outError)
{
    ClearError(outError);
    if (path == nullptr || path[0] == '\0') {
        SetError(outError, "Cannot save an authoring document without a path");
        return false;
    }

    std::string jsonText;
    if (!SaveSectorAuthoringDocumentToJsonString(document, jsonText, outError)) {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        SetError(outError, std::string("Failed to open authoring document for writing: ") + path);
        return false;
    }
    file.write(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
    if (!file) {
        SetError(outError, std::string("Failed to write authoring document: ") + path);
        return false;
    }
    return true;
}

} // namespace game
