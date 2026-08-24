#include "sector_editor/services/asset_pruning/SectorEditorAssetPruning.h"

#include <string>
#include <unordered_set>

namespace game {
namespace {

void AddTextureReference(
        std::unordered_set<std::string>& usedTextureIds,
        const std::string& materialId)
{
    if (!materialId.empty()) {
        usedTextureIds.insert(materialId);
    }
}

void AddTextureReferences(
        std::unordered_set<std::string>& usedTextureIds,
        const SectorTopologyWallPartSettings& part)
{
    AddTextureReference(usedTextureIds, part.materialId);
    AddTextureReference(usedTextureIds, part.decal.materialId);
}

std::unordered_set<std::string> CollectUsedTextureIds(
        const SectorAuthoringGraph& authoringGraph,
        const SectorTopologyMap& map)
{
    std::unordered_set<std::string> usedTextureIds;
    usedTextureIds.reserve(
            3 + authoringGraph.faceAnchors.size() * 10
            + authoringGraph.lineSides.size() * 8
            + map.runtimeObjects.size());

    // These are required editor defaults, even when the current map does not
    // reference them yet.
    usedTextureIds.insert("wall");
    usedTextureIds.insert("floor");
    usedTextureIds.insert("ceiling");

    for (const SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
        AddTextureReference(usedTextureIds, anchor.floorMaterialId);
        AddTextureReference(usedTextureIds, anchor.ceilingMaterialId);
        AddTextureReference(usedTextureIds, anchor.floorDecal.materialId);
        AddTextureReference(usedTextureIds, anchor.ceilingDecal.materialId);
        AddTextureReferences(usedTextureIds, anchor.defaultWall);
        AddTextureReferences(usedTextureIds, anchor.defaultLower);
        AddTextureReferences(usedTextureIds, anchor.defaultUpper);
    }

    for (const SectorAuthoringLineSide& side : authoringGraph.lineSides) {
        AddTextureReferences(usedTextureIds, side.wall);
        AddTextureReferences(usedTextureIds, side.lower);
        AddTextureReferences(usedTextureIds, side.upper);
        AddTextureReferences(usedTextureIds, side.middle);
    }

    AddTextureReference(usedTextureIds, map.skySettings.materialId);
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "door") {
            AddTextureReference(usedTextureIds, object.door.materialId);
        }
    }
    return usedTextureIds;
}

std::unordered_set<std::string> CollectUsedSoundIds(
        const SectorAuthoringGraph& authoringGraph,
        const SectorTopologyMap& map)
{
    std::unordered_set<std::string> usedSoundIds;
    usedSoundIds.reserve(map.runtimeObjects.size() * 2
            + authoringGraph.faceAnchors.size()
            + authoringGraph.soundEmitters.size());
    for (const SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
        if (anchor.roomtone.mode == SectorRoomtoneMode::Play
                && !anchor.roomtone.soundId.empty()) {
            usedSoundIds.insert(anchor.roomtone.soundId);
        }
    }
    for (const SectorAuthoringSoundEmitter& emitter : authoringGraph.soundEmitters) {
        if (!emitter.soundId.empty()) usedSoundIds.insert(emitter.soundId);
    }
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind != "door") continue;
        if (!object.door.openSoundId.empty()) {
            usedSoundIds.insert(object.door.openSoundId);
        }
        if (!object.door.closeSoundId.empty()) {
            usedSoundIds.insert(object.door.closeSoundId);
        }
    }
    return usedSoundIds;
}

template<typename AssetMap>
std::size_t EraseUnusedAssets(
        AssetMap& assetsById,
        const std::unordered_set<std::string>& usedIds)
{
    std::size_t removedCount = 0;
    for (auto it = assetsById.begin(); it != assetsById.end();) {
        if (usedIds.find(it->first) != usedIds.end()) {
            ++it;
            continue;
        }
        it = assetsById.erase(it);
        ++removedCount;
    }
    return removedCount;
}

} // namespace

SectorEditorAssetPruneResult PruneUnusedSectorEditorAssets(
        const SectorAuthoringGraph& authoringGraph,
        SectorTopologyMap& map,
        const SectorEditorAssetPruneOptions& options)
{
    SectorEditorAssetPruneResult result;
    if (options.pruneTextures) {
        result.removedTextureCount = EraseUnusedAssets(
                map.resolvedMaterialsById,
                CollectUsedTextureIds(authoringGraph, map));
    }
    if (options.pruneSounds) {
        result.removedSoundCount = EraseUnusedAssets(
                map.audioSettings.soundsById,
                CollectUsedSoundIds(authoringGraph, map));
    }
    return result;
}

} // namespace game
