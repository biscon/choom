#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/sounds/SectorEditorSoundCatalogState.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

class SectorEditorRuntimeObjectEditingService;

struct SectorEditorSoundServiceContext {
    SectorEditorState& state;
    SectorAuthoringGraph& authoringGraph;
    SectorTopologyMap& map;
    SectorEditorSoundCatalogState& catalog;
    SectorEditorAudioAssetPickerSessionState& audioAssetPickerSession;
    std::string& statusText;
    engine::EngineContext& engineContext;
    SectorEditorRuntimeObjectEditingService* runtimeObjectEditing = nullptr;
};

class SectorEditorSoundService {
public:
    explicit SectorEditorSoundService(SectorEditorSoundServiceContext context);

    const SectorSoundDefinition* Find(const std::string& id) const;
    std::vector<std::string> SortedIds(SectorSoundType type) const;
    engine::SoundHandle SoundHandleForId(const std::string& id) const;
    engine::MusicHandle MusicHandleForId(const std::string& id) const;

    void RefreshCatalogHandles();
    void Shutdown();

    bool OpenDoorPicker(int runtimeObjectId, SectorEditorDoorSoundTarget target);
    void ClosePicker();
    bool ApplyPickerSelection();
    void DrawPickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::FontHandle font);

private:
    void PreviewPickerSelection();
    void StopPreview(SectorEditorAudioPreviewState& preview, bool unloadScope);
    void UpdatePreview(
            SectorEditorAudioPreviewState& preview,
            std::string& message);

    SectorEditorSoundServiceContext context_;
};

} // namespace game
