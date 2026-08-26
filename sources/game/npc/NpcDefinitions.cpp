#include "game/npc/NpcDefinitions.h"

#include "game/npc/ai/NpcAiTypes.h"

#include "util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

constexpr float MinAnimationSpeed = 0.01f;
constexpr float MaxAnimationSpeed = 10.0f;
constexpr float MinMovementSpeed = 0.1f;
constexpr float MaxMovementSpeed = 200.0f;
constexpr float MaxWorldDistance = 10000.0f;
constexpr float MaxKnockbackImpulse = 100.0f;
constexpr int MaxStunMilliseconds = 60000;

[[noreturn]] void Fail(const std::string& message)
{
    throw std::runtime_error(message);
}

std::string LowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (const unsigned char character : value) {
        lowered.push_back(static_cast<char>(std::tolower(character)));
    }
    return lowered;
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

std::string RequireString(
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

std::string OptionalString(
        const Json& object,
        const char* field,
        const std::string& fallback,
        const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) return fallback;
    if (!it->is_string()) {
        Fail(context + "." + field + " must be a string");
    }
    return it->get<std::string>();
}

bool OptionalBool(
        const Json& object,
        const char* field,
        bool fallback,
        const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) return fallback;
    if (!it->is_boolean()) {
        Fail(context + "." + field + " must be a boolean");
    }
    return it->get<bool>();
}

float OptionalFloat(
        const Json& object,
        const char* field,
        float fallback,
        const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) return fallback;
    if (!it->is_number()) {
        Fail(context + "." + field + " must be a number");
    }
    const double number = it->get<double>();
    if (!std::isfinite(number)
            || number < -std::numeric_limits<float>::max()
            || number > std::numeric_limits<float>::max()) {
        Fail(context + "." + field + " must be a finite float");
    }
    return static_cast<float>(number);
}

int OptionalInt(
        const Json& object,
        const char* field,
        int fallback,
        const std::string& context)
{
    const auto it = object.find(field);
    if (it == object.end()) return fallback;
    if (!it->is_number_integer()) {
        Fail(context + "." + field + " must be an integer");
    }
    const long long number = it->get<long long>();
    if (number < std::numeric_limits<int>::min()
            || number > std::numeric_limits<int>::max()) {
        Fail(context + "." + field + " is outside the supported integer range");
    }
    return static_cast<int>(number);
}

void ValidateOrFail(const NpcDefinition& definition)
{
    std::string error;
    if (!ValidateNpcDefinition(definition, error)) {
        Fail(error);
    }
}

} // namespace

const std::array<NpcActionMetadata, kNpcActionCount>& NpcActionMetadataTable()
{
    static const std::array<NpcActionMetadata, kNpcActionCount> metadata = {{
            {NpcAction::Idle, "idle", "Idle", false, 0.0f, false},
            {NpcAction::Walk, "walk", "Walk", true, 1.5f, false},
            {NpcAction::Run, "run", "Run", true, 3.0f, false},
            {NpcAction::Attack, "attack", "Attack", false, 0.0f, true},
            {NpcAction::Hurt, "hurt", "Hurt", false, 0.0f, true},
            {NpcAction::Death, "death", "Death", false, 0.0f, true}
    }};
    return metadata;
}

const NpcActionMetadata& GetNpcActionMetadata(NpcAction action)
{
    const size_t index = static_cast<size_t>(action);
    return NpcActionMetadataTable()[
            index < kNpcActionCount ? index : static_cast<size_t>(NpcAction::Idle)];
}

NpcDefinition MakeDefaultNpcDefinition()
{
    NpcDefinition definition;
    for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
        GetNpcAction(definition, metadata.action).movementSpeed =
                metadata.defaultMovementSpeed;
    }
    return definition;
}

NpcActionDefinition& GetNpcAction(NpcDefinition& definition, NpcAction action)
{
    return definition.actions[static_cast<size_t>(action)];
}

const NpcActionDefinition& GetNpcAction(
        const NpcDefinition& definition,
        NpcAction action)
{
    return definition.actions[static_cast<size_t>(action)];
}

bool IsValidNpcDefinitionId(std::string_view id)
{
    if (id.empty() || id.size() > 63) return false;
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
    });
}

bool IsValidNpcCharacterModelPath(std::string_view path)
{
    const std::string prefix = std::string{kNpcCharacterModelsAssetRoot} + "/";
    if (path.size() <= prefix.size()
            || path.compare(0, prefix.size(), prefix) != 0
            || path.find('\\') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path parsed{std::string{path}};
    if (parsed.is_absolute()
            || parsed.lexically_normal().generic_string() != path) {
        return false;
    }
    for (const std::filesystem::path& segment : parsed) {
        if (segment == "." || segment == "..") return false;
    }
    const std::string extension = LowerAscii(parsed.extension().generic_string());
    return extension == ".glb" || extension == ".gltf";
}

bool IsValidNpcAudioPath(std::string_view path)
{
    const bool windowsDrivePath = path.size() >= 2
            && std::isalpha(static_cast<unsigned char>(path[0]))
            && path[1] == ':';
    if (path.empty() || path.front() == '/' || path.front() == '\\'
            || windowsDrivePath
            || path.find('\\') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path parsed{std::string{path}};
    if (parsed.is_absolute()
            || parsed.lexically_normal().generic_string() != path) {
        return false;
    }
    for (const std::filesystem::path& segment : parsed) {
        if (segment == "." || segment == "..") return false;
    }
    const std::string extension = LowerAscii(parsed.extension().generic_string());
    return extension == ".ogg" || extension == ".wav" || extension == ".mp3";
}

bool ValidateNpcDefinition(
        const NpcDefinition& definition,
        std::string& outError)
{
    if (!IsValidNpcDefinitionId(definition.id)) {
        outError = "NPC id must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    if (definition.name.size() > 255
            || definition.name.find('\n') != std::string::npos
            || definition.name.find('\r') != std::string::npos) {
        outError = "NPC name must contain at most 255 characters on one line";
        return false;
    }
    if (!IsValidNpcCharacterModelPath(definition.modelPath)) {
        outError = "NPC model must be a normalized .glb or .gltf path under assets/models/characters";
        return false;
    }
    if (!definition.aiType.empty()
            && !IsValidNpcDefinitionId(definition.aiType)) {
        outError = "NPC AI type must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    if (!definition.aiType.empty()) {
        const NpcAiTypeDescriptor* aiType = FindNpcAiType(definition.aiType);
        if (aiType == nullptr) {
            outError = "NPC AI type is not registered: " + definition.aiType;
            return false;
        }
        if (!IsNpcAiTypeCompatible(*aiType, definition.hostile)) {
            outError = "NPC AI type is incompatible with the Hostile setting";
            return false;
        }
        if (definition.aiType == kSeekAndDestroyNpcAiType) {
            const NpcActionDefinition& attack = GetNpcAction(
                    definition, NpcAction::Attack);
            if (attack.animation.empty() || attack.damage <= 0) {
                outError = "Seek & Destroy NPCs require an Attack animation and positive damage";
                return false;
            }
        }
    }
    const NpcPerceptionDefinition& perception = definition.perception;
    if (!std::isfinite(perception.visionRangeWorld)
            || perception.visionRangeWorld < 0.0f
            || perception.visionRangeWorld > MaxWorldDistance
            || !std::isfinite(perception.visionAngleDegrees)
            || perception.visionAngleDegrees <= 0.0f
            || perception.visionAngleDegrees >= 360.0f
            || !std::isfinite(perception.hearingRangeWorld)
            || perception.hearingRangeWorld < 0.0f
            || perception.hearingRangeWorld > MaxWorldDistance
            || perception.investigationDurationMilliseconds < 0
            || perception.investigationDurationMilliseconds > 600000) {
        outError = "NPC perception ranges must be finite and non-negative, vision angle must be between 0 and 360 degrees, and investigation duration must be between 0 and 600000 ms";
        return false;
    }
    if (definition.baseHealth < kMinimumNpcBaseHealth
            || definition.baseHealth > kMaximumNpcBaseHealth) {
        outError = "NPC base health must be between 1 and 1000000";
        return false;
    }
    if (!std::isfinite(definition.corpseDespawnDelaySeconds)
            || definition.corpseDespawnDelaySeconds < 0.0f
            || definition.corpseDespawnDelaySeconds
                    > kMaximumNpcCorpseDespawnDelaySeconds) {
        outError = "NPC corpse despawn delay must be between 0 and 600 seconds";
        return false;
    }
    if (!std::isfinite(definition.corpseFadeDurationSeconds)
            || definition.corpseFadeDurationSeconds
                    < kMinimumNpcCorpseFadeDurationSeconds
            || definition.corpseFadeDurationSeconds
                    > kMaximumNpcCorpseFadeDurationSeconds) {
        outError = "NPC corpse fade duration must be between 0.001 and 60 seconds";
        return false;
    }
    if (!std::isfinite(definition.animationBlendSeconds)
            || definition.animationBlendSeconds < kMinimumNpcAnimationBlendSeconds
            || definition.animationBlendSeconds > kMaximumNpcAnimationBlendSeconds) {
        outError = "NPC animation blend time must be between 0.01 and 2 seconds";
        return false;
    }
    const NpcAmbientVocalizationDefinition& ambient =
            definition.ambientVocalizations;
    if (!std::isfinite(ambient.minimumDelaySeconds)
            || !std::isfinite(ambient.maximumDelaySeconds)
            || ambient.minimumDelaySeconds < 0.0f
            || ambient.maximumDelaySeconds < ambient.minimumDelaySeconds
            || ambient.maximumDelaySeconds > kMaximumNpcAmbientDelaySeconds) {
        outError = "NPC ambient vocalization delays must be between 0 and 600 seconds, with minimum no greater than maximum";
        return false;
    }
    std::unordered_set<std::string> ambientPaths;
    for (const std::string& path : ambient.soundPaths) {
        if (!IsValidNpcAudioPath(path)) {
            outError = "NPC ambient vocalization sounds must be relative .ogg, .wav, or .mp3 paths beneath assets/audio";
            return false;
        }
        if (!ambientPaths.insert(path).second) {
            outError = "NPC ambient vocalization sounds must be unique";
            return false;
        }
    }
    for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
        const NpcActionDefinition& action = GetNpcAction(definition, metadata.action);
        if (!action.soundPath.empty()
                && (!metadata.hasSound || !IsValidNpcAudioPath(action.soundPath))) {
            outError = std::string{"NPC "} + metadata.displayName
                    + " sound must be a relative .ogg, .wav, or .mp3 path beneath assets/audio";
            return false;
        }
        if (!action.attackSoundPath.empty()
                && (metadata.action != NpcAction::Attack
                    || !IsValidNpcAudioPath(action.attackSoundPath))) {
            outError = std::string{"NPC "} + metadata.displayName
                    + " attack sound must be a relative .ogg, .wav, or .mp3 path beneath assets/audio";
            return false;
        }
        if (!std::isfinite(action.animationSpeed)
                || action.animationSpeed < MinAnimationSpeed
                || action.animationSpeed > MaxAnimationSpeed) {
            outError = std::string{"NPC "} + metadata.displayName
                    + " animation speed must be between 0.01 and 10";
            return false;
        }
        if (metadata.hasMovementSpeed
                && (!std::isfinite(action.movementSpeed)
                        || action.movementSpeed < MinMovementSpeed
                        || action.movementSpeed > MaxMovementSpeed)) {
            outError = std::string{"NPC "} + metadata.displayName
                    + " movement speed must be between 0.1 and 200 world units per second";
            return false;
        }
        if (metadata.action == NpcAction::Attack
                && (!std::isfinite(action.hitPhase)
                    || action.hitPhase < 0.0f || action.hitPhase > 1.0f
                    || !std::isfinite(action.rangeWorld)
                    || action.rangeWorld <= 0.0f
                    || action.rangeWorld > MaxWorldDistance
                    || action.damage < 0 || action.damage > 1000000
                    || !std::isfinite(action.knockbackImpulseWorldPerSecond)
                    || action.knockbackImpulseWorldPerSecond < 0.0f
                    || action.knockbackImpulseWorldPerSecond > MaxKnockbackImpulse
                    || action.stunMilliseconds < 0
                    || action.stunMilliseconds > MaxStunMilliseconds)) {
            outError = "NPC Attack hit phase, range, damage, knockback, or stun is outside its supported range";
            return false;
        }
    }
    outError.clear();
    return true;
}

bool ParseNpcDefinitionJson(
        std::string_view jsonText,
        NpcDefinition& outDefinition,
        std::string& outError)
{
    try {
        const Json root = Json::parse(jsonText.begin(), jsonText.end());
        if (!root.is_object()) Fail("NPC definition root must be an object");
        RejectUnknownFields(
                root,
                {"formatVersion", "id", "name", "hostile", "aiType", "perception", "canOpenDoors",
                 "baseHealth", "despawnOnDeath", "corpseDespawnDelaySeconds",
                 "corpseFadeDurationSeconds", "modelPath",
                 "animationBlendSeconds", "ambientVocalizations", "actions"},
                "NPC definition");

        const Json& version = RequireField(root, "formatVersion", "NPC definition");
        if (!version.is_number_integer()
                || version.get<int>() != kNpcDefinitionFormatVersion) {
            Fail("NPC definition.formatVersion must be 1");
        }

        NpcDefinition parsed = MakeDefaultNpcDefinition();
        parsed.id = RequireString(root, "id", "NPC definition");
        parsed.name = OptionalString(root, "name", {}, "NPC definition");
        parsed.hostile = OptionalBool(root, "hostile", false, "NPC definition");
        parsed.aiType = OptionalString(root, "aiType", {}, "NPC definition");
        parsed.canOpenDoors = OptionalBool(
                root, "canOpenDoors", true, "NPC definition");
        parsed.baseHealth = OptionalInt(
                root, "baseHealth", kDefaultNpcBaseHealth, "NPC definition");
        parsed.despawnOnDeath = OptionalBool(
                root, "despawnOnDeath", false, "NPC definition");
        parsed.corpseDespawnDelaySeconds = OptionalFloat(
                root,
                "corpseDespawnDelaySeconds",
                kDefaultNpcCorpseDespawnDelaySeconds,
                "NPC definition");
        parsed.corpseFadeDurationSeconds = OptionalFloat(
                root,
                "corpseFadeDurationSeconds",
                kDefaultNpcCorpseFadeDurationSeconds,
                "NPC definition");
        parsed.modelPath = RequireString(root, "modelPath", "NPC definition");
        parsed.animationBlendSeconds = OptionalFloat(
                root,
                "animationBlendSeconds",
                kDefaultNpcAnimationBlendSeconds,
                "NPC definition");

        const auto perception = root.find("perception");
        if (perception != root.end()) {
            const std::string context = "NPC definition.perception";
            if (!perception->is_object()) Fail(context + " must be an object");
            RejectUnknownFields(
                    *perception,
                    {"visionRangeWorld", "visionAngleDegrees", "hearingRangeWorld",
                     "investigationDurationMilliseconds"},
                    context);
            parsed.perception.visionRangeWorld = OptionalFloat(
                    *perception, "visionRangeWorld",
                    kDefaultNpcVisionRangeWorld, context);
            parsed.perception.visionAngleDegrees = OptionalFloat(
                    *perception, "visionAngleDegrees",
                    kDefaultNpcVisionAngleDegrees, context);
            parsed.perception.hearingRangeWorld = OptionalFloat(
                    *perception, "hearingRangeWorld",
                    kDefaultNpcHearingRangeWorld, context);
            parsed.perception.investigationDurationMilliseconds = OptionalInt(
                    *perception, "investigationDurationMilliseconds",
                    kDefaultNpcInvestigationDurationMilliseconds, context);
        }

        const auto ambient = root.find("ambientVocalizations");
        if (ambient != root.end()) {
            const std::string context = "NPC definition.ambientVocalizations";
            if (!ambient->is_object()) Fail(context + " must be an object");
            RejectUnknownFields(
                    *ambient,
                    {"sounds", "minimumDelaySeconds", "maximumDelaySeconds"},
                    context);
            parsed.ambientVocalizations.minimumDelaySeconds = OptionalFloat(
                    *ambient,
                    "minimumDelaySeconds",
                    kDefaultNpcAmbientMinimumDelaySeconds,
                    context);
            parsed.ambientVocalizations.maximumDelaySeconds = OptionalFloat(
                    *ambient,
                    "maximumDelaySeconds",
                    kDefaultNpcAmbientMaximumDelaySeconds,
                    context);
            const auto sounds = ambient->find("sounds");
            if (sounds != ambient->end()) {
                if (!sounds->is_array()) Fail(context + ".sounds must be an array");
                parsed.ambientVocalizations.soundPaths.reserve(sounds->size());
                for (const Json& sound : *sounds) {
                    if (!sound.is_string()) {
                        Fail(context + ".sounds must contain only strings");
                    }
                    parsed.ambientVocalizations.soundPaths.push_back(
                            sound.get<std::string>());
                }
            }
        }

        const Json& actions = RequireField(root, "actions", "NPC definition");
        if (!actions.is_object()) Fail("NPC definition.actions must be an object");
        std::unordered_set<std::string> actionKeys;
        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            actionKeys.insert(metadata.jsonKey);
        }
        RejectUnknownFields(actions, actionKeys, "NPC definition.actions");

        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            const auto it = actions.find(metadata.jsonKey);
            if (it == actions.end()) continue;
            const std::string context =
                    std::string{"NPC definition.actions."} + metadata.jsonKey;
            if (!it->is_object()) Fail(context + " must be an object");
            std::unordered_set<std::string> actionFields{
                    "animation", "animationSpeed"};
            if (metadata.hasMovementSpeed) actionFields.insert("movementSpeed");
            if (metadata.hasSound) actionFields.insert("sound");
            if (metadata.action == NpcAction::Attack) {
                actionFields.insert("attackSound");
                actionFields.insert("hitPhase");
                actionFields.insert("rangeWorld");
                actionFields.insert("damage");
                actionFields.insert("knockbackImpulseWorldPerSecond");
                actionFields.insert("stunMilliseconds");
            }
            RejectUnknownFields(
                    *it,
                    actionFields,
                    context);
            NpcActionDefinition& action = GetNpcAction(parsed, metadata.action);
            action.animation = OptionalString(*it, "animation", {}, context);
            if (metadata.hasSound) {
                action.soundPath = OptionalString(*it, "sound", {}, context);
            }
            action.animationSpeed = OptionalFloat(
                    *it, "animationSpeed", action.animationSpeed, context);
            if (metadata.hasMovementSpeed) {
                action.movementSpeed = OptionalFloat(
                        *it, "movementSpeed", action.movementSpeed, context);
            }
            if (metadata.action == NpcAction::Attack) {
                action.attackSoundPath = OptionalString(
                        *it, "attackSound", {}, context);
                action.hitPhase = OptionalFloat(
                        *it, "hitPhase", kDefaultNpcAttackHitPhase, context);
                action.rangeWorld = OptionalFloat(
                        *it, "rangeWorld", kDefaultNpcAttackRangeWorld, context);
                action.damage = OptionalInt(
                        *it, "damage", kDefaultNpcAttackDamage, context);
                action.knockbackImpulseWorldPerSecond = OptionalFloat(
                        *it, "knockbackImpulseWorldPerSecond",
                        kDefaultNpcAttackKnockbackImpulseWorldPerSecond, context);
                action.stunMilliseconds = OptionalInt(
                        *it, "stunMilliseconds",
                        kDefaultNpcAttackStunMilliseconds, context);
            }
        }

        ValidateOrFail(parsed);
        outDefinition = std::move(parsed);
        outError.clear();
        return true;
    } catch (const std::exception& exception) {
        outDefinition = NpcDefinition{};
        outError = exception.what();
        return false;
    }
}

bool SerializeNpcDefinitionJson(
        const NpcDefinition& definition,
        std::string& outJson,
        std::string& outError)
{
    try {
        ValidateOrFail(definition);
        Json root = Json::object();
        root["formatVersion"] = kNpcDefinitionFormatVersion;
        root["id"] = definition.id;
        if (!definition.name.empty()) root["name"] = definition.name;
        if (definition.hostile) root["hostile"] = true;
        if (!definition.aiType.empty()) root["aiType"] = definition.aiType;
        if (!definition.canOpenDoors) root["canOpenDoors"] = false;
        if (definition.baseHealth != kDefaultNpcBaseHealth) {
            root["baseHealth"] = definition.baseHealth;
        }
        if (definition.despawnOnDeath) root["despawnOnDeath"] = true;
        if (definition.corpseDespawnDelaySeconds
                != kDefaultNpcCorpseDespawnDelaySeconds) {
            root["corpseDespawnDelaySeconds"] =
                    definition.corpseDespawnDelaySeconds;
        }
        if (definition.corpseFadeDurationSeconds
                != kDefaultNpcCorpseFadeDurationSeconds) {
            root["corpseFadeDurationSeconds"] =
                    definition.corpseFadeDurationSeconds;
        }
        root["modelPath"] = definition.modelPath;
        if (definition.animationBlendSeconds != kDefaultNpcAnimationBlendSeconds) {
            root["animationBlendSeconds"] = definition.animationBlendSeconds;
        }
        const NpcPerceptionDefinition& perception = definition.perception;
        if (perception.visionRangeWorld != kDefaultNpcVisionRangeWorld
                || perception.visionAngleDegrees != kDefaultNpcVisionAngleDegrees
                || perception.hearingRangeWorld != kDefaultNpcHearingRangeWorld
                || perception.investigationDurationMilliseconds
                        != kDefaultNpcInvestigationDurationMilliseconds) {
            Json perceptionJson = Json::object();
            if (perception.visionRangeWorld != kDefaultNpcVisionRangeWorld) {
                perceptionJson["visionRangeWorld"] = perception.visionRangeWorld;
            }
            if (perception.visionAngleDegrees != kDefaultNpcVisionAngleDegrees) {
                perceptionJson["visionAngleDegrees"] = perception.visionAngleDegrees;
            }
            if (perception.hearingRangeWorld != kDefaultNpcHearingRangeWorld) {
                perceptionJson["hearingRangeWorld"] = perception.hearingRangeWorld;
            }
            if (perception.investigationDurationMilliseconds
                    != kDefaultNpcInvestigationDurationMilliseconds) {
                perceptionJson["investigationDurationMilliseconds"] =
                        perception.investigationDurationMilliseconds;
            }
            root["perception"] = std::move(perceptionJson);
        }

        const NpcAmbientVocalizationDefinition& ambient =
                definition.ambientVocalizations;
        if (!ambient.soundPaths.empty()
                || ambient.minimumDelaySeconds
                        != kDefaultNpcAmbientMinimumDelaySeconds
                || ambient.maximumDelaySeconds
                        != kDefaultNpcAmbientMaximumDelaySeconds) {
            Json ambientJson = Json::object();
            if (!ambient.soundPaths.empty()) {
                ambientJson["sounds"] = ambient.soundPaths;
            }
            if (ambient.minimumDelaySeconds
                    != kDefaultNpcAmbientMinimumDelaySeconds) {
                ambientJson["minimumDelaySeconds"] = ambient.minimumDelaySeconds;
            }
            if (ambient.maximumDelaySeconds
                    != kDefaultNpcAmbientMaximumDelaySeconds) {
                ambientJson["maximumDelaySeconds"] = ambient.maximumDelaySeconds;
            }
            root["ambientVocalizations"] = std::move(ambientJson);
        }

        Json actions = Json::object();
        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            const NpcActionDefinition& action =
                    GetNpcAction(definition, metadata.action);
            Json actionJson = Json::object();
            if (!action.animation.empty()) actionJson["animation"] = action.animation;
            if (metadata.hasSound && !action.soundPath.empty()) {
                actionJson["sound"] = action.soundPath;
            }
            if (action.animationSpeed != 1.0f) {
                actionJson["animationSpeed"] = action.animationSpeed;
            }
            if (metadata.hasMovementSpeed) {
                actionJson["movementSpeed"] = action.movementSpeed;
            }
            if (metadata.action == NpcAction::Attack) {
                if (!action.attackSoundPath.empty()) {
                    actionJson["attackSound"] = action.attackSoundPath;
                }
                if (action.hitPhase != kDefaultNpcAttackHitPhase) {
                    actionJson["hitPhase"] = action.hitPhase;
                }
                if (action.rangeWorld != kDefaultNpcAttackRangeWorld) {
                    actionJson["rangeWorld"] = action.rangeWorld;
                }
                if (action.damage != kDefaultNpcAttackDamage) {
                    actionJson["damage"] = action.damage;
                }
                if (action.knockbackImpulseWorldPerSecond
                        != kDefaultNpcAttackKnockbackImpulseWorldPerSecond) {
                    actionJson["knockbackImpulseWorldPerSecond"] =
                            action.knockbackImpulseWorldPerSecond;
                }
                if (action.stunMilliseconds != kDefaultNpcAttackStunMilliseconds) {
                    actionJson["stunMilliseconds"] = action.stunMilliseconds;
                }
            }
            actions[metadata.jsonKey] = std::move(actionJson);
        }
        root["actions"] = std::move(actions);
        outJson = root.dump(2) + "\n";
        outError.clear();
        return true;
    } catch (const std::exception& exception) {
        outJson.clear();
        outError = exception.what();
        return false;
    }
}

bool LoadNpcDefinition(
        const std::filesystem::path& path,
        NpcDefinition& outDefinition,
        std::string& outError)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outDefinition = NpcDefinition{};
        outError = "Could not open NPC definition: " + path.generic_string();
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        outDefinition = NpcDefinition{};
        outError = "Could not read NPC definition: " + path.generic_string();
        return false;
    }
    return ParseNpcDefinitionJson(contents.str(), outDefinition, outError);
}

bool SaveNpcDefinition(
        const std::filesystem::path& path,
        const NpcDefinition& definition,
        std::string& outError)
{
    std::string json;
    if (!SerializeNpcDefinitionJson(definition, json, outError)) return false;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        outError = "Could not create NPC definition folder: " + error.message();
        return false;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            outError = "Could not write NPC definition: " + temporary.generic_string();
            return false;
        }
        output << json;
        if (!output) {
            outError = "Failed writing NPC definition: " + temporary.generic_string();
            output.close();
            std::filesystem::remove(temporary, error);
            return false;
        }
    }

    const bool targetExists = std::filesystem::exists(path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        outError = "Could not inspect NPC definition target: "
                + path.generic_string();
        return false;
    }
    const std::filesystem::path backup = path.string() + ".bak";
    if (targetExists) {
        std::filesystem::remove(backup, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            outError = "Could not clear stale NPC definition backup: "
                    + backup.generic_string();
            return false;
        }
        std::filesystem::rename(path, backup, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            outError = "Could not prepare NPC definition replacement: "
                    + path.generic_string();
            return false;
        }
    }

    std::filesystem::rename(temporary, path, error);
    if (error) {
        const std::string installError = error.message();
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        if (targetExists) {
            std::filesystem::rename(backup, path, cleanupError);
        }
        outError = "Could not install NPC definition '"
                + path.generic_string() + "': " + installError;
        return false;
    }
    if (targetExists) {
        std::filesystem::remove(backup, error);
        if (error) {
            outError = "NPC definition saved, but its temporary backup could not be removed: "
                    + backup.generic_string();
            return false;
        }
    }
    outError.clear();
    return true;
}

bool DiscoverNpcDefinitions(
        const std::filesystem::path& root,
        NpcDefinitionCatalog& outCatalog)
{
    NpcDefinitionCatalog catalog;
    std::error_code error;
    if (!std::filesystem::exists(root, error)) {
        if (error) {
            catalog.errors.push_back({root.generic_string(), error.message()});
            outCatalog = std::move(catalog);
            return false;
        }
        outCatalog = std::move(catalog);
        return true;
    }
    if (!std::filesystem::is_directory(root, error)) {
        catalog.errors.push_back({root.generic_string(), "NPC definition root is not a directory"});
        outCatalog = std::move(catalog);
        return false;
    }

    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator it(root, error), end;
            !error && it != end;
            it.increment(error)) {
        std::error_code entryError;
        if (it->is_regular_file(entryError)
                && !entryError
                && LowerAscii(it->path().extension().generic_string()) == ".json") {
            paths.push_back(it->path());
        }
    }
    if (error) {
        catalog.errors.push_back({root.generic_string(), error.message()});
    }
    std::sort(paths.begin(), paths.end());

    std::unordered_set<std::string> ids;
    for (const std::filesystem::path& path : paths) {
        NpcDefinition definition;
        std::string loadError;
        if (!LoadNpcDefinition(path, definition, loadError)) {
            catalog.errors.push_back({path.generic_string(), loadError});
            continue;
        }
        if (path.stem().generic_string() != definition.id) {
            catalog.errors.push_back({
                    path.generic_string(),
                    "NPC filename must match id '" + definition.id + "'"});
            continue;
        }
        const std::string foldedId = LowerAscii(definition.id);
        if (!ids.insert(foldedId).second) {
            catalog.errors.push_back({
                    path.generic_string(),
                    "NPC id duplicates another definition: " + definition.id});
            continue;
        }
        catalog.definitions.push_back(std::move(definition));
    }
    std::sort(
            catalog.definitions.begin(),
            catalog.definitions.end(),
            [](const NpcDefinition& left, const NpcDefinition& right) {
                return LowerAscii(left.id) < LowerAscii(right.id);
            });
    const bool valid = catalog.errors.empty();
    outCatalog = std::move(catalog);
    return valid;
}

const NpcDefinition* FindNpcDefinition(
        const NpcDefinitionCatalog& catalog,
        std::string_view id)
{
    const auto found = std::find_if(
            catalog.definitions.begin(),
            catalog.definitions.end(),
            [id](const NpcDefinition& definition) {
                return definition.id == id;
            });
    return found == catalog.definitions.end() ? nullptr : &*found;
}

} // namespace game
