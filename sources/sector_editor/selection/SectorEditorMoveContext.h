#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace game {

struct SectorEditorMoveContext {
    SectorEditorState& state;
    std::string& statusText;

    std::function<Vector2(Vector2)> screenToMap;
    std::function<Vector2(Vector2)> snapMapPoint;
    std::function<void(int)> selectRuntimeObject;
    std::function<void(const SectorPlacedRuntimeObject&)> updateCachedRuntimeObjectDraw;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<void()> refreshRuntimeObjectsAfterAuthoringEdit;
};

} // namespace game
