#pragma once

#include "game/SoundSetAudio.h"

namespace game {

using FootstepCatalogSet = SoundSetCatalogSet;
using FootstepCatalog = SoundSetCatalog;
using LoadedFootstepSet = LoadedSoundSet;
using FootstepPlaybackState = SoundSetPlaybackState;

bool IsValidFootstepSetId(std::string_view id);
FootstepCatalog DiscoverFootstepCatalog(const std::string& footstepsRootPath);
const FootstepCatalogSet* FindFootstepCatalogSet(
        const FootstepCatalog& catalog,
        std::string_view id);

void ReserveFootstepPlaybackState(
        FootstepPlaybackState& state,
        size_t maximumVariationCount,
        size_t maximumSetIdLength = 0);
size_t SelectFootstepVariation(
        FootstepPlaybackState& state,
        std::string_view setId,
        size_t variationCount);
float SelectFootstepPitch(FootstepPlaybackState& state);

engine::SoundPlaybackHandle PlayFootstep(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedFootstepSet& set,
        FootstepPlaybackState& state,
        float volume);
engine::SoundPlaybackHandle PlayFootstepAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedFootstepSet& set,
        FootstepPlaybackState& state,
        float volume,
        const engine::PositionalSoundSettings& positional);

} // namespace game
