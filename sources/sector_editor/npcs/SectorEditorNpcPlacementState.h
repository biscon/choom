#pragma once

#include "game/npc/NpcDefinitions.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorEditorNpcPlacementState {
    std::string lastDefinitionId;
    uint64_t catalogRevision = 0;
    bool optionsInitialized = false;
    std::vector<std::string> definitionIds;
    std::vector<std::string> optionLabelStorage;
    std::vector<const char*> optionLabels;
    std::string inspectorDefinitionId;
    std::string missingOptionLabel;
    std::vector<const char*> inspectorOptionLabels;

    char instanceIdBuffer[64] = {};
    int bufferedObjectId = -1;
    std::string instanceIdError;
};

void RefreshSectorEditorNpcPlacementOptions(
        SectorEditorNpcPlacementState& state,
        const NpcDefinitionCatalog& catalog,
        uint64_t catalogRevision);

int FindSectorEditorNpcPlacementOption(
        const SectorEditorNpcPlacementState& state,
        const std::string& definitionId);

std::string ResolveSectorEditorNpcPlacementDefault(
        const SectorEditorNpcPlacementState& state);

int PrepareSectorEditorNpcInspectorOptions(
        SectorEditorNpcPlacementState& state,
        const std::string& definitionId);

} // namespace game
