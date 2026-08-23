#pragma once

#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/materials/SectorEditorMaterialRegistryEditorState.h"
#include "sector_demo/SectorMaterialRegistry.h"

#include <filesystem>
#include <string>

namespace engine { class AssetManager; }

namespace game {

class SectorEditorMaterialRegistryEditorService {
public:
    SectorEditorMaterialRegistryEditorService(
            SectorEditorMaterialRegistryEditorState& state,
            SectorMaterialRegistry& registry,
            SectorAuthoringGraph& authoringGraph,
            SectorTopologyMap& topologyMap,
            SectorEditorDerivationDocumentAccess derivation,
            SectorEditorDocumentLifecycleAccess lifecycle,
            std::string& statusText,
            std::filesystem::path registryPath,
            std::filesystem::path levelsRoot);

    void Open();
    void Cancel(engine::AssetManager* assets);
    void Shutdown(engine::AssetManager& assets);
    bool SaveAndClose(engine::AssetManager& assets);

    SectorEditorMaterialRegistryDraft* SelectedDraft();
    bool SelectIndex(int index);
    void AddMaterial();
    void ApplyIdBuffer();
    void OpenAlbedoPicker();
    void OpenAlbedoPickerFromRoot(const std::filesystem::path& assetsRoot);
    void ApplyAlbedoPickerFilter();
    bool SelectAlbedoPickerIndex(int index);
    bool HasAlbedoPickerSelection() const;
    std::string SelectedAlbedoPickerPath() const;
    bool ConfirmAlbedoPicker(engine::AssetManager& assets);
    void CancelAlbedoPicker(engine::AssetManager* assets);
    void EnsureAlbedoPickerPreview(engine::AssetManager& assets);
    bool RequestDeleteSelected();
    void CancelDelete();
    void ConfirmDeleteSelected();
    void EnsurePreview(engine::AssetManager& assets);

    SectorEditorMaterialRegistryEditorState& State() { return state_; }

private:
    void Close(engine::AssetManager* assets);
    void SyncBuffers();
    void RebuildListLabels();
    void RebuildAlbedoPickerList(const std::string& preferredPath = {});
    bool ApplyAlbedoPath(const std::string& path);
    bool ValidateDrafts(std::string& error) const;
    bool CurrentDocumentReferences(std::string_view id) const;

    SectorEditorMaterialRegistryEditorState& state_;
    SectorMaterialRegistry& registry_;
    SectorAuthoringGraph& authoringGraph_;
    SectorTopologyMap& topologyMap_;
    SectorEditorDerivationDocumentAccess derivation_;
    SectorEditorDocumentLifecycleAccess lifecycle_;
    std::string& statusText_;
    std::filesystem::path registryPath_;
    std::filesystem::path levelsRoot_;
};

} // namespace game
