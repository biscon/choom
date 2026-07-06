#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <string>

namespace game {

void MarkSectorEditorTopologyDocumentEdited(
        SectorEditorState& state,
        std::string& statusText,
        const char* status);

} // namespace game
