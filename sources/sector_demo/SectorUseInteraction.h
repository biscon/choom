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
    StaticProp,
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

struct SectorObjectUseTargetAccumulator {
    SectorUseTarget nearest;
    bool nearestSelectable = false;
};

SectorUseTarget FindSectorUseTarget(
        engine::World& world,
        const engine::AssetManager* assets,
        Vector3 eyePosition,
        Vector3 forward,
        const SectorCollisionWorld* collisionWorld,
        bool includeDynamicProps = true);

void ConsiderSectorObjectUseBounds(
        SectorObjectUseTargetAccumulator& accumulator,
        Ray ray,
        engine::Entity entity,
        SectorUseTargetKind kind,
        BoundingBox bounds,
        bool selectable);
SectorUseTarget FinishSectorObjectUseTarget(
        const SectorObjectUseTargetAccumulator& accumulator,
        float topologyHitDistance = -1.0f);
SectorUseTarget FindSectorObjectUseTarget(
        engine::World& world,
        const engine::AssetManager& assets,
        Ray ray,
        const SectorCollisionWorld* collisionWorld);
std::string_view SectorObjectUseTargetInstanceId(
        engine::World& world,
        const SectorUseTarget& target);

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
