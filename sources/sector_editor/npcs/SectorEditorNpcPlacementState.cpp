#include "sector_editor/npcs/SectorEditorNpcPlacementState.h"

namespace game {

void RefreshSectorEditorNpcPlacementOptions(
        SectorEditorNpcPlacementState& state,
        const NpcDefinitionCatalog& catalog,
        uint64_t catalogRevision)
{
    if (state.optionsInitialized && state.catalogRevision == catalogRevision) {
        return;
    }
    state.catalogRevision = catalogRevision;
    state.optionsInitialized = true;
    state.definitionIds.clear();
    state.optionLabelStorage.clear();
    state.optionLabels.clear();
    state.inspectorDefinitionId.clear();
    state.missingOptionLabel.clear();
    state.inspectorOptionLabels.clear();
    state.definitionIds.reserve(catalog.definitions.size());
    state.optionLabelStorage.reserve(catalog.definitions.size());
    for (const NpcDefinition& definition : catalog.definitions) {
        state.definitionIds.push_back(definition.id);
        state.optionLabelStorage.push_back(
                definition.name.empty()
                        ? definition.id
                        : definition.name + " (" + definition.id + ")");
    }
    state.optionLabels.reserve(state.optionLabelStorage.size());
    for (const std::string& label : state.optionLabelStorage) {
        state.optionLabels.push_back(label.c_str());
    }
    if (FindSectorEditorNpcPlacementOption(state, state.lastDefinitionId) < 0) {
        state.lastDefinitionId = state.definitionIds.empty()
                ? std::string{}
                : state.definitionIds.front();
    }
}

int FindSectorEditorNpcPlacementOption(
        const SectorEditorNpcPlacementState& state,
        const std::string& definitionId)
{
    for (size_t index = 0; index < state.definitionIds.size(); ++index) {
        if (state.definitionIds[index] == definitionId) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::string ResolveSectorEditorNpcPlacementDefault(
        const SectorEditorNpcPlacementState& state)
{
    const int selected = FindSectorEditorNpcPlacementOption(
            state,
            state.lastDefinitionId);
    if (selected >= 0) return state.lastDefinitionId;
    return state.definitionIds.empty() ? std::string{} : state.definitionIds.front();
}

int PrepareSectorEditorNpcInspectorOptions(
        SectorEditorNpcPlacementState& state,
        const std::string& definitionId)
{
    if (state.inspectorDefinitionId != definitionId
            || state.inspectorOptionLabels.empty()) {
        state.inspectorDefinitionId = definitionId;
        state.missingOptionLabel.clear();
        state.inspectorOptionLabels = state.optionLabels;
        const int found = FindSectorEditorNpcPlacementOption(state, definitionId);
        if (found < 0 && !definitionId.empty()) {
            state.missingOptionLabel = "<Missing: " + definitionId + ">";
            state.inspectorOptionLabels.push_back(state.missingOptionLabel.c_str());
        }
    }
    const int found = FindSectorEditorNpcPlacementOption(state, definitionId);
    if (found >= 0) return found;
    return state.inspectorOptionLabels.empty()
            ? -1
            : static_cast<int>(state.inspectorOptionLabels.size()) - 1;
}

} // namespace game
