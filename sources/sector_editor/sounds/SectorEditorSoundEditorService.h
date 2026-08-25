#pragma once

#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/sounds/SectorEditorSoundEditorState.h"
#include "sector_demo/SectorAuthoringGraph.h"

#include <string>
#include <string_view>
#include <vector>

namespace game {

class SectorEditorSoundEditorService {
public:
    SectorEditorSoundEditorService(
            SectorEditorSoundEditorState& state,
            SectorAuthoringGraph& authoringGraph,
            SectorTopologyMap& topologyMap,
            SectorEditorDerivationDocumentAccess derivation,
            SectorEditorDocumentLifecycleAccess lifecycle,
            std::string& statusText);

    void Open();
    void Cancel();
    bool SaveAndClose();
    bool SetRoomtoneFadeMilliseconds(int milliseconds);

    SectorEditorSoundDraft* SelectedDraft();
    const SectorEditorSoundDraft* SelectedDraft() const;
    bool SelectIndex(int index);
    void AddSound();
    bool ApplyIdBuffer();
    bool SetSelectedType(SectorSoundType type);
    bool SetSelectedPath(const std::string& path);
    bool RequestDeleteSelected();
    void CancelDelete();
    bool ConfirmDeleteSelected();

    bool SelectedIsReferenced() const;
    std::vector<std::string> UsageLabels(std::string_view id) const;
    void RefreshSelectedUsage();

    SectorEditorSoundEditorState& State() { return state_; }
    const SectorEditorSoundEditorState& State() const { return state_; }

private:
    void Close();
    void SyncBuffers();
    void RebuildListLabels();
    bool ValidateDrafts(std::string& error) const;
    void SyncCompiledAudio(const SectorLevelAudioSettings& settings);

    SectorEditorSoundEditorState& state_;
    SectorAuthoringGraph& authoringGraph_;
    SectorTopologyMap& topologyMap_;
    SectorEditorDerivationDocumentAccess derivation_;
    SectorEditorDocumentLifecycleAccess lifecycle_;
    std::string& statusText_;
};

} // namespace game
