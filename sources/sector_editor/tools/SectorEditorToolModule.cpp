#include "sector_editor/tools/SectorEditorToolModule.h"

#include "sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.h"
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
    if (tool == SectorEditorTool::AuthoringInsertVertex) {
        return &SectorEditorInsertVertexToolModule();
    }
    return nullptr;
}

} // namespace game
