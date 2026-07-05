#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorDoorTextureSettingsModalCallbacks {
    std::function<const SectorPlacedRuntimeObject*()> selectedRuntimeObject;
    std::function<bool(const char*, const std::function<bool(SectorPlacedRuntimeObject&)>&)>
            mutateSelectedRuntimeObject;
};

struct SectorEditorDoorTextureSettingsModalContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    DoorTextureSettingsModalState& modalState;
    const SectorTopologyMap& topologyMap;
    int selectedRuntimeObjectId = -1;
    std::string& statusText;
    const SectorEditorDoorTextureSettingsModalCallbacks& callbacks;
};

bool OpenSectorEditorDoorTextureSettingsModal(
        DoorTextureSettingsModalState& modalState,
        const SectorPlacedRuntimeObject* selectedObject,
        std::string& statusText);

void DrawSectorEditorDoorTextureSettingsModal(
        SectorEditorDoorTextureSettingsModalContext& context);

} // namespace game
