#pragma once

#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"
#include "sector_editor/document/SectorEditorDocumentState.h"

#include <cstdint>
#include <string>

namespace game {

void MarkSectorEditorTopologyDocumentEdited(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        std::string& statusText,
        const char* status);

} // namespace game
