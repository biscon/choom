#include "sector_editor/tools/SectorEditorToolModule.h"

#include "sector_editor/tools/line/SectorEditorLineTool.h"

namespace game {

const SectorEditorToolModule* FindSectorEditorToolModule(SectorEditorTool tool)
{
    if (tool == SectorEditorTool::AuthoringLine) {
        return &SectorEditorLineToolModule();
    }
    return nullptr;
}

} // namespace game
