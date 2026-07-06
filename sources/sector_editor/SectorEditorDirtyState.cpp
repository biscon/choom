#include "sector_editor/SectorEditorDirtyState.h"

namespace game {

void MarkSectorEditorTopologyDocumentEdited(
        SectorEditorState& state,
        std::string& statusText,
        const char* status)
{
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    ++state.topologyRenderRevision;
    state.topologyRenderCache.valid = false;
    if (status != nullptr && status[0] != '\0') {
        statusText = status;
    }
}

} // namespace game
