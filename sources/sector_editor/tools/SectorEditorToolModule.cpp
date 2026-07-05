#include "sector_editor/tools/SectorEditorToolModule.h"

#include "sector_editor/tools/line/SectorEditorLineTool.h"
#include "sector_editor/tools/rectangle/SectorEditorRectangleTool.h"

namespace game {

const SectorEditorToolModule* FindSectorEditorToolModule(SectorEditorTool tool)
{
    if (tool == SectorEditorTool::AuthoringLine) {
        return &SectorEditorLineToolModule();
    }
    if (tool == SectorEditorTool::AuthoringRectangle) {
        return &SectorEditorRectangleToolModule();
    }
    return nullptr;
}

} // namespace game
