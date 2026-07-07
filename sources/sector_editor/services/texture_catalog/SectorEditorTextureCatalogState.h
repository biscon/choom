#pragma once

#include "engine/assets/AssetManager.h"

#include <string>
#include <unordered_map>

namespace game {

struct TextureCatalogState {
    engine::AssetScopeHandle editorTextureScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::TextureHandle> editorTextureHandlesById;
};

} // namespace game
