#include "sector_demo/SectorTopologySerialization.h"

#include "game/npc/NpcDefinitions.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorTriggers.h"

#include "util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace game {

namespace {

using Json = nlohmann::ordered_json;

constexpr float Pi = 3.14159265358979323846f;
constexpr const char* RuntimeObjectKindBillboard = "billboard";
constexpr const char* RuntimeObjectKindStaticModel = "static_model";
constexpr const char* RuntimeObjectKindDynamicModel = "dynamic_model";
constexpr const char* RuntimeObjectKindNpc = "npc";
constexpr const char* RuntimeObjectKindDoor = "door";

[[noreturn]] void Fail(const std::string& message);
float ReadOptionalFloat(
        const Json& object,
        const char* field,
        const std::string& context,
        float defaultValue);
std::string ReadString(
        const Json& object,
        const char* field,
        const std::string& context);

bool IsValidAudioPath(const std::string& path)
{
    if (path.empty()) return false;
    const std::filesystem::path parsed(path);
    const bool windowsDrivePath = path.size() >= 2
            && std::isalpha(static_cast<unsigned char>(path[0]))
            && path[1] == ':';
    if (parsed.is_absolute()
            || windowsDrivePath
            || path.front() == '\\'
            || path.find('\\') != std::string::npos
            || path.find("..") != std::string::npos) {
        return false;
    }
    std::string extension = parsed.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
    return extension == ".ogg" || extension == ".wav" || extension == ".mp3";
}

bool IsValidFootstepSetId(const std::string& id)
{
    if (id.empty() || id.front() == '/' || id.front() == '\\') return false;
    std::string segment;
    for (const char character : id) {
        if (character == '\\') return false;
        if (character == '/') {
            if (segment.empty() || segment == "." || segment == "..") return false;
            segment.clear();
            continue;
        }
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_' && character != '-' && character != '.') {
            return false;
        }
        segment.push_back(character);
    }
    return !segment.empty() && segment != "." && segment != "..";
}

void ValidateOptionalFootstepSet(const std::string& id, const std::string& context)
{
    if (!id.empty() && !IsValidFootstepSetId(id)) {
        Fail(context + " must be a safe relative footstep set id");
    }
}

void ValidateAudioSettings(
        const SectorLevelAudioSettings& settings,
        const std::string& context)
{
    if (!settings.musicPath.empty() && !IsValidAudioPath(settings.musicPath)) {
        Fail(context + ".music must be a relative .ogg, .wav, or .mp3 path beneath assets/audio");
    }
    if (!std::isfinite(settings.musicVolume)
            || settings.musicVolume < 0.0f
            || settings.musicVolume > 1.0f) {
        Fail(context + ".musicVolume must be a finite number between 0 and 1");
    }
    for (const auto& entry : settings.soundsById) {
        const SectorSoundDefinition& sound = entry.second;
        if (entry.first.empty()) {
            Fail(context + ".sounds contains an empty sound ID");
        }
        if (sound.id != entry.first) {
            Fail(context + ".sounds." + entry.first + ".id must match its registry key");
        }
        if (!IsValidAudioPath(sound.path)) {
            Fail(context + ".sounds." + entry.first
                    + " must be a relative .ogg, .wav, or .mp3 path beneath assets/audio");
        }
        if (sound.type != SectorSoundType::Sound
                && sound.type != SectorSoundType::Music) {
            Fail(context + ".sounds." + entry.first + ".type is invalid");
        }
    }
}

SectorSoundType ReadSoundType(const Json& value, const std::string& context)
{
    if (!value.is_string()) {
        Fail(context + " must be 'sound' or 'music'");
    }
    const std::string type = value.get<std::string>();
    if (type == "sound") return SectorSoundType::Sound;
    if (type == "music") return SectorSoundType::Music;
    Fail(context + " must be 'sound' or 'music'");
}

const char* WriteSoundType(SectorSoundType type)
{
    switch (type) {
        case SectorSoundType::Sound: return "sound";
        case SectorSoundType::Music: return "music";
    }
    Fail("sound type is invalid");
}

void ReadAudioSettings(const Json& value, SectorLevelAudioSettings& settings)
{
    if (!value.is_object()) Fail("root.audio must be an object");
    const auto musicIt = value.find("music");
    if (musicIt != value.end()) {
        if (!musicIt->is_string() || musicIt->get<std::string>().empty()) {
            Fail("root.audio.music must be a non-empty string");
        }
        settings.musicPath = musicIt->get<std::string>();
    }
    settings.musicVolume = ReadOptionalFloat(
            value,
            "musicVolume",
            "root.audio",
            settings.musicVolume);
    const auto soundsIt = value.find("sounds");
    if (soundsIt != value.end()) {
        if (!soundsIt->is_object()) Fail("root.audio.sounds must be an object");
        for (const auto& entry : soundsIt->items()) {
            if (entry.key().empty()) {
                Fail("root.audio.sounds entries require non-empty IDs");
            }
            SectorSoundDefinition sound;
            sound.id = entry.key();
            if (entry.value().is_string()) {
                sound.path = entry.value().get<std::string>();
            } else if (entry.value().is_object()) {
                sound.path = ReadString(entry.value(), "path", "root.audio.sounds." + entry.key());
                const auto typeIt = entry.value().find("type");
                if (typeIt != entry.value().end()) {
                    sound.type = ReadSoundType(
                            *typeIt,
                            "root.audio.sounds." + entry.key() + ".type");
                }
            } else {
                Fail("root.audio.sounds entries must be legacy string paths or typed objects");
            }
            if (sound.path.empty()) {
                Fail("root.audio.sounds entries require non-empty paths");
            }
            settings.soundsById.emplace(entry.key(), std::move(sound));
        }
    }
    ValidateAudioSettings(settings, "root.audio");
}

void WriteAudioSettings(Json& root, const SectorLevelAudioSettings& settings)
{
    ValidateAudioSettings(settings, "root.audio");
    if (settings.musicPath.empty() && settings.soundsById.empty()) return;
    Json audio = Json::object();
    if (!settings.musicPath.empty()) {
        audio["music"] = settings.musicPath;
        if (settings.musicVolume != SectorLevelAudioSettings::DefaultMusicVolume) {
            audio["musicVolume"] = settings.musicVolume;
        }
    }
    if (!settings.soundsById.empty()) {
        audio["sounds"] = Json::object();
        std::vector<std::string> ids;
        ids.reserve(settings.soundsById.size());
        for (const auto& entry : settings.soundsById) ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());
        for (const std::string& id : ids) {
            const SectorSoundDefinition& sound = settings.soundsById.at(id);
            audio["sounds"][id] = Json{
                    {"path", sound.path},
                    {"type", WriteSoundType(sound.type)}
            };
        }
    }
    root["audio"] = std::move(audio);
}

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

std::string ReadOptionalString(
        const Json& object,
        const char* field,
        const std::string& context,
        const std::string& defaultValue = {})
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    if (!it->is_string()) {
        Fail(context + "." + field + " must be a string");
    }
    return it->get<std::string>();
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

float ReadOptionalFloat(
        const Json& object,
        const char* field,
        const std::string& context,
        float defaultValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    return ReadFloat(object, field, context);
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

SectorAuthoringEditorSettings ReadAuthoringEditorSettings(const Json& root)
{
    SectorAuthoringEditorSettings settings;
    const auto settingsIt = root.find("editorSettings");
    if (settingsIt == root.end()) {
        return settings;
    }
    if (!settingsIt->is_object()) {
        Fail("root.editorSettings must be an object");
    }
    settings.gridSize = ReadOptionalClampedInt(
            *settingsIt,
            "gridSize",
            "root.editorSettings",
            settings.gridSize,
            SectorAuthoringEditorGridSizeMin,
            SectorAuthoringEditorGridSizeMax);
    return settings;
}

void WriteAuthoringEditorSettings(
        Json& root,
        const SectorAuthoringEditorSettings& source)
{
    const int gridSize = std::clamp(
            source.gridSize,
            SectorAuthoringEditorGridSizeMin,
            SectorAuthoringEditorGridSizeMax);
    if (gridSize != SectorAuthoringEditorGridSizeDefault) {
        root["editorSettings"] = Json{{"gridSize", gridSize}};
    }
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

Vector2 ReadUnitVector2(const Json& value, const std::string& context)
{
    const Vector2 vector = ReadVector2(value, context);
    if (vector.x < 0.0f || vector.x > 1.0f
            || vector.y < 0.0f || vector.y > 1.0f) {
        Fail(context + " values must be between 0 and 1");
    }
    return vector;
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

float DegreesToRadians(float degrees)
{
    return degrees * Pi / 180.0f;
}

float RadiansToDegrees(float radians)
{
    return radians * 180.0f / Pi;
}

bool IsDefaultBillboardOrigin(Vector2 origin)
{
    return origin.x == 0.5f && origin.y == 1.0f;
}

bool IsDefaultBillboardDirectionalClips(const SectorPlacedBillboard& billboard)
{
    return billboard.frontClip == "Front"
            && billboard.backClip == "Back"
            && billboard.leftClip == "Left"
            && billboard.rightClip == "Right";
}

const char* WriteSectorDoorMotionType(SectorDoorMotionType motion);
const char* WriteSectorDoorVisualType(SectorDoorVisualType visual);
const char* WriteSectorDoorModelFit(SectorDoorModelFit fit);
const char* WriteSectorDoorHinge(SectorDoorHinge hinge);
const char* WriteSectorDoorSwingSide(SectorDoorSwingSide side);

constexpr const char* DoorFaceJsonNames[SectorDoorFaceCount] = {
        "front",
        "back",
        "left",
        "right",
        "top",
        "bottom"};

bool IsValidDoorUvScale(float scale)
{
    return std::isfinite(scale)
            && scale >= TopologyUvScaleMin
            && scale <= TopologyUvScaleMax;
}

bool IsDefaultDoorFaceUv(const SectorDoorFaceUv& uv)
{
    return uv.scale.x == 1.0f
            && uv.scale.y == 1.0f
            && uv.offset.x == 0.0f
            && uv.offset.y == 0.0f;
}

bool IsDefaultDoorFaceUvSet(const SectorDoorFaceUvSet& uvs)
{
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        if (!IsDefaultDoorFaceUv(uvs.faces[i])) {
            return false;
        }
    }
    return true;
}

void ValidateDoorFaceUv(const SectorDoorFaceUv& uv, const std::string& context)
{
    if (!IsValidDoorUvScale(uv.scale.x) || !IsValidDoorUvScale(uv.scale.y)) {
        Fail(context + ".scale values must be finite floats between 0.001 and 64");
    }
    if (!std::isfinite(uv.offset.x) || !std::isfinite(uv.offset.y)) {
        Fail(context + ".offset values must be finite floats");
    }
}

void ValidateDoorFaceUvSet(const SectorDoorFaceUvSet& uvs, const std::string& context)
{
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        ValidateDoorFaceUv(uvs.faces[i], context + ".faceUvs." + DoorFaceJsonNames[i]);
    }
}

void ValidatePlacedBillboard(const SectorPlacedBillboard& billboard, const std::string& context)
{
    if (!std::isfinite(billboard.sizeWorld.x)
            || !std::isfinite(billboard.sizeWorld.y)
            || billboard.sizeWorld.x <= 0.0f
            || billboard.sizeWorld.y <= 0.0f) {
        Fail(context + ".width and .height must be finite positive values");
    }
    if (!std::isfinite(billboard.originNormalized.x)
            || !std::isfinite(billboard.originNormalized.y)
            || billboard.originNormalized.x < 0.0f
            || billboard.originNormalized.x > 1.0f
            || billboard.originNormalized.y < 0.0f
            || billboard.originNormalized.y > 1.0f) {
        Fail(context + ".originNormalized values must be finite floats between 0 and 1");
    }
}

void ValidatePlacedDoorForSerialization(const SectorPlacedDoor& door, const std::string& context)
{
    if (!IsValidSectorTopologyId(door.anchor.lineDefId)
            || !IsValidSectorTopologyId(door.anchor.frontSectorId)
            || !IsValidSectorTopologyId(door.anchor.backSectorId)
            || !IsValidSectorTopologyId(door.anchor.frontSideDefId)
            || !IsValidSectorTopologyId(door.anchor.backSideDefId)) {
        Fail(context + ".anchor IDs must be positive integers");
    }
    if (!std::isfinite(door.width)
            || !std::isfinite(door.height)
            || !std::isfinite(door.thickness)
            || !std::isfinite(door.normalOffset)
            || !std::isfinite(door.heightOffsetWorld)
            || !std::isfinite(door.modelScale)
            || !std::isfinite(door.openAngleDegrees)
            || !std::isfinite(door.angularSpeedDegrees)
            || !std::isfinite(door.openDistance)
            || !std::isfinite(door.speed)
            || !std::isfinite(door.initialOpenFraction)
            || !std::isfinite(door.interactionDistance)
            || !std::isfinite(door.autoOpenDistance)) {
        Fail(context + " numeric values must be finite");
    }
    if (door.width < 0.0f || door.height < 0.0f || door.openDistance < 0.0f) {
        Fail(context + ".width, .height, and .openDistance must be non-negative");
    }
    if (door.thickness <= 0.0f) {
        Fail(context + ".thickness must be positive");
    }
    if (door.speed < 0.0f) {
        Fail(context + ".speed must be non-negative");
    }
    if (door.modelScale <= 0.0f) {
        Fail(context + ".modelScale must be positive");
    }
    if (door.openAngleDegrees <= 0.0f || door.openAngleDegrees > 170.0f) {
        Fail(context + ".openAngleDegrees must be greater than 0 and at most 170");
    }
    if (door.angularSpeedDegrees < 0.0f) {
        Fail(context + ".angularSpeedDegrees must be non-negative");
    }
    if (door.initialOpenFraction < 0.0f || door.initialOpenFraction > 1.0f) {
        Fail(context + ".initialOpenFraction must be between 0 and 1");
    }
    if (door.interactionDistance <= 0.0f || door.autoOpenDistance <= 0.0f) {
        Fail(context + ".interactionDistance and .autoOpenDistance must be positive");
    }
    ValidateDoorFaceUvSet(door.faceUvs, context);
    (void)WriteSectorDoorMotionType(door.motion);
    (void)WriteSectorDoorVisualType(door.visual);
    (void)WriteSectorDoorModelFit(door.modelFit);
    (void)WriteSectorDoorHinge(door.hinge);
    (void)WriteSectorDoorSwingSide(door.swingSide);
    if (door.visual == SectorDoorVisualType::Model) {
        if (door.modelAssetId.empty()) {
            Fail(context + ".modelAssetId must be non-empty when visual is 'model'");
        }
        if (door.motion != SectorDoorMotionType::Swing) {
            Fail(context + ".visual 'model' requires motion 'swing'");
        }
    }
}

float ReadOptionalPositiveFloat(
        const Json& object,
        const char* field,
        const std::string& context,
        float defaultValue)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        return defaultValue;
    }
    const float value = ReadFloat(object, field, context);
    if (value <= 0.0f) {
        Fail(context + "." + field + " must be positive");
    }
    return value;
}

SectorDoorMotionType ReadSectorDoorMotionType(const Json& object, const char* field, const std::string& context)
{
    const std::string value = ReadOptionalString(object, field, context, "slide_vertical");
    if (value == "slide_vertical") {
        return SectorDoorMotionType::SlideVertical;
    }
    if (value == "slide_left") {
        return SectorDoorMotionType::SlideLeft;
    }
    if (value == "slide_right") {
        return SectorDoorMotionType::SlideRight;
    }
    if (value == "swing") {
        return SectorDoorMotionType::Swing;
    }
    Fail(context + "." + field + " must be 'slide_vertical', 'slide_left', 'slide_right', or 'swing'");
}

SectorDoorVisualType ReadSectorDoorVisualType(
        const Json& object, const char* field, const std::string& context)
{
    const std::string value = ReadOptionalString(object, field, context, "procedural");
    if (value == "procedural") return SectorDoorVisualType::Procedural;
    if (value == "model") return SectorDoorVisualType::Model;
    Fail(context + "." + field + " must be 'procedural' or 'model'");
}

SectorDoorModelFit ReadSectorDoorModelFit(
        const Json& object, const char* field, const std::string& context)
{
    const std::string value = ReadOptionalString(object, field, context, "fit_inside");
    if (value == "manual") return SectorDoorModelFit::Manual;
    if (value == "fit_width") return SectorDoorModelFit::FitWidth;
    if (value == "fit_inside") return SectorDoorModelFit::FitInside;
    Fail(context + "." + field + " must be 'manual', 'fit_width', or 'fit_inside'");
}

SectorDoorHinge ReadSectorDoorHinge(
        const Json& object, const char* field, const std::string& context)
{
    const std::string value = ReadOptionalString(object, field, context, "start");
    if (value == "start") return SectorDoorHinge::Start;
    if (value == "end") return SectorDoorHinge::End;
    Fail(context + "." + field + " must be 'start' or 'end'");
}

SectorDoorSwingSide ReadSectorDoorSwingSide(
        const Json& object, const char* field, const std::string& context)
{
    const std::string value = ReadOptionalString(object, field, context, "front");
    if (value == "front") return SectorDoorSwingSide::Front;
    if (value == "back") return SectorDoorSwingSide::Back;
    Fail(context + "." + field + " must be 'front' or 'back'");
}

SectorCoord ReadCoordPairElement(const Json& value, size_t index, const std::string& context)
{
    if (index >= value.size()) {
        Fail(context + " must be an array of two integers");
    }
    if (!value[index].is_number_integer() && !value[index].is_number_unsigned()) {
        Fail(context + " values must be JSON integers");
    }
    if (value[index].is_number_unsigned()) {
        const uint64_t number = value[index].get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<SectorCoord>::max())) {
            Fail(context + " value is outside the SectorCoord range");
        }
        return static_cast<SectorCoord>(number);
    }
    const int64_t number = value[index].get<int64_t>();
    if (number < std::numeric_limits<SectorCoord>::min()
            || number > std::numeric_limits<SectorCoord>::max()) {
        Fail(context + " value is outside the SectorCoord range");
    }
    return static_cast<SectorCoord>(number);
}

void ReadSectorCoordPair(
        const Json& object,
        const char* field,
        const std::string& context,
        SectorCoord& outX,
        SectorCoord& outY)
{
    const Json& value = RequireArrayField(object, field, context);
    if (value.size() != 2) {
        Fail(context + "." + field + " must be an array of two integers");
    }
    const std::string pairContext = context + "." + field;
    outX = ReadCoordPairElement(value, 0, pairContext);
    outY = ReadCoordPairElement(value, 1, pairContext);
}

SectorDoorAnchor ReadSectorDoorAnchor(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorDoorAnchor anchor;
    anchor.lineDefId = ReadInt(value, "lineDefId", context);
    anchor.frontSectorId = ReadInt(value, "frontSectorId", context);
    anchor.backSectorId = ReadInt(value, "backSectorId", context);
    anchor.frontSideDefId = ReadInt(value, "frontSideDefId", context);
    anchor.backSideDefId = ReadInt(value, "backSideDefId", context);
    ReadSectorCoordPair(value, "endpointA", context, anchor.endpointAX, anchor.endpointAY);
    ReadSectorCoordPair(value, "endpointB", context, anchor.endpointBX, anchor.endpointBY);
    return anchor;
}

SectorDoorFaceUv ReadDoorFaceUv(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorDoorFaceUv uv;
    uv.scale = ReadVector2(RequireField(value, "scale", context), context + ".scale");
    uv.offset = ReadVector2(RequireField(value, "offset", context), context + ".offset");
    ValidateDoorFaceUv(uv, context);
    return uv;
}

void ReadOptionalDoorFaceUvs(
        const Json& object,
        const char* field,
        const std::string& context,
        SectorDoorFaceUvSet& outUvs)
{
    const auto it = object.find(field);
    if (it == object.end()) {
        outUvs = SectorDoorFaceUvSet{};
        return;
    }
    if (!it->is_object()) {
        Fail(context + "." + field + " must be an object");
    }
    outUvs = SectorDoorFaceUvSet{};
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        const auto faceIt = it->find(DoorFaceJsonNames[i]);
        if (faceIt != it->end()) {
            outUvs.faces[i] = ReadDoorFaceUv(*faceIt, context + "." + field + "." + DoorFaceJsonNames[i]);
        }
    }
}

SectorPlacedDoor ReadPlacedDoor(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPlacedDoor door;
    door.anchor = ReadSectorDoorAnchor(RequireObjectField(value, "anchor", context),
            context + ".anchor");
    door.width = ReadOptionalFloat(value, "width", context, door.width);
    door.height = ReadOptionalFloat(value, "height", context, door.height);
    door.thickness = ReadOptionalFloat(value, "thickness", context, door.thickness);
    door.normalOffset = ReadOptionalFloat(value, "normalOffset", context, door.normalOffset);
    door.heightOffsetWorld = ReadOptionalFloat(
            value, "heightOffsetWorld", context, door.heightOffsetWorld);
    door.visual = ReadSectorDoorVisualType(value, "visual", context);
    door.modelAssetId = ReadOptionalString(value, "modelAssetId", context, door.modelAssetId);
    door.modelFit = ReadSectorDoorModelFit(value, "modelFit", context);
    door.modelScale = ReadOptionalFloat(value, "modelScale", context, door.modelScale);
    door.motion = ReadSectorDoorMotionType(value, "motion", context);
    door.hinge = ReadSectorDoorHinge(value, "hinge", context);
    door.swingSide = ReadSectorDoorSwingSide(value, "swingSide", context);
    door.openAngleDegrees = ReadOptionalFloat(
            value, "openAngleDegrees", context, door.openAngleDegrees);
    door.angularSpeedDegrees = ReadOptionalFloat(
            value, "angularSpeedDegrees", context, door.angularSpeedDegrees);
    door.openDistance = ReadOptionalFloat(value, "openDistance", context, door.openDistance);
    door.speed = ReadOptionalFloat(value, "speed", context, door.speed);
    door.initialOpenFraction = ReadOptionalFloat(
            value,
            "initialOpenFraction",
            context,
            door.initialOpenFraction);
    door.autoOpen = ReadOptionalBool(value, "autoOpen", context, door.autoOpen);
    door.interactionDistance = ReadOptionalFloat(
            value,
            "interactionDistance",
            context,
            door.interactionDistance);
    door.autoOpenDistance = ReadOptionalFloat(
            value,
            "autoOpenDistance",
            context,
            door.autoOpenDistance);
    door.textureId = ReadOptionalString(value, "textureId", context, door.textureId);
    door.openSoundId = ReadOptionalString(value, "openSoundId", context, door.openSoundId);
    door.closeSoundId = ReadOptionalString(value, "closeSoundId", context, door.closeSoundId);
    ReadOptionalDoorFaceUvs(value, "faceUvs", context, door.faceUvs);
    ValidatePlacedDoorForSerialization(door, context);
    return door;
}

SectorPlacedBillboard ReadPlacedBillboard(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPlacedBillboard billboard;
    billboard.spriteAnimationPath = ReadString(value, "spriteAnimationPath", context);
    billboard.sizeWorld.x = ReadOptionalPositiveFloat(value, "width", context, billboard.sizeWorld.x);
    billboard.sizeWorld.y = ReadOptionalPositiveFloat(value, "height", context, billboard.sizeWorld.y);
    billboard.keepAspectRatio = ReadOptionalBool(value, "keepAspectRatio", context, billboard.keepAspectRatio);
    const auto originIt = value.find("originNormalized");
    if (originIt != value.end()) {
        billboard.originNormalized = ReadUnitVector2(*originIt, context + ".originNormalized");
    }
    billboard.directional = ReadOptionalBool(value, "directional", context, billboard.directional);
    billboard.clip = ReadOptionalString(value, "clip", context, billboard.clip);
    billboard.frontClip = ReadOptionalString(value, "frontClip", context, billboard.frontClip);
    billboard.backClip = ReadOptionalString(value, "backClip", context, billboard.backClip);
    billboard.leftClip = ReadOptionalString(value, "leftClip", context, billboard.leftClip);
    billboard.rightClip = ReadOptionalString(value, "rightClip", context, billboard.rightClip);
    billboard.playing = ReadOptionalBool(value, "playing", context, billboard.playing);
    ValidatePlacedBillboard(billboard, context);
    return billboard;
}

SectorPlacedStaticModel ReadPlacedStaticModel(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPlacedStaticModel staticModel;
    staticModel.modelPath = ReadOptionalString(value, "modelPath", context, staticModel.modelPath);
    staticModel.rotationXRadians = DegreesToRadians(ReadOptionalFloat(
            value,
            "rotationXDegrees",
            context,
            0.0f));
    staticModel.rotationZRadians = DegreesToRadians(ReadOptionalFloat(
            value,
            "rotationZDegrees",
            context,
            0.0f));
    staticModel.heightOffsetWorld = ReadOptionalFloat(
            value,
            "heightOffsetWorld",
            context,
            staticModel.heightOffsetWorld);
    staticModel.scale = ReadOptionalPositiveFloat(
            value,
            "scale",
            context,
            staticModel.scale);
    staticModel.collision = ReadOptionalBool(
            value,
            "collision",
            context,
            staticModel.collision);
    return staticModel;
}

SectorPlacedDynamicModel ReadPlacedDynamicModel(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPlacedDynamicModel model;
    model.modelPath = ReadOptionalString(value, "modelPath", context, model.modelPath);
    model.rotationXRadians = DegreesToRadians(ReadOptionalFloat(
            value, "rotationXDegrees", context, 0.0f));
    model.rotationZRadians = DegreesToRadians(ReadOptionalFloat(
            value, "rotationZDegrees", context, 0.0f));
    model.heightOffsetWorld = ReadOptionalFloat(
            value, "heightOffsetWorld", context, model.heightOffsetWorld);
    model.scale = ReadOptionalPositiveFloat(value, "scale", context, model.scale);
    model.collision = ReadOptionalBool(value, "collision", context, model.collision);
    model.animation = ReadOptionalString(value, "animation", context, model.animation);
    model.loop = ReadOptionalBool(value, "loop", context, model.loop);
    model.animationSpeed = ReadOptionalPositiveFloat(
            value, "animationSpeed", context, model.animationSpeed);
    const std::string shadowMode = ReadOptionalString(
            value, "shadowMode", context, "contact");
    if (shadowMode == "none") {
        model.shadowMode = SectorDynamicModelShadowMode::None;
    } else if (shadowMode == "contact") {
        model.shadowMode = SectorDynamicModelShadowMode::Contact;
    } else if (shadowMode == "dynamic"
            || shadowMode == "projected_silhouette") {
        model.shadowMode = SectorDynamicModelShadowMode::Dynamic;
    } else {
        Fail(context + ".shadowMode must be 'none', 'contact', or 'dynamic'");
    }
    return model;
}

SectorPlacedNpc ReadPlacedNpc(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorPlacedNpc npc;
    npc.definitionId = ReadString(value, "definitionId", context);
    npc.instanceId = ReadOptionalString(value, "instanceId", context, npc.instanceId);
    npc.scale = ReadOptionalPositiveFloat(value, "scale", context, npc.scale);
    const std::string shadowMode = ReadOptionalString(
            value, "shadowMode", context, "contact");
    if (shadowMode == "none") {
        npc.shadowMode = SectorDynamicModelShadowMode::None;
    } else if (shadowMode == "contact") {
        npc.shadowMode = SectorDynamicModelShadowMode::Contact;
    } else if (shadowMode == "dynamic"
            || shadowMode == "projected_silhouette") {
        npc.shadowMode = SectorDynamicModelShadowMode::Dynamic;
    } else {
        Fail(context + ".shadowMode must be 'none', 'contact', or 'dynamic'");
    }
    return npc;
}

SectorPlacedRuntimeObject ReadRuntimeObject(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorPlacedRuntimeObject object;
    object.id = ReadInt(value, "id", context);
    if (!IsValidSectorTopologyId(object.id)) {
        Fail(context + ".id must be a positive integer");
    }
    object.kind = ReadOptionalString(value, "kind", context, object.kind);
    if (!object.kind.empty()) {
        if (object.kind == RuntimeObjectKindBillboard) {
            object.billboard = ReadPlacedBillboard(RequireObjectField(value, "billboard", context),
                    context + ".billboard");
        } else if (object.kind == RuntimeObjectKindStaticModel) {
            object.staticModel = ReadPlacedStaticModel(
                    RequireObjectField(value, "staticModel", context),
                    context + ".staticModel");
        } else if (object.kind == RuntimeObjectKindDynamicModel) {
            object.dynamicModel = ReadPlacedDynamicModel(
                    RequireObjectField(value, "dynamicModel", context),
                    context + ".dynamicModel");
        } else if (object.kind == RuntimeObjectKindNpc) {
            object.npc = ReadPlacedNpc(
                    RequireObjectField(value, "npc", context),
                    context + ".npc");
        } else if (object.kind == RuntimeObjectKindDoor) {
            object.door = ReadPlacedDoor(RequireObjectField(value, "door", context),
                    context + ".door");
        } else {
            Fail(context + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
        }
    } else {
        if (value.contains("definitionId")) {
            Fail(context + ".definitionId-only runtime objects are legacy unsupported data; use kind 'billboard'");
        }
        Fail(context + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
    }
    object.position = ReadVector3(RequireField(value, "position", context), context + ".position");
    object.yawRadians = DegreesToRadians(ReadFloat(value, "yawDegrees", context));
    return object;
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
    settings.objectProbeSpacingWorld = ReadOptionalClampedFloat(
            value,
            "objectProbeSpacingWorld",
            context,
            settings.objectProbeSpacingWorld,
            0.25f,
            128.0f);
    const bool hasLowerHeight = value.find("objectProbeLowerHeightWorld") != value.end();
    const bool hasUpperHeight = value.find("objectProbeUpperHeightWorld") != value.end();
    const bool hasLegacyHeight = value.find("objectProbeHeightWorld") != value.end();
    if (!hasLowerHeight && !hasUpperHeight && hasLegacyHeight) {
        settings.objectProbeUpperHeightWorld = ReadOptionalClampedFloat(
                value,
                "objectProbeHeightWorld",
                context,
                settings.objectProbeUpperHeightWorld,
                0.0f,
                16.0f);
        if (settings.objectProbeUpperHeightWorld
                - settings.objectProbeLowerHeightWorld
                < kObjectProbeMinimumLayerSeparationWorld) {
            settings.objectProbeLowerHeightWorld =
                    settings.objectProbeUpperHeightWorld;
        }
    } else {
        settings.objectProbeLowerHeightWorld = ReadOptionalClampedFloat(
                value,
                "objectProbeLowerHeightWorld",
                context,
                settings.objectProbeLowerHeightWorld,
                0.0f,
                16.0f);
        settings.objectProbeUpperHeightWorld = ReadOptionalClampedFloat(
                value,
                "objectProbeUpperHeightWorld",
                context,
                settings.objectProbeUpperHeightWorld,
                0.0f,
                16.0f);
    }
    if (settings.objectProbeLowerHeightWorld
            > settings.objectProbeUpperHeightWorld) {
        std::swap(
                settings.objectProbeLowerHeightWorld,
                settings.objectProbeUpperHeightWorld);
    }
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
    const auto objectProbeDebugDrawMaxDistanceWorldIt =
            value.find("objectProbeDebugDrawMaxDistanceWorld");
    if (objectProbeDebugDrawMaxDistanceWorldIt != value.end()) {
        settings.objectProbeDebugDrawMaxDistanceWorld =
                ReadFloat(value, "objectProbeDebugDrawMaxDistanceWorld", context);
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

SectorTopologyFogSettings ReadFogSettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorTopologyFogSettings settings = DefaultSectorTopologyFogSettings();
    const auto modeIt = value.find("mode");
    if (modeIt != value.end()) {
        if (!modeIt->is_string()) Fail(context + ".mode must be a string");
        const std::string mode = modeIt->get<std::string>();
        if (mode == "legacyHeight") settings.mode = SectorTopologyFogMode::LegacyHeight;
        else if (mode == "distance") settings.mode = SectorTopologyFogMode::Distance;
        else Fail(context + ".mode must be 'legacyHeight' or 'distance'");
    }
    const auto enabledIt = value.find("enabled");
    if (enabledIt != value.end()) {
        if (!enabledIt->is_boolean()) {
            Fail(context + ".enabled must be a boolean");
        }
        settings.enabled = enabledIt->get<bool>();
    }
    const auto colorIt = value.find("color");
    if (colorIt != value.end()) {
        if (!colorIt->is_object()) {
            Fail(context + ".color must be an object");
        }
        const Color defaults = DefaultSectorTopologyFogSettings().color;
        settings.color = Color{
                ReadOptionalColorChannel(*colorIt, "r", context + ".color", defaults.r),
                ReadOptionalColorChannel(*colorIt, "g", context + ".color", defaults.g),
                ReadOptionalColorChannel(*colorIt, "b", context + ".color", defaults.b),
                255
        };
    }
    const auto readOptionalFloat = [&](const char* field, float& output) {
        if (value.find(field) != value.end()) {
            output = ReadFloat(value, field, context);
        }
    };
    readOptionalFloat("startDistanceWorld", settings.startDistanceWorld);
    readOptionalFloat("endDistanceWorld", settings.endDistanceWorld);
    readOptionalFloat("falloffExponent", settings.falloffExponent);
    readOptionalFloat("brightness", settings.brightness);
    readOptionalFloat("density", settings.density);
    readOptionalFloat("maxOpacity", settings.maxOpacity);
    readOptionalFloat("referenceHeightWorld", settings.referenceHeightWorld);
    readOptionalFloat("heightFalloff", settings.heightFalloff);
    return NormalizeSectorTopologyFogSettings(settings);
}

SectorIlluminationStatistics ReadIlluminationStatistics(
        const Json& value,
        const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorIlluminationStatistics statistics;
    statistics.rgbMin = ReadVector3(value.at("rgbMin"), context + ".rgbMin");
    statistics.rgbMax = ReadVector3(value.at("rgbMax"), context + ".rgbMax");
    statistics.auxiliaryMin = ReadFloat(value, "auxiliaryMin", context);
    statistics.auxiliaryMax = ReadFloat(value, "auxiliaryMax", context);
    statistics.sampleCount = value.at("sampleCount").get<uint64_t>();
    statistics.rgbChannelsAboveOne =
            value.at("rgbChannelsAboveOne").get<uint64_t>();
    if (statistics.sampleCount == 0
            || statistics.rgbMin.x < 0.0f || statistics.rgbMin.y < 0.0f
            || statistics.rgbMin.z < 0.0f
            || statistics.rgbMax.x < statistics.rgbMin.x
            || statistics.rgbMax.y < statistics.rgbMin.y
            || statistics.rgbMax.z < statistics.rgbMin.z
            || statistics.auxiliaryMin < 0.0f
            || statistics.auxiliaryMax < statistics.auxiliaryMin
            || statistics.auxiliaryMax > 1.0f) {
        Fail(context + " contains invalid ranges");
    }
    return statistics;
}

SectorBakedObjectLightProbeMetadata ReadBakedObjectLightProbeMetadata(
        const Json& value,
        const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorBakedObjectLightProbeMetadata metadata;
    metadata.path = ReadString(value, "path", context);
    metadata.version = ReadInt(value, "version", context);
    metadata.sourceHash = ReadString(value, "sourceHash", context);
    metadata.count = ReadInt(value, "count", context);
    metadata.probeSpacingWorld = ReadFloat(value, "probeSpacingWorld", context);
    const auto lowerHeight = value.find("probeLowerHeightWorld");
    const auto upperHeight = value.find("probeUpperHeightWorld");
    if (lowerHeight != value.end() && upperHeight != value.end()) {
        metadata.probeLowerHeightWorld = ReadFloat(
                value, "probeLowerHeightWorld", context);
        metadata.probeUpperHeightWorld = ReadFloat(
                value, "probeUpperHeightWorld", context);
    } else {
        const float legacyHeight = ReadFloat(value, "probeHeightWorld", context);
        metadata.probeLowerHeightWorld = legacyHeight;
        metadata.probeUpperHeightWorld = legacyHeight;
    }
    metadata.format = ReadString(value, "format", context);
    const auto statisticsIt = value.find("storedStatistics");
    if (statisticsIt != value.end()) {
        metadata.storedStatistics = ReadIlluminationStatistics(
                *statisticsIt, context + ".storedStatistics");
    }

    if (metadata.path.empty()) {
        Fail(context + ".path must not be empty");
    }
    if (metadata.version <= 0) {
        Fail(context + ".version must be positive");
    }
    if (metadata.sourceHash.empty()) {
        Fail(context + ".sourceHash must not be empty");
    }
    if (metadata.count < 0) {
        Fail(context + ".count must not be negative");
    }
    if (metadata.probeSpacingWorld <= 0.0f) {
        Fail(context + ".probeSpacingWorld must be positive");
    }
    if (metadata.probeLowerHeightWorld < 0.0f
            || metadata.probeUpperHeightWorld
                    < metadata.probeLowerHeightWorld) {
        Fail(context + " probe heights must be non-negative and ordered");
    }
    if (metadata.format.empty()) {
        Fail(context + ".format must not be empty");
    }
    return metadata;
}

SectorBakedStaticModelLightmapMetadata ReadBakedStaticModelLightmapMetadata(
        const Json& value,
        const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }

    SectorBakedStaticModelLightmapMetadata metadata;
    metadata.path = ReadString(value, "path", context);
    metadata.version = ReadInt(value, "version", context);
    metadata.sourceHash = ReadString(value, "sourceHash", context);
    metadata.modelCount = ReadInt(value, "modelCount", context);
    metadata.objectCount = ReadInt(value, "objectCount", context);
    metadata.format = ReadString(value, "format", context);
    if (metadata.path.empty()) {
        Fail(context + ".path must not be empty");
    }
    if (metadata.version <= 0) {
        Fail(context + ".version must be positive");
    }
    if (metadata.sourceHash.empty()) {
        Fail(context + ".sourceHash must not be empty");
    }
    if (metadata.modelCount <= 0 || metadata.objectCount <= 0) {
        Fail(context + " counts must be positive");
    }
    if (metadata.format.empty()) {
        Fail(context + ".format must not be empty");
    }
    return metadata;
}

SectorLightmapAtlasMetadata ReadLightmapAtlasMetadata(
        const Json& value,
        const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorLightmapAtlasMetadata atlas;
    atlas.path = ReadString(value, "path", context);
    atlas.width = ReadInt(value, "width", context);
    atlas.height = ReadInt(value, "height", context);
    if (atlas.path.empty()) {
        Fail(context + ".path must not be empty");
    }
    if (atlas.width <= 0 || atlas.height <= 0) {
        Fail(context + " dimensions must be positive");
    }
    return atlas;
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
    const auto versionIt = value.find("version");
    metadata.version = versionIt == value.end()
            ? 0
            : ReadInt(value, "version", context);
    const auto formatIt = value.find("format");
    metadata.format = formatIt == value.end()
            ? std::string{}
            : ReadString(value, "format", context);
    metadata.sourceHash = ReadString(value, "sourceHash", context);
    const auto storedStatisticsIt = value.find("storedStatistics");
    if (storedStatisticsIt != value.end()) {
        metadata.storedStatistics = ReadIlluminationStatistics(
                *storedStatisticsIt, context + ".storedStatistics");
    }
    if (metadata.path.empty()) {
        Fail(context + ".path must not be empty");
    }
    if (metadata.width <= 0 || metadata.height <= 0) {
        Fail(context + " dimensions must be positive");
    }
    if (metadata.sourceHash.empty()) {
        Fail(context + ".sourceHash must not be empty");
    }
    const auto additionalAtlasesIt = value.find("additionalAtlases");
    if (additionalAtlasesIt != value.end()) {
        if (!additionalAtlasesIt->is_array()) {
            Fail(context + ".additionalAtlases must be an array");
        }
        for (size_t index = 0; index < additionalAtlasesIt->size(); ++index) {
            SectorLightmapAtlasMetadata atlas = ReadLightmapAtlasMetadata(
                    (*additionalAtlasesIt)[index],
                    context + ".additionalAtlases[" + std::to_string(index) + "]");
            if (atlas.width != metadata.width
                    || atlas.height != metadata.height) {
                Fail(context + ".additionalAtlases dimensions must match the primary atlas");
            }
            if (atlas.path == metadata.path
                    || std::any_of(
                            metadata.additionalAtlases.begin(),
                            metadata.additionalAtlases.end(),
                            [&](const SectorLightmapAtlasMetadata& existing) {
                                return existing.path == atlas.path;
                            })) {
                Fail(context + ".additionalAtlases contains a duplicate path");
            }
            metadata.additionalAtlases.push_back(std::move(atlas));
        }
    }
    const auto objectProbesIt = value.find("objectProbes");
    if (objectProbesIt != value.end()) {
        metadata.objectProbes =
                ReadBakedObjectLightProbeMetadata(*objectProbesIt, context + ".objectProbes");
    }
    const auto staticModelsIt = value.find("staticModels");
    if (staticModelsIt != value.end()) {
        metadata.staticModels =
                ReadBakedStaticModelLightmapMetadata(
                        *staticModelsIt,
                        context + ".staticModels");
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

void RequireFinite(float value, const std::string& context);

Json WritePlacedBillboard(const SectorPlacedBillboard& billboard, const std::string& context)
{
    ValidatePlacedBillboard(billboard, context);

    Json json{
            {"spriteAnimationPath", billboard.spriteAnimationPath},
            {"width", billboard.sizeWorld.x},
            {"height", billboard.sizeWorld.y}
    };
    if (!billboard.keepAspectRatio) {
        json["keepAspectRatio"] = false;
    }
    if (!IsDefaultBillboardOrigin(billboard.originNormalized)) {
        json["originNormalized"] = WriteVector2(billboard.originNormalized, context + ".originNormalized");
    }
    if (billboard.directional) {
        json["directional"] = true;
        if (!IsDefaultBillboardDirectionalClips(billboard)) {
            json["frontClip"] = billboard.frontClip;
            json["backClip"] = billboard.backClip;
            json["leftClip"] = billboard.leftClip;
            json["rightClip"] = billboard.rightClip;
        }
    } else if (!billboard.clip.empty()) {
        json["clip"] = billboard.clip;
    }
    if (!billboard.playing) {
        json["playing"] = false;
    }
    return json;
}

const char* WriteSectorDoorMotionType(SectorDoorMotionType motion)
{
    switch (motion) {
        case SectorDoorMotionType::SlideVertical:
            return "slide_vertical";
        case SectorDoorMotionType::SlideLeft:
            return "slide_left";
        case SectorDoorMotionType::SlideRight:
            return "slide_right";
        case SectorDoorMotionType::Swing:
            return "swing";
    }
    Fail("door motion has an invalid value");
}

const char* WriteSectorDoorVisualType(SectorDoorVisualType visual)
{
    switch (visual) {
        case SectorDoorVisualType::Procedural: return "procedural";
        case SectorDoorVisualType::Model: return "model";
    }
    Fail("door visual has an invalid value");
}

const char* WriteSectorDoorModelFit(SectorDoorModelFit fit)
{
    switch (fit) {
        case SectorDoorModelFit::Manual: return "manual";
        case SectorDoorModelFit::FitWidth: return "fit_width";
        case SectorDoorModelFit::FitInside: return "fit_inside";
    }
    Fail("door model fit has an invalid value");
}

const char* WriteSectorDoorHinge(SectorDoorHinge hinge)
{
    switch (hinge) {
        case SectorDoorHinge::Start: return "start";
        case SectorDoorHinge::End: return "end";
    }
    Fail("door hinge has an invalid value");
}

const char* WriteSectorDoorSwingSide(SectorDoorSwingSide side)
{
    switch (side) {
        case SectorDoorSwingSide::Front: return "front";
        case SectorDoorSwingSide::Back: return "back";
    }
    Fail("door swing side has an invalid value");
}

Json WriteSectorCoordPair(SectorCoord x, SectorCoord y)
{
    return Json::array({x, y});
}

Json WriteSectorDoorAnchor(const SectorDoorAnchor& anchor)
{
    return Json{
            {"lineDefId", anchor.lineDefId},
            {"frontSectorId", anchor.frontSectorId},
            {"backSectorId", anchor.backSectorId},
            {"frontSideDefId", anchor.frontSideDefId},
            {"backSideDefId", anchor.backSideDefId},
            {"endpointA", WriteSectorCoordPair(anchor.endpointAX, anchor.endpointAY)},
            {"endpointB", WriteSectorCoordPair(anchor.endpointBX, anchor.endpointBY)}
    };
}

Json WriteDoorFaceUv(const SectorDoorFaceUv& uv, const std::string& context)
{
    ValidateDoorFaceUv(uv, context);
    return Json{
            {"scale", Json::array({uv.scale.x, uv.scale.y})},
            {"offset", Json::array({uv.offset.x, uv.offset.y})}
    };
}

Json WriteDoorFaceUvSet(const SectorDoorFaceUvSet& uvs, const std::string& context)
{
    ValidateDoorFaceUvSet(uvs, context);
    Json value = Json::object();
    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        if (!IsDefaultDoorFaceUv(uvs.faces[i])) {
            value[DoorFaceJsonNames[i]] = WriteDoorFaceUv(
                    uvs.faces[i],
                    context + "." + DoorFaceJsonNames[i]);
        }
    }
    return value;
}

Json WritePlacedDoor(const SectorPlacedDoor& door)
{
    Json json{
            {"anchor", WriteSectorDoorAnchor(door.anchor)}
    };
    if (door.width != 0.0f) {
        json["width"] = door.width;
    }
    if (door.height != 0.0f) {
        json["height"] = door.height;
    }
    if (door.thickness != 0.25f) {
        json["thickness"] = door.thickness;
    }
    if (door.normalOffset != 0.0f) {
        json["normalOffset"] = door.normalOffset;
    }
    if (door.heightOffsetWorld != 0.0f) {
        json["heightOffsetWorld"] = door.heightOffsetWorld;
    }
    if (door.visual != SectorDoorVisualType::Procedural) {
        json["visual"] = WriteSectorDoorVisualType(door.visual);
    }
    if (!door.modelAssetId.empty()) {
        json["modelAssetId"] = door.modelAssetId;
    }
    if (door.modelFit != SectorDoorModelFit::FitInside) {
        json["modelFit"] = WriteSectorDoorModelFit(door.modelFit);
    }
    if (door.modelScale != 1.0f) {
        json["modelScale"] = door.modelScale;
    }
    if (door.motion != SectorDoorMotionType::SlideVertical) {
        json["motion"] = WriteSectorDoorMotionType(door.motion);
    }
    if (door.hinge != SectorDoorHinge::Start) {
        json["hinge"] = WriteSectorDoorHinge(door.hinge);
    }
    if (door.swingSide != SectorDoorSwingSide::Front) {
        json["swingSide"] = WriteSectorDoorSwingSide(door.swingSide);
    }
    if (door.openAngleDegrees != 90.0f) {
        json["openAngleDegrees"] = door.openAngleDegrees;
    }
    if (door.angularSpeedDegrees != 90.0f) {
        json["angularSpeedDegrees"] = door.angularSpeedDegrees;
    }
    if (door.openDistance != 0.0f) {
        json["openDistance"] = door.openDistance;
    }
    if (door.speed != 1.5f) {
        json["speed"] = door.speed;
    }
    if (door.initialOpenFraction != 0.0f) {
        json["initialOpenFraction"] = door.initialOpenFraction;
    }
    if (door.autoOpen) {
        json["autoOpen"] = true;
    }
    if (door.interactionDistance != 1.5f) {
        json["interactionDistance"] = door.interactionDistance;
    }
    if (door.autoOpenDistance != 2.0f) {
        json["autoOpenDistance"] = door.autoOpenDistance;
    }
    if (!door.textureId.empty()) {
        json["textureId"] = door.textureId;
    }
    if (!door.openSoundId.empty()) {
        json["openSoundId"] = door.openSoundId;
    }
    if (!door.closeSoundId.empty()) {
        json["closeSoundId"] = door.closeSoundId;
    }
    if (!IsDefaultDoorFaceUvSet(door.faceUvs)) {
        json["faceUvs"] = WriteDoorFaceUvSet(door.faceUvs, "door.faceUvs");
    }
    return json;
}

Json WriteRuntimeObject(const SectorPlacedRuntimeObject& object, const std::string& context)
{
    if (!IsValidSectorTopologyId(object.id)) {
        Fail(context + ".id must be a positive integer");
    }
    RequireFinite(object.yawRadians, context + ".yawRadians");
    const float yawDegrees = RadiansToDegrees(object.yawRadians);
    RequireFinite(yawDegrees, context + ".yawDegrees");

    Json json{
            {"id", object.id}
    };
    if (object.kind.empty()) {
        Fail(context + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
    } else {
        if (object.kind == RuntimeObjectKindBillboard) {
            json["kind"] = object.kind;
            json["billboard"] = WritePlacedBillboard(object.billboard, context + ".billboard");
        } else if (object.kind == RuntimeObjectKindStaticModel) {
            RequireFinite(
                    object.staticModel.rotationXRadians,
                    context + ".staticModel.rotationXRadians");
            RequireFinite(
                    object.staticModel.rotationZRadians,
                    context + ".staticModel.rotationZRadians");
            const float rotationXDegrees =
                    RadiansToDegrees(object.staticModel.rotationXRadians);
            const float rotationZDegrees =
                    RadiansToDegrees(object.staticModel.rotationZRadians);
            RequireFinite(
                    rotationXDegrees,
                    context + ".staticModel.rotationXDegrees");
            RequireFinite(
                    rotationZDegrees,
                    context + ".staticModel.rotationZDegrees");
            RequireFinite(object.staticModel.heightOffsetWorld, context + ".staticModel.heightOffsetWorld");
            if (!std::isfinite(object.staticModel.scale)
                    || object.staticModel.scale <= 0.0f) {
                Fail(context + ".staticModel.scale must be a finite positive value");
            }
            Json staticModel = Json::object();
            if (!object.staticModel.modelPath.empty()) {
                staticModel["modelPath"] = object.staticModel.modelPath;
            }
            if (rotationXDegrees != 0.0f) {
                staticModel["rotationXDegrees"] = rotationXDegrees;
            }
            if (rotationZDegrees != 0.0f) {
                staticModel["rotationZDegrees"] = rotationZDegrees;
            }
            if (object.staticModel.heightOffsetWorld != 0.0f) {
                staticModel["heightOffsetWorld"] = object.staticModel.heightOffsetWorld;
            }
            if (object.staticModel.scale != 1.0f) {
                staticModel["scale"] = object.staticModel.scale;
            }
            if (object.staticModel.collision) {
                staticModel["collision"] = true;
            }
            json["kind"] = object.kind;
            json["staticModel"] = std::move(staticModel);
        } else if (object.kind == RuntimeObjectKindDynamicModel) {
            const SectorPlacedDynamicModel& model = object.dynamicModel;
            RequireFinite(model.rotationXRadians, context + ".dynamicModel.rotationXRadians");
            RequireFinite(model.rotationZRadians, context + ".dynamicModel.rotationZRadians");
            RequireFinite(model.heightOffsetWorld, context + ".dynamicModel.heightOffsetWorld");
            if (!std::isfinite(model.scale) || model.scale <= 0.0f) {
                Fail(context + ".dynamicModel.scale must be a finite positive value");
            }
            if (!std::isfinite(model.animationSpeed) || model.animationSpeed <= 0.0f) {
                Fail(context + ".dynamicModel.animationSpeed must be a finite positive value");
            }
            if (model.shadowMode != SectorDynamicModelShadowMode::None
                    && model.shadowMode != SectorDynamicModelShadowMode::Contact
                    && model.shadowMode != SectorDynamicModelShadowMode::Dynamic) {
                Fail(context + ".dynamicModel.shadowMode is invalid");
            }
            const float rotationXDegrees = RadiansToDegrees(model.rotationXRadians);
            const float rotationZDegrees = RadiansToDegrees(model.rotationZRadians);
            Json dynamicModel = Json::object();
            if (!model.modelPath.empty()) dynamicModel["modelPath"] = model.modelPath;
            if (rotationXDegrees != 0.0f) dynamicModel["rotationXDegrees"] = rotationXDegrees;
            if (rotationZDegrees != 0.0f) dynamicModel["rotationZDegrees"] = rotationZDegrees;
            if (model.heightOffsetWorld != 0.0f) dynamicModel["heightOffsetWorld"] = model.heightOffsetWorld;
            if (model.scale != 1.0f) dynamicModel["scale"] = model.scale;
            if (model.collision) dynamicModel["collision"] = true;
            if (!model.animation.empty()) dynamicModel["animation"] = model.animation;
            if (!model.loop) dynamicModel["loop"] = false;
            if (model.animationSpeed != 1.0f) dynamicModel["animationSpeed"] = model.animationSpeed;
            if (model.shadowMode == SectorDynamicModelShadowMode::None) {
                dynamicModel["shadowMode"] = "none";
            } else if (model.shadowMode == SectorDynamicModelShadowMode::Dynamic) {
                dynamicModel["shadowMode"] = "dynamic";
            }
            json["kind"] = object.kind;
            json["dynamicModel"] = std::move(dynamicModel);
        } else if (object.kind == RuntimeObjectKindNpc) {
            if (!IsValidNpcDefinitionId(object.npc.definitionId)) {
                Fail(context + ".npc.definitionId is invalid");
            }
            if (!object.npc.instanceId.empty()
                    && !IsValidNpcInstanceId(object.npc.instanceId)) {
                Fail(context + ".npc.instanceId is invalid");
            }
            if (!std::isfinite(object.npc.scale) || object.npc.scale <= 0.0f) {
                Fail(context + ".npc.scale must be a finite positive value");
            }
            if (object.npc.shadowMode != SectorDynamicModelShadowMode::None
                    && object.npc.shadowMode != SectorDynamicModelShadowMode::Contact
                    && object.npc.shadowMode
                            != SectorDynamicModelShadowMode::Dynamic) {
                Fail(context + ".npc.shadowMode is invalid");
            }
            Json npc{{"definitionId", object.npc.definitionId}};
            if (!object.npc.instanceId.empty()) npc["instanceId"] = object.npc.instanceId;
            if (object.npc.scale != 1.0f) npc["scale"] = object.npc.scale;
            if (object.npc.shadowMode == SectorDynamicModelShadowMode::None) {
                npc["shadowMode"] = "none";
            } else if (object.npc.shadowMode
                    == SectorDynamicModelShadowMode::Dynamic) {
                npc["shadowMode"] = "dynamic";
            }
            json["kind"] = object.kind;
            json["npc"] = std::move(npc);
        } else if (object.kind == RuntimeObjectKindDoor) {
            json["kind"] = object.kind;
            json["door"] = WritePlacedDoor(object.door);
        } else {
            Fail(context + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
        }
    }
    json["position"] = WriteVector3(object.position, context + ".position");
    json["yawDegrees"] = yawDegrees;
    return json;
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
            {"indirectBounceStrength", std::clamp(settings.indirectBounceStrength, 0.0f, 1.0f)},
            {"objectProbeSpacingWorld", std::clamp(settings.objectProbeSpacingWorld, 0.25f, 128.0f)},
            {"objectProbeLowerHeightWorld", std::clamp(settings.objectProbeLowerHeightWorld, 0.0f, 16.0f)},
            {"objectProbeUpperHeightWorld", std::clamp(settings.objectProbeUpperHeightWorld, 0.0f, 16.0f)}
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
    RequireFinite(
            settings.objectProbeDebugDrawMaxDistanceWorld,
            "previewSettings.objectProbeDebugDrawMaxDistanceWorld");
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
            {"headBobFrequency", normalized.headBobFrequency},
            {"objectProbeDebugDrawMaxDistanceWorld",
             normalized.objectProbeDebugDrawMaxDistanceWorld}
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

Json WriteLightDustSettings(const SectorLightDustSettings& source)
{
    const SectorLightDustSettings settings = NormalizeSectorLightDustSettings(source);
    const SectorLightDustSettings defaults;
    Json value = Json::object();
    if (settings.enabled != defaults.enabled) value["enabled"] = settings.enabled;
    if (settings.amount != defaults.amount) value["amount"] = settings.amount;
    if (settings.extentScale != defaults.extentScale) value["extentScale"] = settings.extentScale;
    if (settings.minimumSizeWorld != defaults.minimumSizeWorld) {
        value["minimumSizeWorld"] = settings.minimumSizeWorld;
    }
    if (settings.maximumSizeWorld != defaults.maximumSizeWorld) {
        value["maximumSizeWorld"] = settings.maximumSizeWorld;
    }
    if (settings.opacity != defaults.opacity) value["opacity"] = settings.opacity;
    if (settings.driftSpeedWorld != defaults.driftSpeedWorld) {
        value["driftSpeedWorld"] = settings.driftSpeedWorld;
    }
    if (settings.turbulenceWorld != defaults.turbulenceWorld) {
        value["turbulenceWorld"] = settings.turbulenceWorld;
    }
    if (settings.scatteringTint.r != defaults.scatteringTint.r
            || settings.scatteringTint.g != defaults.scatteringTint.g
            || settings.scatteringTint.b != defaults.scatteringTint.b) {
        value["scatteringTint"] = WriteColor(settings.scatteringTint);
    }
    return value;
}

Json WriteLightProxySettings(const SectorLightProxySettings& source)
{
    const SectorLightProxySettings settings = NormalizeSectorLightProxySettings(source);
    const SectorLightProxySettings defaults;
    Json value = Json::object();
    Json halo = Json::object();
    if (settings.halo.enabled != defaults.halo.enabled) halo["enabled"] = settings.halo.enabled;
    if (settings.halo.radiusWorld != defaults.halo.radiusWorld) halo["radiusWorld"] = settings.halo.radiusWorld;
    if (settings.halo.centerOffsetWorld.x != defaults.halo.centerOffsetWorld.x
            || settings.halo.centerOffsetWorld.y != defaults.halo.centerOffsetWorld.y
            || settings.halo.centerOffsetWorld.z != defaults.halo.centerOffsetWorld.z) {
        halo["centerOffsetWorld"] = WriteVector3(
                settings.halo.centerOffsetWorld, "light atmosphere halo center offset");
    }
    if (settings.halo.brightness != defaults.halo.brightness) halo["brightness"] = settings.halo.brightness;
    if (settings.halo.maxExtinction != defaults.halo.maxExtinction) halo["maxExtinction"] = settings.halo.maxExtinction;
    if (settings.halo.edgeSoftness != defaults.halo.edgeSoftness) halo["edgeSoftness"] = settings.halo.edgeSoftness;
    if (settings.halo.scatteringTint.r != defaults.halo.scatteringTint.r
            || settings.halo.scatteringTint.g != defaults.halo.scatteringTint.g
            || settings.halo.scatteringTint.b != defaults.halo.scatteringTint.b) {
        halo["scatteringTint"] = WriteColor(settings.halo.scatteringTint);
    }
    if (!halo.empty()) value["halo"] = std::move(halo);
    Json shaft = Json::object();
    if (settings.shaft.enabled != defaults.shaft.enabled) shaft["enabled"] = settings.shaft.enabled;
    if (settings.shaft.originOffsetWorld.x != defaults.shaft.originOffsetWorld.x
            || settings.shaft.originOffsetWorld.y != defaults.shaft.originOffsetWorld.y
            || settings.shaft.originOffsetWorld.z != defaults.shaft.originOffsetWorld.z) {
        shaft["originOffsetWorld"] = WriteVector3(
                settings.shaft.originOffsetWorld, "light atmosphere shaft origin offset");
    }
    if (settings.shaft.lengthScale != defaults.shaft.lengthScale) shaft["lengthScale"] = settings.shaft.lengthScale;
    if (settings.shaft.widthScale != defaults.shaft.widthScale) shaft["widthScale"] = settings.shaft.widthScale;
    if (settings.shaft.brightness != defaults.shaft.brightness) shaft["brightness"] = settings.shaft.brightness;
    if (settings.shaft.maxExtinction != defaults.shaft.maxExtinction) shaft["maxExtinction"] = settings.shaft.maxExtinction;
    if (settings.shaft.edgeSoftness != defaults.shaft.edgeSoftness) shaft["edgeSoftness"] = settings.shaft.edgeSoftness;
    if (settings.shaft.scatteringTint.r != defaults.shaft.scatteringTint.r
            || settings.shaft.scatteringTint.g != defaults.shaft.scatteringTint.g
            || settings.shaft.scatteringTint.b != defaults.shaft.scatteringTint.b) {
        shaft["scatteringTint"] = WriteColor(settings.shaft.scatteringTint);
    }
    if (!shaft.empty()) value["shaft"] = std::move(shaft);
    return value;
}

Json WriteLightAtmosphereSettings(const SectorLightAtmosphereSettings& source)
{
    const SectorLightAtmosphereSettings settings = NormalizeSectorLightAtmosphereSettings(source);
    Json value = Json::object();
    if (!IsDefaultSectorLightProxySettings(settings.proxy)) {
        value["proxy"] = WriteLightProxySettings(settings.proxy);
    }
    if (!IsDefaultSectorLightDustSettings(settings.dust)) {
        value["dust"] = WriteLightDustSettings(settings.dust);
    }
    return value;
}

template<typename T>
void WriteOptionalLightAtmosphere(Json& lightJson, const T& light)
{
    if (!IsDefaultSectorLightAtmosphereSettings(light.atmosphere)) {
        lightJson["atmosphere"] = WriteLightAtmosphereSettings(light.atmosphere);
    }
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
    WriteOptionalLightAtmosphere(lightJson, light);
    return lightJson;
}

Json WriteDynamicPointLight(const SectorTopologyDynamicPointLight& light, const std::string& context)
{
    RequireFinite(light.intensity, context + ".intensity");
    RequireFinite(light.radius, context + ".radius");
    RequireFinite(light.shadowBias, context + ".shadowBias");
    RequireFinite(light.shadowStrength, context + ".shadowStrength");
    RequireFinite(light.shadowSoftness, context + ".shadowSoftness");
    const int shadowPriority = ClampDynamicSpotLightShadowPriority(light.shadowPriority);
    const float shadowBias = ClampDynamicSpotLightShadowBias(light.shadowBias);
    const float shadowStrength = ClampDynamicSpotLightShadowStrength(light.shadowStrength);
    const float shadowSoftness = ClampDynamicSpotLightShadowSoftness(light.shadowSoftness);
    Json lightJson{
            {"id", light.id},
            {"position", WriteVector3(light.position, context + ".position")},
            {"radius", light.radius},
            {"intensity", light.intensity},
            {"color", WriteColor(light.color)}};
    if (!light.enabled) lightJson["enabled"] = false;
    WriteDynamicLightFlickerFields(lightJson, light, context);
    if (light.castsShadow) lightJson["castsShadow"] = true;
    if (shadowPriority != DynamicSpotLightDefaultShadowPriority) {
        lightJson["shadowPriority"] = shadowPriority;
    }
    if (shadowBias != DynamicSpotLightDefaultShadowBias) lightJson["shadowBias"] = shadowBias;
    if (shadowStrength != DynamicSpotLightDefaultShadowStrength) {
        lightJson["shadowStrength"] = shadowStrength;
    }
    if (shadowSoftness != DynamicSpotLightDefaultShadowSoftness) {
        lightJson["shadowSoftness"] = shadowSoftness;
    }
    WriteOptionalLightAtmosphere(lightJson, light);
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
    if (!light.castsShadow) {
        lightJson["castsShadow"] = false;
    }
    WriteOptionalLightAtmosphere(lightJson, light);
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

Json WriteFogSettings(const SectorTopologyFogSettings& settings)
{
    RequireFinite(settings.startDistanceWorld, "fogSettings.startDistanceWorld");
    RequireFinite(settings.endDistanceWorld, "fogSettings.endDistanceWorld");
    RequireFinite(settings.falloffExponent, "fogSettings.falloffExponent");
    RequireFinite(settings.brightness, "fogSettings.brightness");
    RequireFinite(settings.density, "fogSettings.density");
    RequireFinite(settings.maxOpacity, "fogSettings.maxOpacity");
    RequireFinite(settings.referenceHeightWorld, "fogSettings.referenceHeightWorld");
    RequireFinite(settings.heightFalloff, "fogSettings.heightFalloff");
    const SectorTopologyFogSettings normalized = NormalizeSectorTopologyFogSettings(settings);
    Json result{
            {"enabled", normalized.enabled},
            {"color", WriteColor(normalized.color)},
            {"startDistanceWorld", normalized.startDistanceWorld},
            {"density", normalized.density},
            {"maxOpacity", normalized.maxOpacity},
            {"referenceHeightWorld", normalized.referenceHeightWorld},
            {"heightFalloff", normalized.heightFalloff}
    };
    if (normalized.mode == SectorTopologyFogMode::Distance) result["mode"] = "distance";
    if (normalized.endDistanceWorld != DefaultSectorTopologyFogSettings().endDistanceWorld) {
        result["endDistanceWorld"] = normalized.endDistanceWorld;
    }
    if (normalized.falloffExponent != DefaultSectorTopologyFogSettings().falloffExponent) {
        result["falloffExponent"] = normalized.falloffExponent;
    }
    if (normalized.brightness != DefaultSectorTopologyFogSettings().brightness) {
        result["brightness"] = normalized.brightness;
    }
    return result;
}

bool IsDefaultFogSettings(const SectorTopologyFogSettings& settings)
{
    RequireFinite(settings.startDistanceWorld, "fogSettings.startDistanceWorld");
    RequireFinite(settings.endDistanceWorld, "fogSettings.endDistanceWorld");
    RequireFinite(settings.falloffExponent, "fogSettings.falloffExponent");
    RequireFinite(settings.brightness, "fogSettings.brightness");
    RequireFinite(settings.density, "fogSettings.density");
    RequireFinite(settings.maxOpacity, "fogSettings.maxOpacity");
    RequireFinite(settings.referenceHeightWorld, "fogSettings.referenceHeightWorld");
    RequireFinite(settings.heightFalloff, "fogSettings.heightFalloff");
    const SectorTopologyFogSettings normalized = NormalizeSectorTopologyFogSettings(settings);
    const SectorTopologyFogSettings defaults = DefaultSectorTopologyFogSettings();
    return normalized.enabled == defaults.enabled
            && normalized.mode == defaults.mode
            && normalized.color.r == defaults.color.r
            && normalized.color.g == defaults.color.g
            && normalized.color.b == defaults.color.b
            && normalized.color.a == defaults.color.a
            && normalized.startDistanceWorld == defaults.startDistanceWorld
            && normalized.endDistanceWorld == defaults.endDistanceWorld
            && normalized.falloffExponent == defaults.falloffExponent
            && normalized.brightness == defaults.brightness
            && normalized.density == defaults.density
            && normalized.maxOpacity == defaults.maxOpacity
            && normalized.referenceHeightWorld == defaults.referenceHeightWorld
            && normalized.heightFalloff == defaults.heightFalloff;
}

Json WriteIlluminationStatistics(const SectorIlluminationStatistics& statistics);

Json WriteBakedObjectLightProbeMetadata(const SectorBakedObjectLightProbeMetadata& metadata)
{
    Json result{
            {"path", metadata.path},
            {"version", metadata.version},
            {"sourceHash", metadata.sourceHash},
            {"count", metadata.count},
            {"probeSpacingWorld", metadata.probeSpacingWorld},
            {"probeLowerHeightWorld", metadata.probeLowerHeightWorld},
            {"probeUpperHeightWorld", metadata.probeUpperHeightWorld},
            {"format", metadata.format}
    };
    if (metadata.storedStatistics.sampleCount > 0) {
        result["storedStatistics"] =
                WriteIlluminationStatistics(metadata.storedStatistics);
    }
    return result;
}

Json WriteIlluminationStatistics(const SectorIlluminationStatistics& statistics)
{
    return Json{
            {"rgbMin", WriteVector3(statistics.rgbMin, "illumination statistics rgbMin")},
            {"rgbMax", WriteVector3(statistics.rgbMax, "illumination statistics rgbMax")},
            {"auxiliaryMin", statistics.auxiliaryMin},
            {"auxiliaryMax", statistics.auxiliaryMax},
            {"sampleCount", statistics.sampleCount},
            {"rgbChannelsAboveOne", statistics.rgbChannelsAboveOne}};
}

Json WriteBakedStaticModelLightmapMetadata(
        const SectorBakedStaticModelLightmapMetadata& metadata)
{
    return Json{
            {"path", metadata.path},
            {"version", metadata.version},
            {"sourceHash", metadata.sourceHash},
            {"modelCount", metadata.modelCount},
            {"objectCount", metadata.objectCount},
            {"format", metadata.format}
    };
}

Json WriteBakedLightmap(const SectorLightmapMetadata& metadata)
{
    std::vector<std::string> atlasPaths{metadata.path};
    for (const SectorLightmapAtlasMetadata& atlas : metadata.additionalAtlases) {
        if (atlas.path.empty() || atlas.width <= 0 || atlas.height <= 0) {
            Fail("bakedLightmap.additionalAtlases entries must have a path and positive dimensions");
        }
        if (atlas.width != metadata.width || atlas.height != metadata.height) {
            Fail("bakedLightmap.additionalAtlases dimensions must match the primary atlas");
        }
        if (std::find(atlasPaths.begin(), atlasPaths.end(), atlas.path)
                != atlasPaths.end()) {
            Fail("bakedLightmap.additionalAtlases contains a duplicate path");
        }
        atlasPaths.push_back(atlas.path);
    }
    Json lightmap = Json{
            {"path", metadata.path},
            {"width", metadata.width},
            {"height", metadata.height},
            {"version", metadata.version},
            {"format", metadata.format},
            {"sourceHash", metadata.sourceHash}
    };
    if (metadata.storedStatistics.sampleCount > 0) {
        lightmap["storedStatistics"] =
                WriteIlluminationStatistics(metadata.storedStatistics);
    }
    if (!metadata.objectProbes.path.empty()) {
        lightmap["objectProbes"] = WriteBakedObjectLightProbeMetadata(metadata.objectProbes);
    }
    if (!metadata.staticModels.path.empty()) {
        lightmap["staticModels"] =
                WriteBakedStaticModelLightmapMetadata(metadata.staticModels);
    }
    if (!metadata.additionalAtlases.empty()) {
        lightmap["additionalAtlases"] = Json::array();
        for (const SectorLightmapAtlasMetadata& atlas
                : metadata.additionalAtlases) {
            lightmap["additionalAtlases"].push_back(Json{
                    {"path", atlas.path},
                    {"width", atlas.width},
                    {"height", atlas.height}});
        }
    }
    return lightmap;
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
    ValidateAudioSettings(map.audioSettings, "root.audio");
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
    ValidateAudioSettings(map.audioSettings, "root.audio");
    const auto issues = ValidateSectorTopologyMap(map);
    const auto error = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    if (error != issues.end()) {
        Fail("Authoring map-level validation failed: " + FormatSectorTopologyValidationIssue(*error));
    }
}

void ValidateRuntimeObjects(const SectorTopologyMap& map, const std::string& context)
{
    std::vector<int> objectIds;
    objectIds.reserve(map.runtimeObjects.size());
    std::set<std::string> npcInstanceIds;
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        const std::string objectContext = context + ".runtimeObjects[" + std::to_string(object.id) + "]";
        if (!IsValidSectorTopologyId(object.id)) {
            Fail(objectContext + ".id must be a positive integer");
        }
        if (object.kind.empty()) {
            Fail(objectContext + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
        } else {
            if (object.kind == RuntimeObjectKindBillboard) {
                ValidatePlacedBillboard(object.billboard, objectContext + ".billboard");
            } else if (object.kind == RuntimeObjectKindStaticModel) {
                if (!std::isfinite(object.staticModel.rotationXRadians)) {
                    Fail(objectContext + ".staticModel.rotationXRadians must be finite");
                }
                if (!std::isfinite(object.staticModel.rotationZRadians)) {
                    Fail(objectContext + ".staticModel.rotationZRadians must be finite");
                }
                if (!std::isfinite(object.staticModel.heightOffsetWorld)) {
                    Fail(objectContext + ".staticModel.heightOffsetWorld must be finite");
                }
                if (!std::isfinite(object.staticModel.scale)
                        || object.staticModel.scale <= 0.0f) {
                    Fail(objectContext + ".staticModel.scale must be a finite positive value");
                }
            } else if (object.kind == RuntimeObjectKindDynamicModel) {
                const SectorPlacedDynamicModel& model = object.dynamicModel;
                if (!std::isfinite(model.rotationXRadians)
                        || !std::isfinite(model.rotationZRadians)
                        || !std::isfinite(model.heightOffsetWorld)) {
                    Fail(objectContext + ".dynamicModel transforms must be finite");
                }
                if (!std::isfinite(model.scale) || model.scale <= 0.0f) {
                    Fail(objectContext + ".dynamicModel.scale must be a finite positive value");
                }
                if (!std::isfinite(model.animationSpeed) || model.animationSpeed <= 0.0f) {
                    Fail(objectContext + ".dynamicModel.animationSpeed must be a finite positive value");
                }
                if (model.shadowMode != SectorDynamicModelShadowMode::None
                        && model.shadowMode != SectorDynamicModelShadowMode::Contact
                        && model.shadowMode != SectorDynamicModelShadowMode::Dynamic) {
                    Fail(objectContext + ".dynamicModel.shadowMode is invalid");
                }
            } else if (object.kind == RuntimeObjectKindNpc) {
                if (!IsValidNpcDefinitionId(object.npc.definitionId)) {
                    Fail(objectContext + ".npc.definitionId is invalid");
                }
                if (!object.npc.instanceId.empty()) {
                    if (!IsValidNpcInstanceId(object.npc.instanceId)) {
                        Fail(objectContext + ".npc.instanceId is invalid");
                    }
                    if (!npcInstanceIds.insert(object.npc.instanceId).second) {
                        Fail(objectContext + ".npc.instanceId duplicates another NPC instance ID");
                    }
                }
                if (!std::isfinite(object.npc.scale) || object.npc.scale <= 0.0f) {
                    Fail(objectContext + ".npc.scale must be a finite positive value");
                }
                if (object.npc.shadowMode != SectorDynamicModelShadowMode::None
                        && object.npc.shadowMode != SectorDynamicModelShadowMode::Contact
                        && object.npc.shadowMode
                                != SectorDynamicModelShadowMode::Dynamic) {
                    Fail(objectContext + ".npc.shadowMode is invalid");
                }
            } else if (object.kind == RuntimeObjectKindDoor) {
                ValidatePlacedDoorForSerialization(object.door, objectContext + ".door");
            } else {
                Fail(objectContext + ".kind must be 'billboard', 'static_model', 'dynamic_model', 'npc', or 'door'");
            }
        }
        if (!std::isfinite(object.position.x)
                || !std::isfinite(object.position.y)
                || !std::isfinite(object.position.z)) {
            Fail(objectContext + ".position values must be finite floats");
        }
        if (!std::isfinite(object.yawRadians)) {
            Fail(objectContext + ".yawRadians must be finite");
        }
        if (std::find(objectIds.begin(), objectIds.end(), object.id) != objectIds.end()) {
            Fail(objectContext + ".id duplicates another runtime object ID");
        }
        objectIds.push_back(object.id);
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

SectorLightDustSettings ReadLightDustSettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) {
        Fail(context + " must be an object");
    }
    SectorLightDustSettings settings;
    settings.enabled = ReadOptionalBool(value, "enabled", context, settings.enabled);
    settings.amount = ReadOptionalClampedInt(value, "amount", context, settings.amount, 0, 128);
    settings.extentScale = ReadOptionalClampedFloat(
            value, "extentScale", context, settings.extentScale, 0.05f, 2.0f);
    settings.minimumSizeWorld = ReadOptionalClampedFloat(
            value,
            "minimumSizeWorld",
            context,
            settings.minimumSizeWorld,
            0.002f,
            0.25f);
    settings.maximumSizeWorld = ReadOptionalClampedFloat(
            value,
            "maximumSizeWorld",
            context,
            settings.maximumSizeWorld,
            0.002f,
            0.25f);
    settings.opacity = ReadOptionalClampedFloat(
            value, "opacity", context, settings.opacity, 0.0f, 1.0f);
    settings.driftSpeedWorld = ReadOptionalClampedFloat(
            value, "driftSpeedWorld", context, settings.driftSpeedWorld, 0.0f, 0.5f);
    settings.turbulenceWorld = ReadOptionalClampedFloat(
            value, "turbulenceWorld", context, settings.turbulenceWorld, 0.0f, 0.5f);
    const auto tintIt = value.find("scatteringTint");
    if (tintIt != value.end()) {
        settings.scatteringTint = ReadColor(*tintIt, context + ".scatteringTint");
    }
    return NormalizeSectorLightDustSettings(settings);
}

SectorLightProxySettings ReadLightProxySettings(const Json& value, const std::string& context)
{
    if (!value.is_object()) Fail(context + " must be an object");
    SectorLightProxySettings settings;
    const auto tintIt = value.find("tint");
    if (tintIt != value.end()) {
        const Color legacyTint = ReadColor(*tintIt, context + ".tint");
        settings.halo.scatteringTint = legacyTint;
        settings.shaft.scatteringTint = legacyTint;
    }
    const auto haloIt = value.find("halo");
    if (haloIt != value.end()) {
        if (!haloIt->is_object()) Fail(context + ".halo must be an object");
        settings.halo.enabled = ReadOptionalBool(*haloIt, "enabled", context + ".halo", settings.halo.enabled);
        settings.halo.radiusWorld = ReadOptionalClampedFloat(*haloIt, "radiusWorld", context + ".halo", settings.halo.radiusWorld, 0.01f, 64.0f);
        const auto centerOffsetIt = haloIt->find("centerOffsetWorld");
        if (centerOffsetIt != haloIt->end()) {
            settings.halo.centerOffsetWorld = ReadVector3(
                    *centerOffsetIt, context + ".halo.centerOffsetWorld");
        }
        settings.halo.brightness = ReadOptionalClampedFloat(*haloIt, "brightness", context + ".halo", settings.halo.brightness, 0.0f, 16.0f);
        settings.halo.maxExtinction = ReadOptionalClampedFloat(*haloIt, "maxOpacity", context + ".halo", settings.halo.maxExtinction, 0.0f, 1.0f);
        settings.halo.maxExtinction = ReadOptionalClampedFloat(*haloIt, "maxExtinction", context + ".halo", settings.halo.maxExtinction, 0.0f, 1.0f);
        settings.halo.edgeSoftness = ReadOptionalClampedFloat(*haloIt, "edgeSoftness", context + ".halo", settings.halo.edgeSoftness, 0.01f, 1.0f);
        const auto scatteringTintIt = haloIt->find("scatteringTint");
        if (scatteringTintIt != haloIt->end()) {
            settings.halo.scatteringTint = ReadColor(
                    *scatteringTintIt, context + ".halo.scatteringTint");
        }
    }
    const auto shaftIt = value.find("shaft");
    if (shaftIt != value.end()) {
        if (!shaftIt->is_object()) Fail(context + ".shaft must be an object");
        settings.shaft.enabled = ReadOptionalBool(*shaftIt, "enabled", context + ".shaft", settings.shaft.enabled);
        const auto originOffsetIt = shaftIt->find("originOffsetWorld");
        if (originOffsetIt != shaftIt->end()) {
            settings.shaft.originOffsetWorld = ReadVector3(
                    *originOffsetIt, context + ".shaft.originOffsetWorld");
        }
        settings.shaft.lengthScale = ReadOptionalClampedFloat(*shaftIt, "lengthScale", context + ".shaft", settings.shaft.lengthScale, 0.01f, 2.0f);
        settings.shaft.widthScale = ReadOptionalClampedFloat(*shaftIt, "widthScale", context + ".shaft", settings.shaft.widthScale, 0.01f, 2.0f);
        settings.shaft.brightness = ReadOptionalClampedFloat(*shaftIt, "brightness", context + ".shaft", settings.shaft.brightness, 0.0f, 16.0f);
        settings.shaft.maxExtinction = ReadOptionalClampedFloat(*shaftIt, "maxOpacity", context + ".shaft", settings.shaft.maxExtinction, 0.0f, 1.0f);
        settings.shaft.maxExtinction = ReadOptionalClampedFloat(*shaftIt, "maxExtinction", context + ".shaft", settings.shaft.maxExtinction, 0.0f, 1.0f);
        settings.shaft.edgeSoftness = ReadOptionalClampedFloat(*shaftIt, "edgeSoftness", context + ".shaft", settings.shaft.edgeSoftness, 0.01f, 1.0f);
        const auto scatteringTintIt = shaftIt->find("scatteringTint");
        if (scatteringTintIt != shaftIt->end()) {
            settings.shaft.scatteringTint = ReadColor(
                    *scatteringTintIt, context + ".shaft.scatteringTint");
        }
    }
    return NormalizeSectorLightProxySettings(settings);
}

SectorLightAtmosphereSettings ReadOptionalLightAtmosphereSettings(
        const Json& light,
        const std::string& context)
{
    SectorLightAtmosphereSettings settings;
    const auto atmosphereIt = light.find("atmosphere");
    if (atmosphereIt == light.end()) {
        return settings;
    }
    if (!atmosphereIt->is_object()) {
        Fail(context + ".atmosphere must be an object");
    }
    const auto proxyIt = atmosphereIt->find("proxy");
    if (proxyIt != atmosphereIt->end()) {
        settings.proxy = ReadLightProxySettings(*proxyIt, context + ".atmosphere.proxy");
    }
    const auto dustIt = atmosphereIt->find("dust");
    if (dustIt != atmosphereIt->end()) {
        settings.dust = ReadLightDustSettings(*dustIt, context + ".atmosphere.dust");
    }
    return NormalizeSectorLightAtmosphereSettings(settings);
}

void ReadMapLevelFields(const Json& root, SectorTopologyMap& map, bool allowBakedLightmap)
{
    const auto audioIt = root.find("audio");
    if (audioIt != root.end()) ReadAudioSettings(*audioIt, map.audioSettings);

    const auto levelMarkersIt = root.find("levelMarkers");
    if (levelMarkersIt != root.end()) {
        if (!levelMarkersIt->is_array()) {
            Fail("root.levelMarkers must be an array");
        }
        for (size_t i = 0; i < levelMarkersIt->size(); ++i) {
            const Json& value = (*levelMarkersIt)[i];
            const std::string context = "root.levelMarkers[" + std::to_string(i) + "]";
            if (!value.is_object()) {
                Fail(context + " must be an object");
            }
            SectorCompiledLevelMarker marker;
            marker.sourceAuthoringMarkerId = ReadInt(value, "editorId", context);
            marker.id = ReadString(value, "id", context);
            marker.position = ReadVector3(RequireField(value, "position", context), context + ".position");
            SectorCoord exactX = 0;
            SectorCoord exactZ = 0;
            if (!VisibleAuthoringToSectorCoord(marker.position.x, exactX)
                    || !VisibleAuthoringToSectorCoord(marker.position.z, exactZ)) {
                Fail(context + ".position X/Z must be exact authoring coordinates");
            }
            marker.yawRadians = ReadFloat(value, "orientationDegrees", context) * (Pi / 180.0f);
            map.levelMarkers.push_back(std::move(marker));
        }
    }

    const auto triggersIt = root.find("triggers");
    if (triggersIt != root.end()) {
        if (!triggersIt->is_array()) Fail("root.triggers must be an array");
        for (size_t i = 0; i < triggersIt->size(); ++i) {
            const Json& value = (*triggersIt)[i];
            const std::string context = "root.triggers[" + std::to_string(i) + "]";
            if (!value.is_object()) Fail(context + " must be an object");
            SectorCompiledTrigger trigger;
            trigger.sourceAuthoringTriggerId = ReadInt(value, "editorId", context);
            trigger.id = ReadString(value, "id", context);
            const std::string shape = ReadString(value, "shape", context);
            if (shape == "rectangle") trigger.shape = SectorTriggerShapeKind::Rectangle;
            else if (shape == "polygon") trigger.shape = SectorTriggerShapeKind::Polygon;
            else Fail(context + ".shape must be 'rectangle' or 'polygon'");
            const Json& points = RequireArrayField(value, "points", context);
            for (size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
                const std::string pointContext = context + ".points[" + std::to_string(pointIndex) + "]";
                if (!points[pointIndex].is_object()) Fail(pointContext + " must be an object");
                trigger.points.push_back(SectorTriggerPoint{
                        ReadCoord(points[pointIndex], "x", pointContext),
                        ReadCoord(points[pointIndex], "z", pointContext)});
            }
            trigger.enabled = ReadOptionalBool(value, "enabled", context, true);
            trigger.repeat = ReadOptionalBool(value, "repeat", context, false);
            trigger.delayMilliseconds = value.contains("delayMilliseconds")
                    ? ReadInt(value, "delayMilliseconds", context) : 0;
            trigger.script = ReadOptionalString(value, "script", context);
            std::string geometryError;
            if (!IsValidSectorAuthoringId(trigger.sourceAuthoringTriggerId)
                    || !IsValidSectorTriggerReferenceId(trigger.id)
                    || !IsValidSectorTriggerScriptName(trigger.script)
                    || trigger.delayMilliseconds < 0
                    || !ValidateSectorTriggerPolygon(trigger.points, trigger.shape, &geometryError)) {
                Fail(context + " is invalid" + (geometryError.empty() ? std::string{} : ": " + geometryError));
            }
            map.triggers.push_back(std::move(trigger));
        }
        std::set<int> editorIds;
        std::set<std::string> ids;
        for (const SectorCompiledTrigger& trigger : map.triggers) {
            if (!editorIds.insert(trigger.sourceAuthoringTriggerId).second) Fail("root.triggers has duplicate editorId");
            if (!ids.insert(trigger.id).second) Fail("root.triggers has duplicate id");
        }
    }

    const auto runtimeObjectsIt = root.find("runtimeObjects");
    if (runtimeObjectsIt != root.end()) {
        if (!runtimeObjectsIt->is_array()) {
            Fail("root.runtimeObjects must be an array");
        }
        const Json& runtimeObjects = *runtimeObjectsIt;
        for (size_t i = 0; i < runtimeObjects.size(); ++i) {
            const std::string context = "root.runtimeObjects[" + std::to_string(i) + "]";
            map.runtimeObjects.push_back(ReadRuntimeObject(runtimeObjects[i], context));
        }
    }

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
            light.atmosphere = ReadOptionalLightAtmosphereSettings(value, context);
            light.castsShadow = ReadOptionalBool(value, "castsShadow", context, true);
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
            light.atmosphere = ReadOptionalLightAtmosphereSettings(value, context);
            light.castsShadow = ReadOptionalBool(value, "castsShadow", context, true);
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
            light.atmosphere = ReadOptionalLightAtmosphereSettings(value, context);
            light.castsShadow = ReadOptionalBool(value, "castsShadow", context, false);
            light.shadowPriority = ReadOptionalClampedInt(
                    value, "shadowPriority", context,
                    DynamicSpotLightDefaultShadowPriority,
                    DynamicSpotLightMinShadowPriority,
                    DynamicSpotLightMaxShadowPriority);
            light.shadowBias = ReadOptionalClampedFloat(
                    value, "shadowBias", context,
                    DynamicSpotLightDefaultShadowBias,
                    DynamicSpotLightMinShadowBias,
                    DynamicSpotLightMaxShadowBias);
            light.shadowStrength = ReadOptionalClampedFloat(
                    value, "shadowStrength", context,
                    DynamicSpotLightDefaultShadowStrength,
                    DynamicSpotLightMinShadowStrength,
                    DynamicSpotLightMaxShadowStrength);
            light.shadowSoftness = ReadOptionalClampedFloat(
                    value, "shadowSoftness", context,
                    DynamicSpotLightDefaultShadowSoftness,
                    DynamicSpotLightMinShadowSoftness,
                    DynamicSpotLightMaxShadowSoftness);
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
            light.atmosphere = ReadOptionalLightAtmosphereSettings(value, context);
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

    const auto fogSettingsIt = root.find("fogSettings");
    if (fogSettingsIt != root.end()) {
        map.fogSettings = ReadFogSettings(*fogSettingsIt, "root.fogSettings");
    }

    const auto bakedLightmapIt = root.find("bakedLightmap");
    if (bakedLightmapIt != root.end() && allowBakedLightmap) {
        map.bakedLightmap = ReadBakedLightmap(*bakedLightmapIt, "root.bakedLightmap");
    }

    map.previewSettings = NormalizeSectorPreviewSettings(map.previewSettings);
    map.skySettings = NormalizeSectorTopologySkySettings(map.skySettings);
    map.directionalLight = NormalizeSectorTopologyDirectionalLightSettings(map.directionalLight);
    map.fogSettings = NormalizeSectorTopologyFogSettings(map.fogSettings);
    ValidateRuntimeObjects(map, "root");
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
        anchor.footstepSet = ReadOptionalString(faceAnchors[i], "footstepSet", context);
        ValidateOptionalFootstepSet(anchor.footstepSet, context + ".footstepSet");
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

    const auto fogVolumesIt = value.find("fogVolumes");
    if (fogVolumesIt != value.end()) {
        if (!fogVolumesIt->is_array()) {
            Fail("root.authoringGraph.fogVolumes must be an array");
        }
        for (size_t i = 0; i < fogVolumesIt->size(); ++i) {
            const Json& fogJson = (*fogVolumesIt)[i];
            const std::string context = "root.authoringGraph.fogVolumes[" + std::to_string(i) + "]";
            if (!fogJson.is_object()) {
                Fail(context + " must be an object");
            }
            SectorAuthoringFogVolume volume;
            volume.id = ReadInt(fogJson, "id", context);
            volume.x = ReadCoord(fogJson, "x", context);
            volume.y = ReadCoord(fogJson, "y", context);
            volume.enabled = ReadOptionalBool(fogJson, "enabled", context, volume.enabled);
            const auto shapeIt = fogJson.find("shape");
            if (shapeIt != fogJson.end()) {
                if (!shapeIt->is_string()) Fail(context + ".shape must be a string");
                const std::string shape = shapeIt->get<std::string>();
                if (shape == "ellipsoid") volume.shape = SectorLocalFogShape::Ellipsoid;
                else if (shape == "box") volume.shape = SectorLocalFogShape::Box;
                else Fail(context + ".shape must be 'ellipsoid' or 'box'");
            }
            const auto analyticStyleIt = fogJson.find("analyticStyle");
            const auto legacyBoxStyleIt = fogJson.find("boxStyle");
            if (analyticStyleIt != fogJson.end()
                    && legacyBoxStyleIt != fogJson.end()) {
                Fail(context + " cannot contain both .analyticStyle and legacy .boxStyle");
            }
            const auto styleIt = analyticStyleIt != fogJson.end()
                    ? analyticStyleIt
                    : legacyBoxStyleIt;
            if (styleIt != fogJson.end()) {
                const char* field = analyticStyleIt != fogJson.end()
                        ? ".analyticStyle"
                        : ".boxStyle";
                if (!styleIt->is_string()) Fail(context + field + " must be a string");
                const std::string analyticStyle = styleIt->get<std::string>();
                if (analyticStyle == "cloudy") volume.analyticStyle = SectorAnalyticFogStyle::Cloudy;
                else if (analyticStyle == "room") volume.analyticStyle = SectorAnalyticFogStyle::Room;
                else Fail(context + field + " must be 'cloudy' or 'room'");
            }
            volume.yawDegrees = ReadOptionalFloat(
                    fogJson, "yawDegrees", context, volume.yawDegrees);
            volume.bottomOffsetWorld = ReadOptionalFloat(
                    fogJson, "bottomOffsetWorld", context, volume.bottomOffsetWorld);
            volume.radiusXWorld = ReadOptionalFloat(fogJson, "radiusXWorld", context, volume.radiusXWorld);
            volume.radiusZWorld = ReadOptionalFloat(fogJson, "radiusZWorld", context, volume.radiusZWorld);
            volume.heightWorld = ReadOptionalFloat(fogJson, "heightWorld", context, volume.heightWorld);
            const auto colorIt = fogJson.find("color");
            if (colorIt != fogJson.end()) {
                if (!colorIt->is_object()) {
                    Fail(context + ".color must be an object");
                }
                volume.color = Color{
                        ReadOptionalColorChannel(*colorIt, "r", context + ".color", volume.color.r),
                        ReadOptionalColorChannel(*colorIt, "g", context + ".color", volume.color.g),
                        ReadOptionalColorChannel(*colorIt, "b", context + ".color", volume.color.b),
                        255};
            }
            volume.maxOpacity = ReadOptionalFloat(fogJson, "maxOpacity", context, volume.maxOpacity);
            volume.analyticStartDistanceWorld = ReadOptionalFloat(
                    fogJson, "analyticStartDistanceWorld", context, volume.analyticStartDistanceWorld);
            volume.analyticEndDistanceWorld = ReadOptionalFloat(
                    fogJson, "analyticEndDistanceWorld", context, volume.analyticEndDistanceWorld);
            volume.analyticFalloffExponent = ReadOptionalFloat(
                    fogJson, "analyticFalloffExponent", context, volume.analyticFalloffExponent);
            volume.edgeSoftness = ReadOptionalFloat(fogJson, "edgeSoftness", context, volume.edgeSoftness);
            volume.noiseScaleWorld = ReadOptionalFloat(
                    fogJson, "noiseScaleWorld", context, volume.noiseScaleWorld);
            volume.noiseAmount = ReadOptionalFloat(fogJson, "noiseAmount", context, volume.noiseAmount);
            volume.flowDirectionDegrees = ReadOptionalFloat(
                    fogJson, "flowDirectionDegrees", context, volume.flowDirectionDegrees);
            volume.flowSpeedWorld = ReadOptionalFloat(
                    fogJson, "flowSpeedWorld", context, volume.flowSpeedWorld);
            graph.fogVolumes.push_back(NormalizeSectorAuthoringFogVolume(volume));
        }
    }

    const auto levelMarkersIt = value.find("levelMarkers");
    if (levelMarkersIt != value.end()) {
        if (!levelMarkersIt->is_array()) {
            Fail("root.authoringGraph.levelMarkers must be an array");
        }
        for (size_t i = 0; i < levelMarkersIt->size(); ++i) {
            const Json& markerJson = (*levelMarkersIt)[i];
            const std::string context = "root.authoringGraph.levelMarkers[" + std::to_string(i) + "]";
            if (!markerJson.is_object()) {
                Fail(context + " must be an object");
            }
            SectorAuthoringLevelMarker marker;
            marker.id = ReadInt(markerJson, "editorId", context);
            marker.referenceId = ReadString(markerJson, "id", context);
            marker.x = ReadCoord(markerJson, "x", context);
            marker.y = ReadFloat(markerJson, "y", context);
            marker.z = ReadCoord(markerJson, "z", context);
            marker.orientationDegrees = ReadFloat(markerJson, "orientationDegrees", context);
            graph.levelMarkers.push_back(std::move(marker));
        }
    }

    const auto triggersIt = value.find("triggers");
    if (triggersIt != value.end()) {
        if (!triggersIt->is_array()) Fail("root.authoringGraph.triggers must be an array");
        for (size_t i = 0; i < triggersIt->size(); ++i) {
            const Json& triggerJson = (*triggersIt)[i];
            const std::string context = "root.authoringGraph.triggers[" + std::to_string(i) + "]";
            if (!triggerJson.is_object()) Fail(context + " must be an object");
            SectorAuthoringTrigger trigger;
            trigger.editorId = ReadInt(triggerJson, "editorId", context);
            trigger.id = ReadString(triggerJson, "id", context);
            const std::string shape = ReadString(triggerJson, "shape", context);
            if (shape == "rectangle") trigger.shape = SectorTriggerShapeKind::Rectangle;
            else if (shape == "polygon") trigger.shape = SectorTriggerShapeKind::Polygon;
            else Fail(context + ".shape must be 'rectangle' or 'polygon'");
            const Json& points = RequireArrayField(triggerJson, "points", context);
            for (size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
                const std::string pointContext = context + ".points[" + std::to_string(pointIndex) + "]";
                if (!points[pointIndex].is_object()) Fail(pointContext + " must be an object");
                trigger.points.push_back(SectorTriggerPoint{
                        ReadCoord(points[pointIndex], "x", pointContext),
                        ReadCoord(points[pointIndex], "z", pointContext)});
            }
            trigger.enabled = ReadOptionalBool(triggerJson, "enabled", context, true);
            trigger.repeat = ReadOptionalBool(triggerJson, "repeat", context, false);
            trigger.delayMilliseconds = triggerJson.contains("delayMilliseconds")
                    ? ReadInt(triggerJson, "delayMilliseconds", context) : 0;
            trigger.script = ReadOptionalString(triggerJson, "script", context);
            graph.triggers.push_back(std::move(trigger));
        }
    }

    const std::vector<SectorAuthoringValidationIssue> issues =
            ValidateSectorAuthoringGraphReferences(graph);
    const auto markerError = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return (issue.objectKind == SectorAuthoringObjectKind::LevelMarker
                        || issue.objectKind == SectorAuthoringObjectKind::Trigger)
                && issue.severity == SectorAuthoringValidationSeverity::Error;
    });
    if (markerError != issues.end()) {
        Fail("Invalid authoring map object: " + markerError->message);
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
    document.derivation.topology.runtimeObjects = document.mapData.runtimeObjects;
    document.derivation.topology.previewSettings = document.mapData.previewSettings;
    document.derivation.topology.skySettings = document.mapData.skySettings;
    document.derivation.topology.directionalLight = document.mapData.directionalLight;
    document.derivation.topology.fogSettings = document.mapData.fogSettings;
    document.derivation.topology.audioSettings = document.mapData.audioSettings;
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
    document.editorSettings = ReadAuthoringEditorSettings(root);
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
    ValidateRuntimeObjects(map, "root");

    if (!map.runtimeObjects.empty()) {
        root["runtimeObjects"] = Json::array();
        for (const SectorPlacedRuntimeObject* object : SortedById(map.runtimeObjects)) {
            const std::string context = "runtime object " + std::to_string(object->id);
            root["runtimeObjects"].push_back(WriteRuntimeObject(*object, context));
        }
    }

    root["staticLights"] = Json::array();
    for (const SectorTopologyStaticPointLight* light : SortedById(map.staticLights)) {
        const std::string context = "static light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        RequireFinite(light->sourceRadius, context + ".sourceRadius");
        Json lightJson{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"sourceRadius", light->sourceRadius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        };
        if (!light->castsShadow) {
            lightJson["castsShadow"] = false;
        }
        WriteOptionalLightAtmosphere(lightJson, *light);
        root["staticLights"].push_back(std::move(lightJson));
    }

    root["staticSpotLights"] = Json::array();
    for (const SectorTopologyStaticSpotLight* light : SortedById(map.staticSpotLights)) {
        const std::string context = "static spot light " + std::to_string(light->id);
        root["staticSpotLights"].push_back(WriteStaticSpotLight(*light, context));
    }

    root["dynamicPointLights"] = Json::array();
    for (const SectorTopologyDynamicPointLight* light : SortedById(map.dynamicPointLights)) {
        const std::string context = "dynamic point light " + std::to_string(light->id);
        root["dynamicPointLights"].push_back(WriteDynamicPointLight(*light, context));
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
    if (!IsDefaultFogSettings(map.fogSettings)) {
        root["fogSettings"] = WriteFogSettings(map.fogSettings);
    }
    WriteAudioSettings(root, map.audioSettings);
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
        if (!anchor->footstepSet.empty()) {
            ValidateOptionalFootstepSet(anchor->footstepSet, context + ".footstepSet");
            anchorJson["footstepSet"] = anchor->footstepSet;
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

    if (!graph.fogVolumes.empty()) {
        graphJson["fogVolumes"] = Json::array();
        const SectorAuthoringFogVolume defaults;
        for (const SectorAuthoringFogVolume* source : SortedById(graph.fogVolumes)) {
            const std::string context = "authoring fog volume " + std::to_string(source->id);
            RequireFinite(source->bottomOffsetWorld, context + ".bottomOffsetWorld");
            RequireFinite(source->yawDegrees, context + ".yawDegrees");
            RequireFinite(source->radiusXWorld, context + ".radiusXWorld");
            RequireFinite(source->radiusZWorld, context + ".radiusZWorld");
            RequireFinite(source->heightWorld, context + ".heightWorld");
            RequireFinite(source->maxOpacity, context + ".maxOpacity");
            RequireFinite(source->analyticStartDistanceWorld, context + ".analyticStartDistanceWorld");
            RequireFinite(source->analyticEndDistanceWorld, context + ".analyticEndDistanceWorld");
            RequireFinite(source->analyticFalloffExponent, context + ".analyticFalloffExponent");
            RequireFinite(source->edgeSoftness, context + ".edgeSoftness");
            RequireFinite(source->noiseScaleWorld, context + ".noiseScaleWorld");
            RequireFinite(source->noiseAmount, context + ".noiseAmount");
            RequireFinite(source->flowDirectionDegrees, context + ".flowDirectionDegrees");
            RequireFinite(source->flowSpeedWorld, context + ".flowSpeedWorld");
            const SectorAuthoringFogVolume volume = NormalizeSectorAuthoringFogVolume(*source);
            Json fogJson{{"id", volume.id}, {"x", volume.x}, {"y", volume.y}};
            if (volume.enabled != defaults.enabled) fogJson["enabled"] = volume.enabled;
            if (volume.shape != defaults.shape) fogJson["shape"] = "box";
            if (volume.analyticStyle != defaults.analyticStyle) fogJson["analyticStyle"] = "room";
            if (volume.yawDegrees != defaults.yawDegrees) fogJson["yawDegrees"] = volume.yawDegrees;
            if (volume.bottomOffsetWorld != defaults.bottomOffsetWorld) fogJson["bottomOffsetWorld"] = volume.bottomOffsetWorld;
            if (volume.radiusXWorld != defaults.radiusXWorld) fogJson["radiusXWorld"] = volume.radiusXWorld;
            if (volume.radiusZWorld != defaults.radiusZWorld) fogJson["radiusZWorld"] = volume.radiusZWorld;
            if (volume.heightWorld != defaults.heightWorld) fogJson["heightWorld"] = volume.heightWorld;
            if (volume.color.r != defaults.color.r || volume.color.g != defaults.color.g
                    || volume.color.b != defaults.color.b) fogJson["color"] = WriteColor(volume.color);
            if (volume.maxOpacity != defaults.maxOpacity) fogJson["maxOpacity"] = volume.maxOpacity;
            if (volume.analyticStartDistanceWorld != defaults.analyticStartDistanceWorld) fogJson["analyticStartDistanceWorld"] = volume.analyticStartDistanceWorld;
            if (volume.analyticEndDistanceWorld != defaults.analyticEndDistanceWorld) fogJson["analyticEndDistanceWorld"] = volume.analyticEndDistanceWorld;
            if (volume.analyticFalloffExponent != defaults.analyticFalloffExponent) fogJson["analyticFalloffExponent"] = volume.analyticFalloffExponent;
            if (volume.edgeSoftness != defaults.edgeSoftness) fogJson["edgeSoftness"] = volume.edgeSoftness;
            if (volume.noiseScaleWorld != defaults.noiseScaleWorld) fogJson["noiseScaleWorld"] = volume.noiseScaleWorld;
            if (volume.noiseAmount != defaults.noiseAmount) fogJson["noiseAmount"] = volume.noiseAmount;
            if (volume.flowDirectionDegrees != defaults.flowDirectionDegrees) fogJson["flowDirectionDegrees"] = volume.flowDirectionDegrees;
            if (volume.flowSpeedWorld != defaults.flowSpeedWorld) fogJson["flowSpeedWorld"] = volume.flowSpeedWorld;
            graphJson["fogVolumes"].push_back(std::move(fogJson));
        }
    }

    if (!graph.levelMarkers.empty()) {
        const std::vector<SectorAuthoringValidationIssue> issues =
                ValidateSectorAuthoringGraphReferences(graph);
        const auto markerError = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
            return issue.objectKind == SectorAuthoringObjectKind::LevelMarker
                    && issue.severity == SectorAuthoringValidationSeverity::Error;
        });
        if (markerError != issues.end()) {
            Fail("Invalid authoring level marker: " + markerError->message);
        }
        graphJson["levelMarkers"] = Json::array();
        for (const SectorAuthoringLevelMarker* marker : SortedById(graph.levelMarkers)) {
            RequireFinite(marker->y, "authoring level marker y");
            RequireFinite(marker->orientationDegrees, "authoring level marker orientationDegrees");
            graphJson["levelMarkers"].push_back(Json{
                    {"editorId", marker->id},
                    {"id", marker->referenceId},
                    {"x", marker->x},
                    {"y", marker->y},
                    {"z", marker->z},
                    {"orientationDegrees", marker->orientationDegrees}});
        }
    }

    if (!graph.triggers.empty()) {
        const std::vector<SectorAuthoringValidationIssue> issues =
                ValidateSectorAuthoringGraphReferences(graph);
        const auto triggerError = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
            return issue.objectKind == SectorAuthoringObjectKind::Trigger
                    && issue.severity == SectorAuthoringValidationSeverity::Error;
        });
        if (triggerError != issues.end()) Fail("Invalid authoring trigger: " + triggerError->message);

        graphJson["triggers"] = Json::array();
        std::vector<const SectorAuthoringTrigger*> triggers;
        triggers.reserve(graph.triggers.size());
        for (const SectorAuthoringTrigger& trigger : graph.triggers) triggers.push_back(&trigger);
        std::sort(triggers.begin(), triggers.end(), [](const auto* left, const auto* right) {
            return left->editorId < right->editorId;
        });
        for (const SectorAuthoringTrigger* trigger : triggers) {
            Json triggerJson{
                    {"editorId", trigger->editorId},
                    {"id", trigger->id},
                    {"shape", trigger->shape == SectorTriggerShapeKind::Rectangle ? "rectangle" : "polygon"},
                    {"points", Json::array()}};
            for (SectorTriggerPoint point : trigger->points) {
                triggerJson["points"].push_back(Json{{"x", point.x}, {"z", point.z}});
            }
            if (!trigger->enabled) triggerJson["enabled"] = false;
            if (trigger->repeat) triggerJson["repeat"] = true;
            if (trigger->delayMilliseconds != 0) triggerJson["delayMilliseconds"] = trigger->delayMilliseconds;
            if (!trigger->script.empty()) triggerJson["script"] = trigger->script;
            graphJson["triggers"].push_back(std::move(triggerJson));
        }
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
    WriteAuthoringEditorSettings(root, document.editorSettings);
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
        sector.footstepSet = ReadOptionalString(value, "footstepSet", context);
        ValidateOptionalFootstepSet(sector.footstepSet, context + ".footstepSet");
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
    ValidateRuntimeObjects(map, "root");

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
        if (!sector->footstepSet.empty()) {
            ValidateOptionalFootstepSet(sector->footstepSet, context + ".footstepSet");
            sectorJson["footstepSet"] = sector->footstepSet;
        }
        root["sectors"].push_back(std::move(sectorJson));
    }

    if (!map.levelMarkers.empty()) {
        root["levelMarkers"] = Json::array();
        std::vector<const SectorCompiledLevelMarker*> markers;
        markers.reserve(map.levelMarkers.size());
        for (const SectorCompiledLevelMarker& marker : map.levelMarkers) {
            markers.push_back(&marker);
        }
        std::sort(markers.begin(), markers.end(), [](const auto* left, const auto* right) {
            return left->sourceAuthoringMarkerId < right->sourceAuthoringMarkerId;
        });
        for (const SectorCompiledLevelMarker* marker : markers) {
            root["levelMarkers"].push_back(Json{
                    {"editorId", marker->sourceAuthoringMarkerId},
                    {"id", marker->id},
                    {"position", WriteVector3(marker->position, "level marker position")},
                    {"orientationDegrees", marker->yawRadians * (180.0f / Pi)}});
        }
    }

    if (!map.triggers.empty()) {
        root["triggers"] = Json::array();
        std::vector<const SectorCompiledTrigger*> triggers;
        triggers.reserve(map.triggers.size());
        for (const SectorCompiledTrigger& trigger : map.triggers) triggers.push_back(&trigger);
        std::sort(triggers.begin(), triggers.end(), [](const auto* left, const auto* right) {
            return left->sourceAuthoringTriggerId < right->sourceAuthoringTriggerId;
        });
        std::set<int> editorIds;
        std::set<std::string> ids;
        for (const SectorCompiledTrigger* trigger : triggers) {
            std::string geometryError;
            if (!IsValidSectorAuthoringId(trigger->sourceAuthoringTriggerId)
                    || !editorIds.insert(trigger->sourceAuthoringTriggerId).second
                    || !IsValidSectorTriggerReferenceId(trigger->id)
                    || !ids.insert(trigger->id).second
                    || trigger->delayMilliseconds < 0
                    || !IsValidSectorTriggerScriptName(trigger->script)
                    || !ValidateSectorTriggerPolygon(trigger->points, trigger->shape, &geometryError)) {
                Fail("Invalid compiled trigger '" + trigger->id + "'"
                        + (geometryError.empty() ? std::string{} : ": " + geometryError));
            }
            Json triggerJson{
                    {"editorId", trigger->sourceAuthoringTriggerId},
                    {"id", trigger->id},
                    {"shape", trigger->shape == SectorTriggerShapeKind::Rectangle ? "rectangle" : "polygon"},
                    {"points", Json::array()}};
            for (SectorTriggerPoint point : trigger->points) {
                triggerJson["points"].push_back(Json{{"x", point.x}, {"z", point.z}});
            }
            if (!trigger->enabled) triggerJson["enabled"] = false;
            if (trigger->repeat) triggerJson["repeat"] = true;
            if (trigger->delayMilliseconds != 0) triggerJson["delayMilliseconds"] = trigger->delayMilliseconds;
            if (!trigger->script.empty()) triggerJson["script"] = trigger->script;
            root["triggers"].push_back(std::move(triggerJson));
        }
    }

    if (!map.runtimeObjects.empty()) {
        root["runtimeObjects"] = Json::array();
        for (const SectorPlacedRuntimeObject* object : SortedById(map.runtimeObjects)) {
            const std::string context = "runtime object " + std::to_string(object->id);
            root["runtimeObjects"].push_back(WriteRuntimeObject(*object, context));
        }
    }

    root["staticLights"] = Json::array();
    for (const SectorTopologyStaticPointLight* light : SortedById(map.staticLights)) {
        const std::string context = "static light " + std::to_string(light->id);
        RequireFinite(light->intensity, context + ".intensity");
        RequireFinite(light->radius, context + ".radius");
        RequireFinite(light->sourceRadius, context + ".sourceRadius");
        Json lightJson{
                {"id", light->id},
                {"position", WriteVector3(light->position, context + ".position")},
                {"radius", light->radius},
                {"sourceRadius", light->sourceRadius},
                {"intensity", light->intensity},
                {"color", WriteColor(light->color)}
        };
        if (!light->castsShadow) {
            lightJson["castsShadow"] = false;
        }
        WriteOptionalLightAtmosphere(lightJson, *light);
        root["staticLights"].push_back(std::move(lightJson));
    }

    root["staticSpotLights"] = Json::array();
    for (const SectorTopologyStaticSpotLight* light : SortedById(map.staticSpotLights)) {
        const std::string context = "static spot light " + std::to_string(light->id);
        root["staticSpotLights"].push_back(WriteStaticSpotLight(*light, context));
    }

    root["dynamicPointLights"] = Json::array();
    for (const SectorTopologyDynamicPointLight* light : SortedById(map.dynamicPointLights)) {
        const std::string context = "dynamic point light " + std::to_string(light->id);
        root["dynamicPointLights"].push_back(WriteDynamicPointLight(*light, context));
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
    if (!IsDefaultFogSettings(map.fogSettings)) {
        root["fogSettings"] = WriteFogSettings(map.fogSettings);
    }
    WriteAudioSettings(root, map.audioSettings);
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
