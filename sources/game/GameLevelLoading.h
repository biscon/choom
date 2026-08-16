#pragma once

namespace game {

constexpr float GameLevelLoadingFadeSeconds = 0.250f;

enum class GameLevelLoadPhase {
    Stopped,
    Loading,
    Fading,
    Active
};

enum class GameLevelNavigationGate {
    Waiting,
    Ready,
    Unavailable
};

struct GameLevelLoadingState {
    GameLevelLoadPhase phase = GameLevelLoadPhase::Stopped;
    float displayedProgress = 0.0f;
    float fadeElapsedSeconds = 0.0f;
};

void BeginGameLevelLoading(GameLevelLoadingState& state);
void StopGameLevelLoading(GameLevelLoadingState& state);
void UpdateGameLevelLoadingProgress(
        GameLevelLoadingState& state,
        float assetProgress,
        float navigationProgress,
        bool complete);
void BeginGameLevelLoadingFade(GameLevelLoadingState& state);
bool AdvanceGameLevelLoadingFade(
        GameLevelLoadingState& state,
        float deltaSeconds);
void ActivateGameLevel(GameLevelLoadingState& state);

bool IsGameLevelLoading(const GameLevelLoadingState& state);
bool IsGameLevelLoadOverlayVisible(const GameLevelLoadingState& state);
float GameLevelLoadOverlayOpacity(const GameLevelLoadingState& state);
GameLevelNavigationGate EvaluateGameLevelNavigationGate(
        bool hasNpcs,
        bool navigationTerminal,
        bool navigationReady);

} // namespace game
