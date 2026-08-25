#pragma once

#include "engine/ecs/Entity.h"

#include <raylib.h>

#include <string_view>

namespace engine {
class AssetManager;
class World;
struct FontAsset;
}

namespace game {

class SectorCollisionWorld;

enum class SectorUseTargetKind {
    None,
    Item,
    DynamicProp,
    Door
};

struct SectorUseTarget {
    engine::Entity entity = engine::NullEntity();
    SectorUseTargetKind kind = SectorUseTargetKind::None;
    Vector3 targetPosition = {};
    float facingDot = -1.0f;
    float distance = 0.0f;
};

struct SectorUseHighlight {
    engine::Entity entity = engine::NullEntity();
    float strength = 0.0f;
};

struct SectorUseHighlightState {
    SectorUseHighlight highlight;
    float pulseElapsedSeconds = 0.0f;
    float releaseElapsedSeconds = 0.0f;
    float releaseStartStrength = 0.0f;
    bool releasing = false;
};

SectorUseTarget FindSectorUseTarget(
        engine::World& world,
        const engine::AssetManager* assets,
        Vector3 eyePosition,
        Vector3 forward,
        const SectorCollisionWorld* collisionWorld,
        bool includeDynamicProps = true);

std::string_view SectorUseTargetTitle(
        engine::World& world,
        const SectorUseTarget& target);

void ResetSectorUseHighlight(SectorUseHighlightState& state);

void UpdateSectorUseHighlight(
        SectorUseHighlightState& state,
        const SectorUseTarget& target,
        float dt);

void DrawSectorUsePrompt(
        Rectangle viewport,
        const engine::FontAsset* font,
        std::string_view title,
        std::string_view action = "Use");

void DrawSectorUseMessage(
        Rectangle viewport,
        const engine::FontAsset* font,
        std::string_view message,
        float elapsedSeconds);

} // namespace game
