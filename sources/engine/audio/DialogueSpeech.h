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
    std::array<float, DialogueMoodCount> charactersPerSecond{24, 28, 30, 20, 36, 16, 22};
    std::array<float, 2> gapSeconds{0.02f, 0.06f};
    float volume = 0.65f;
    float commaPause = 0.12f;
    float clausePause = 0.18f;
    float sentencePause = 0.26f;
    float ellipsisPause = 0.45f;
};

struct DialogueClip {
    SoundHandle sound{};
    float seconds = 0.0f;
    uint32_t source = 0;
};
struct DialogueVoice {
    std::string id;
    std::array<std::vector<DialogueClip>, DialogueMoodCount> banks;
    std::array<float, 2> pitchRange{0.96f, 1.04f};
    std::array<float, 2> volumeRange{0.92f, 1.08f};
};
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

// Fixed-size per-speaker state. Sources are stable bank-local indices (0..63).
struct DialogueSelectionState {
    uint64_t available = 0;
    uint64_t remaining = 0;
    std::array<uint32_t, 3> recent{UINT32_MAX, UINT32_MAX, UINT32_MAX};
};
struct DialogueSelectionHistory {
    std::array<DialogueSelectionState, DialogueMoodCount> banks;
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
    DialogueSelectionState selectionAfter;
};
struct DialogueTimeline {
    std::vector<DialogueReveal> reveals;
    std::vector<DialogueCue> cues;
    double seconds = 0.0;
    size_t revealCursor = 0;
};
// Logical text clock held at each fragment until its playback finishes.
struct DialogueSequence {
    double position = 0.0;
    double fragmentElapsed = 0.0;
    size_t nextCue = 0;
    bool fragmentActive = false;
    bool usesDevice = false;
    bool finished = false;
};
struct DialoguePlayback {
    SoundPlaybackHandle handle{};
    uint64_t token = 0;
    DialogueSequence sequence;
    bool releasing = false;
    float releaseRemaining = 0.0f;
    SoundPlaybackSettings mix;
};

void ReserveDialogueTimeline(DialogueTimeline& timeline);
// Pure, bounded planner. Does not touch a device or allocate after Reserve.
bool BuildDialogueTimeline(std::string_view text, DialogueMood mood,
        uint32_t seed, const DialogueSettings& settings,
        const DialogueVoice* voice, DialogueTimeline& timeline,
        const DialogueSelectionHistory* history = nullptr);
// Commit only cues that actually started; planning and cancelled future cues do not consume the pool.
void CommitDialogueSelection(DialogueSelectionHistory& history, const DialogueCue& cue);
size_t AdvanceDialogueReveal(DialogueTimeline& timeline, double elapsed);
// Returns at most one fragment to start. Device completion gates progress;
// absent audio uses the same duration estimate. Excess frame time is not replayed.
const DialogueCue* AdvanceDialogueSequence(const DialogueTimeline& timeline,
        DialogueSequence& sequence, float dt, bool devicePlaying, bool paused = false);

void BeginLoadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        const std::filesystem::path& assetsRoot);
void PrepareDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library,
        size_t sourceBudget = 4);
bool DialogueVoicesFinished(const AssetManager& assets, const DialogueVoiceLibrary& library);
void UnloadDialogueVoices(AssetManager& assets, DialogueVoiceLibrary& library);
const DialogueVoice* FindDialogueVoice(const DialogueVoiceLibrary& library, std::string_view id);

const DialogueCue* UpdateDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback, const DialogueTimeline& timeline,
        uint64_t token, bool active, bool audible, Vector3 position, float dt,
        bool positional = true);
void StopDialoguePlayback(AssetManager& assets, AudioSystem& audio,
        DialoguePlayback& playback);

} // namespace engine
