#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace game {

struct SoundSetCatalogSet {
    std::string id;
    std::vector<std::string> relativePaths;
};

struct SoundSetCatalog {
    std::vector<SoundSetCatalogSet> sets;
    std::string warning;
};

struct LoadedSoundSet {
    std::string id;
    std::vector<engine::SoundHandle> sounds;
};

struct SoundSetPlaybackState {
    std::string setId;
    std::vector<size_t> shuffleBag;
    size_t nextBagIndex = 0;
    size_t lastVariantIndex = static_cast<size_t>(-1);
    uint32_t randomState = 0x6d2b79f5u;
};

bool IsValidSoundSetId(std::string_view id);
SoundSetCatalog DiscoverSoundSetCatalog(
        const std::string& rootPath,
        std::string_view audioRelativeRoot);
const SoundSetCatalogSet* FindSoundSetCatalogSet(
        const SoundSetCatalog& catalog,
        std::string_view id);

void ReserveSoundSetPlaybackState(
        SoundSetPlaybackState& state,
        size_t maximumVariationCount,
        size_t maximumSetIdLength = 0);
size_t SelectSoundSetVariation(
        SoundSetPlaybackState& state,
        std::string_view setId,
        size_t variationCount);
float SelectSoundSetPitch(
        SoundSetPlaybackState& state,
        float minimumPitch,
        float maximumPitch);

engine::SoundPlaybackHandle PlaySoundSet(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedSoundSet& set,
        SoundSetPlaybackState& state,
        float volume,
        float minimumPitch,
        float maximumPitch);
engine::SoundPlaybackHandle PlaySoundSetAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedSoundSet& set,
        SoundSetPlaybackState& state,
        float volume,
        float minimumPitch,
        float maximumPitch,
        const engine::PositionalSoundSettings& positional);

} // namespace game
