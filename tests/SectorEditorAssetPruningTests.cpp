#include "sector_editor/SectorEditorAssetPruneModal.h"
#include "sector_editor/services/asset_pruning/SectorEditorAssetPruning.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

void AddTexture(game::SectorTopologyMap& map, const std::string& id)
{
    game::SectorTextureDefinition texture;
    texture.id = id;
    texture.path = "assets/images/" + id + ".png";
    map.texturesById.emplace(id, std::move(texture));
}

void AddSound(game::SectorTopologyMap& map, const std::string& id)
{
    game::SectorSoundDefinition sound;
    sound.id = id;
    sound.path = id + ".wav";
    map.audioSettings.soundsById.emplace(id, std::move(sound));
}

void TestPruneKeepsAllReferenceKindsAndDefaults()
{
    game::SectorAuthoringGraph graph;
    game::SectorAuthoringFaceAnchor anchor;
    anchor.floorTextureId = "face_floor";
    anchor.ceilingTextureId = "face_ceiling";
    anchor.floorDecal.textureId = "floor_decal";
    anchor.ceilingDecal.textureId = "ceiling_decal";
    anchor.defaultWall.textureId = "default_wall";
    anchor.defaultWall.decal.textureId = "default_wall_decal";
    anchor.defaultLower.textureId = "default_lower";
    anchor.defaultLower.decal.textureId = "default_lower_decal";
    anchor.defaultUpper.textureId = "default_upper";
    anchor.defaultUpper.decal.textureId = "default_upper_decal";
    graph.faceAnchors.push_back(anchor);

    game::SectorAuthoringLineSide side;
    side.wall.textureId = "side_wall";
    side.wall.decal.textureId = "side_wall_decal";
    side.lower.textureId = "side_lower";
    side.lower.decal.textureId = "side_lower_decal";
    side.upper.textureId = "side_upper";
    side.upper.decal.textureId = "side_upper_decal";
    side.middle.textureId = "side_middle";
    side.middle.decal.textureId = "side_middle_decal";
    graph.lineSides.push_back(side);

    game::SectorTopologyMap map;
    map.skySettings.textureId = "map_sky";
    game::SectorPlacedRuntimeObject door;
    door.kind = "door";
    door.door.textureId = "door_texture";
    door.door.openSoundId = "door_open";
    door.door.closeSoundId = "door_close";
    map.runtimeObjects.push_back(door);

    const char* keptTextures[] = {
            "wall", "floor", "ceiling",
            "face_floor", "face_ceiling", "floor_decal", "ceiling_decal",
            "default_wall", "default_wall_decal",
            "default_lower", "default_lower_decal",
            "default_upper", "default_upper_decal",
            "side_wall", "side_wall_decal", "side_lower", "side_lower_decal",
            "side_upper", "side_upper_decal", "side_middle", "side_middle_decal",
            "map_sky", "door_texture"};
    for (const char* id : keptTextures) AddTexture(map, id);
    AddTexture(map, "unused_texture");
    AddSound(map, "door_open");
    AddSound(map, "door_close");
    AddSound(map, "unused_sound");

    const game::SectorEditorAssetPruneResult result =
            game::PruneUnusedSectorEditorAssets(graph, map, {});

    Check(result.removedTextureCount == 1, "combined prune removes one unused texture");
    Check(result.removedSoundCount == 1, "combined prune removes one unused sound");
    for (const char* id : keptTextures) {
        Check(map.texturesById.find(id) != map.texturesById.end(),
              "combined prune retains every referenced and protected texture");
    }
    Check(map.audioSettings.soundsById.find("door_open")
                  != map.audioSettings.soundsById.end(),
          "combined prune retains the door open sound");
    Check(map.audioSettings.soundsById.find("door_close")
                  != map.audioSettings.soundsById.end(),
          "combined prune retains the door close sound");
}

void TestPruneCategoriesAreIndependent()
{
    game::SectorAuthoringGraph graph;
    game::SectorTopologyMap map;
    map.skySettings.textureId.clear();
    AddTexture(map, "wall");
    AddTexture(map, "floor");
    AddTexture(map, "ceiling");
    AddTexture(map, "unused_texture");
    AddSound(map, "unused_sound");

    game::SectorEditorAssetPruneOptions texturesOnly;
    texturesOnly.pruneSounds = false;
    const game::SectorEditorAssetPruneResult textureResult =
            game::PruneUnusedSectorEditorAssets(graph, map, texturesOnly);
    Check(textureResult.removedTextureCount == 1
                  && textureResult.removedSoundCount == 0,
          "texture-only prune reports only texture removals");
    Check(map.audioSettings.soundsById.find("unused_sound")
                  != map.audioSettings.soundsById.end(),
          "texture-only prune leaves sounds untouched");

    AddTexture(map, "another_unused_texture");
    game::SectorEditorAssetPruneOptions soundsOnly;
    soundsOnly.pruneTextures = false;
    const game::SectorEditorAssetPruneResult soundResult =
            game::PruneUnusedSectorEditorAssets(graph, map, soundsOnly);
    Check(soundResult.removedTextureCount == 0
                  && soundResult.removedSoundCount == 1,
          "sound-only prune reports only sound removals");
    Check(map.texturesById.find("another_unused_texture")
                  != map.texturesById.end(),
          "sound-only prune leaves textures untouched");
}

void TestMissingReferencesAndCleanMapsAreSafe()
{
    game::SectorAuthoringGraph graph;
    game::SectorAuthoringFaceAnchor anchor;
    anchor.floorTextureId = "missing_texture";
    graph.faceAnchors.push_back(anchor);

    game::SectorTopologyMap map;
    map.skySettings.textureId.clear();
    game::SectorPlacedRuntimeObject door;
    door.kind = "door";
    door.door.openSoundId = "missing_sound";
    map.runtimeObjects.push_back(door);

    const game::SectorEditorAssetPruneResult result =
            game::PruneUnusedSectorEditorAssets(graph, map, {});
    Check(result.removedTextureCount == 0 && result.removedSoundCount == 0,
          "missing references and empty registries are a safe no-op");
}

void TestPruneModalDefaultsAndReset()
{
    game::SectorEditorAssetPruneModalState state;
    state.pruneTextures = false;
    state.pruneSounds = false;
    game::OpenSectorEditorAssetPruneModal(state);
    Check(state.open && state.pruneTextures && state.pruneSounds,
          "prune modal opens with both categories selected");
    game::CloseSectorEditorAssetPruneModal(state);
    Check(!state.open && state.pruneTextures && state.pruneSounds,
          "prune modal close resets its draft choices");
}

} // namespace

int main()
{
    TestPruneKeepsAllReferenceKindsAndDefaults();
    TestPruneCategoriesAreIndependent();
    TestMissingReferencesAndCleanMapsAreSafe();
    TestPruneModalDefaultsAndReset();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorAssetPruningTests failure(s)\n";
        return 1;
    }
    return 0;
}
