#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/audio/AudioSystem.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

inline constexpr size_t DialogueMaximumCodepoints = 2048;
enum class DialogueMood : uint8_t { Neutral, Happy, Angry, Afraid, Panicked, Pained, Relieved, Count };
inline constexpr size_t DialogueMoodCount = static_cast<size_t>(DialogueMood::Count);
const char* DialogueMoodName(DialogueMood mood);
bool ParseDialogueMood(std::string_view text, DialogueMood& mood);

struct DialogueSettings {
    std::array<float, DialogueMoodCount> charactersPerSecond{30, 34, 34, 28, 40, 26, 28};
    float volume = 0.65f;
    float commaPause = 0.10f;
    float clausePause = 0.15f;
    float sentencePause = 0.20f;
    float ellipsisPause = 0.35f;
};

enum class DialogueVowel : uint8_t { Ah, Eh, Ee, Oh, Oo, Uh, Count };
inline constexpr size_t DialogueVowelCount = static_cast<size_t>(DialogueVowel::Count);
const char* DialogueVowelName(DialogueVowel vowel);
enum class DialogueConsonant : uint8_t { T, D, K, P, B, G, S, Z, V, F, H, Sh, Th, Ch, Ks, Count };
inline constexpr size_t DialogueConsonantCount = static_cast<size_t>(DialogueConsonant::Count);
const char* DialogueConsonantName(DialogueConsonant consonant);
struct DialogueProsody {
    float pitchSemitones = 0;
    float contourSemitones = 2;
    float gain = 1;
    float wobbleHz = 0;
    float wobbleSemitones = 0;
    float wordAccentSemitones = 0;
};
struct DialogueClip {
    SoundHandle sound{};
    float seconds = 0.0f;
    uint32_t source = 0;
};
struct DialogueVoice {
    std::string id;
    std::array<std::array<DialogueClip, DialogueVowelCount>, DialogueMoodCount> banks{};
    std::array<std::array<DialogueClip, DialogueConsonantCount>, DialogueMoodCount> consonants{};
    std::array<DialogueProsody, DialogueMoodCount> prosody{};
    bool continuousPitch = false;
};
struct DialogueVoiceCatalogEntry {
    std::string id;
    std::string label;
    std::filesystem::path manifest;
};
// Metadata only; safe without an audio device. Read during load/editor open.
std::vector<DialogueVoiceCatalogEntry> ReadDialogueVoiceCatalog(const std::filesystem::path& assetsRoot);

struct DialogueSourceRequest {
    std::filesystem::path path;
    size_t voice = 0;
    DialogueMood mood = DialogueMood::Neutral;
    uint32_t source = 0;
};
struct DialogueVoiceLibrary {
    AssetScopeHandle scope{};
    DialogueSettings settings;
    std::vector<DialogueVoice> voices;
    std::vector<DialogueSourceRequest> pending;
    size_t prepared = 0;
};

struct DialogueReveal {
    double seconds = 0.0;
    size_t byteCount = 0;
};
struct DialogueCue {
    double start = 0.0;
    double end = 0.0;
    SoundHandle sound{};
    uint32_t source = UINT32_MAX;
    float pitch = 1.0f;
    float volume = 1.0f;
    size_t beginByte = 0;
    size_t endByte = 0;
    DialogueMood bankMood = DialogueMood::Neutral;
    float wobbleHz = 0;
    float wobbleSemitones = 0;
    uint32_t nextPitchCue = UINT32_MAX;
};
struct DialogueTimeline {
    std::vector<DialogueReveal> reveals;
    std::vector<DialogueCue> cues;
    double seconds = 0.0;
    size_t revealCursor = 0;
};
// Text clock and cue cursor never depend on playback completion.
struct DialogueSequence {
    double position = 0.0;
    size_t nextCue = 0;
    bool finished = false;
};
struct DialoguePlaybackSlot {
    SoundPlaybackHandle handle{};
    DialogueCue cue;
    double elapsed = 0;
    bool releasing = false;
    float releaseRemaining = 0.0f;
    float pitch = 1.0f;
};
struct DialoguePlayback {
    // Separate lanes prevent short consonants stealing sustained vowels.
    std::array<DialoguePlaybackSlot, 6> slots{}; // four vowels, two consonants
    uint64_t token = 0;
    DialogueSequence sequence;
};

void ReserveDialogueTimeline(DialogueTimeline& timeline);
// Pure, bounded planner. Does not touch a device or allocate after Reserve.
bool BuildDialogueTimeline(std::string_view text, DialogueMood mood,
        const DialogueSettings& settings, const DialogueVoice* voice, DialogueTimeline& timeline);
size_t AdvanceDialogueReveal(DialogueTimeline& timeline, double elapsed);
// Consume all due events; newest vowel and (optionally) consonant, <=50 ms late.
const DialogueCue* AdvanceDialogueSequence(const DialogueTimeline& timeline,
        DialogueSequence& sequence, float dt, bool paused = false,
        const DialogueCue** consonant = nullptr);
float DialogueCuePitch(const DialogueCue& cue, double elapsed);
bool IsDialogueConsonant(const DialogueCue& cue);
float DialogueTimelinePitch(const DialogueTimeline& timeline, const DialogueCue& cue, double elapsed);

void BeginLoadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        const std::filesystem::path& assetsRoot);
void PrepareDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        size_t sourceBudget = 4);
bool DialogueVoicesFinished(const AssetManager& assets, const DialogueVoiceLibrary& library);
void UnloadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library);
const DialogueVoice* FindDialogueVoice(const DialogueVoiceLibrary& library, std::string_view id);

void UpdateDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback, const DialogueTimeline& timeline,
        uint64_t token, bool active, bool audible, Vector3 position, float dt,
        bool positional = true);
void StopDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback);

} // namespace engine
