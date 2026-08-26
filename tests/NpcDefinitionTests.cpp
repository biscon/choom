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
    game::NpcActionDefinition& attack = game::GetNpcAction(
            original, game::NpcAction::Attack);
    attack.hitPhase = 0.6f;
    attack.rangeWorld = 1.25f;
    attack.damage = 23;
    attack.knockbackImpulseWorldPerSecond = 2.5f;
    attack.stunMilliseconds = 500;
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
                  && document["actions"]["run"]["animation"] == "Walk"
                  && document["actions"]["attack"]["animation"] == "Attack"
                  && document["actions"]["attack"]["damage"] == 23
                  && document["actions"]["attack"]["attackSound"]
                          == "npc/zombie/attack.wav"
                  && document["actions"]["attack"]["sound"]
                          == "npc/zombie/impact.wav"
                  && document["aiType"] == "seek_and_destroy"
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
                  && !Json::parse(json).contains("ambientVocalizations"),
          "default combat, blend, and door fields are omitted from serialized definitions");

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
    service.SetSelectedActionSound(
            game::NpcAction::Attack, "npc/zombie/impact.wav");
    service.SetSelectedAttackSound("npc/zombie/attack.wav");
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
                  && game::GetNpcAction(
                          service.SelectedDraft()->definition,
                          game::NpcAction::Attack).soundPath
                          == "npc/zombie/impact.wav"
                  && game::GetNpcAction(
                          service.SelectedDraft()->definition,
                          game::NpcAction::Attack).attackSoundPath
                          == "npc/zombie/attack.wav"
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
