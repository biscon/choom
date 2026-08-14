#include "game/npc/NpcDefinitions.h"
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
    return definition;
}

void TestRoundTripDefaultsAndSharedClips()
{
    game::NpcDefinition original = MakeDefinition("zombie_test");
    original.animationBlendSeconds = 0.35f;
    std::string json;
    std::string error;
    Check(game::SerializeNpcDefinitionJson(original, json, error),
          "valid NPC definition serializes");
    const Json document = Json::parse(json);
    Check(document["formatVersion"] == 1
                  && Near(document["animationBlendSeconds"].get<float>(), 0.35f)
                  && document["actions"]["walk"]["animation"] == "Walk"
                  && document["actions"]["run"]["animation"] == "Walk",
          "serialized actions may share a GLB animation");

    game::NpcDefinition parsed;
    Check(game::ParseNpcDefinitionJson(json, parsed, error),
          "serialized NPC definition parses");
    Check(parsed.id == original.id
                  && parsed.name == original.name
                  && parsed.hostile
                  && parsed.modelPath == original.modelPath
                  && Near(parsed.animationBlendSeconds, 0.35f)
                  && game::GetNpcAction(parsed, game::NpcAction::Walk).animation == "Walk"
                  && game::GetNpcAction(parsed, game::NpcAction::Run).animation == "Walk"
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Run).animationSpeed, 1.6f)
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Walk).movementSpeed, 1.5f)
                  && Near(game::GetNpcAction(parsed, game::NpcAction::Run).movementSpeed, 3.0f),
          "NPC fields and action defaults round-trip");

    game::NpcDefinition defaults = MakeDefinition("defaults_test");
    Check(game::SerializeNpcDefinitionJson(defaults, json, error)
                  && !Json::parse(json).contains("animationBlendSeconds"),
          "default animation blend duration is omitted from serialized definitions");

    Json incomplete = document;
    incomplete["actions"] = Json::object();
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
    TestDiscoveryErrorsAreRetained();
    TestDraftSaveCancelRenameDeleteAndSessionView();
    TestCatalogErrorsBlockEditorSave();
    if (failures == 0) {
        std::cout << "NPC definition tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
