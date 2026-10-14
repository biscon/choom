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
} // namespace

const DialogueVoice* FindDialogueVoice(const DialogueVoiceLibrary& library, std::string_view id)
{
    for (const auto& voice : library.voices) if (voice.id == id) return &voice;
    TraceLog(LOG_WARNING, "Dialogue voice unavailable: %.*s", static_cast<int>(id.size()), id.data());
    return nullptr;
}

void BeginLoadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        const std::filesystem::path& assetsRoot)
{
    UnloadDialogueVoices(assets, library);
    library.scope = assets.CreateScope("dialogue_voices");
    try {
        const auto config = ReadJson(assetsRoot / "config/dialogue_speech.json");
        DialogueSettings settings;
        const auto rates = config.value("characters_per_second", Json::object());
        for (size_t i = 0; i < DialogueMoodCount; ++i)
            settings.charactersPerSecond[i] = Number(rates, DialogueMoodName(static_cast<DialogueMood>(i)), settings.charactersPerSecond[i], 8, 60);
        settings.volume = Number(config, "volume", settings.volume, 0, 1);
        settings.commaPause = Number(config, "comma_pause_ms", 100, 0, 1000) / 1000;
        settings.clausePause = Number(config, "clause_pause_ms", 150, 0, 1000) / 1000;
        settings.sentencePause = Number(config, "sentence_pause_ms", 200, 0, 1500) / 1000;
        settings.ellipsisPause = Number(config, "ellipsis_pause_ms", 350, 0, 2000) / 1000;
        library.settings = settings;
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "Dialogue configuration unavailable: %s; using defaults", error.what());
    }
    const auto catalog = ReadDialogueVoiceCatalog(assetsRoot);
    library.voices.reserve(catalog.size());
    library.pending.reserve(catalog.size() * DialogueMoodCount * DialogueVowelCount);
    for (const auto& entry : catalog) {
        try {
            const auto manifest = ReadJson(entry.manifest);
            DialogueVoice voice;
            voice.id = entry.id;
            voice.continuousPitch = manifest.value("continuous_pitch", false);
            std::vector<DialogueSourceRequest> pending;
            pending.reserve(DialogueMoodCount * DialogueVowelCount);
            for (size_t m = 0; m < DialogueMoodCount; ++m) {
                const auto mood = static_cast<DialogueMood>(m);
                const auto* name = DialogueMoodName(mood);
                const auto& moods = manifest.at("moods");
                if (!moods.contains(name)) continue;
                const auto& bank = moods.at(name);
                if (!bank.is_object()) throw std::runtime_error("Expected vowel-keyed mood bank");
                for (size_t v = 0; v < DialogueVowelCount; ++v) {
                    const auto* vowel = DialogueVowelName(static_cast<DialogueVowel>(v));
                    if (bank.contains(vowel)) pending.push_back({ChildPath(entry.manifest.parent_path(), bank.at(vowel).get<std::string>()), library.voices.size(), mood, static_cast<uint32_t>(v)});
                }
                if (manifest.contains("prosody") && manifest.at("prosody").contains(name)) {
                    const auto& p = manifest.at("prosody").at(name);
                    auto& out = voice.prosody[m];
                    out.pitchSemitones = Number(p, "pitch_semitones", 0, -3, 3);
                    out.contourSemitones = Number(p, "contour_semitones", 2, 0, 6);
                    out.gain = Number(p, "gain", 1, 0, 1.5f);
                    out.wobbleHz = Number(p, "wobble_hz", 0, 0, 8);
                    out.wobbleSemitones = Number(p, "wobble_semitones", 0, 0, 1);
                    out.wordAccentSemitones = Number(p, "word_accent_semitones", 0, 0, 1.5f);
                }
            }
            library.voices.push_back(std::move(voice));
            library.pending.insert(library.pending.end(), pending.begin(), pending.end());
        } catch (const std::exception& error) {
            TraceLog(LOG_WARNING, "Dialogue voice '%s' unavailable: %s", entry.id.c_str(), error.what());
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
        auto& voice = library.voices[request.voice];
        auto& clip = voice.banks[static_cast<size_t>(request.mood)][request.source];
        if (IsWaveValid(wave) && wave.sampleRate == 48000 && wave.sampleSize == 16 && wave.channels == 1) {
            const std::string key = "dialogue:speech:" + request.path.generic_string();
            const SoundHandle sound = assets.CreateSoundFromPcm(library.scope, key.c_str(),
                    static_cast<const int16_t*>(wave.data), wave.frameCount, wave.sampleRate);
            if (!IsNull(sound)) clip = {sound, float(wave.frameCount) / wave.sampleRate, request.source};
        }
        UnloadWave(wave);
    }
    if (library.prepared == library.pending.size()) {
        // Completed preparation never runs again in normal gameplay.
        for (auto& voice : library.voices) {
            for (auto& bank : voice.banks)
                for (auto& clip : bank) if (assets.HasFailed(clip.sound)) clip = {};
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
    for (auto& slot : playback.slots)
        if (!IsNull(slot.handle)) audio.StopSound(assets, slot.handle);
    playback = {};
}

void UpdateDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback, const DialogueTimeline& timeline, uint64_t token,
        bool active, bool audible, Vector3 position, float rawDt, bool positional)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    if (audio.IsPaused()) return;
    const bool changed = playback.token != token;
    if (changed) { playback.sequence = {}; playback.token = token; }
    for (auto& slot : playback.slots) {
        if (IsNull(slot.handle)) continue;
        if (!audio.IsSoundPlaying(slot.handle)) { slot = {}; continue; }
        if ((changed || !active || !audible) && !slot.releasing) {
            slot.releasing = true;
            slot.releaseRemaining = 0.04f;
        }
        if (slot.releasing && slot.releaseRemaining == 0 && dt > 0) {
            audio.StopSound(assets, slot.handle);
            slot = {};
            continue;
        }
        slot.elapsed += dt;
        // A token change can replace the timeline. Releasing sounds retain
        // their last pitch and never follow indices into the replacement line.
        if (!slot.releasing) slot.pitch = DialogueTimelinePitch(timeline, slot.cue, playback.sequence.position + dt);
        auto mix = SoundPlaybackSettings{slot.cue.volume, slot.pitch, 0};
        if (slot.releasing) {
            slot.releaseRemaining = std::max(0.0f, slot.releaseRemaining - std::min(dt, 0.02f));
            mix.volume *= slot.releaseRemaining / 0.04f;
        }
        audio.SetSoundPlaybackSettings(assets, slot.handle, mix);
        if (positional && !slot.releasing) audio.SetSoundPosition(slot.handle, position);
    }
    if (!active) return;
    const auto* cue = AdvanceDialogueSequence(timeline, playback.sequence, dt);
    if (!audible || !cue || IsNull(cue->sound)) return;
    for (auto& slot : playback.slots) {
        if (!IsNull(slot.handle)) continue;
        PositionalSoundSettings spatial;
        spatial.position = position;
        spatial.minimumDistanceWorld = 1;
        spatial.maximumDistanceWorld = 25;
        slot.pitch = DialogueTimelinePitch(timeline, *cue, playback.sequence.position);
        const SoundPlaybackSettings mix{cue->volume, slot.pitch, 0};
        slot.cue = *cue;
        slot.handle = positional ? audio.PlaySoundAt(assets, cue->sound, spatial, mix)
                : audio.PlaySound(assets, cue->sound, mix);
        break;
    }
}
} // namespace engine
