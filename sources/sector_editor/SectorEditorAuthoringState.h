#pragma once

#include "sector_editor/SectorEditorTypes.h"

namespace game {

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorEditorState& state,
        const SectorTopologyMap& sourceMap);

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        const char* status);

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        const char* successStatus = nullptr,
        const char* failureStatus = nullptr);

} // namespace game
