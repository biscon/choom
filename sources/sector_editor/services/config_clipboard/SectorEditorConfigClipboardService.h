#pragma once

#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/config_clipboard/SectorEditorConfigClipboardTypes.h"

#include <string>

namespace engine {
class AssetManager;
}

namespace game {

class SectorEditorLightEditingService;
class SectorEditorMaterialEditingService;
class SectorEditorRuntimeObjectEditingService;

SectorEditorConfigTarget ResolveSectorEditorConfigTarget(
        SectorEditorMode mode,
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        const SelectionState& selectionState,
        const SectorEditorPreviewSelectionState& previewSelectionState);

bool ApplySectorEditorSectorConfig(
        SectorAuthoringFaceAnchor& destination,
        const SectorAuthoringFaceAnchor& source);

struct SectorEditorConfigClipboardServiceContext {
    SectorEditorState& editorState;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& map;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    const SelectionState& selectionState;
    const SectorEditorPreviewSelectionState& previewSelectionState;
    SectorEditorUiState& uiState;
    SectorEditorConfigClipboardState& clipboard;
    SectorEditorLightEditingService& lightEditing;
    SectorEditorRuntimeObjectEditingService& runtimeObjectEditing;
    SectorEditorMaterialEditingService& materialEditing;
    engine::AssetManager& assets;
    std::string& statusText;
};

class SectorEditorConfigClipboardService {
public:
    explicit SectorEditorConfigClipboardService(
            SectorEditorConfigClipboardServiceContext context);

    SectorEditorConfigTarget CurrentTarget() const;
    bool CanCopy() const;
    bool CanPaste() const;
    bool Copy();
    bool Paste();

private:
    SectorEditorConfigClipboardServiceContext context_;
};

} // namespace game
