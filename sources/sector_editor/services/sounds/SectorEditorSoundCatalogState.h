#pragma once

#include "engine/assets/AssetHandles.h"

#include <string>
#include <unordered_map>

namespace game {

struct SectorEditorSoundCatalogState {
    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::SoundHandle> soundsById;
    std::unordered_map<std::string, engine::MusicHandle> musicById;
};

} // namespace game
