#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogState.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorTextureCatalogServiceContext {
    SectorTopologyMap& map;
    TextureCatalogState& textureState;
    std::string& defaultFloorTextureId;
    std::string& defaultCeilingTextureId;
    std::string& defaultWallTextureId;
    std::string& defaultLowerWallTextureId;
    std::string& defaultUpperWallTextureId;
};

class SectorEditorTextureCatalogService {
public:
    explicit SectorEditorTextureCatalogService(SectorEditorTextureCatalogServiceContext context);

    const SectorTextureDefinition* FindTexture(const std::string& textureId) const;
    bool HasTexture(const std::string& textureId) const;
    std::vector<std::string> TextureIds() const;

    void PopulatePickerOptions(TexturePickerState& picker, const std::string& currentTexture) const;

    engine::TextureHandle TextureHandleForId(const std::string& textureId) const;
    void RefreshTextureHandles(engine::AssetManager& assets);
    void RefreshDefaultTextureIds();

    void SelectAddMapTexturePath(AddMapTextureState& modalState, int pathIndex) const;
    bool ValidateAddMapTextureId(const AddMapTextureState& modalState, std::string& error) const;
    SectorEditorAddTextureResult RegisterSelectedMapTexture(AddMapTextureState& modalState);

private:
    SectorEditorTextureCatalogServiceContext context_;
};

} // namespace game
