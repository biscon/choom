#include "game/GameLevelLoading.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

float UnitOrZero(float value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

} // namespace

void BeginGameLevelLoading(GameLevelLoadingState& state)
{
    state.phase = GameLevelLoadPhase::Loading;
    state.displayedProgress = 0.0f;
    state.fadeElapsedSeconds = 0.0f;
}

void StopGameLevelLoading(GameLevelLoadingState& state)
{
    state = GameLevelLoadingState{};
}

void UpdateGameLevelLoadingProgress(
        GameLevelLoadingState& state,
        float assetProgress,
        float navigationProgress,
        bool complete)
{
    if (state.phase != GameLevelLoadPhase::Loading) return;
    const float raw = UnitOrZero(assetProgress) * 0.80f
            + UnitOrZero(navigationProgress) * 0.20f;
    const float capped = complete ? 1.0f : std::min(raw, 0.99f);
    state.displayedProgress = std::max(state.displayedProgress, capped);
}

void BeginGameLevelLoadingFade(GameLevelLoadingState& state)
{
    state.phase = GameLevelLoadPhase::Fading;
    state.displayedProgress = 1.0f;
    state.fadeElapsedSeconds = 0.0f;
}

bool AdvanceGameLevelLoadingFade(
        GameLevelLoadingState& state,
        float deltaSeconds)
{
    if (state.phase != GameLevelLoadPhase::Fading) return false;
    const float delta = std::isfinite(deltaSeconds)
            ? std::max(0.0f, deltaSeconds) : 0.0f;
    state.fadeElapsedSeconds = std::min(
            GameLevelLoadingFadeSeconds,
            state.fadeElapsedSeconds + delta);
    return state.fadeElapsedSeconds >= GameLevelLoadingFadeSeconds;
}

void ActivateGameLevel(GameLevelLoadingState& state)
{
    state.phase = GameLevelLoadPhase::Active;
    state.displayedProgress = 1.0f;
    state.fadeElapsedSeconds = GameLevelLoadingFadeSeconds;
}

bool IsGameLevelLoading(const GameLevelLoadingState& state)
{
    return state.phase == GameLevelLoadPhase::Loading
            || state.phase == GameLevelLoadPhase::Fading;
}

bool IsGameLevelLoadOverlayVisible(const GameLevelLoadingState& state)
{
    return IsGameLevelLoading(state);
}

float GameLevelLoadOverlayOpacity(const GameLevelLoadingState& state)
{
    if (state.phase == GameLevelLoadPhase::Loading) return 1.0f;
    if (state.phase != GameLevelLoadPhase::Fading) return 0.0f;
    return 1.0f - std::clamp(
            state.fadeElapsedSeconds / GameLevelLoadingFadeSeconds,
            0.0f,
            1.0f);
}

GameLevelNavigationGate EvaluateGameLevelNavigationGate(
        bool hasNpcs,
        bool navigationTerminal,
        bool navigationReady)
{
    if (!navigationTerminal) return GameLevelNavigationGate::Waiting;
    if (hasNpcs && !navigationReady) {
        return GameLevelNavigationGate::Unavailable;
    }
    return GameLevelNavigationGate::Ready;
}

} // namespace game
