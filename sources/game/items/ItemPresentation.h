#pragma once

#include <raylib.h>

namespace game {

enum class ItemPresentationPhase {
    Settled,
    DropFrozen,
    DropFalling,
    PickupVacuum
};

struct ItemPresentationState {
    ItemPresentationPhase phase = ItemPresentationPhase::Settled;
    Vector3 pickupStartWorld = {};
    float elapsedSeconds = 0.0f;
    float dropLiftWorld = 0.0f;
    float scaleMultiplier = 1.0f;
};

struct ItemPresentationFrame {
    Vector3 visualOffset = {};
    float scaleMultiplier = 1.0f;
    bool removalReady = false;
};

void BeginItemPickupVacuum(
        ItemPresentationState& state,
        Vector3 renderedPosition);

void BeginFrozenItemDrop(
        ItemPresentationState& state,
        float liftWorld);

ItemPresentationFrame AdvanceItemPresentation(
        ItemPresentationState& state,
        Vector3 physicalPosition,
        Vector3 playerFeetPosition,
        float pickupDurationSeconds,
        float pickupTargetHeightWorld,
        float dropGravityWorldPerSecondSquared,
        bool inventoryOpen,
        float dt);

bool IsItemSettled(const ItemPresentationState& state);
bool IsItemPickupVacuuming(const ItemPresentationState& state);

} // namespace game
