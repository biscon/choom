#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "game/FpsWeaponRegistry.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/document/SectorEditorDocumentState.h"

#include <string_view>

namespace game {

struct SectorEditorFootstepServiceContext {
    SectorEditorState& editorState;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    FpsApplicationSettings& applicationSettings;
    std::string& statusText;
    engine::EngineContext& engineContext;
};

class SectorEditorFootstepService {
public:
    explicit SectorEditorFootstepService(SectorEditorFootstepServiceContext context)
        : context_(context) {}

    bool OpenForAuthoringFaceAnchor(int faceAnchorId);
    std::string EffectiveSetId(std::string_view overrideId) const;
    void Close();
    bool ApplySelection();
    void PreviewSelection();
    void UpdatePreview();

    void DrawModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);

private:
    void ClosePreview();
    std::string SelectedSetId() const;
    std::string EffectiveSelectedSetId() const;

    SectorEditorFootstepServiceContext context_;
};

} // namespace game
