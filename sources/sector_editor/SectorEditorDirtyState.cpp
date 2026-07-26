#include "sector_editor/SectorEditorDirtyState.h"

namespace game {

void MarkSectorEditorTopologyDocumentEdited(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        std::string& statusText,
        const char* status)
{
    lifecycle.topologyDocumentDirty = true;
    lifecycle.hasUnsavedChanges = true;
    ++topologyRenderRevision;
    topologyRenderCache.valid = false;
    if (status != nullptr && status[0] != '\0') {
        statusText = status;
    }
}

} // namespace game
