#include "game/SectorGameSession.h"

#include "sector_demo/SectorFpsController.h"
#include "game/items/ItemDropPlacement.h"
#include "game/items/ItemPresentation.h"
#include "engine/components/AnimatedModel.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorStaticModelLightmap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {

namespace {

Vector3 GameplayForward(const SectorEditorPreviewControllerState& controller)
{
    const SectorViewPose pose = SectorFpsControllerPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig);
    const float horizontal = std::cos(pose.pitchRadians);
    return Vector3{
            std::cos(pose.yawRadians) * horizontal,
            std::sin(pose.pitchRadians),
            std::sin(pose.yawRadians) * horizontal};
}

void UpdateAudioListener(
        engine::AudioSystem& audio,
        const Camera3D& camera)
{
    audio.SetListener(engine::AudioListener{
            camera.position,
            Vector3Subtract(camera.target, camera.position),
            camera.up});
}

bool ScopeFinishedOrEmpty(
        const engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    return engine::IsNull(scope) || assets.IsScopeFinished(scope);
}

bool FpsViewmodelLoadFinished(const FpsViewmodelRuntimeState& state)
{
    if (state.loadState == FpsViewmodelLoadState::Failed
            || state.loadState == FpsViewmodelLoadState::Inactive) {
        return true;
    }
    return state.loadState == FpsViewmodelLoadState::Ready
            && state.attachment.loadState
                    != FpsViewmodelAttachmentLoadState::Pending;
}

bool HasRuntimeNpcs(const SectorSceneRuntime& scene)
{
    return std::any_of(
            scene.NpcNavigation().records.begin(),
            scene.NpcNavigation().records.end(),
            [](const NpcNavigationRecord& record) {
                return record.occupied;
            });
}

bool NavigationBuildTerminal(SectorNavigationState state)
{
    return state == SectorNavigationState::Ready
            || state == SectorNavigationState::Empty
            || state == SectorNavigationState::Failed
            || state == SectorNavigationState::Uninitialized
            || state == SectorNavigationState::Stale;
}

SectorScriptAudioApi MakeSectorScriptAudioApi(SectorSceneRuntime& scene)
{
    SectorScriptAudioApi api;
    api.userData = &scene;
    api.playMapSound = [](void* userData, engine::EngineContext& context,
                              const std::string& id, float volume, float pitch,
                              std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->PlayLevelSound(
                context, id, volume, pitch, error);
    };
    api.playSoundEmitter = [](void* userData, engine::EngineContext& context,
                                  const std::string& id, const float* volume,
                                  float pitch, std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->PlaySoundEmitter(
                context, id, volume, pitch, error);
    };
    api.stopSoundEmitter = [](void* userData, engine::EngineContext& context,
                                  const std::string& id, std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->StopSoundEmitter(
                context, id, error);
    };
    return api;
}

float NavigationLoadProgress(const SectorNavigationWorld& navigation)
{
    const SectorNavigationState state = navigation.State();
    if (NavigationBuildTerminal(state)) return 1.0f;
    if (state != SectorNavigationState::Building) return 0.0f;
    const SectorNavigationBuildStatistics& statistics =
            navigation.BuildStatistics();
    if (statistics.tileCoordinateCount <= 0) return 0.0f;
    return std::clamp(
            static_cast<float>(statistics.builtTileCoordinateCount)
                    / static_cast<float>(statistics.tileCoordinateCount),
            0.0f,
            1.0f);
}

bool InitialNavigationObstaclesSettled(
        const SectorNavigationWorld& navigation)
{
    if (navigation.State() != SectorNavigationState::Ready) return true;
    const SectorNavigationDynamicObstacleStatistics& statistics =
            navigation.DynamicObstacleStatistics();
    return statistics.pendingCount == 0
            && statistics.removingCount == 0
            && statistics.backlogCount == 0;
}

std::string NavigationLoadFailure(const SectorNavigationWorld& navigation)
{
    const std::vector<SectorNavigationDiagnostic>& diagnostics =
            navigation.Diagnostics();
    for (auto it = diagnostics.rbegin(); it != diagnostics.rend(); ++it) {
        if (it->severity == SectorNavigationDiagnosticSeverity::Error
                && !it->message.empty()) {
            return it->message;
        }
    }
    return std::string{"navigation finished "}
            + SectorNavigationStateName(navigation.State());
}

bool FirstScriptValueIsTrue(const std::vector<engine::ScriptValue>& values)
{
    return !values.empty()
            && std::holds_alternative<bool>(values.front())
            && std::get<bool>(values.front());
}

BoundingBox ItemVisualBounds(
        const engine::AssetManager& assets,
        engine::ModelHandle model,
        const SectorObjectTransform& transform,
        float scale,
        bool animated)
{
    BoundingBox local = kItemDropFallbackLocalBounds;
    if (const engine::ModelAsset* asset = assets.GetModelAsset(model)) {
        if (animated && asset->hasAnimatedLocalBounds) {
            local = asset->animatedLocalBounds;
        } else if (asset->hasLocalBounds) {
            local = asset->localBounds;
        }
    }
    return TransformItemDropBounds(
            local,
            BuildSectorStaticModelAuthoredTransform(
                    transform.position,
                    transform.rotationXRadians,
                    transform.yawRadians,
                    transform.rotationZRadians,
                    scale));
}

} // namespace

void SectorGameSession::ShowCarryRefusal()
{
    std::snprintf(
            itemMessage.data(),
            itemMessage.size(),
            "%s",
            "I can't carry anymore.");
    itemMessageElapsedSeconds = 0.0f;
}

void SectorGameSession::ShowDropRefusal()
{
    std::snprintf(
            itemMessage.data(),
            itemMessage.size(),
            "%s",
            "I can't drop that here.");
    itemMessageElapsedSeconds = 0.0f;
}

void SectorGameSession::ShowOutOfAmmo()
{
    std::snprintf(
            itemMessage.data(),
            itemMessage.size(),
            "%s",
            "Out of ammo.");
    itemMessageElapsedSeconds = 0.0f;
}

void SectorGameSession::RefreshMouseLookCapture()
{
    if (!running || paused) return;
    SetSectorFreeflyMouseLookEnabled(
            controller.freeflyController,
            !consoleInputCaptured
                    && !inventoryUi.open
                    && heldObjectUse.phase == ItemHeldUsePhase::Inactive);
}

void SectorGameSession::SetInventoryOpen(bool open)
{
    if (inventoryUi.open == open) return;
    inventoryUi.open = open;
    if (!open) ClearItemInventoryInteraction(inventoryUi);
    if (open) {
        if (itemCampaign != nullptr) {
            NormalizeItemInventorySelection(
                    inventoryUi, itemCampaign->inventory);
        }
        useTarget = {};
        ResetSectorUseHighlight(useHighlightState);
        usePromptTitle = {};
    }
    RefreshMouseLookCapture();
}

void SectorGameSession::ClearHeldObjectUse()
{
    heldObjectUse = {};
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    RefreshMouseLookCapture();
}

bool SectorGameSession::BeginHeldObjectUse(std::uint64_t runtimeId)
{
    if (itemCampaign == nullptr || itemRegistry == nullptr || runtimeId == 0
            || heldObjectUse.phase != ItemHeldUsePhase::Inactive) {
        return false;
    }
    const auto found = std::find_if(
            itemCampaign->inventory.entries.begin(),
            itemCampaign->inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (found == itemCampaign->inventory.entries.end()
            || found->onUseScript.empty()) {
        return false;
    }
    const ItemDefinition* definition = FindItemDefinition(
            *itemRegistry, found->definitionId);
    if (definition == nullptr || definition->type != ItemType::Object) {
        return false;
    }
    heldObjectUse.phase = ItemHeldUsePhase::Targeting;
    heldObjectUse.runtimeId = runtimeId;
    heldObjectUse.task = {};
    SetInventoryOpen(false);
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    RefreshMouseLookCapture();
    return true;
}

bool SectorGameSession::ConsumeHeldObjectEntry(std::uint64_t runtimeId)
{
    if (itemCampaign == nullptr || itemRegistry == nullptr) return false;
    std::size_t affectedIndex = 0;
    if (CompleteObjectInventoryUse(
                *itemCampaign,
                *itemRegistry,
                runtimeId,
                true,
                &affectedIndex) != ItemObjectUseResult::Consumed) {
        return false;
    }
    NormalizeItemInventorySelection(
            inventoryUi, itemCampaign->inventory, affectedIndex);
    return true;
}

void SectorGameSession::InvokeHeldObjectUse(engine::EngineContext& context)
{
    if (heldObjectUse.phase != ItemHeldUsePhase::Targeting
            || itemCampaign == nullptr || itemRegistry == nullptr) {
        return;
    }
    const auto found = std::find_if(
            itemCampaign->inventory.entries.begin(),
            itemCampaign->inventory.entries.end(),
            [this](const ItemInventoryEntry& entry) {
                return entry.runtimeId == heldObjectUse.runtimeId;
            });
    const std::string_view targetInstanceId =
            SectorObjectUseTargetInstanceId(context.world, useTarget);
    if (found == itemCampaign->inventory.entries.end()
            || found->onUseScript.empty() || targetInstanceId.empty()) {
        ClearHeldObjectUse();
        return;
    }
    const ItemDefinition* definition = FindItemDefinition(
            *itemRegistry, found->definitionId);
    if (definition == nullptr || definition->type != ItemType::Object) {
        ClearHeldObjectUse();
        return;
    }
    const std::string scriptName = found->onUseScript;
    const std::uint64_t runtimeId = found->runtimeId;
    const std::array<engine::ScriptValue, 1> arguments{
            engine::ScriptValue{std::string{targetInstanceId}}};
    const engine::ScriptCallOutcome outcome =
            engine::ScriptSystemCallObservedForegroundHook(
                    scripts,
                    scriptName,
                    arguments.data(),
                    arguments.size());
    if (outcome.result == engine::ScriptCallResult::Completed) {
        if (FirstScriptValueIsTrue(outcome.immediateValues)) {
            ConsumeHeldObjectEntry(runtimeId);
        }
        ClearHeldObjectUse();
        return;
    }
    if (outcome.result == engine::ScriptCallResult::Started) {
        heldObjectUse.phase = ItemHeldUsePhase::Pending;
        heldObjectUse.task = outcome.task;
        useTarget = {};
        ResetSectorUseHighlight(useHighlightState);
        RefreshMouseLookCapture();
        return;
    }
    if (outcome.result == engine::ScriptCallResult::ForegroundBusy
            || outcome.result == engine::ScriptCallResult::AlreadyRunning) {
        return;
    }
    if (outcome.result == engine::ScriptCallResult::Missing) {
        TraceLog(
                LOG_WARNING,
                "[Lua WARNING] carried Object has no callable use function '%s'",
                scriptName.c_str());
    } else if (outcome.result == engine::ScriptCallResult::Error) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] carried Object use function '%s' failed: %s",
                scriptName.c_str(), outcome.error.c_str());
    }
    ClearHeldObjectUse();
}

void SectorGameSession::UpdatePendingHeldObjectUse()
{
    if (heldObjectUse.phase != ItemHeldUsePhase::Pending) return;
    engine::ScriptObservedCallOutcome outcome;
    if (!engine::ScriptSystemTakeObservedCallOutcome(
                scripts, heldObjectUse.task, outcome)) {
        return;
    }
    const std::uint64_t runtimeId = heldObjectUse.runtimeId;
    if (outcome.state == engine::ScriptTaskState::Completed
            && FirstScriptValueIsTrue(outcome.values)) {
        ConsumeHeldObjectEntry(runtimeId);
    } else if (outcome.state == engine::ScriptTaskState::Failed) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] carried Object use callback failed: %s",
                outcome.error.c_str());
    }
    ClearHeldObjectUse();
}

bool SectorGameSession::HandleEscape()
{
    if (inventoryUi.open) {
        if (CancelItemInventorySplit(inventoryUi)) return true;
        SetInventoryOpen(false);
        return true;
    }
    const ItemHeldUseInputDecision decision = EvaluateItemHeldUseInput(
            heldObjectUse.phase, ItemHeldUseInput::Escape);
    if (decision.effect == ItemHeldUseEffect::CancelToGameplay) {
        ClearHeldObjectUse();
    }
    return decision.consumeEvent;
}

void SectorGameSession::RenderInventoryUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::FontHandle usePromptFont)
{
    if (!IsActive()) return;
    logicalViewport = config.overlayBounds;
    if (heldObjectUse.phase != ItemHeldUsePhase::Inactive
            && itemModelAssets != nullptr && itemCampaign != nullptr) {
        DrawHeldItemCursor(
                assets,
                *itemModelAssets,
                itemCampaign->inventory,
                heldObjectUse.runtimeId,
                input.MousePosition());
    }
    if (!inventoryUi.open || itemRegistry == nullptr
            || itemModelAssets == nullptr || itemCampaign == nullptr
            || applicationSettings == nullptr) {
        return;
    }
    const ItemInventoryUIAction action = DrawItemInventoryUI(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            *itemRegistry,
            *itemModelAssets,
            *itemCampaign,
            applicationSettings->playerInventory,
            playerHealth,
            inventoryUi);
    if (pendingInventoryAction.type == ItemInventoryUIActionType::None
            && action.type != ItemInventoryUIActionType::None) {
        pendingInventoryAction = action;
        if (action.type == ItemInventoryUIActionType::UseHealth) {
            SetInventoryOpen(false);
        }
    }
    if (itemMessage[0] != '\0') {
        DrawSectorUseMessage(
                config.overlayBounds,
                assets.GetFont(usePromptFont),
                itemMessage.data(),
                itemMessageElapsedSeconds);
    }
}

bool SectorGameSession::DropInventoryEntry(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        std::uint64_t runtimeId,
        std::size_t& affectedIndex)
{
    affectedIndex = 0;
    if (itemRegistry == nullptr || itemModelAssets == nullptr
            || itemCampaign == nullptr || applicationSettings == nullptr
            || !collision.sectorCollisionWorldValid) {
        return false;
    }
    const auto entryIt = std::find_if(
            itemCampaign->inventory.entries.begin(),
            itemCampaign->inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (entryIt == itemCampaign->inventory.entries.end()) return false;
    affectedIndex = entryIt->slotIndex >= 0
            ? static_cast<std::size_t>(entryIt->slotIndex) : 0u;
    const ItemDefinition* definition = FindItemDefinition(
            *itemRegistry, entryIt->definitionId);
    if (definition == nullptr) return false;
    const std::uint64_t dropQuantity = entryIt->quantity;
    if (dropQuantity == 0
            || dropQuantity
                    > static_cast<std::uint64_t>(kMaximumItemStackSize)) {
        return false;
    }

    ItemLevelCampaignState& level = FindOrCreateItemLevelCampaignState(
            *itemCampaign, levelName, topologyMap.runtimeObjects.size());
    int objectId = std::max(1, level.nextDroppedObjectId);
    while (FindSectorPlacedRuntimeObject(topologyMap, objectId) != nullptr) {
        if (objectId == std::numeric_limits<int>::max()) return false;
        ++objectId;
    }

    BoundingBox localBounds = kItemDropFallbackLocalBounds;
    if (const ItemModelAssetEntry* modelEntry = FindItemModelAsset(
                *itemModelAssets, definition->id)) {
        if (const engine::ModelAsset* model = context.assets.GetModelAsset(
                    modelEntry->model)) {
            if (model->hasLocalBounds) localBounds = model->localBounds;
        }
    }
    Vector3 forward = GameplayForward(controller);
    forward.y = 0.0f;
    const float forwardLength = std::sqrt(
            forward.x * forward.x + forward.z * forward.z);
    if (!(forwardLength > 0.0001f)) return false;
    forward.x /= forwardLength;
    forward.z /= forwardLength;
    const SectorRuntimeObjectState& objects = scene.RuntimeObjects();
    const SectorFpsControllerConfig playerConfig =
            EffectiveSectorFpsControllerConfig(
                    controller.fpsControllerState,
                    controller.fpsControllerConfig);
    const Vector3 eyePosition = SectorFpsControllerEyePosition(
            controller.fpsControllerState, playerConfig);
    const float dropYawRadians = BuildItemDropRandomYawRadians(
            runtimeId, static_cast<std::uint64_t>(objectId));
    const auto slotOrigins = BuildItemDropSlotOrigins(
            controller.fpsControllerState.feetPosition,
            forward,
            playerConfig.playerRadius,
            localBounds);
    ItemDropCandidate candidate;
    float candidateLiftWorld = 0.0f;
    for (const Vector3 slotOrigin : slotOrigins) {
        const ItemDropCandidate slotCandidate = BuildItemDropCandidate(
                collision.sectorCollisionWorld,
                controller.fpsControllerState.currentSectorId,
                slotOrigin,
                localBounds,
                dropYawRadians);
        const float slotLiftWorld = ItemDropLiftToCenterAtHeight(
                slotCandidate, eyePosition.y);
        const ItemDropCandidate sweptCandidate = BuildLiftedItemDropSweep(
                slotCandidate, slotLiftWorld);
        if (!slotCandidate.valid || !sweptCandidate.valid
                || !ItemDropFitsTopology(
                        collision.sectorCollisionWorld, slotCandidate)
                || !ItemDropFitsTopology(
                        collision.sectorCollisionWorld, sweptCandidate)) {
            continue;
        }
        bool blocked = false;
        for (const SectorDynamicDoorCollider& door : objects.dynamicDoorColliders) {
            if (ItemDropBoundsOverlap(sweptCandidate.worldBounds, door)) {
                blocked = true;
                break;
            }
        }
        if (blocked
                || ItemDropBoundsOverlapAnyPropCollider(
                        sweptCandidate.worldBounds, objects.staticModelColliders)
                || ItemDropBoundsOverlapAnyPropCollider(
                        sweptCandidate.worldBounds, objects.dynamicModelColliders)
                || ItemDropBoundsOverlapPlayer(
                        sweptCandidate.worldBounds,
                        controller.fpsControllerState.feetPosition,
                        playerConfig.playerRadius,
                        playerConfig.playerHeight)) {
            continue;
        }
        context.world.ForEach<SectorObjectTransform, SectorItem>(
                [&context, &sweptCandidate, &blocked](
                        engine::Entity,
                        SectorObjectTransform& transform,
                        SectorItem& item) {
                    if (blocked) return;
                    if (IsItemPickupVacuuming(item.presentation)) return;
                    blocked = ItemDropBoundsOverlap(
                            sweptCandidate.worldBounds,
                            ItemVisualBounds(
                                    context.assets,
                                    item.model,
                                    transform,
                                    item.scale,
                                    false));
                });
        if (!blocked) {
            candidate = slotCandidate;
            candidateLiftWorld = slotLiftWorld;
            break;
        }
    }
    if (!candidate.valid) return false;

    SectorPlacedRuntimeObject drop;
    drop.id = objectId;
    drop.kind = "item";
    drop.position = SectorWorldToAuthoringPosition(candidate.originWorld);
    drop.yawRadians = candidate.yawRadians;
    drop.item.definitionId = definition->id;
    drop.item.instanceId = AllocateSectorItemInstanceId(topologyMap, objectId);
    drop.item.quantity = static_cast<int>(dropQuantity);
    drop.item.onUseScript = definition->type == ItemType::Object
            ? entryIt->onUseScript : std::string{};
    drop.item.sessionDrop = true;
    engine::Entity spawned = engine::NullEntity();
    if (!scene.SpawnItemRuntimeObject(
                context, topologyMap, drop, &spawned)) {
        return false;
    }
    if (context.world.IsAlive(spawned)
            && context.world.Has<SectorItem>(spawned)
            && context.world.Has<SectorObjectVisualOffset>(spawned)) {
        SectorItem& spawnedItem = context.world.Get<SectorItem>(spawned);
        BeginFrozenItemDrop(
                spawnedItem.presentation, candidateLiftWorld);
        context.world.Get<SectorObjectVisualOffset>(spawned).position =
                Vector3{0.0f, candidateLiftWorld, 0.0f};
    }
    if (!RemoveInventoryEntryQuantity(
                itemCampaign->inventory,
                runtimeId,
                dropQuantity,
                nullptr)) {
        QueueRemoveSectorRuntimeObjectByEntity(
                context.world, scene.RuntimeObjects(), spawned);
        context.world.FlushDestroyedEntities();
        return false;
    }
    if (definition->type == ItemType::Weapon
            && itemCampaign->weapons.activeWeaponId == definition->weaponId
            && !InventoryOwnsWeapon(
                    itemCampaign->inventory,
                    *itemRegistry,
                    definition->weaponId)) {
        fpsPlayer.QueueUnequip(&itemCampaign->weapons);
    }
    level.droppedItems.push_back(drop);
    topologyMap.runtimeObjects.push_back(drop);
    level.nextDroppedObjectId = objectId == std::numeric_limits<int>::max()
            ? objectId : objectId + 1;
    return true;
}

void SectorGameSession::ProcessInventoryAction(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (pendingInventoryAction.type == ItemInventoryUIActionType::None) return;
    const ItemInventoryUIAction action = pendingInventoryAction;
    pendingInventoryAction = {};
    if (itemCampaign == nullptr || itemRegistry == nullptr) return;
    std::size_t affectedIndex = 0;
    if (action.type == ItemInventoryUIActionType::UseHealth) {
        const auto found = std::find_if(
                itemCampaign->inventory.entries.begin(),
                itemCampaign->inventory.entries.end(),
                [&action](const ItemInventoryEntry& entry) {
                    return entry.runtimeId == action.runtimeId;
                });
        affectedIndex = found == itemCampaign->inventory.entries.end()
                ? 0u
                : static_cast<std::size_t>(std::max(0, found->slotIndex));
        UseHealthInventoryEntry(
                *itemCampaign, *itemRegistry, playerHealth, action.runtimeId);
    } else if (action.type == ItemInventoryUIActionType::UseObject) {
        BeginHeldObjectUse(action.runtimeId);
        return;
    } else if (action.type == ItemInventoryUIActionType::Drop) {
        if (!DropInventoryEntry(
                    context, scene, action.runtimeId, affectedIndex)) {
            ShowDropRefusal();
            return;
        }
    } else if (action.type == ItemInventoryUIActionType::Transfer) {
        const ItemInventoryTransactionResult transaction =
                TransferItemInventoryEntry(
                        itemCampaign->inventory,
                        *itemRegistry,
                        action.runtimeId,
                        action.targetSlotIndex,
                        applicationSettings != nullptr
                                ? applicationSettings->playerInventory.maxSlots
                                : 0);
        if (transaction.type != ItemInventoryTransactionType::Rejected) {
            inventoryUi.selectedRuntimeId = transaction.selectedRuntimeId;
        }
        return;
    } else if (action.type == ItemInventoryUIActionType::Split) {
        const ItemInventoryTransactionResult transaction =
                SplitItemInventoryEntry(
                        itemCampaign->inventory,
                        action.runtimeId,
                        action.quantity,
                        action.targetSlotIndex,
                        applicationSettings != nullptr
                                ? applicationSettings->playerInventory.maxSlots
                                : 0);
        if (transaction.type == ItemInventoryTransactionType::Split) {
            inventoryUi.selectedRuntimeId = transaction.selectedRuntimeId;
        }
        return;
    }
    NormalizeItemInventorySelection(
            inventoryUi, itemCampaign->inventory, affectedIndex);
}

void SectorGameSession::UpdateItemPresentations(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (applicationSettings == nullptr) return;
    completedItemPresentations.clear();
    const SectorFpsControllerConfig playerConfig =
            EffectiveSectorFpsControllerConfig(
                    controller.fpsControllerState,
                    controller.fpsControllerConfig);
    context.world.ForEach<
            SectorObjectTransform,
            SectorObjectVisualOffset,
            SectorItem>(
            [this, dt, &playerConfig](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObjectVisualOffset& visualOffset,
                    SectorItem& item) {
                const ItemPresentationFrame frame = AdvanceItemPresentation(
                        item.presentation,
                        transform.position,
                        controller.fpsControllerState.feetPosition,
                        applicationSettings->playerInventory
                                .pickupVacuumDurationSeconds,
                        applicationSettings->playerInventory
                                .pickupVacuumTargetHeightWorld,
                        playerConfig.gravity,
                        inventoryUi.open,
                        dt);
                visualOffset.position = frame.visualOffset;
                item.presentation.scaleMultiplier = frame.scaleMultiplier;
                if (!frame.removalReady) return;
                if (completedItemPresentations.size()
                        == completedItemPresentations.capacity()) {
                    std::fprintf(
                            stderr,
                            "[SectorGameSession WARNING] item presentation completion capacity exceeded during update\n");
                }
                completedItemPresentations.push_back(entity);
            });

    bool queuedAny = false;
    for (engine::Entity entity : completedItemPresentations) {
        if (!QueueRemoveSectorRuntimeObjectByEntity(
                    context.world, scene.RuntimeObjects(), entity)) {
            TraceLog(
                    LOG_ERROR,
                    "Completed item pickup entity %u could not be queued for removal",
                    entity.index);
            continue;
        }
        queuedAny = true;
    }
    if (queuedAny) context.world.FlushDestroyedEntities();
}

bool SectorGameSession::CommitItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        engine::Entity entity,
        int placedObjectId,
        const std::string& instanceId)
{
    if (itemRegistry == nullptr || itemCampaign == nullptr
            || applicationSettings == nullptr
            || !context.world.IsAlive(entity)
            || !context.world.Has<SectorItem>(entity)
            || !context.world.Has<SectorObjectTransform>(entity)
            || !context.world.Has<SectorObjectVisualOffset>(entity)) {
        return false;
    }
    SectorItem& item = context.world.Get<SectorItem>(entity);
    if (item.placedObjectId != placedObjectId
            || item.instanceId != instanceId) {
        return false;
    }
    const ItemPickupPlan plan = PreflightItemPickup(
            itemCampaign->inventory,
            *itemRegistry,
            applicationSettings->playerInventory,
            item.definitionId,
            item.quantity,
            item.onUseScript);
    if (plan.result != ItemPickupCapacityResult::Fits) {
        item.takePending = false;
        if (plan.result == ItemPickupCapacityResult::WeightLimit
                || plan.result == ItemPickupCapacityResult::SlotLimit
                || plan.result == ItemPickupCapacityResult::NumericOverflow) {
            ShowCarryRefusal();
        }
        return false;
    }
    ItemLevelCampaignState& level = FindOrCreateItemLevelCampaignState(
            *itemCampaign, levelName, topologyMap.runtimeObjects.size());
    if (item.origin == SectorItemOrigin::SessionDrop) {
        const bool exists = std::any_of(
                level.droppedItems.begin(), level.droppedItems.end(),
                [placedObjectId](const SectorPlacedRuntimeObject& object) {
                    return object.id == placedObjectId;
                });
        if (!exists) {
            item.takePending = false;
            return false;
        }
    }
    const bool runtimeTracked = std::any_of(
            scene.RuntimeObjects().placedObjectEntities.begin(),
            scene.RuntimeObjects().placedObjectEntities.end(),
            [entity](const SectorPlacedRuntimeObjectEntity& entry) {
                return entry.entity == entity;
            });
    if (!runtimeTracked) {
        item.takePending = false;
        return false;
    }
    if (!CommitItemPickup(itemCampaign->inventory, plan)) {
        item.takePending = false;
        return false;
    }
    if (plan.definition->type == ItemType::Weapon
            && itemCampaign->weapons.activeWeaponId.empty()
            && weaponRegistry != nullptr
            && applicationSettings != nullptr) {
        fpsPlayer.EquipWeapon(
                context.assets,
                scene.Renderer(),
                *weaponRegistry,
                *applicationSettings,
                plan.definition->weaponId,
                &itemCampaign->weapons);
    }
    if (item.origin == SectorItemOrigin::SessionDrop) {
        RemoveSessionDroppedItem(level, placedObjectId);
    } else {
        RecordAuthoredItemCollected(level, placedObjectId);
    }
    topologyMap.runtimeObjects.erase(
            std::remove_if(
                    topologyMap.runtimeObjects.begin(),
                    topologyMap.runtimeObjects.end(),
                    [placedObjectId](const SectorPlacedRuntimeObject& object) {
                        return object.id == placedObjectId;
                    }),
            topologyMap.runtimeObjects.end());
    const SectorObjectTransform& transform =
            context.world.Get<SectorObjectTransform>(entity);
    const SectorObjectVisualOffset& visualOffset =
            context.world.Get<SectorObjectVisualOffset>(entity);
    const Vector3 renderedOrigin = Vector3Add(
            transform.position, visualOffset.position);
    Vector3 visualCenterWorld = renderedOrigin;
    const engine::ModelAsset* modelAsset =
            context.assets.GetModelAsset(item.model);
    if (modelAsset != nullptr && modelAsset->hasLocalBounds) {
        SectorObjectTransform renderedTransform = transform;
        renderedTransform.position = renderedOrigin;
        const BoundingBox renderedBounds = ItemVisualBounds(
                context.assets,
                item.model,
                renderedTransform,
                item.scale,
                false);
        visualCenterWorld = Vector3Scale(
                Vector3Add(renderedBounds.min, renderedBounds.max),
                0.5f);
    }
    BeginItemPickupVacuum(
            item.presentation,
            renderedOrigin,
            visualCenterWorld);
    item.takePending = true;
    item.shadowMode = SectorDynamicModelShadowMode::None;
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    return true;
}

bool SectorGameSession::RequestItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        engine::Entity entity)
{
    if (pendingItemTake.active || itemRegistry == nullptr
            || itemCampaign == nullptr || applicationSettings == nullptr
            || !context.world.IsAlive(entity)
            || !context.world.Has<SectorItem>(entity)) {
        return false;
    }
    SectorItem& item = context.world.Get<SectorItem>(entity);
    if (item.takePending) return false;
    const ItemPickupPlan plan = PreflightItemPickup(
            itemCampaign->inventory,
            *itemRegistry,
            applicationSettings->playerInventory,
            item.definitionId,
            item.quantity,
            item.onUseScript);
    if (plan.result != ItemPickupCapacityResult::Fits) {
        if (plan.result == ItemPickupCapacityResult::WeightLimit
                || plan.result == ItemPickupCapacityResult::SlotLimit
                || plan.result == ItemPickupCapacityResult::NumericOverflow) {
            ShowCarryRefusal();
        }
        return true;
    }
    if (item.onTakeScript.empty()) {
        CommitItemTake(
                context, scene, entity, item.placedObjectId, item.instanceId);
        return true;
    }
    const engine::ScriptCallOutcome outcome =
            engine::ScriptSystemCallObservedForegroundHook(
                    scripts, item.onTakeScript);
    if (outcome.result == engine::ScriptCallResult::Completed) {
        if (FirstScriptValueIsTrue(outcome.immediateValues)) {
            CommitItemTake(
                    context, scene, entity,
                    item.placedObjectId, item.instanceId);
        }
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Started) {
        item.takePending = true;
        pendingItemTake.task = outcome.task;
        pendingItemTake.entity = entity;
        pendingItemTake.placedObjectId = item.placedObjectId;
        pendingItemTake.instanceId = item.instanceId;
        pendingItemTake.active = true;
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Missing) {
        TraceLog(
                LOG_WARNING,
                "[Lua WARNING] item '%s' has no callable take function '%s'",
                item.instanceId.c_str(), item.onTakeScript.c_str());
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Error) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] item '%s' take function '%s' failed: %s",
                item.instanceId.c_str(),
                item.onTakeScript.c_str(),
                outcome.error.c_str());
        return true;
    }
    return false;
}

void SectorGameSession::UpdatePendingItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (!pendingItemTake.active) return;
    engine::ScriptObservedCallOutcome outcome;
    if (!engine::ScriptSystemTakeObservedCallOutcome(
                scripts, pendingItemTake.task, outcome)) {
        return;
    }
    const PendingItemTake pending = pendingItemTake;
    pendingItemTake = {};
    if (context.world.IsAlive(pending.entity)
            && context.world.Has<SectorItem>(pending.entity)) {
        context.world.Get<SectorItem>(pending.entity).takePending = false;
    }
    if (outcome.state == engine::ScriptTaskState::Completed
            && FirstScriptValueIsTrue(outcome.values)) {
        CommitItemTake(
                context,
                scene,
                pending.entity,
                pending.placedObjectId,
                pending.instanceId);
    } else if (outcome.state == engine::ScriptTaskState::Failed) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] item take callback failed: %s",
                outcome.error.c_str());
    }
}

bool SectorGameSession::StartNew(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorLevelEntryRequest& entry,
        const SectorMaterialRegistry& materials,
        const FpsWeaponRegistry& registry,
        const ItemRegistry& items,
        const ItemModelAssetState& itemAssets,
        ItemCampaignState& campaign,
        const FpsApplicationSettings& settings,
        PlayerAudioRuntime& playerAudioRuntime,
        engine::PersistentScriptStore& persistentStore,
        bool loadingSave,
        std::string& error)
{
    failureError.clear();
    playerHealth = MakeHealth(100);
    playerKnockbackVelocity = {};
    playerStunRemainingSeconds = 0.0f;
    godMode = false;
    aiFrozen = false;
    gameOver = false;
    playerStamina = MakePlayerStamina(settings.playerStamina);
    ClearPlayerWindedCamera(windedCamera);
    breathingAudio = PlayerBreathingAudioRuntime{};
    const std::string& requestedLevelName = entry.levelName;
    const std::string path = ApplicationLevelAssetPath(requestedLevelName);
    if (path.empty()) {
        error = "Invalid first level name '" + requestedLevelName + "'";
        return false;
    }

    SectorTopologyMap loaded;
    if (!LoadSectorRuntimeLevel(path, materials, loaded, error)) {
        return false;
    }
    ReconcileItemCampaignLevel(campaign, requestedLevelName, loaded);
    ItemLevelCampaignState& campaignLevel = FindOrCreateItemLevelCampaignState(
            campaign, requestedLevelName, loaded.runtimeObjects.size());
    const std::size_t dropCapacity = campaignLevel.droppedItems.size()
            + static_cast<std::size_t>(std::max(1, settings.playerInventory.maxSlots));
    campaignLevel.droppedItems.reserve(dropCapacity);
    loaded.runtimeObjects.reserve(
            loaded.runtimeObjects.size()
            + static_cast<std::size_t>(std::max(1, settings.playerInventory.maxSlots)));
    const SectorCompiledLevelMarker* entryMarker = nullptr;
    if (!ResolveSectorLevelEntryMarker(loaded, entry.markerId, entryMarker, error)) {
        return false;
    }
    const std::string resolvedEntryMarkerId =
            entryMarker != nullptr ? entryMarker->id : std::string{};
    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                loaded,
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    if (!scene.Rebuild(
                context,
                loaded,
                "sector_game",
                settings.footsteps.defaultSet,
                settings.footsteps.volume,
                error)) {
        return false;
    }

    topologyMap = std::move(loaded);
    scene.RuntimeObjects().placedObjectEntities.reserve(
            topologyMap.runtimeObjects.capacity());
    completedItemPresentations.clear();
    completedItemPresentations.reserve(topologyMap.runtimeObjects.capacity());
    entryMarker = resolvedEntryMarkerId.empty()
            ? nullptr
            : FindSectorCompiledLevelMarker(topologyMap, resolvedEntryMarkerId);
    levelName = requestedLevelName;
    levelPath = path;
    controller = SectorEditorPreviewControllerState{};
    collision = SectorEditorPreviewCollisionState{};
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    itemMessage = {};
    itemMessageElapsedSeconds = 0.0f;
    pendingItemTake = {};
    inventoryUi = {};
    pendingInventoryAction = {};
    heldObjectUse = {};
    logicalViewport = Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f};
    navigationDebug = SectorGameNavigationDebugState{};
    controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    ResetSectorFreeflyController(
            controller.freeflyController,
            scene.Renderer().RendererPose());
    controller.previewControlMode = SectorPreviewControlMode::FreeFly;
    if (!BuildCollisionAndPlayer(scene, true, entryMarker, &error)) {
        if (error.empty()) {
            error = collision.sectorCollisionWorldWarning.empty()
                    ? "Could not build the game collision world"
                    : collision.sectorCollisionWorldWarning;
        }
        scene.Shutdown(context);
        topologyMap = SectorTopologyMap{};
        levelName.clear();
        levelPath.clear();
        return false;
    }
    EnterSectorFreeflyController(controller.freeflyController);
    weaponRegistry = &registry;
    itemRegistry = &items;
    itemModelAssets = &itemAssets;
    itemCampaign = &campaign;
    materialRegistry = &materials;
    applicationSettings = &settings;
    playerAudio = &playerAudioRuntime;
    persistentScripts = &persistentStore;
    fpsPlayer.Begin(
            context.assets,
            scene.Renderer(),
            registry,
            settings,
            "fps_game_viewmodel",
            false);
    if (!campaign.weapons.activeWeaponId.empty()) {
        if (InventoryOwnsWeapon(
                    campaign.inventory,
                    items,
                    campaign.weapons.activeWeaponId)) {
            fpsPlayer.EquipWeapon(
                    context.assets,
                    scene.Renderer(),
                    registry,
                    settings,
                    campaign.weapons.activeWeaponId,
                    &campaign.weapons);
        } else {
            campaign.weapons.activeWeaponId.clear();
        }
    }
    running = true;
    paused = false;
    consoleInputCaptured = false;
    InitializeSectorScriptHost(
            scriptHost,
            scene.RuntimeObjects(),
            topologyMap,
            scripts,
            &scene.Navigation(),
            &scene.NpcNavigation(),
            MakeSectorScriptAudioApi(scene),
            &playerHealth);
    pendingLoadingSave = loadingSave;
    BeginGameLevelLoading(loading);
    error.clear();
    return true;
}

void SectorGameSession::Shutdown(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (playerAudio != nullptr) {
        StopPlayerBreathingAudio(
                context.assets,
                context.audio,
                *playerAudio,
                breathingAudio);
    } else {
        breathingAudio = PlayerBreathingAudioRuntime{};
    }
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    fpsPlayer.End(context.assets, scene.Renderer());
    if (running) {
        LeaveSectorFreeflyController();
    }
    scene.Shutdown(context);
    topologyMap = SectorTopologyMap{};
    controller = SectorEditorPreviewControllerState{};
    collision = SectorEditorPreviewCollisionState{};
    navigationDebug = SectorGameNavigationDebugState{};
    playerStamina = PlayerStamina{};
    playerKnockbackVelocity = {};
    playerStunRemainingSeconds = 0.0f;
    godMode = false;
    aiFrozen = false;
    gameOver = false;
    ClearPlayerWindedCamera(windedCamera);
    StopGameLevelLoading(loading);
    levelName.clear();
    levelPath.clear();
    running = false;
    paused = false;
    consoleInputCaptured = false;
    pendingLoadingSave = false;
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    itemMessage = {};
    itemMessageElapsedSeconds = 0.0f;
    pendingItemTake = {};
    inventoryUi = {};
    pendingInventoryAction = {};
    heldObjectUse = {};
    completedItemPresentations.clear();
    logicalViewport = Rectangle{0.0f, 0.0f, 1920.0f, 1080.0f};
    weaponRegistry = nullptr;
    itemRegistry = nullptr;
    itemModelAssets = nullptr;
    itemCampaign = nullptr;
    materialRegistry = nullptr;
    applicationSettings = nullptr;
    playerAudio = nullptr;
    persistentScripts = nullptr;
}

void SectorGameSession::SuspendForEditor(engine::EngineContext& context)
{
    Pause();
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    pendingItemTake = {};
    heldObjectUse = {};
    inventoryUi.open = false;
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
}

void SectorGameSession::Pause()
{
    if (!running || paused) {
        return;
    }
    paused = true;
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    LeaveSectorFreeflyController();
}

void SectorGameSession::Resume(SectorSceneRuntime& scene)
{
    if (!running) {
        return;
    }
    paused = false;
    RefreshMouseLookCapture();
    ApplyPlayerPose(scene);
}

void SectorGameSession::SetConsoleInputCaptured(bool captured)
{
    if (!running || consoleInputCaptured == captured) return;
    consoleInputCaptured = captured;
    RefreshMouseLookCapture();
}

void SectorGameSession::SetGodMode(bool enabled)
{
    godMode = enabled;
    if (enabled) {
        playerKnockbackVelocity = {};
        playerStunRemainingSeconds = 0.0f;
    }
}

void SectorGameSession::Update(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (running && IsLoading()) {
        UpdateLoading(context, scene, dt);
        return;
    }
    if (!running || paused) {
        return;
    }
    if (gameOver) return;
    playerStunRemainingSeconds = std::max(
            0.0f, playerStunRemainingSeconds - std::max(0.0f, dt));
    if (!consoleInputCaptured) {
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.key.key != KEY_I) return;
                    const ItemHeldUseInputDecision decision =
                            EvaluateItemHeldUseInput(
                                    heldObjectUse.phase,
                                    ItemHeldUseInput::ToggleInventory);
                    if (decision.effect == ItemHeldUseEffect::ReopenInventory) {
                        ClearHeldObjectUse();
                        SetInventoryOpen(true);
                    } else if (heldObjectUse.phase
                            == ItemHeldUsePhase::Inactive) {
                        SetInventoryOpen(!inventoryUi.open);
                    }
                    if (decision.consumeEvent
                            || heldObjectUse.phase == ItemHeldUsePhase::Inactive) {
                        engine::ConsumeEvent(event);
                    }
                });
    }
    if (itemMessage[0] != '\0') {
        itemMessageElapsedSeconds += std::max(0.0f, dt);
        if (itemMessageElapsedSeconds >= 2.25f) {
            itemMessage = {};
            itemMessageElapsedSeconds = 0.0f;
        }
    }

    context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key != KEY_F8) return;
                navigationDebug.visible = !navigationDebug.visible;
                engine::ConsumeEvent(event);
            });

    const Vector3 playerPosition =
            controller.fpsControllerState.feetPosition;
    const SectorFpsControllerConfig obstacleConfig =
            EffectiveSectorFpsControllerConfig(
                    controller.fpsControllerState,
                    controller.fpsControllerConfig);
    const SectorDoorPlayerObstacle playerObstacle{
            controller.fpsControllerState.feetPosition,
            obstacleConfig.playerRadius,
            obstacleConfig.playerHeight};
    struct ScriptTakeoverContext {
        engine::EngineContext* engine = nullptr;
        SectorScriptHost* host = nullptr;
    } takeoverContext{&context, &scriptHost};
    struct PlayerDamageAudioContext {
        engine::AssetManager* assets = nullptr;
        engine::AudioSystem* audio = nullptr;
        PlayerAudioRuntime* playerAudio = nullptr;
    } damageAudioContext{&context.assets, &context.audio, playerAudio};
    NpcAiGameplayContext npcGameplay;
    npcGameplay.playerFeetPosition = playerPosition;
    npcGameplay.playerEyePosition = SectorFpsControllerEyePosition(
            controller.fpsControllerState,
            controller.fpsControllerConfig);
    npcGameplay.playerHealth = &playerHealth;
    npcGameplay.playerKnockbackVelocity = &playerKnockbackVelocity;
    npcGameplay.playerStunRemainingSeconds = &playerStunRemainingSeconds;
    npcGameplay.scriptUserData = &takeoverContext;
    npcGameplay.interruptScriptMovement = [](
            void* userData, engine::Entity, const char* instanceId) {
        auto* takeover = static_cast<ScriptTakeoverContext*>(userData);
        if (takeover == nullptr || takeover->engine == nullptr
                || takeover->host == nullptr) return;
        InterruptSectorScriptNpcMoveForAi(
                *takeover->engine, *takeover->host, instanceId);
    };
    npcGameplay.playerDamageUserData = &damageAudioContext;
    npcGameplay.playerDamaged = [](void* userData, int appliedDamage) {
        auto* damageAudio = static_cast<PlayerDamageAudioContext*>(userData);
        if (appliedDamage <= 0 || damageAudio == nullptr
                || damageAudio->assets == nullptr
                || damageAudio->audio == nullptr
                || damageAudio->playerAudio == nullptr) {
            return;
        }
        PlayPlayerSound(
                *damageAudio->assets,
                *damageAudio->audio,
                *damageAudio->playerAudio,
                "pain");
    };
    npcGameplay.godMode = godMode;
    npcGameplay.frozen = aiFrozen;
    scene.Update(
            context,
            topologyMap,
            dt,
            &playerPosition,
            controller.fpsControllerState.currentSectorId,
            &playerObstacle,
            &npcGameplay);
    if (IsDepleted(playerHealth)) {
        gameOver = true;
        SetInventoryOpen(false);
        ClearHeldObjectUse();
        LeaveSectorFreeflyController();
        return;
    }
    ProcessInventoryAction(context, scene);
    const bool gameplayInputCaptured = consoleInputCaptured
            || inventoryUi.open
            || heldObjectUse.phase != ItemHeldUsePhase::Inactive;
    if (itemCampaign != nullptr) {
        UpdateItemHealingEffects(*itemCampaign, playerHealth, dt);
    }
    UpdateSectorScriptOperations(context, scriptHost);
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();

    SectorFpsControllerInput input;
    if (!gameplayInputCaptured) {
        input.moveForward = context.input.IsKeyDown(KEY_W);
        input.moveBackward = context.input.IsKeyDown(KEY_S);
        input.strafeLeft = context.input.IsKeyDown(KEY_A);
        input.strafeRight = context.input.IsKeyDown(KEY_D);
        input.run = context.input.IsKeyDown(KEY_LEFT_SHIFT)
                || context.input.IsKeyDown(KEY_RIGHT_SHIFT);
        input.mouseLookEnabled =
                AdvanceSectorFreeflyMouseLookCapture(
                        controller.freeflyController);
        if (input.mouseLookEnabled) {
            input.mouseDelta = context.input.MouseDelta();
        }
    }
    if (!gameplayInputCaptured) context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&input](engine::InputEvent& event) {
                if (event.key.key == KEY_SPACE) {
                    input.jumpPressed = true;
                } else if (event.key.key == KEY_LEFT_CONTROL
                        || event.key.key == KEY_RIGHT_CONTROL) {
                    input.crouchTogglePressed = true;
                } else {
                    return;
                }
                engine::ConsumeEvent(event);
            });

    if (applicationSettings != nullptr && itemCampaign != nullptr) {
        if (playerStunRemainingSeconds > 0.0f) {
            input.run = false;
            input.movementSpeedScale = 0.5f;
        }
        if (input.run && !CanPlayerStaminaSprint(playerStamina)) {
            input.run = false;
        }
        if (input.jumpPressed
                && !CanPlayerStaminaJump(
                        playerStamina,
                        applicationSettings->playerStamina)) {
            input.jumpPressed = false;
        }
    }

    const float previousVisualEyeY = scene.Renderer().RendererPose().position.y;
    input.externalHorizontalMovementDelta = Vector2Scale(
            playerKnockbackVelocity, std::max(0.0f, dt));
    UpdateSectorEditorGameplayPreview(
            objects.dynamicDoorColliders,
            objects.staticModelColliders,
            collision,
            controller,
            false,
            input,
            previousVisualEyeY,
            dt,
            &scene.NpcNavigation().collisionCylinders);
    playerKnockbackVelocity = Vector2Scale(
            playerKnockbackVelocity,
            std::exp(-8.0f * std::max(0.0f, dt)));
    if (Vector2Length(playerKnockbackVelocity) < 0.01f) {
        playerKnockbackVelocity = {};
    }
    if (applicationSettings != nullptr) {
        UpdatePlayerStamina(
                playerStamina,
                applicationSettings->playerStamina,
                controller.frameEvents.sprinting,
                controller.frameEvents.jumped,
                dt);
        const float staminaRatio = PlayerStaminaRatio(playerStamina);
        UpdatePlayerWindedCamera(
                windedCamera,
                applicationSettings->playerStamina.windedCamera,
                staminaRatio,
                dt);
        if (playerAudio != nullptr) {
            UpdatePlayerBreathingAudio(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    breathingAudio,
                    applicationSettings->playerStamina.breathingAudio,
                    staminaRatio,
                    dt);
        }
    }
    UpdateSectorScriptTriggers(
            scriptHost,
            Vector2{
                    controller.fpsControllerState.feetPosition.x,
                    controller.fpsControllerState.feetPosition.z},
            dt);
    usePromptTitle = {};
    if (heldObjectUse.phase == ItemHeldUsePhase::Targeting) {
        const Vector2 mouse = context.input.MousePosition();
        const bool mouseInside = mouse.x >= logicalViewport.x
                && mouse.y >= logicalViewport.y
                && mouse.x < logicalViewport.x + logicalViewport.width
                && mouse.y < logicalViewport.y + logicalViewport.height;
        if (mouseInside && logicalViewport.width >= 1.0f
                && logicalViewport.height >= 1.0f) {
            const Ray ray = GetScreenToWorldRayEx(
                    Vector2{
                            mouse.x - logicalViewport.x,
                            mouse.y - logicalViewport.y},
                    scene.Renderer().RenderCamera(),
                    std::max(1, static_cast<int>(
                            std::lround(logicalViewport.width))),
                    std::max(1, static_cast<int>(
                            std::lround(logicalViewport.height))));
            useTarget = FindSectorObjectUseTarget(
                    context.world,
                    context.assets,
                    ray,
                    collision.sectorCollisionWorldValid
                            ? &collision.sectorCollisionWorld : nullptr);
        } else {
            useTarget = {};
        }
        UpdateSectorUseHighlight(useHighlightState, useTarget, dt);
    } else if (heldObjectUse.phase == ItemHeldUsePhase::Inactive
            && !inventoryUi.open) {
        const SectorViewPose interactionPose = SectorFpsControllerPose(
                controller.fpsControllerState,
                controller.fpsControllerConfig);
        useTarget = FindSectorUseTarget(
                context.world,
                &context.assets,
                interactionPose.position,
                GameplayForward(controller),
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld : nullptr,
                true);
        UpdateSectorUseHighlight(useHighlightState, useTarget, dt);
        const std::string_view promptTitle = SectorUseTargetTitle(
                context.world, useTarget);
        if (!promptTitle.empty()) {
            std::snprintf(
                    usePromptTitle.data(),
                    usePromptTitle.size(),
                    "%.*s",
                    static_cast<int>(promptTitle.size()),
                    promptTitle.data());
        }
    } else {
        useTarget = {};
        ResetSectorUseHighlight(useHighlightState);
    }
    if (heldObjectUse.phase != ItemHeldUsePhase::Inactive) {
        context.input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this, &context](engine::InputEvent& event) {
                    ItemHeldUseInput heldInput;
                    if (event.mouseButton.button == MOUSE_BUTTON_RIGHT) {
                        heldInput = ItemHeldUseInput::RightClick;
                    } else if (event.mouseButton.button == MOUSE_BUTTON_LEFT) {
                        const bool valid =
                                (useTarget.kind == SectorUseTargetKind::StaticProp
                                        || useTarget.kind
                                                == SectorUseTargetKind::DynamicProp)
                                && !SectorObjectUseTargetInstanceId(
                                            context.world, useTarget).empty();
                        heldInput = valid
                                ? ItemHeldUseInput::ValidLeftClick
                                : ItemHeldUseInput::InvalidLeftClick;
                    } else {
                        return;
                    }
                    const ItemHeldUseInputDecision decision =
                            EvaluateItemHeldUseInput(
                                    heldObjectUse.phase, heldInput);
                    if (decision.effect
                            == ItemHeldUseEffect::CancelToGameplay) {
                        ClearHeldObjectUse();
                    } else if (decision.effect
                            == ItemHeldUseEffect::InvokeTarget) {
                        InvokeHeldObjectUse(context);
                    }
                    if (decision.consumeEvent) engine::ConsumeEvent(event);
                });
    }
    if (!gameplayInputCaptured) context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &context, &scene](engine::InputEvent& event) {
                if (event.key.key != KEY_E
                        || useTarget.kind == SectorUseTargetKind::None) {
                    return;
                }
                bool handled = false;
                if (useTarget.kind == SectorUseTargetKind::Item) {
                    handled = RequestItemTake(
                            context, scene, useTarget.entity);
                } else if (useTarget.kind == SectorUseTargetKind::Door) {
                    handled = RequestSectorScriptDoorUse(
                            context, scriptHost, useTarget.entity);
                } else if (useTarget.kind == SectorUseTargetKind::DynamicProp
                        && context.world.IsAlive(useTarget.entity)
                        && context.world.Has<SectorDynamicModel>(useTarget.entity)) {
                    SectorDynamicModel& prop =
                            context.world.Get<SectorDynamicModel>(useTarget.entity);
                    const engine::ScriptCallOutcome outcome =
                            engine::ScriptSystemCallForegroundHook(
                                    scripts, prop.onUseScript);
                    handled = outcome.result != engine::ScriptCallResult::ForegroundBusy
                            && outcome.result != engine::ScriptCallResult::AlreadyRunning;
                    if (handled && prop.singleUse
                            && (outcome.result == engine::ScriptCallResult::Completed
                                    || outcome.result == engine::ScriptCallResult::Started)) {
                        prop.useConsumed = true;
                    }
                    if (outcome.result == engine::ScriptCallResult::Missing) {
                        TraceLog(
                                LOG_WARNING,
                                "[Lua WARNING] prop '%s' has no callable use function '%s'",
                                prop.instanceId.c_str(),
                                prop.onUseScript.c_str());
                    } else if (outcome.result == engine::ScriptCallResult::Error) {
                        TraceLog(
                                LOG_ERROR,
                                "[Lua ERROR] prop '%s' use function '%s' failed: %s",
                                prop.instanceId.c_str(),
                                prop.onUseScript.c_str(),
                                outcome.error.c_str());
                    }
                }
                if (handled) engine::ConsumeEvent(event);
            });
    engine::ScriptSystemUpdate(context, scripts, dt);
    UpdatePendingItemTake(context, scene);
    UpdatePendingHeldObjectUse();
    UpdateItemPresentations(context, scene, dt);
    UpdateSectorScriptDoorPermission(context, scriptHost);
    if (scriptHost.dynamicLightsDirty) {
        scene.Renderer().RefreshDynamicLightSources(topologyMap);
        scriptHost.dynamicLightsDirty = false;
    }
    if (controller.frameEvents.footstep && applicationSettings != nullptr) {
        scene.PlayFootstepForSector(
                context,
                controller.fpsControllerState.currentSectorId,
                applicationSettings->footsteps.volume);
        scene.EmitPlayerSound(
                controller.fpsControllerState.feetPosition,
                applicationSettings->footsteps.noiseRadiusWorld);
    }
    if (playerAudio != nullptr) {
        if (controller.frameEvents.jumped) {
            PlayPlayerSound(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    "jump");
        }
        if (controller.frameEvents.landed) {
            PlayPlayerSound(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    "land");
        }
    }
    if (controller.frameEvents.landed && applicationSettings != nullptr) {
        scene.PlayFootstepForSector(
                context,
                controller.fpsControllerState.currentSectorId,
                std::clamp(
                        applicationSettings->footsteps.volume
                                * applicationSettings->footsteps
                                        .landingImpactVolumeMultiplier,
                        0.0f,
                        1.0f));
        scene.EmitPlayerSound(
                controller.fpsControllerState.feetPosition,
                applicationSettings->footsteps.landingNoiseRadiusWorld);
    }
    bool acceptedShot = false;
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        fpsPlayer.Update(
                context.assets,
                scene.Renderer(),
                *weaponRegistry,
                *applicationSettings,
                dt,
                nullptr,
                itemCampaign != nullptr ? &itemCampaign->weapons : nullptr,
                &context.audio);
    }
    ApplyPlayerPose(scene);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        acceptedShot = fpsPlayer.HandleInput(
                context.input,
                *weaponRegistry,
                context.assets,
                context.audio,
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr,
                scene.Renderer(),
                true,
                !gameplayInputCaptured,
                gameplayInputCaptured,
                itemRegistry,
                itemCampaign);
        if (fpsPlayer.ConsumeReloadOutOfAmmoRequest()) {
            ShowOutOfAmmo();
        }
        if (acceptedShot) {
            const FpsShotResult request = fpsPlayer.State().firing.lastShot;
            FpsShotResult resolvedShot;
            scene.ResolvePlayerWeaponShot(
                    context,
                    collision.sectorCollisionWorldValid
                            ? &collision.sectorCollisionWorld
                            : nullptr,
                    request.rayOrigin,
                    request.rayDirection,
                    fpsPlayer.State().firing.shotSequence,
                    fpsPlayer.State().firing.definition,
                    resolvedShot);
            fpsPlayer.RecordShotResolution(resolvedShot);
            scene.EmitPlayerSound(
                    request.rayOrigin,
                    fpsPlayer.State().firing.definition.noiseRadiusWorld);
            ApplyPlayerPose(scene);
        }
        fpsPlayer.UpdateTransformsAndLight(
                scene.Renderer(),
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr);
    }
    UpdateAudioListener(context.audio, scene.Renderer().RenderCamera());
    const SectorFpsControllerConfig visibilityConfig =
            NormalizeSectorFpsControllerConfig(
                    controller.fpsControllerConfig);
    scene.Renderer().UpdateVisibilityDebug(
            controller.fpsControllerState.currentSectorId,
            ClampRuntimeVisibilitySeedRadiusWorld(
                    visibilityConfig.playerRadius),
            true,
            &objects.dynamicPortalBlockers,
            &context.world);
    ConsumeScriptTransitionRequest(context, scene);
}

void SectorGameSession::UpdateLoading(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (loading.phase == GameLevelLoadPhase::Fading) {
        if (!AdvanceGameLevelLoadingFade(loading, dt)) return;
        std::string error;
        if (!ActivateLoadedMap(context, error)) {
            const std::string reason = error.empty()
                    ? "Map script activation failed" : error;
            Shutdown(context, scene);
            failureError = reason;
            return;
        }
        ActivateGameLevel(loading);
        return;
    }
    if (loading.phase != GameLevelLoadPhase::Loading) return;

    scene.UpdateLoadPreparation(context, topologyMap);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        fpsPlayer.Update(
                context.assets,
                scene.Renderer(),
                *weaponRegistry,
                *applicationSettings,
                0.0f,
                nullptr,
                itemCampaign != nullptr ? &itemCampaign->weapons : nullptr,
                &context.audio);
    }
    ApplyPlayerPose(scene);

    size_t finishedAssets = 0;
    size_t totalAssets = 0;
    scene.AccumulateLoadAssetProgress(
            context.assets, finishedAssets, totalAssets);
    const engine::AssetScopeHandle viewmodelScope =
            fpsPlayer.State().assetScope;
    if (!engine::IsNull(viewmodelScope)) {
        size_t finished = 0;
        size_t total = 0;
        context.assets.GetScopeProgressCounts(
                viewmodelScope, finished, total);
        finishedAssets += finished;
        totalAssets += total;
    }
    const float assetProgress = totalAssets == 0
            ? 1.0f
            : static_cast<float>(finishedAssets)
                    / static_cast<float>(totalAssets);

    const SectorNavigationState navigationState = scene.Navigation().State();
    const bool navigationTerminal = NavigationBuildTerminal(navigationState);
    const bool hasNpcs = HasRuntimeNpcs(scene);
    const GameLevelNavigationGate navigationGate =
            EvaluateGameLevelNavigationGate(
                    hasNpcs,
                    navigationTerminal,
                    navigationState == SectorNavigationState::Ready);
    if (navigationGate == GameLevelNavigationGate::Unavailable) {
        const std::string reason = "Level '" + levelName
                + "' cannot activate NPCs: "
                + NavigationLoadFailure(scene.Navigation());
        Shutdown(context, scene);
        failureError = reason;
        return;
    }

    const SectorRuntimeObjectState& objects = scene.RuntimeObjects();
    const bool assetsFinished = scene.AreLoadAssetScopesFinished(context.assets)
            && ScopeFinishedOrEmpty(context.assets, viewmodelScope);
    const bool runtimeObjectsFinished =
            objects.spriteAnimationPendingCount == 0
            && objects.staticModelPendingCount == 0;
    const bool viewmodelFinished = FpsViewmodelLoadFinished(
            fpsPlayer.State());
    const bool complete = assetsFinished
            && runtimeObjectsFinished
            && viewmodelFinished
            && navigationGate == GameLevelNavigationGate::Ready
            && InitialNavigationObstaclesSettled(scene.Navigation());
    UpdateGameLevelLoadingProgress(
            loading,
            assetProgress,
            NavigationLoadProgress(scene.Navigation()),
            complete);
    if (complete) BeginGameLevelLoadingFade(loading);
}

bool SectorGameSession::ActivateLoadedMap(
        engine::EngineContext& context,
        std::string& error)
{
    if (persistentScripts == nullptr) {
        error = "Persistent script store is unavailable";
        return false;
    }
    if (!engine::ScriptSystemCreateForMap(
                context,
                scripts,
                *persistentScripts,
                levelName,
                levelPath,
                ASSETS_PATH,
                &scriptHost,
                RegisterSectorScriptBindings,
                pendingLoadingSave,
                error)) {
        ResetSectorScriptHost(scriptHost);
        return false;
    }
    pendingLoadingSave = false;
    error.clear();
    return true;
}

void SectorGameSession::RenderViewmodel(
        engine::AssetManager& assets,
        SectorSceneRuntime& scene)
{
    if (!running || IsLoadScreenOpaque()) {
        return;
    }
    fpsPlayer.Render(
            assets,
            scene.Renderer(),
            topologyMap,
            scene.RuntimeObjects(),
            controller.fpsControllerState.currentSectorId);
}

void SectorGameSession::RenderHud(
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle usePromptFont,
        Rectangle playableViewport) const
{
    if (IsActive() && weaponRegistry != nullptr) {
        const bool showAmmo = itemCampaign != nullptr
                && itemRegistry != nullptr
                && !fpsPlayer.State().activeWeaponId.empty();
        const std::uint64_t reserveRounds = showAmmo
                ? CountInventoryAmmoForWeapon(
                        itemCampaign->inventory,
                        *itemRegistry,
                        fpsPlayer.State().activeWeaponId)
                : 0;
        fpsPlayer.RenderHud(
                playableViewport,
                *weaponRegistry,
                assets.GetFont(font),
                &playerHealth,
                &playerStamina,
                reserveRounds,
                showAmmo);
        if (itemMessage[0] != '\0') {
            DrawSectorUseMessage(
                    playableViewport,
                    assets.GetFont(usePromptFont),
                    itemMessage.data(),
                    itemMessageElapsedSeconds);
        } else if (!inventoryUi.open
                && heldObjectUse.phase == ItemHeldUsePhase::Inactive) {
            DrawSectorUsePrompt(
                    playableViewport,
                    assets.GetFont(usePromptFont),
                    usePromptTitle.data(),
                    useTarget.kind == SectorUseTargetKind::Item
                            ? "Take" : "Use");
        }
    }
}

void SectorGameSession::RenderNavigationDebugWorld(
        const SectorSceneRuntime& scene) const
{
    if (!IsActive() || !navigationDebug.visible) return;
    DrawSectorNavigationDebugWorld(
            navigationDebug.drawSettings,
            scene.Navigation(),
            scene.NpcNavigation(),
            scene.Renderer());
}

void SectorGameSession::RenderNavigationDebugPanel(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont,
        const SectorSceneRuntime& scene) const
{
    if (!IsActive() || !navigationDebug.visible) return;
    DrawSectorGameNavigationDebugPanel(
            config,
            assets,
            smallFont,
            scene.Navigation(),
            scene.NpcNavigation(),
            scriptHost);
}

bool SectorGameSession::RebuildFromMap(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorTopologyMap& map,
        std::string& error)
{
    if (!running) {
        error = "No game session is running";
        return false;
    }
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    pendingItemTake = {};
    heldObjectUse = {};
    inventoryUi.open = false;
    const SectorFpsControllerState savedPlayer = controller.fpsControllerState;
    SectorTopologyMap reconciledMap = map;
    if (itemCampaign != nullptr) {
        ReconcileItemCampaignLevel(
                *itemCampaign, levelName, reconciledMap);
    }
    if (applicationSettings != nullptr) {
        ItemLevelCampaignState& campaignLevel =
                FindOrCreateItemLevelCampaignState(
                        *itemCampaign,
                        levelName,
                        reconciledMap.runtimeObjects.size());
        const std::size_t extra = static_cast<std::size_t>(std::max(
                1, applicationSettings->playerInventory.maxSlots));
        campaignLevel.droppedItems.reserve(
                campaignLevel.droppedItems.size() + extra);
        reconciledMap.runtimeObjects.reserve(
                reconciledMap.runtimeObjects.size() + extra);
    }
    if (!scene.Rebuild(
            context,
            reconciledMap,
                "sector_game",
                applicationSettings != nullptr
                        ? applicationSettings->footsteps.defaultSet
                        : FootstepApplicationSettings{}.defaultSet,
                applicationSettings != nullptr
                        ? applicationSettings->footsteps.volume
                        : FootstepApplicationSettings{}.volume,
                error)) {
        return false;
    }
    topologyMap = std::move(reconciledMap);
    scene.RuntimeObjects().placedObjectEntities.reserve(
            topologyMap.runtimeObjects.capacity());
    completedItemPresentations.clear();
    completedItemPresentations.reserve(topologyMap.runtimeObjects.capacity());
    ResetFpsCameraRecoil(fpsPlayer.State().firing.cameraRecoil);
    controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    controller.fpsControllerState = savedPlayer;
    if (!BuildCollisionAndPlayer(scene, false, nullptr, &error)) {
        error = collision.sectorCollisionWorldWarning.empty()
                ? "Could not rebuild the game collision world"
                : collision.sectorCollisionWorldWarning;
        return false;
    }
    ApplyPlayerPose(scene);
    if (persistentScripts == nullptr) {
        error = "Persistent script store is unavailable";
        return false;
    }
    InitializeSectorScriptHost(
            scriptHost,
            scene.RuntimeObjects(),
            topologyMap,
            scripts,
            &scene.Navigation(),
            &scene.NpcNavigation(),
            MakeSectorScriptAudioApi(scene),
            &playerHealth);
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    pendingLoadingSave = false;
    BeginGameLevelLoading(loading);
    error.clear();
    return true;
}

bool SectorGameSession::ReloadCurrentMap(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        bool remainPaused,
        std::string& error)
{
    if (!running) {
        error = "No game session is running";
        return false;
    }
    const std::string mapId = levelName;
    const FpsWeaponRegistry* savedWeaponRegistry = weaponRegistry;
    const ItemRegistry* savedItemRegistry = itemRegistry;
    const ItemModelAssetState* savedItemModelAssets = itemModelAssets;
    ItemCampaignState* savedItemCampaign = itemCampaign;
    const SectorMaterialRegistry* savedMaterialRegistry = materialRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    const Health savedHealth = playerHealth;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedItemRegistry == nullptr
            || savedItemModelAssets == nullptr
            || savedItemCampaign == nullptr || savedMaterialRegistry == nullptr
            || savedSettings == nullptr
            || savedPlayerAudio == nullptr || savedPersistent == nullptr) {
        error = "Game services became unavailable during reload";
        return false;
    }
    if (!StartNew(
                context,
                scene,
                SectorLevelEntryRequest{mapId, std::nullopt},
                *savedMaterialRegistry,
                *savedWeaponRegistry,
                *savedItemRegistry,
                *savedItemModelAssets,
                *savedItemCampaign,
                *savedSettings,
                *savedPlayerAudio,
                *savedPersistent,
                false,
                error)) {
        return false;
    }
    if (remainPaused) Pause();
    playerHealth = savedHealth;
    error.clear();
    return true;
}

std::string SectorGameSession::TakeFailureError()
{
    std::string result = std::move(failureError);
    failureError.clear();
    return result;
}

void SectorGameSession::ConsumeScriptTransitionRequest(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (scripts.mapAbortRequested) {
        const std::string reason = scripts.mapAbortError.empty()
                ? "Map init() failed" : scripts.mapAbortError;
        Shutdown(context, scene);
        failureError = reason;
        return;
    }
    if (!scripts.mapChangeRequested) return;

    const std::string requestedMap = std::move(scripts.requestedMapId);
    const std::string requestedSpawn = std::move(scripts.requestedSpawnId);
    scripts.mapChangeRequested = false;
    scripts.requestedMapId.clear();
    scripts.requestedSpawnId.clear();

    const FpsWeaponRegistry* savedWeaponRegistry = weaponRegistry;
    const ItemRegistry* savedItemRegistry = itemRegistry;
    const ItemModelAssetState* savedItemModelAssets = itemModelAssets;
    ItemCampaignState* savedItemCampaign = itemCampaign;
    const SectorMaterialRegistry* savedMaterialRegistry = materialRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    const Health savedHealth = playerHealth;
    const PlayerStamina savedStamina = playerStamina;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedItemRegistry == nullptr
            || savedItemModelAssets == nullptr
            || savedItemCampaign == nullptr || savedMaterialRegistry == nullptr
            || savedSettings == nullptr
            || savedPlayerAudio == nullptr || savedPersistent == nullptr) {
        failureError = "Map change failed because session services are unavailable";
        return;
    }

    std::string error;
    const SectorLevelEntryRequest entry{
            requestedMap,
            requestedSpawn.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{requestedSpawn}};
    if (!StartNew(
                context,
                scene,
                entry,
                *savedMaterialRegistry,
                *savedWeaponRegistry,
                *savedItemRegistry,
                *savedItemModelAssets,
                *savedItemCampaign,
                *savedSettings,
                *savedPlayerAudio,
                *savedPersistent,
                false,
                error)) {
        failureError = "Map change to '" + requestedMap + "' failed: "
                + (error.empty() ? "unknown error" : error);
    } else {
        playerHealth = savedHealth;
        playerStamina = savedStamina;
    }
}

bool SectorGameSession::BuildCollisionAndPlayer(
        SectorSceneRuntime& scene,
        bool initializePlayer,
        const SectorCompiledLevelMarker* entryMarker,
        std::string* error)
{
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();
    if (!RebuildSectorEditorCollisionWorld(
                topologyMap,
                collision,
                controller,
                objects.staticModelColliders)) {
        return false;
    }
    if (initializePlayer) {
        if (entryMarker != nullptr) {
            controller.fpsControllerState = SectorFpsControllerState{};
            controller.fpsControllerState.feetPosition =
                    SectorAuthoringToWorldPosition(entryMarker->position);
            controller.fpsControllerState.yawRadians = entryMarker->yawRadians;
            controller.fpsControllerState.pitchRadians = 0.0f;
        } else {
            controller.fpsControllerState = SectorFpsControllerStateFromCameraPose(
                    scene.Renderer().RendererPose(),
                    controller.fpsControllerConfig);
        }
    }
    controller.previewControlMode = SectorPreviewControlMode::Gameplay;
    InitializeSectorEditorGameplayVerticalState(
            collision,
            controller,
            objects.staticModelColliders);
    if (entryMarker != nullptr
            && (controller.fpsControllerState.currentSectorId == 0
                    || !collision.previewVerticalResult.hasSector
                    || collision.previewVerticalResult.cannotFit)) {
        if (error != nullptr) {
            *error = "Level entry marker '" + entryMarker->id
                    + "' is not a valid player position";
        }
        return false;
    }
    ApplyPlayerPose(scene);
    return true;
}

void SectorGameSession::ApplyPlayerPose(SectorSceneRuntime& scene)
{
    const SectorViewPose basePose = SectorFpsControllerVisualPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig,
            controller.visualStepOffsetY,
            controller.headBobState.offset,
            controller.landingDipState.offsetY);
    SectorViewPose windedPose = basePose;
    windedPose.position.y += windedCamera.verticalOffsetWorld;
    windedPose.pitchRadians = ClampSectorFpsPitch(
            windedPose.pitchRadians
                    + windedCamera.pitchOffsetDegrees * DEG2RAD);
    scene.Renderer().ApplyRendererPose(ApplySectorFpsViewRotationOffset(
            windedPose,
            fpsPlayer.State().firing.cameraRecoil.rotationDegrees),
            false);
    controller.freeflyController.pose = basePose;
}

} // namespace game
