#include "game/FootstepAudio.h"

namespace game {

namespace {

constexpr float MinimumFootstepPitch = 0.96f;
constexpr float MaximumFootstepPitch = 1.04f;

} // namespace

bool IsValidFootstepSetId(std::string_view id)
{
    return IsValidSoundSetId(id);
}

FootstepCatalog DiscoverFootstepCatalog(const std::string& footstepsRootPath)
{
    FootstepCatalog catalog = DiscoverSoundSetCatalog(footstepsRootPath, "footsteps");
    if (!catalog.warning.empty()) {
        const std::string genericPrefix = "Sound set directory";
        if (catalog.warning.rfind(genericPrefix, 0) == 0) {
            catalog.warning.replace(0, genericPrefix.size(), "Footstep directory");
        }
    }
    return catalog;
}

const FootstepCatalogSet* FindFootstepCatalogSet(
        const FootstepCatalog& catalog,
        std::string_view id)
{
    return FindSoundSetCatalogSet(catalog, id);
}

void ReserveFootstepPlaybackState(
        FootstepPlaybackState& state,
        size_t maximumVariationCount,
        size_t maximumSetIdLength)
{
    ReserveSoundSetPlaybackState(
            state, maximumVariationCount, maximumSetIdLength);
}

size_t SelectFootstepVariation(
        FootstepPlaybackState& state,
        std::string_view setId,
        size_t variationCount)
{
    return SelectSoundSetVariation(state, setId, variationCount);
}

float SelectFootstepPitch(FootstepPlaybackState& state)
{
    return SelectSoundSetPitch(
            state, MinimumFootstepPitch, MaximumFootstepPitch);
}

engine::SoundPlaybackHandle PlayFootstep(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedFootstepSet& set,
        FootstepPlaybackState& state,
        float volume)
{
    return PlaySoundSet(
            assets,
            audio,
            set,
            state,
            volume,
            MinimumFootstepPitch,
            MaximumFootstepPitch);
}

engine::SoundPlaybackHandle PlayFootstepAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedFootstepSet& set,
        FootstepPlaybackState& state,
        float volume,
        const engine::PositionalSoundSettings& positional)
{
    return PlaySoundSetAt(
            assets,
            audio,
            set,
            state,
            volume,
            MinimumFootstepPitch,
            MaximumFootstepPitch,
            positional);
}

} // namespace game
