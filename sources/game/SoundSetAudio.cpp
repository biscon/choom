#include "game/SoundSetAudio.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <map>
#include <numeric>
#include <system_error>

namespace game {

namespace {

uint32_t NextRandom(uint32_t& state)
{
    if (state == 0) state = 0x6d2b79f5u;
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

bool SupportedAudioExtension(std::string extension)
{
    std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3";
}

bool SplitVariationStem(
        const std::string& stem,
        std::string& outSetName,
        uint64_t& outVariation)
{
    const size_t separator = stem.rfind('_');
    if (separator == std::string::npos
            || separator == 0
            || separator + 1 >= stem.size()) {
        return false;
    }
    uint64_t variation = 0;
    for (size_t i = separator + 1; i < stem.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(stem[i]);
        if (!std::isdigit(character)) return false;
        variation = variation * 10u + static_cast<uint64_t>(character - '0');
    }
    outSetName = stem.substr(0, separator);
    outVariation = variation;
    return !outSetName.empty();
}

struct DiscoveredVariation {
    uint64_t number = 0;
    std::string path;
};

void ShuffleBag(SoundSetPlaybackState& state)
{
    for (size_t i = state.shuffleBag.size(); i > 1; --i) {
        const size_t selected = static_cast<size_t>(NextRandom(state.randomState)) % i;
        std::swap(state.shuffleBag[i - 1], state.shuffleBag[selected]);
    }
    if (state.shuffleBag.size() > 1
            && state.shuffleBag.front() == state.lastVariantIndex) {
        const size_t swapIndex = 1u
                + static_cast<size_t>(NextRandom(state.randomState))
                        % (state.shuffleBag.size() - 1u);
        std::swap(state.shuffleBag.front(), state.shuffleBag[swapIndex]);
    }
    state.nextBagIndex = 0;
}

engine::SoundPlaybackHandle PlaySelectedSoundSet(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedSoundSet& set,
        SoundSetPlaybackState& state,
        float volume,
        float minimumPitch,
        float maximumPitch,
        const engine::PositionalSoundSettings* positional)
{
    if (set.sounds.empty()) return engine::NullSoundPlaybackHandle();
    const size_t index = SelectSoundSetVariation(state, set.id, set.sounds.size());
    if (index >= set.sounds.size()) return engine::NullSoundPlaybackHandle();
    const engine::SoundPlaybackSettings settings{
            std::clamp(volume, 0.0f, 1.0f),
            SelectSoundSetPitch(state, minimumPitch, maximumPitch),
            0.0f};
    return positional == nullptr
            ? audio.PlaySound(assets, set.sounds[index], settings)
            : audio.PlaySoundAt(assets, set.sounds[index], *positional, settings);
}

} // namespace

bool IsValidSoundSetId(std::string_view id)
{
    if (id.empty() || id.front() == '/' || id.front() == '\\') return false;
    std::string segment;
    for (const char character : id) {
        if (character == '\\') return false;
        if (character == '/') {
            if (segment.empty() || segment == "." || segment == "..") return false;
            segment.clear();
            continue;
        }
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_' && character != '-' && character != '.') {
            return false;
        }
        segment.push_back(character);
    }
    return !segment.empty() && segment != "." && segment != "..";
}

SoundSetCatalog DiscoverSoundSetCatalog(
        const std::string& rootPath,
        std::string_view audioRelativeRoot)
{
    SoundSetCatalog catalog;
    const std::filesystem::path root = std::filesystem::path(rootPath).lexically_normal();
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        catalog.warning = "Sound set directory is unavailable: " + root.generic_string();
        return catalog;
    }

    std::map<std::string, std::vector<DiscoveredVariation>> variationsBySet;
    std::filesystem::recursive_directory_iterator iterator(
            root,
            std::filesystem::directory_options::skip_permission_denied,
            error);
    const std::filesystem::recursive_directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) continue;
        const std::filesystem::path path = iterator->path();
        if (!SupportedAudioExtension(path.extension().string())) continue;

        std::string baseName;
        uint64_t variation = 0;
        if (!SplitVariationStem(path.stem().string(), baseName, variation)) continue;
        std::filesystem::path relative = std::filesystem::relative(path, root, error);
        if (error) break;
        const std::string setId = (relative.parent_path() / baseName).generic_string();
        if (!IsValidSoundSetId(setId)) continue;
        const std::string audioRelativePath =
                (std::filesystem::path(audioRelativeRoot) / relative).generic_string();
        variationsBySet[setId].push_back(DiscoveredVariation{variation, audioRelativePath});
    }
    if (error) catalog.warning = "Sound set scan was incomplete: " + error.message();

    catalog.sets.reserve(variationsBySet.size());
    for (auto& entry : variationsBySet) {
        std::sort(
                entry.second.begin(),
                entry.second.end(),
                [](const DiscoveredVariation& left, const DiscoveredVariation& right) {
                    if (left.number != right.number) return left.number < right.number;
                    return left.path < right.path;
                });
        SoundSetCatalogSet set;
        set.id = entry.first;
        set.relativePaths.reserve(entry.second.size());
        for (const DiscoveredVariation& variation : entry.second) {
            if (set.relativePaths.empty() || set.relativePaths.back() != variation.path) {
                set.relativePaths.push_back(variation.path);
            }
        }
        catalog.sets.push_back(std::move(set));
    }
    return catalog;
}

const SoundSetCatalogSet* FindSoundSetCatalogSet(
        const SoundSetCatalog& catalog,
        std::string_view id)
{
    const auto found = std::lower_bound(
            catalog.sets.begin(),
            catalog.sets.end(),
            id,
            [](const SoundSetCatalogSet& set, std::string_view value) {
                return set.id < value;
            });
    return found == catalog.sets.end() || found->id != id ? nullptr : &*found;
}

void ReserveSoundSetPlaybackState(
        SoundSetPlaybackState& state,
        size_t maximumVariationCount,
        size_t maximumSetIdLength)
{
    state.shuffleBag.reserve(maximumVariationCount);
    state.setId.reserve(maximumSetIdLength);
}

size_t SelectSoundSetVariation(
        SoundSetPlaybackState& state,
        std::string_view setId,
        size_t variationCount)
{
    if (variationCount == 0) return static_cast<size_t>(-1);
    if (state.setId != setId || state.shuffleBag.size() != variationCount) {
        state.setId.assign(setId.begin(), setId.end());
        state.shuffleBag.resize(variationCount);
        std::iota(state.shuffleBag.begin(), state.shuffleBag.end(), size_t{0});
        state.nextBagIndex = variationCount;
        state.lastVariantIndex = static_cast<size_t>(-1);
    }
    if (state.nextBagIndex >= state.shuffleBag.size()) ShuffleBag(state);
    const size_t selected = state.shuffleBag[state.nextBagIndex++];
    state.lastVariantIndex = selected;
    return selected;
}

float SelectSoundSetPitch(
        SoundSetPlaybackState& state,
        float minimumPitch,
        float maximumPitch)
{
    if (!std::isfinite(minimumPitch) || minimumPitch <= 0.0f) minimumPitch = 1.0f;
    if (!std::isfinite(maximumPitch) || maximumPitch < minimumPitch) {
        maximumPitch = minimumPitch;
    }
    const float unit = static_cast<float>(NextRandom(state.randomState) & 0xffffu) / 65535.0f;
    return minimumPitch + unit * (maximumPitch - minimumPitch);
}

engine::SoundPlaybackHandle PlaySoundSet(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedSoundSet& set,
        SoundSetPlaybackState& state,
        float volume,
        float minimumPitch,
        float maximumPitch)
{
    return PlaySelectedSoundSet(
            assets, audio, set, state, volume, minimumPitch, maximumPitch, nullptr);
}

engine::SoundPlaybackHandle PlaySoundSetAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const LoadedSoundSet& set,
        SoundSetPlaybackState& state,
        float volume,
        float minimumPitch,
        float maximumPitch,
        const engine::PositionalSoundSettings& positional)
{
    return PlaySelectedSoundSet(
            assets, audio, set, state, volume, minimumPitch, maximumPitch, &positional);
}

} // namespace game
