#include "game/npc/NpcDefinitions.h"
#include "game/npc/ai/NpcAiTypes.h"
#include "sector_editor/npcs/SectorEditorNpcEditorService.h"

#include "util/json.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using Json = nlohmann::ordered_json;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float left, float right, float epsilon = 0.00001f)
{
    return std::fabs(left - right) <= epsilon;
}

struct Sandbox {
    std::filesystem::path root =
            std::filesystem::temp_directory_path() / "engine_npc_definition_tests";

    Sandbox()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
    }

    ~Sandbox()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

game::NpcDefinition MakeDefinition(
        std::string id,
        std::string model = "assets/models/characters/Zombie1.glb")
{
    game::NpcDefinition definition = game::MakeDefaultNpcDefinition();
    definition.id = std::move(id);
    definition.name = "Test Character";
    definition.hostile = true;
    definition.modelPath = std::move(model);
    game::GetNpcAction(definition, game::NpcAction::Idle).animation = "Idle";
    game::GetNpcAction(definition, game::NpcAction::Walk).animation = "Walk";
    game::GetNpcAction(definition, game::NpcAction::Run).animation = "Walk";
    game::GetNpcAction(definition, game::NpcAction::Run).animationSpeed = 1.6f;
    game::GetNpcAction(definition, game::NpcAction::Attack).animation = "Attack";
    game::GetNpcAction(definition, game::NpcAction::Hurt).animation = "Hit";
    game::GetNpcAction(definition, game::NpcAction::Death).animation = "Death";
    return definition;
}

void TestRoundTripDefaultsAndSharedClips()
{
    game::NpcDefinition original = MakeDefinition("zombie_test");
    original.animationBlendSeconds = 0.35f;
    original.baseHealth = 175;
    original.despawnOnDeath = true;
    original.corpseDespawnDelaySeconds = 2.5f;
    original.corpseFadeDurationSeconds = 0.9f;
    original.aiType = game::kSeekAndDestroyNpcAiType;
    original.perception.visionRangeWorld = 18.0f;
    original.perception.visionAngleDegrees = 100.0f;
    original.perception.hearingRangeWorld = 9.0f;
    original.perception.investigationDurationMilliseconds = 2750;
    original.playerDetectedSoundPath = "npc/zombie/player_detected.wav";
    game::GetNpcAction(
            original,
            game::NpcAction::Walk).footstepPhases = {0.12f, 0.60f};
    game::NpcActionDefinition& attack = game::GetNpcAction(
            original, game::NpcAction::Attack);
    attack.hitPhase = 0.6f;
    attack.rangeWorld = 1.25f;
    attack.advanceSpeedMultiplier = 0.8f;
    attack.aimTrackingEndPhase = 0.35f;
    attack.hitArcDegrees = 95.0f;
    attack.damage = 23;
    attack.knockbackImpulseWorldPerSecond = 2.5f;
    attack.stunMilliseconds = 500;
    attack.cameraImpact.enabled = false;
    attack.cameraImpact.pitchKickDegrees = 4.0f;
    attack.cameraImpact.rollKickDegrees = 5.0f;
    attack.cameraImpact.springFrequencyHz = 6.0f;
    attack.cameraImpact.springDampingRatio = 0.9f;
    attack.cameraImpact.maxPitchDegrees = 8.0f;
    attack.cameraImpact.maxRollDegrees = 11.0f;
    attack.soundPath = "npc/zombie/impact.wav";
    attack.attackSoundPath = "npc/zombie/attack.wav";
    original.ambientVocalizations.soundPaths = {
            "npc/zombie/moan_01.ogg",
            "npc/zombie/moan_02.wav"};
    original.ambientVocalizations.minimumDelaySeconds = 4.0f;
    original.ambientVocalizations.maximumDelaySeconds = 9.0f;
    game::GetNpcAction(
            original, game::NpcAction::Hurt).soundPath =
            "npc/zombie/hurt.wav";
    game::GetNpcAction(
            original, game::NpcAction::Death).soundPath =
            "npc/zombie/death.mp3";
    std::string json;
    std::string error;
    Check(game::SerializeNpcDefinitionJson(original, json, error),
          "valid NPC definition serializes");
    const Json document = Json::parse(json);
    Check(document["formatVersion"] == 1
                  && Near(document["animationBlendSeconds"].get<float>(), 0.35f)
                  && document["actions"]["walk"]["animation"] == "Walk"
                  && Near(document["actions"]["walk"]
                                  ["footstepPhases"][0].get<float>(),
                          0.12f)
                  && Near(document["actions"]["walk"]
                                  ["footstepPhases"][1].get<float>(),
                          0.60f)
                  && document["actions"]["run"]["animation"] == "Walk"
                  && document["actions"]["attack"]["animation"] == "Attack"
                  && document["actions"]["attack"]["damage"] == 23
                  && Near(document["actions"]["attack"]
                                  ["advanceSpeedMultiplier"].get<float>(),
                          0.8f)
                  && Near(document["actions"]["attack"]
                                  ["aimTrackingEndPhase"].get<float>(),
                          0.35f)
                  && Near(document["actions"]["attack"]
                                  ["hitArcDegrees"].get<float>(),
                          95.0f)
                  && document["actions"]["attack"]["attackSound"]
                          == "npc/zombie/attack.wav"
                  && document["actions"]["attack"]["sound"]
                          == "npc/zombie/impact.wav"
                  && document["actions"]["attack"]["cameraImpact"]["enabled"]
                          == false
                  && Near(document["actions"]["attack"]["cameraImpact"]
                                  ["pitchKickDegrees"].get<float>(), 4.0f)
                  && Near(document["actions"]["attack"]["cameraImpact"]
                                  ["springDampingRatio"].get<float>(), 0.9f)
                  && document["aiType"] == "seek_and_destroy"
                  && document["playerDetectedSound"]
                          == "npc/zombie/player_detected.wav"
                  && Near(document["perception"]["visionRangeWorld"].get<float>(), 18.0f)
                  && document["actions"]["hurt"]["animation"] == "Hit"
                  && document["actions"]["hurt"]["sound"]
                          == "npc/zombie/hurt.wav"
                  && document["actions"]["death"]["animation"] == "Death"
                  && document["actions"]["death"]["sound"]
                          == "npc/zombie/death.mp3"
                  && document["ambientVocalizations"]["sounds"].size() == 2
                  && Near(document["ambientVocalizations"]
                                  ["minimumDelaySeconds"].get<float>(), 4.0f)
                  && Near(document["ambientVocalizations"]
                                  ["maximumDelaySeconds"].get<float>(), 9.0f)
                  && document["baseHealth"] == 175
                  && document["despawnOnDeath"] == true
                  && Near(document["corpseDespawnDelaySeconds"].get<float>(), 2.5f)
                  && Near(document["corpseFadeDurationSeconds"].get<float>(), 0.9f),
          "serialized actions may share a GLB animation");

    game::NpcDefinition parsed;
    Check(game::ParseNpcDefinitionJson(json, parsed, error),
          "serialized NPC definition parses");
    Check(parsed.id == original.id
                  && parsed.name == original.name
                  && parsed.hostile
                  && parsed.aiType == game::kSeekAndDestroyNpcAiType
                  && parsed.playerDetectedSoundPath
                          == "npc/zombie/player_detected.wav"
                  && Near(parsed.perception.visionRangeWorld, 18.0f)
                  && parsed.perception.investigationDurationMilliseconds == 2750
                  && parsed.canOpenDoors
                  && parsed.baseHealth == 175
                  && parsed.despawnOnDeath
                  && Near(parsed.corpseDespawnDelaySeconds, 2.5f)
                  && Near(parsed.corpseFadeDurationSeconds, 0.9f)
                  && parsed.modelPath == original.modelPath
                  && Near(parsed.animationBlendSeconds, 0.35f)
                  && parsed.ambientVocalizations.soundPaths
                          == original.ambientVocalizations.soundPaths
                  && Near(parsed.ambientVocalizations.minimumDelaySeconds, 4.0f)
                  && Near(parsed.ambientVocalizations.maximumDelaySeconds, 9.0f)
                  && game::GetNpcAction(
                          parsed, game::NpcAction::Hurt).soundPath
                          == "npc/zombie/hurt.wav"
                  && game::GetNpcAction(
                          parsed, game::NpcAction::Death).soundPath
                          == "npc/zombie/death.mp3"
                  && game::GetNpcAction(parsed, game::NpcAction::Walk).animation == "Walk"
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Walk).footstepPhases[0],
                          0.12f)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Walk).footstepPhases[1],
                          0.60f)
                  && game::GetNpcAction(parsed, game::NpcAction::Run).animation == "Walk"
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Run).animationSpeed, 1.6f)
                  && game::GetNpcAction(parsed, game::NpcAction::Attack).damage == 23
                  && game::GetNpcAction(
                          parsed, game::NpcAction::Attack).soundPath
                          == "npc/zombie/impact.wav"
                  && game::GetNpcAction(
                          parsed, game::NpcAction::Attack).attackSoundPath
                          == "npc/zombie/attack.wav"
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Attack).hitPhase, 0.6f)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack)
                                  .advanceSpeedMultiplier,
                          0.8f)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack)
                                  .aimTrackingEndPhase,
                          0.35f)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack).hitArcDegrees,
                          95.0f)
                  && !game::GetNpcAction(
                          parsed, game::NpcAction::Attack).cameraImpact.enabled
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack).cameraImpact
                                  .pitchKickDegrees,
                          4.0f)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack).cameraImpact
                                  .maxRollDegrees,
                          11.0f)
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Walk).movementSpeed, 1.5f)
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Run).movementSpeed, 3.0f),
          "NPC fields and action defaults round-trip");

    game::NpcDefinition defaults = MakeDefinition("defaults_test");
    Check(game::SerializeNpcDefinitionJson(defaults, json, error)
                  && !Json::parse(json).contains("animationBlendSeconds")
                  && !Json::parse(json).contains("canOpenDoors")
                  && !Json::parse(json).contains("baseHealth")
                  && !Json::parse(json).contains("despawnOnDeath")
                  && !Json::parse(json).contains("corpseDespawnDelaySeconds")
                  && !Json::parse(json).contains("corpseFadeDurationSeconds")
                  && !Json::parse(json).contains("playerDetectedSound")
                  && !Json::parse(json).contains("headLook")
                  && !Json::parse(json).contains("ambientVocalizations")
                  && !Json::parse(json)["actions"]["attack"]
                          .contains("cameraImpact")
                  && !Json::parse(json)["actions"]["attack"]
                          .contains("advanceSpeedMultiplier")
                  && !Json::parse(json)["actions"]["attack"]
                          .contains("aimTrackingEndPhase")
                  && !Json::parse(json)["actions"]["attack"]
                          .contains("hitArcDegrees"),
          "default combat, blend, and door fields are omitted from serialized definitions");
    Check(!Json::parse(json)["actions"]["walk"]
                          .contains("footstepPhases")
                  && game::GetNpcAction(
                          defaults,
                          game::NpcAction::Walk).footstepPhases
                          == game::kDefaultNpcFootstepPhases,
          "default NPC footstep phases remain backward-compatible and are omitted");

    Json partialCamera = Json::parse(R"({
        "formatVersion": 1,
        "id": "partial_camera",
        "modelPath": "assets/models/characters/Fred.glb",
        "actions": {"attack": {"cameraImpact": {"rollKickDegrees": 6.0}}}
    })");
    Check(game::ParseNpcDefinitionJson(
                  partialCamera.dump(), parsed, error)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack).cameraImpact
                                  .pitchKickDegrees,
                          game::kDefaultNpcAttackCameraImpactPitchKickDegrees)
                  && Near(game::GetNpcAction(
                                  parsed,
                                  game::NpcAction::Attack).cameraImpact
                                  .rollKickDegrees,
                          6.0f),
          "partial camera impact objects retain defaults for omitted fields");
    Check(game::SerializeNpcDefinitionJson(parsed, json, error)
                  && Json::parse(json)["actions"]["attack"]["cameraImpact"]
                          .size() == 1
                  && Near(Json::parse(json)["actions"]["attack"]
                                  ["cameraImpact"]["rollKickDegrees"]
                                  .get<float>(),
                          6.0f),
          "camera impact serialization omits individual default-valued fields");

    defaults.canOpenDoors = false;
    Check(game::SerializeNpcDefinitionJson(defaults, json, error)
                  && Json::parse(json)["canOpenDoors"] == false
                  && game::ParseNpcDefinitionJson(json, parsed, error)
                  && !parsed.canOpenDoors,
          "disabled door-opening capability round-trips explicitly");

    Json incomplete = document;
    incomplete["actions"] = Json::object();
    incomplete.erase("aiType");
    Check(game::ParseNpcDefinitionJson(incomplete.dump(), parsed, error)
                  && game::GetNpcAction(parsed, game::NpcAction::Idle).animation.empty()
                  && game::GetNpcAction(parsed, game::NpcAction::Walk).animation.empty(),
          "unassigned actions remain valid and default safely");
}

void TestHeadLookDefinitionRoundTripAndValidation()
{
    game::NpcDefinition definition = MakeDefinition("friendly_look");
    definition.hostile = false;
    definition.headLook.enabled = true;
    definition.headLook.boneName = "Head";
    definition.headLook.rangeWorld = 7.5f;
    definition.headLook.maxYawDegrees = 55.0f;
    definition.headLook.maxPitchDegrees = 25.0f;
    std::string json;
    std::string error;
    game::NpcDefinition parsed;
    Check(game::SerializeNpcDefinitionJson(definition, json, error)
                  && Json::parse(json)["headLook"]["enabled"] == true
                  && Json::parse(json)["headLook"]["boneName"] == "Head"
                  && game::ParseNpcDefinitionJson(json, parsed, error)
                  && parsed.headLook.enabled
                  && parsed.headLook.boneName == "Head"
                  && Near(parsed.headLook.rangeWorld, 7.5f)
                  && Near(parsed.headLook.maxYawDegrees, 55.0f)
                  && Near(parsed.headLook.maxPitchDegrees, 25.0f),
          "friendly NPC head-look settings round-trip through optional JSON");

    definition.headLook.boneName.clear();
    Check(!game::ValidateNpcDefinition(definition, error),
          "enabled head look requires a configured bone");
    definition.headLook.boneName = "Head";
    definition.hostile = true;
    Check(!game::ValidateNpcDefinition(definition, error),
          "hostile NPC definitions reject enabled friendly head look");
    definition.hostile = false;
    definition.headLook.maxYawDegrees = 90.01f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "head-look yaw beyond the anatomical editor limit is rejected");
    definition.headLook.maxYawDegrees = 45.0f;
    definition.headLook.maxPitchDegrees = 60.01f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "head-look pitch beyond the anatomical editor limit is rejected");

    Json unknownHeadLookField = Json::parse(json);
    unknownHeadLookField["headLook"]["rollDegrees"] = 5.0f;
    Check(!game::ParseNpcDefinitionJson(
                  unknownHeadLookField.dump(), parsed, error),
          "unknown NPC head-look JSON fields are rejected");
}

void TestValidation()
{
    game::NpcDefinition definition = MakeDefinition("Fred-Johnson_2");
    std::string error;
    Check(game::ValidateNpcDefinition(definition, error),
          "portable mixed-case NPC ID validates");

    definition.id = "bad/id";
    Check(!game::ValidateNpcDefinition(definition, error),
          "path-like NPC ID is rejected");
    definition = MakeDefinition("fred");
    definition.modelPath = "assets/models/barrel.glb";
    Check(!game::ValidateNpcDefinition(definition, error),
          "model outside the character folder is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(definition, game::NpcAction::Idle).animationSpeed = 0.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "non-positive animation speed is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(definition, game::NpcAction::Run).movementSpeed = 201.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "out-of-range movement speed is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Walk).footstepPhases = {0.8f, 0.2f};
    Check(!game::ValidateNpcDefinition(definition, error),
          "unordered NPC footstep phases are rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Run).footstepPhases = {0.2f, 1.0f};
    Check(!game::ValidateNpcDefinition(definition, error),
          "out-of-range NPC footstep phases are rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Idle).footstepPhases = {0.1f, 0.6f};
    Check(!game::ValidateNpcDefinition(definition, error),
          "non-locomotion actions cannot define footstep phases");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).advanceSpeedMultiplier = 4.01f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "out-of-range attack advance multiplier is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).aimTrackingEndPhase = 0.75f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "attack aim tracking beyond the hit phase is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).hitArcDegrees = 361.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "out-of-range attack hit arc is rejected");
    definition = MakeDefinition("fred");
    definition.animationBlendSeconds = 0.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "zero animation blend duration is rejected");
    definition.animationBlendSeconds = 3.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "excessive animation blend duration is rejected");
    definition = MakeDefinition("fred");
    definition.baseHealth = 0;
    Check(!game::ValidateNpcDefinition(definition, error),
          "zero NPC health is rejected");
    definition = MakeDefinition("fred");
    definition.corpseDespawnDelaySeconds = -0.001f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "negative corpse delay is rejected");
    definition = MakeDefinition("fred");
    definition.corpseFadeDurationSeconds = 0.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "zero corpse fade duration is rejected");
    definition = MakeDefinition("fred");
    definition.playerDetectedSoundPath = "../detected.wav";
    Check(!game::ValidateNpcDefinition(definition, error),
          "unsafe NPC player-detected audio path is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition, game::NpcAction::Idle).soundPath = "npc/idle.wav";
    Check(!game::ValidateNpcDefinition(definition, error),
          "sound assignment on a non-vocal action is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition, game::NpcAction::Hurt).soundPath = "../hurt.wav";
    Check(!game::ValidateNpcDefinition(definition, error),
          "unsafe NPC action audio path is rejected");
    definition = MakeDefinition("fred");
    definition.ambientVocalizations.soundPaths = {
            "npc/moan.wav", "npc/moan.wav"};
    Check(!game::ValidateNpcDefinition(definition, error),
          "duplicate ambient vocalization paths are rejected");
    definition = MakeDefinition("fred");
    definition.ambientVocalizations.minimumDelaySeconds = 13.0f;
    definition.ambientVocalizations.maximumDelaySeconds = 12.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "inverted ambient quiet-time range is rejected");

    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).cameraImpact.springFrequencyHz = 0.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "zero camera impact spring frequency is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).cameraImpact.rollKickDegrees = 46.0f;
    Check(!game::ValidateNpcDefinition(definition, error),
          "excessive camera impact kick is rejected");
    definition = MakeDefinition("fred");
    game::GetNpcAction(
            definition,
            game::NpcAction::Attack).cameraImpact.maxPitchDegrees = NAN;
    Check(!game::ValidateNpcDefinition(definition, error),
          "non-finite camera impact values are rejected");

    Json invalidActionSound = Json::parse(R"({
        "formatVersion": 1,
        "id": "fred",
        "name": "Fred",
        "modelPath": "assets/models/characters/Fred.glb",
        "actions": {"idle": {"sound": "npc/idle.wav"}}
    })");
    Check(!game::ParseNpcDefinitionJson(
                  invalidActionSound.dump(), definition, error),
          "non-vocal action sound field is rejected by the JSON parser");

    Json invalidFootstepCount = Json::parse(R"({
        "formatVersion": 1,
        "id": "fred",
        "modelPath": "assets/models/characters/Fred.glb",
        "actions": {"walk": {"footstepPhases": [0.25]}}
    })");
    Check(!game::ParseNpcDefinitionJson(
                  invalidFootstepCount.dump(), definition, error),
          "footstep phase JSON requires exactly two values");

    Json invalidIdleFootsteps = Json::parse(R"({
        "formatVersion": 1,
        "id": "fred",
        "modelPath": "assets/models/characters/Fred.glb",
        "actions": {"idle": {"footstepPhases": [0.0, 0.5]}}
    })");
    Check(!game::ParseNpcDefinitionJson(
                  invalidIdleFootsteps.dump(), definition, error),
          "footstep phase JSON is rejected for non-locomotion actions");

    Json unknownCameraField = Json::parse(R"({
        "formatVersion": 1,
        "id": "fred",
        "modelPath": "assets/models/characters/Fred.glb",
        "actions": {"attack": {"cameraImpact": {"shake": 1}}}
    })");
    Check(!game::ParseNpcDefinitionJson(
                  unknownCameraField.dump(), definition, error),
          "unknown camera impact fields are rejected by the JSON parser");
}

void TestSeekAndDestroyPluginBoundary()
{
    const game::NpcAiTypeDescriptor* type = game::FindNpcAiType(
            game::kSeekAndDestroyNpcAiType);
    Check(type != nullptr && type->update != nullptr
                    && type->alignment == game::NpcAiAlignment::Hostile,
          "Seek & Destroy is registered as a hostile AI plugin");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    4.0f, 1.0f, false, true})
                    == game::NpcAiIntent::ChasePlayer,
          "Seek & Destroy chases outside melee range");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    1.05f, 1.0f, false, true,
                    game::NpcAiIntent::ChasePlayer})
                    == game::NpcAiIntent::AttackPlayer,
          "Seek & Destroy engages inside the melee navigation tolerance");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    1.20f, 1.0f, false, true,
                    game::NpcAiIntent::AttackPlayer})
                    == game::NpcAiIntent::AttackPlayer,
          "Seek & Destroy remains engaged inside the wider melee hysteresis band");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    1.26f, 1.0f, false, true,
                    game::NpcAiIntent::AttackPlayer})
                    == game::NpcAiIntent::ChasePlayer,
          "Seek & Destroy resumes chasing outside the melee hysteresis band");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    3.0f, 1.0f, true, true})
                    == game::NpcAiIntent::AttackPlayer,
          "Seek & Destroy preserves attack commitment outside melee range");
    Check(type != nullptr && type->update(game::NpcAiPluginInput{
                    0.5f, 1.0f, true, false,
                    game::NpcAiIntent::AttackPlayer})
                    == game::NpcAiIntent::Idle,
          "Seek & Destroy resets to idle when the player is dead");
}

void TestDiscoveryErrorsAreRetained()
{
    Sandbox sandbox;
    std::string error;
    Check(game::SaveNpcDefinition(
                  sandbox.root / "alpha.json", MakeDefinition("alpha"), error),
          "discovery fixture saves");
    {
        std::ofstream malformed(sandbox.root / "broken.json");
        malformed << "{not json";
    }
    game::NpcDefinitionCatalog catalog;
    Check(!game::DiscoverNpcDefinitions(sandbox.root, catalog)
                  && catalog.definitions.size() == 1
                  && catalog.errors.size() == 1,
          "discovery keeps valid definitions and reports malformed files");

    Check(game::SaveNpcDefinition(
                  sandbox.root / "wrong_filename.json", MakeDefinition("other"), error),
          "filename mismatch fixture saves");
    Check(!game::DiscoverNpcDefinitions(sandbox.root, catalog)
                  && catalog.errors.size() == 2,
          "definition filename must match its stable ID");
}

void TestDraftSaveCancelRenameDeleteAndSessionView()
{
    Sandbox sandbox;
    const std::filesystem::path definitionsRoot = sandbox.root / "npcs";
    const std::filesystem::path modelPath = sandbox.root / "Zombie1.glb";
    {
        std::ofstream model(modelPath, std::ios::binary);
        model << "model data must survive definition deletion";
    }
    std::string error;
    Check(game::SaveNpcDefinition(
                  definitionsRoot / "alpha.json", MakeDefinition("alpha"), error)
                  && game::SaveNpcDefinition(
                          definitionsRoot / "beta.json", MakeDefinition("beta"), error),
          "editor service fixtures save");

    game::SectorEditorNpcEditorState state;
    game::SectorEditorNpcEditorSessionState session;
    std::string status;
    game::SectorEditorNpcEditorService service{
            state, session, status, definitionsRoot};
    Check(service.Open() && state.drafts.size() == 2,
          "NPC editor service opens a valid catalog");
    Check(service.SelectIndex(1), "second NPC can be selected");
    session.listScroll.offset.y = 42.0f;
    session.formScroll.offset.y = 84.0f;
    service.SelectedDraft()->definition.name = "Discarded";
    service.Cancel(nullptr);
    Check(!state.open, "Cancel closes the NPC editor");

    Check(service.Open()
                  && service.SelectedDraft() != nullptr
                  && service.SelectedDraft()->definition.id == "beta"
                  && service.SelectedDraft()->definition.name == "Test Character"
                  && Near(session.listScroll.offset.y, 42.0f)
                  && Near(session.formScroll.offset.y, 84.0f),
          "reopen restores session selection and scrolls but discards draft edits");

    service.SelectedDraft()->definition.id = "renamed_beta";
    session.selectedNpcId = "renamed_beta";
    Check(service.SaveAndClose(nullptr)
                  && std::filesystem::exists(definitionsRoot / "renamed_beta.json")
                  && !std::filesystem::exists(definitionsRoot / "beta.json"),
          "Save commits an ID-based file rename");

    Check(service.Open()
                  && service.SelectedDraft() != nullptr
                  && service.SelectedDraft()->definition.id == "renamed_beta",
          "saved rename updates the session selection");
    service.SetSelectedAnimationBlendSeconds(0.45f);
    Check(Near(service.SelectedDraft()->definition.animationBlendSeconds, 0.45f),
          "NPC editor service updates animation blending through its draft API");
    service.SetSelectedHostile(false);
    game::NpcHeadLookDefinition headLook;
    headLook.enabled = true;
    headLook.boneName = "Head";
    headLook.rangeWorld = 6.0f;
    service.SetSelectedHeadLook(headLook);
    Check(service.SelectedDraft()->definition.headLook.enabled
                  && service.SelectedDraft()->definition.headLook.boneName
                          == "Head",
          "NPC editor service updates friendly head-look settings");
    service.SetSelectedHostile(true);
    Check(!service.SelectedDraft()->definition.headLook.enabled
                  && service.SelectedDraft()->definition.headLook.boneName
                          == "Head",
          "making an NPC hostile disables head look without erasing its tuning");
    service.SetSelectedCanOpenDoors(false);
    Check(!service.SelectedDraft()->definition.canOpenDoors,
          "NPC editor service updates door-opening capability through its draft API");
    service.SetSelectedBaseHealth(240);
    service.SetSelectedDespawnOnDeath(true);
    service.SetSelectedCorpseDespawnDelayMilliseconds(3200);
    service.SetSelectedCorpseFadeDurationMilliseconds(650);
    Check(service.SelectedDraft()->definition.baseHealth == 240
                  && service.SelectedDraft()->definition.despawnOnDeath
                  && Near(service.SelectedDraft()->definition.corpseDespawnDelaySeconds, 3.2f)
                  && Near(service.SelectedDraft()->definition.corpseFadeDurationSeconds, 0.65f),
          "NPC editor service updates health and corpse behavior through its draft API");
    service.SetSelectedActionSound(
            game::NpcAction::Hurt, "npc/zombie/hurt.wav");
    service.SetSelectedPlayerDetectedSound(
            "npc/zombie/player_detected.wav");
    service.SetSelectedActionSound(
            game::NpcAction::Attack, "npc/zombie/impact.wav");
    service.SetSelectedAttackSound("npc/zombie/attack.wav");
    service.SetSelectedAttackMotion(0.75f, 0.2f, 90.0f);
    game::NpcAttackCameraImpactDefinition cameraImpact;
    cameraImpact.pitchKickDegrees = 6.5f;
    cameraImpact.rollKickDegrees = 7.5f;
    cameraImpact.springDampingRatio = 0.6f;
    service.SetSelectedAttackCameraImpact(cameraImpact);
    service.SetSelectedFootstepPhase(game::NpcAction::Walk, 0, 0.15f);
    service.SetSelectedFootstepPhase(game::NpcAction::Walk, 1, 0.65f);
    service.SetSelectedAmbientDelayRange(6.0f, 11.0f);
    Check(service.AddSelectedAmbientSound("npc/zombie/moan_01.wav")
                  && !service.AddSelectedAmbientSound(
                          "npc/zombie/moan_01.wav")
                  && service.ReplaceSelectedAmbientSound(
                          0, "npc/zombie/moan_02.wav")
                  && game::GetNpcAction(
                          service.SelectedDraft()->definition,
                          game::NpcAction::Hurt).soundPath
                          == "npc/zombie/hurt.wav"
                  && service.SelectedDraft()->definition
                                  .playerDetectedSoundPath
                          == "npc/zombie/player_detected.wav"
                  && game::GetNpcAction(
                          service.SelectedDraft()->definition,
                          game::NpcAction::Attack).soundPath
                          == "npc/zombie/impact.wav"
                  && game::GetNpcAction(
                          service.SelectedDraft()->definition,
                          game::NpcAction::Attack).attackSoundPath
                          == "npc/zombie/attack.wav"
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Attack)
                                  .advanceSpeedMultiplier,
                          0.75f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Attack)
                                  .aimTrackingEndPhase,
                          0.2f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Attack).hitArcDegrees,
                          90.0f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Attack).cameraImpact
                                  .pitchKickDegrees,
                          6.5f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Attack).cameraImpact
                                  .springDampingRatio,
                          0.6f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Walk).footstepPhases[0],
                          0.15f)
                  && Near(game::GetNpcAction(
                                  service.SelectedDraft()->definition,
                                  game::NpcAction::Walk).footstepPhases[1],
                          0.65f)
                  && Near(service.SelectedDraft()->definition
                                  .ambientVocalizations.minimumDelaySeconds,
                          6.0f)
                  && Near(service.SelectedDraft()->definition
                                  .ambientVocalizations.maximumDelaySeconds,
                          11.0f)
                  && service.SelectedDraft()->definition
                                  .ambientVocalizations.soundPaths.front()
                          == "npc/zombie/moan_02.wav"
                  && service.RemoveSelectedAmbientSound(0)
                  && service.SelectedDraft()->definition
                          .ambientVocalizations.soundPaths.empty(),
          "NPC editor service updates attack, vocal, and ambient audio drafts");
    service.SetSelectedPlayerDetectedSound({});
    Check(service.SelectedDraft()->definition.playerDetectedSoundPath.empty(),
          "NPC editor service clears the optional player-detected sound");
    service.SelectedDraft()->definition.id = "RENAMED_beta";
    session.selectedNpcId = "RENAMED_beta";
    Check(service.SaveAndClose(nullptr)
                  && std::filesystem::exists(definitionsRoot / "RENAMED_beta.json")
                  && !std::filesystem::exists(definitionsRoot / "renamed_beta.json"),
          "case-only ID rename replaces the old definition path");

    Check(service.Open()
                  && service.SelectedDraft() != nullptr
                  && service.SelectedDraft()->definition.id == "RENAMED_beta",
          "case-only rename keeps the renamed NPC selected");
    service.RequestDeleteSelected();
    service.ConfirmDeleteSelected();
    Check(service.SaveAndClose(nullptr)
                  && !std::filesystem::exists(definitionsRoot / "RENAMED_beta.json")
                  && std::filesystem::exists(modelPath),
          "saved deletion removes only the JSON definition");
}

void TestCatalogErrorsBlockEditorSave()
{
    Sandbox sandbox;
    const std::filesystem::path definitionsRoot = sandbox.root / "npcs";
    std::filesystem::create_directories(definitionsRoot);
    {
        std::ofstream malformed(definitionsRoot / "broken.json");
        malformed << "broken";
    }
    game::SectorEditorNpcEditorState state;
    game::SectorEditorNpcEditorSessionState session;
    std::string status;
    game::SectorEditorNpcEditorService service{
            state, session, status, definitionsRoot};
    Check(!service.Open() && !state.catalogErrors.empty(),
          "editor opens malformed catalog with blocking diagnostics");
    service.AddDefinition();
    service.SelectedDraft()->definition.modelPath =
            "assets/models/characters/TestCharacter.glb";
    Check(!service.SaveAndClose(nullptr)
                  && state.open
                  && std::filesystem::exists(definitionsRoot / "broken.json"),
          "catalog errors block save without deleting malformed input");
    service.Cancel(nullptr);
}

} // namespace

int main()
{
    TestRoundTripDefaultsAndSharedClips();
    TestHeadLookDefinitionRoundTripAndValidation();
    TestValidation();
    TestSeekAndDestroyPluginBoundary();
    TestDiscoveryErrorsAreRetained();
    TestDraftSaveCancelRenameDeleteAndSessionView();
    TestCatalogErrorsBlockEditorSave();
    if (failures == 0) {
        std::cout << "NPC definition tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
