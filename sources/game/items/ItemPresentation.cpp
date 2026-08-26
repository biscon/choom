#include "game/items/ItemPresentation.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float DefaultPickupDurationSeconds = 0.65f;
constexpr float DefaultDropGravityWorldPerSecondSquared = 25.0f;
constexpr float PickupShrinkStart = 0.80f;

float SmoothStep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

} // namespace

void BeginItemPickupVacuum(
        ItemPresentationState& state,
        Vector3 renderedOrigin,
        Vector3 visualCenterWorld)
{
    state = ItemPresentationState{};
    state.phase = ItemPresentationPhase::PickupVacuum;
    state.pickupStartCenterWorld = visualCenterWorld;
    state.pickupCenterFromOriginWorld = Vector3Subtract(
            visualCenterWorld, renderedOrigin);
    state.pickupTargetYWorld = visualCenterWorld.y;
}

void BeginFrozenItemDrop(
        ItemPresentationState& state,
        float liftWorld)
{
    state = ItemPresentationState{};
    state.phase = liftWorld > 0.0f
            ? ItemPresentationPhase::DropFrozen
            : ItemPresentationPhase::Settled;
    state.dropLiftWorld = std::max(0.0f, liftWorld);
}

ItemPresentationFrame AdvanceItemPresentation(
        ItemPresentationState& state,
        Vector3 physicalPosition,
        Vector3 playerFeetPosition,
        float pickupDurationSeconds,
        float pickupTargetHeightWorld,
        float dropGravityWorldPerSecondSquared,
        bool inventoryOpen,
        float dt)
{
    ItemPresentationFrame frame;
    dt = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    switch (state.phase) {
    case ItemPresentationPhase::Settled:
        state.scaleMultiplier = 1.0f;
        return frame;
    case ItemPresentationPhase::DropFrozen:
        frame.visualOffset.y = state.dropLiftWorld;
        if (!inventoryOpen) {
            state.phase = ItemPresentationPhase::DropFalling;
            state.elapsedSeconds = 0.0f;
        }
        return frame;
    case ItemPresentationPhase::DropFalling: {
        const float gravity = std::isfinite(dropGravityWorldPerSecondSquared)
                        && dropGravityWorldPerSecondSquared > 0.0f
                ? dropGravityWorldPerSecondSquared
                : DefaultDropGravityWorldPerSecondSquared;
        state.elapsedSeconds += dt;
        const float fallDistance = 0.5f * gravity
                * state.elapsedSeconds * state.elapsedSeconds;
        frame.visualOffset.y = std::max(
                0.0f, state.dropLiftWorld - fallDistance);
        if (frame.visualOffset.y <= 0.0f) {
            state = ItemPresentationState{};
        }
        return frame;
    }
    case ItemPresentationPhase::PickupVacuum: {
        const float duration = std::isfinite(pickupDurationSeconds)
                        && pickupDurationSeconds > 0.0f
                ? pickupDurationSeconds : DefaultPickupDurationSeconds;
        state.elapsedSeconds += dt;
        const float t = std::clamp(
                state.elapsedSeconds / duration, 0.0f, 1.0f);
        const float eased = SmoothStep(t);
        state.pickupTargetYWorld = std::max(
                state.pickupTargetYWorld,
                playerFeetPosition.y + std::max(
                        0.0f, pickupTargetHeightWorld));
        const Vector3 targetCenter{
                playerFeetPosition.x,
                state.pickupTargetYWorld,
                playerFeetPosition.z};
        const float shrink = std::clamp(
                (t - PickupShrinkStart) / (1.0f - PickupShrinkStart),
                0.0f,
                1.0f);
        frame.scaleMultiplier = 1.0f - shrink;
        const Vector3 renderedCenter = Vector3Lerp(
                state.pickupStartCenterWorld, targetCenter, eased);
        const Vector3 renderedOrigin = Vector3Subtract(
                renderedCenter,
                Vector3Scale(
                        state.pickupCenterFromOriginWorld,
                        frame.scaleMultiplier));
        frame.visualOffset = Vector3Subtract(
                renderedOrigin, physicalPosition);
        state.scaleMultiplier = frame.scaleMultiplier;
        frame.removalReady = t >= 1.0f;
        return frame;
    }
    }
    return frame;
}

bool IsItemSettled(const ItemPresentationState& state)
{
    return state.phase == ItemPresentationPhase::Settled;
}

bool IsItemPickupVacuuming(const ItemPresentationState& state)
{
    return state.phase == ItemPresentationPhase::PickupVacuum;
}

} // namespace game
