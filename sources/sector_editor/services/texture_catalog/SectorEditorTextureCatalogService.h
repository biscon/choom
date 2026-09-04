#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogState.h"
#include "sector_demo/SectorMaterialRegistry.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorTextureCatalogServiceContext {
    SectorMaterialRegistry& registry;
    TextureCatalogState& textureState;
};

class SectorEditorTextureCatalogService {
public:
    explicit SectorEditorTextureCatalogService(SectorEditorTextureCatalogServiceContext context);

    const SectorMaterialDefinition* FindTexture(const std::string& materialId) const;
    bool HasTexture(const std::string& materialId) const;
    std::vector<std::string> TextureIds() const;

    void PopulatePickerOptions(TexturePickerState& picker, const std::string& currentTexture) const;

    engine::TextureHandle TextureHandleForId(const std::string& materialId) const;
    engine::TextureHandle EnsureTextureHandleForId(
            const std::string& materialId,
            engine::AssetManager& assets);
    void RefreshTextureHandles(engine::AssetManager& assets);

private:
    SectorEditorTextureCatalogServiceContext context_;
};

} // namespace game
