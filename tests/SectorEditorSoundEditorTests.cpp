#include "sector_editor/sounds/SectorEditorSoundEditorService.h"

#include <cstdio>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void AddSound(
        game::SectorAuthoringGraph& graph,
        const char* id,
        game::SectorSoundType type,
        const char* path)
{
    graph.audioSettings.soundsById.emplace(
            id, game::SectorSoundDefinition{id, path, type});
}

struct Fixture {
    game::SectorEditorSoundEditorState state;
    game::SectorAuthoringGraph graph;
    game::SectorTopologyMap topology;
    game::SectorEditorDerivationState derivation;
    game::SectorEditorDocumentLifecycleState lifecycle;
    std::string status;

    game::SectorEditorSoundEditorService Service()
    {
        return game::SectorEditorSoundEditorService{
                state, graph, topology,
                game::MakeSectorEditorDerivationDocumentAccess(derivation),
                game::MakeSectorEditorDocumentLifecycleAccess(lifecycle),
                status};
    }
};

void TestDraftLifecycleAndValidation()
{
    Fixture fixture;
    AddSound(fixture.graph, "z_sound", game::SectorSoundType::Sound,
            "sfx/z.wav");
    AddSound(fixture.graph, "ambient", game::SectorSoundType::Music,
            "ambience/ambient.ogg");

    auto editor = fixture.Service();
    editor.Open();
    Check(fixture.state.drafts.size() == 2
                    && fixture.state.drafts[0].definition.id == "ambient"
                    && fixture.state.drafts[1].definition.id == "z_sound",
            "open builds an alphabetically sorted draft list");

    editor.AddSound();
    Check(editor.SelectedDraft() != nullptr
                    && editor.SelectedDraft()->definition.id == "new_sound"
                    && editor.SelectedDraft()->definition.type
                            == game::SectorSoundType::Sound,
            "add creates and selects a uniquely named buffered sound draft");
    Check(!editor.SaveAndClose(), "a draft without a file cannot be saved");
    Check(editor.SetSelectedPath("sfx/new.wav"),
            "a valid picked audio path is accepted");
    std::snprintf(fixture.state.idBuffer, sizeof(fixture.state.idBuffer), "%s",
            "ambient");
    Check(!editor.ApplyIdBuffer(), "duplicate IDs are rejected immediately");
    std::snprintf(fixture.state.idBuffer, sizeof(fixture.state.idBuffer), "%s",
            "new_click");
    Check(editor.ApplyIdBuffer(), "a unique valid ID is accepted");
    Check(editor.SaveAndClose(), "valid drafts save successfully");
    Check(fixture.graph.audioSettings.soundsById.count("new_click") == 1
                    && fixture.topology.audioSettings.soundsById.count("new_click") == 1,
            "save synchronizes authoring and compiled audio settings");
    Check(fixture.lifecycle.topologyDocumentDirty
                    && fixture.lifecycle.hasUnsavedChanges,
            "saving changed sounds marks the level dirty");

    const size_t savedCount = fixture.graph.audioSettings.soundsById.size();
    editor.Open();
    editor.AddSound();
    editor.Cancel();
    Check(fixture.graph.audioSettings.soundsById.size() == savedCount,
            "cancel discards sound drafts");
}

void TestKnownReferencesLockIdentityButAllowPathReplacement()
{
    Fixture fixture;
    AddSound(fixture.graph, "office_tone", game::SectorSoundType::Music,
            "ambience/office.ogg");
    AddSound(fixture.graph, "machine", game::SectorSoundType::Sound,
            "sfx/machine.wav");
    AddSound(fixture.graph, "door_click", game::SectorSoundType::Sound,
            "sfx/door.wav");

    game::SectorAuthoringFaceAnchor anchor;
    anchor.id = 7;
    anchor.roomtone.mode = game::SectorRoomtoneMode::Play;
    anchor.roomtone.soundId = "office_tone";
    fixture.graph.faceAnchors.push_back(anchor);
    fixture.topology.sectors.push_back(game::SectorTopologySector{});
    fixture.topology.sectors.back().id = 34;
    fixture.topology.sectors.back().name = "Great Hall";
    fixture.derivation.authoringDerivation.mapping.sectors.push_back(
            game::SectorAuthoringDerivedSectorMapping{-1, 7, 34});

    game::SectorAuthoringSoundEmitter emitter;
    emitter.id = 9;
    emitter.referenceId = "generator";
    emitter.soundId = "machine";
    fixture.graph.soundEmitters.push_back(emitter);

    game::SectorPlacedRuntimeObject door;
    door.id = 12;
    door.kind = "door";
    door.door.instanceId = "office_door";
    door.door.openSoundId = "door_click";
    fixture.topology.runtimeObjects.push_back(door);

    auto editor = fixture.Service();
    editor.Open();
    int officeIndex = -1;
    int machineIndex = -1;
    int doorIndex = -1;
    for (size_t index = 0; index < fixture.state.drafts.size(); ++index) {
        const std::string& id = fixture.state.drafts[index].definition.id;
        if (id == "office_tone") officeIndex = static_cast<int>(index);
        if (id == "machine") machineIndex = static_cast<int>(index);
        if (id == "door_click") doorIndex = static_cast<int>(index);
    }

    editor.SelectIndex(officeIndex);
    Check(editor.SelectedIsReferenced()
                    && fixture.state.usageText.find("Sector 34 \"Great Hall\"")
                            != std::string::npos,
            "roomtone usage reports the derived sector ID and name");
    Check(editor.SetSelectedPath("ambience/office-new.mp3"),
            "referenced entries allow file replacement");
    Check(!editor.SetSelectedType(game::SectorSoundType::Sound),
            "referenced entries block type changes");
    Check(!editor.RequestDeleteSelected(),
            "referenced entries block removal");
    std::snprintf(fixture.state.idBuffer, sizeof(fixture.state.idBuffer), "%s",
            "renamed_tone");
    Check(!editor.ApplyIdBuffer(), "referenced entries block renaming");

    editor.SelectIndex(machineIndex);
    Check(fixture.state.usageText.find("Sound Emitter \"generator\"")
                    != std::string::npos,
            "Sound Emitter usage is reported");
    editor.SelectIndex(doorIndex);
    Check(fixture.state.usageText.find("Door \"office_door\" open sound")
                    != std::string::npos,
            "door open/close usage is reported");
}

void TestUnusedEntriesCanChangeAndRoomtoneFadeSynchronizes()
{
    Fixture fixture;
    AddSound(fixture.graph, "unused", game::SectorSoundType::Sound,
            "sfx/unused.wav");
    fixture.derivation.authoringDerivation.success = true;
    fixture.derivation.lastValidAuthoringDerivedTopology = game::SectorTopologyMap{};

    auto editor = fixture.Service();
    editor.Open();
    Check(editor.SetSelectedType(game::SectorSoundType::Music),
            "unused entries can change type");
    std::snprintf(fixture.state.idBuffer, sizeof(fixture.state.idBuffer), "%s",
            "renamed_unused");
    Check(editor.ApplyIdBuffer(), "unused entries can be renamed");
    Check(editor.RequestDeleteSelected() && editor.ConfirmDeleteSelected(),
            "unused entries can be removed");
    Check(editor.SaveAndClose()
                    && fixture.graph.audioSettings.soundsById.empty(),
            "removing an unused draft applies on save");

    Check(editor.SetRoomtoneFadeMilliseconds(1750),
            "the roomtone fade editor accepts a changed value");
    Check(fixture.graph.audioSettings.roomtoneFadeMilliseconds == 1750
                    && fixture.topology.audioSettings.roomtoneFadeMilliseconds == 1750
                    && fixture.derivation.authoringDerivation.topology
                            .audioSettings.roomtoneFadeMilliseconds == 1750
                    && fixture.derivation.lastValidAuthoringDerivedTopology
                            ->audioSettings.roomtoneFadeMilliseconds == 1750,
            "roomtone fade remains authoring-owned and synchronizes compiled copies");
}

} // namespace

int main()
{
    TestDraftLifecycleAndValidation();
    TestKnownReferencesLockIdentityButAllowPathReplacement();
    TestUnusedEntriesCanChangeAndRoomtoneFadeSynchronizes();
    if (failures != 0) {
        std::cerr << failures << " Sound Editor test(s) failed\n";
        return 1;
    }
    std::cout << "Sound Editor tests passed\n";
    return 0;
}
