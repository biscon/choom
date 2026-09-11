#include "engine/audio/DialogueSpeech.h"
#include "engine/assets/AssetManager.h"
#include "util/json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace engine {
namespace {
using Json = nlohmann::ordered_json;
Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open " + path.string());
    return Json::parse(file);
}
float Number(const Json& object, const char* key, float fallback, float low, float high)
{
    const float value = object.value(key, fallback);
    if (!std::isfinite(value) || value < low || value > high)
        throw std::runtime_error(std::string("Invalid dialogue setting: ") + key);
    return value;
}
std::filesystem::path ChildPath(const std::filesystem::path& root, const std::string& relative)
{
    const std::filesystem::path path(relative);
    if (path.empty() || path.is_absolute()) throw std::runtime_error("Expected relative dialogue asset path");
    for (const auto& part : path) if (part == "..") throw std::runtime_error("Dialogue asset path escapes its directory");
    return root / path;
}
void ReadRange(const Json& root, const char* key, std::array<float, 2>& range, float low, float high)
{
    if (!root.contains(key)) return;
    range = root.at(key).get<std::array<float, 2>>();
    if (!std::isfinite(range[0]) || !std::isfinite(range[1])
            || range[0] < low || range[1] > high || range[0] > range[1])
        throw std::runtime_error(std::string("Invalid dialogue range: ") + key);
}
} // namespace

const DialogueVoice* FindDialogueVoice(const DialogueVoiceLibrary& library, std::string_view id)
{
    for (const auto& voice : library.voices) if (voice.id == id) return &voice;
    return nullptr;
}

void BeginLoadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        const std::filesystem::path& assetsRoot)
{
    UnloadDialogueVoices(assets, library);
    library.scope = assets.CreateScope("dialogue_voices");
    Json config;
    try {
        config = ReadJson(assetsRoot / "config/dialogue_speech.json");
        DialogueSettings settings;
        const Json& rates = config.at("characters_per_second");
        for (size_t i = 0; i < DialogueMoodCount; ++i)
            settings.charactersPerSecond[i] = Number(rates, DialogueMoodName(static_cast<DialogueMood>(i)), settings.charactersPerSecond[i], 8, 60);
        for (size_t i = 0; i < DialogueMoodCount; ++i)
            if (i != static_cast<size_t>(DialogueMood::Panicked)
                    && settings.charactersPerSecond[i] >= settings.charactersPerSecond[static_cast<size_t>(DialogueMood::Panicked)])
                throw std::runtime_error("Panicked dialogue must have the fastest rate");
        settings.volume = Number(config, "volume", settings.volume, 0, 1);
        settings.commaPause = Number(config, "comma_pause_ms", 120, 0, 1000) / 1000;
        settings.clausePause = Number(config, "clause_pause_ms", 180, 0, 1000) / 1000;
        settings.sentencePause = Number(config, "sentence_pause_ms", 260, 0, 1500) / 1000;
        settings.ellipsisPause = Number(config, "ellipsis_pause_ms", 450, 0, 2000) / 1000;
        settings.gapSeconds[0] = Number(config, "minimum_gap_ms", 20, 0, 500) / 1000;
        settings.gapSeconds[1] = Number(config, "maximum_gap_ms", 60, 0, 500) / 1000;
        if (settings.gapSeconds[0] > settings.gapSeconds[1]) throw std::runtime_error("Invalid dialogue gap range");
        library.settings = settings;
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "Dialogue configuration unavailable: %s; using defaults", error.what());
        config = Json{{"voices", {{"male", "audio/dialogue_voices/male/voice_manifest.json"},
                                  {"female", "audio/dialogue_voices/female/voice_manifest.json"}}}};
    }
    library.voices.reserve(2);
    library.pending.reserve(112);
    for (const char* id : {"male", "female"}) {
        try {
            const auto manifestPath = ChildPath(assetsRoot, config.at("voices").at(id).get<std::string>());
            const Json manifest = ReadJson(manifestPath);
            if (manifest.at("schema_version") != 1 || manifest.at("voice_identity_id") != id)
                throw std::runtime_error("Voice manifest identity/version mismatch");
            DialogueVoice voice;
            voice.id = id;
            ReadRange(manifest, "pitch_range", voice.pitchRange, 0.8f, 1.2f);
            ReadRange(manifest, "volume_range", voice.volumeRange, 0, 1.5f);
            std::vector<DialogueSourceRequest> pending;
            for (size_t m = 0; m < DialogueMoodCount; ++m) {
                voice.banks[m].reserve(8);
                const auto mood = static_cast<DialogueMood>(m);
                const auto& moods = manifest.at("moods");
                if (!moods.contains(DialogueMoodName(mood))) continue;
                const auto& clips = moods.at(DialogueMoodName(mood));
                if (!clips.is_array() || clips.size() > 64) throw std::runtime_error("Invalid dialogue clip list");
                uint32_t source = 0;
                for (const auto& clip : clips) pending.push_back({ChildPath(manifestPath.parent_path(), clip.get<std::string>()), library.voices.size(), mood, source++});
            }
            library.voices.push_back(std::move(voice));
            library.pending.insert(library.pending.end(), pending.begin(), pending.end());
        } catch (const std::exception& error) {
            TraceLog(LOG_WARNING, "Dialogue voice '%s' unavailable: %s", id, error.what());
        }
    }
}

void PrepareDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library, size_t sourceBudget)
{
    while (sourceBudget-- && library.prepared < library.pending.size()) {
        const auto& request = library.pending[library.prepared++];
        Wave wave = LoadWave(request.path.string().c_str());
        if (!IsWaveValid(wave)) {
            TraceLog(LOG_WARNING, "Dialogue source unavailable: %s", request.path.string().c_str());
            continue;
        }
        WaveFormat(&wave, 48000, 16, 1);
        auto& bank = library.voices[request.voice].banks[static_cast<size_t>(request.mood)];
        if (IsWaveValid(wave) && wave.sampleRate == 48000 && wave.sampleSize == 16 && wave.channels == 1) {
            const std::string key = "dialogue:complete:" + request.path.generic_string();
            const SoundHandle sound = assets.CreateSoundFromPcm(library.scope, key.c_str(),
                    static_cast<const int16_t*>(wave.data), wave.frameCount, wave.sampleRate);
            if (!IsNull(sound)) bank.push_back({sound, float(wave.frameCount) / wave.sampleRate, request.source});
        }
        UnloadWave(wave);
    }
    if (library.prepared == library.pending.size()) {
        // Completed preparation never runs again in normal gameplay.
        for (auto& voice : library.voices) for (auto& bank : voice.banks) {
            bank.erase(std::remove_if(bank.begin(), bank.end(), [&](const DialogueClip& clip) {
                return assets.HasFailed(clip.sound);
            }), bank.end());
        }
    }
}

bool DialogueVoicesFinished(const AssetManager& assets, const DialogueVoiceLibrary& library)
{
    return library.prepared == library.pending.size()
            && (IsNull(library.scope) || assets.IsScopeFinished(library.scope));
}
void UnloadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library)
{
    if (!IsNull(library.scope)) assets.UnloadScope(library.scope);
    library = {};
}

void StopDialoguePlayback(AssetManager& assets, AudioSystem& audio, DialoguePlayback& playback)
{
    if (!IsNull(playback.handle)) audio.StopSound(assets, playback.handle);
    playback = {};
}

const DialogueCue* UpdateDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback, const DialogueTimeline& timeline, uint64_t token,
        bool active, bool audible, Vector3 position, float rawDt, bool positional)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    if (audio.IsPaused()) return nullptr;
    const bool changed = playback.token != token;
    if (changed) { playback.sequence = {}; playback.token = token; }
    if ((changed || !active || !audible) && !IsNull(playback.handle) && !playback.releasing) {
        playback.releasing = true;
        playback.releaseRemaining = 0.04f;
    }
    bool devicePlaying = !IsNull(playback.handle) && audio.IsSoundPlaying(playback.handle);
    if (playback.releasing && devicePlaying) {
        // Explicit interruption only. Give the zero-volume mix an update before
        // stopping, even when a frame hitch spans the entire release duration.
        if (playback.releaseRemaining == 0) {
            if (dt == 0) return nullptr; // the same-frame script-start pass is not a release frame
            audio.StopSound(assets, playback.handle);
            devicePlaying = false;
        } else {
            playback.releaseRemaining = std::max(0.0f, playback.releaseRemaining-std::min(dt, 0.02f));
            auto mix = playback.mix;
            mix.volume *= playback.releaseRemaining / 0.04f;
            audio.SetSoundPlaybackSettings(assets, playback.handle, mix);
            return nullptr;
        }
    }
    if (!devicePlaying) {
        playback.handle = {};
        playback.releasing = false;
    } else if (positional) {
        audio.SetSoundPosition(playback.handle, position);
    }
    if (!active) return nullptr;
    const auto* cue = AdvanceDialogueSequence(timeline, playback.sequence, dt, devicePlaying);
    if (!cue || !audible) return nullptr;
    PositionalSoundSettings spatial;
    spatial.position = position;
    spatial.minimumDistanceWorld = 1.0f;
    spatial.maximumDistanceWorld = 25.0f;
    playback.mix = SoundPlaybackSettings{cue->volume, cue->pitch, 0.0f};
    playback.handle = positional ? audio.PlaySoundAt(assets, cue->sound, spatial, playback.mix)
            : audio.PlaySound(assets, cue->sound, playback.mix);
    playback.sequence.usesDevice = !IsNull(playback.handle);
    return playback.sequence.usesDevice ? cue : nullptr;
}
} // namespace engine
