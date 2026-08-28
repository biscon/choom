#include "game/save/GameSaveSerialization.h"
#include "game/save/GameSaveStorage.h"

#include "util/json.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void RequireFinite(float value, const std::string& name)
{
    Require(std::isfinite(value), name + " must be finite");
}

Json Vec2(Vector2 value) { return Json::array({value.x, value.y}); }
Json Vec3(Vector3 value) { return Json::array({value.x, value.y, value.z}); }
Json ColorJson(Color value)
{
    return Json::array({value.r, value.g, value.b, value.a});
}

Vector2 ReadVec2(const Json& value, const std::string& name)
{
    Require(value.is_array() && value.size() == 2, name + " must be a two-number array");
    Vector2 result{value[0].get<float>(), value[1].get<float>()};
    RequireFinite(result.x, name + ".x");
    RequireFinite(result.y, name + ".y");
    return result;
}

Vector3 ReadVec3(const Json& value, const std::string& name)
{
    Require(value.is_array() && value.size() == 3, name + " must be a three-number array");
    Vector3 result{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
    RequireFinite(result.x, name + ".x");
    RequireFinite(result.y, name + ".y");
    RequireFinite(result.z, name + ".z");
    return result;
}

Color ReadColor(const Json& value, const std::string& name)
{
    Require(value.is_array() && value.size() == 4, name + " must be a four-channel array");
    Color result{};
    unsigned int channels[4]{};
    for (std::size_t i = 0; i < 4; ++i) {
        Require(value[i].is_number_unsigned() || value[i].is_number_integer(),
                name + " channels must be integers");
        const int raw = value[i].get<int>();
        Require(raw >= 0 && raw <= 255, name + " channels must be between 0 and 255");
        channels[i] = static_cast<unsigned int>(raw);
    }
    result.r = static_cast<unsigned char>(channels[0]);
    result.g = static_cast<unsigned char>(channels[1]);
    result.b = static_cast<unsigned char>(channels[2]);
    result.a = static_cast<unsigned char>(channels[3]);
    return result;
}

Json HealthJson(const Health& health)
{
    return Json{{"baseMaximum", health.baseMaximum}, {"maximum", health.maximum},
            {"current", health.current}};
}

Health ReadHealth(const Json& value, const std::string& name)
{
    Require(value.is_object(), name + " must be an object");
    Health result{
            value.at("baseMaximum").get<int>(),
            value.at("maximum").get<int>(),
            value.at("current").get<int>()};
    Require(result.baseMaximum > 0 && result.maximum > 0,
            name + " maximum values must be positive");
    Require(result.current >= 0 && result.current <= result.maximum,
            name + ".current is outside its valid range");
    return result;
}

Json AnimatorJson(const GameSaveAnimatorState& state)
{
    return Json{
            {"animation", state.animationName},
            {"targetAnimation", state.targetAnimationName},
            {"frame", state.frame},
            {"targetFrame", state.targetFrame},
            {"speed", state.speed},
            {"transitionDurationSeconds", state.transitionDurationSeconds},
            {"transitionElapsedSeconds", state.transitionElapsedSeconds},
            {"playing", state.playing}, {"loop", state.loop},
            {"reverse", state.reverse}, {"finished", state.finished},
            {"paused", state.paused}, {"targetLoop", state.targetLoop},
            {"targetFinished", state.targetFinished}};
}

GameSaveAnimatorState ReadAnimator(const Json& value, const std::string& name)
{
    Require(value.is_object(), name + " must be an object");
    GameSaveAnimatorState result;
    result.animationName = value.value("animation", std::string{});
    result.targetAnimationName = value.value("targetAnimation", std::string{});
    result.frame = value.value("frame", 0.0f);
    result.targetFrame = value.value("targetFrame", 0.0f);
    result.speed = value.value("speed", 1.0f);
    result.transitionDurationSeconds = value.value("transitionDurationSeconds", 0.0f);
    result.transitionElapsedSeconds = value.value("transitionElapsedSeconds", 0.0f);
    result.playing = value.value("playing", true);
    result.loop = value.value("loop", true);
    result.reverse = value.value("reverse", false);
    result.finished = value.value("finished", false);
    result.paused = value.value("paused", false);
    result.targetLoop = value.value("targetLoop", true);
    result.targetFinished = value.value("targetFinished", false);
    RequireFinite(result.frame, name + ".frame");
    RequireFinite(result.targetFrame, name + ".targetFrame");
    RequireFinite(result.speed, name + ".speed");
    RequireFinite(result.transitionDurationSeconds, name + ".transitionDurationSeconds");
    RequireFinite(result.transitionElapsedSeconds, name + ".transitionElapsedSeconds");
    Require(result.frame >= 0.0f && result.targetFrame >= 0.0f
                    && result.transitionDurationSeconds >= 0.0f
                    && result.transitionElapsedSeconds >= 0.0f,
            name + " contains negative timing values");
    return result;
}

Json InventoryEntryJson(const ItemInventoryEntry& entry)
{
    return Json{{"runtimeId", entry.runtimeId}, {"definitionId", entry.definitionId},
            {"quantity", entry.quantity}, {"onUseScript", entry.onUseScript},
            {"slotIndex", entry.slotIndex}};
}

ItemInventoryEntry ReadInventoryEntry(const Json& value)
{
    ItemInventoryEntry result;
    result.runtimeId = value.at("runtimeId").get<std::uint64_t>();
    result.definitionId = value.at("definitionId").get<std::string>();
    result.quantity = value.at("quantity").get<std::uint64_t>();
    result.onUseScript = value.value("onUseScript", std::string{});
    result.slotIndex = value.at("slotIndex").get<int>();
    Require(result.runtimeId != 0 && !result.definitionId.empty() && result.quantity != 0,
            "inventory entry has invalid identity or quantity");
    return result;
}

Json DroppedItemJson(const SectorPlacedRuntimeObject& object)
{
    return Json{
            {"id", object.id}, {"position", Vec3(object.position)},
            {"yawRadians", object.yawRadians},
            {"definitionId", object.item.definitionId},
            {"instanceId", object.item.instanceId},
            {"quantity", object.item.quantity},
            {"takeDistance", object.item.takeDistance},
            {"onTakeScript", object.item.onTakeScript},
            {"onUseScript", object.item.onUseScript},
            {"rotationXRadians", object.item.rotationXRadians},
            {"rotationZRadians", object.item.rotationZRadians},
            {"heightOffsetWorld", object.item.heightOffsetWorld},
            {"scale", object.item.scale},
            {"shadowMode", static_cast<int>(object.item.shadowMode)}};
}

SectorPlacedRuntimeObject ReadDroppedItem(const Json& value)
{
    Require(value.is_object(), "dropped item must be an object");
    SectorPlacedRuntimeObject result;
    result.id = value.at("id").get<int>();
    result.kind = "item";
    result.position = ReadVec3(value.at("position"), "dropped item.position");
    result.yawRadians = value.value("yawRadians", 0.0f);
    result.item.definitionId = value.at("definitionId").get<std::string>();
    result.item.instanceId = value.at("instanceId").get<std::string>();
    result.item.quantity = value.at("quantity").get<int>();
    result.item.takeDistance = value.value("takeDistance", 1.5f);
    result.item.onTakeScript = value.value("onTakeScript", std::string{});
    result.item.onUseScript = value.value("onUseScript", std::string{});
    result.item.rotationXRadians = value.value("rotationXRadians", 0.0f);
    result.item.rotationZRadians = value.value("rotationZRadians", 0.0f);
    result.item.heightOffsetWorld = value.value("heightOffsetWorld", 0.0f);
    result.item.scale = value.value("scale", 1.0f);
    const int shadow = value.value("shadowMode", 1);
    Require(shadow >= 0 && shadow <= 2, "dropped item.shadowMode is invalid");
    result.item.shadowMode = static_cast<SectorDynamicModelShadowMode>(shadow);
    result.item.sessionDrop = true;
    Require(result.id > 0 && !result.item.definitionId.empty()
                    && !result.item.instanceId.empty() && result.item.quantity > 0,
            "dropped item has invalid required fields");
    RequireFinite(result.yawRadians, "dropped item.yawRadians");
    RequireFinite(result.item.takeDistance, "dropped item.takeDistance");
    RequireFinite(result.item.scale, "dropped item.scale");
    return result;
}

Json ItemCampaignJson(const ItemCampaignState& state)
{
    Json root;
    root["inventory"] = Json{{"nextRuntimeId", state.inventory.nextRuntimeId},
            {"capacityWarnings", state.inventory.capacityWarnings},
            {"entries", Json::array()}};
    for (const ItemInventoryEntry& entry : state.inventory.entries) {
        root["inventory"]["entries"].push_back(InventoryEntryJson(entry));
    }
    root["weapons"] = Json{{"activeWeaponId", state.weapons.activeWeaponId},
            {"capacityWarnings", state.weapons.capacityWarnings},
            {"magazines", Json::array()}};
    for (const PlayerWeaponMagazineState& magazine : state.weapons.magazines) {
        root["weapons"]["magazines"].push_back(
                Json{{"weaponId", magazine.weaponId}, {"loadedRounds", magazine.loadedRounds}});
    }
    root["healingEffects"] = Json::array();
    for (const ItemHealingEffect& effect : state.healingEffects) {
        root["healingEffects"].push_back(Json{{"totalAmount", effect.totalAmount},
                {"durationSeconds", effect.durationSeconds},
                {"elapsedSeconds", effect.elapsedSeconds},
                {"appliedAmount", effect.appliedAmount}});
    }
    root["levels"] = Json::array();
    for (const ItemLevelCampaignState& level : state.levels) {
        Json jsonLevel{{"levelId", level.levelId},
                {"collectedAuthoredItemIds", level.collectedAuthoredItemIds},
                {"nextDroppedObjectId", level.nextDroppedObjectId},
                {"capacityWarnings", level.capacityWarnings},
                {"droppedItems", Json::array()}};
        for (const SectorPlacedRuntimeObject& drop : level.droppedItems) {
            jsonLevel["droppedItems"].push_back(DroppedItemJson(drop));
        }
        root["levels"].push_back(std::move(jsonLevel));
    }
    root["capacityWarnings"] = state.capacityWarnings;
    return root;
}

ItemCampaignState ReadItemCampaign(const Json& root)
{
    Require(root.is_object(), "itemCampaign must be an object");
    ItemCampaignState state;
    const Json& inventory = root.at("inventory");
    state.inventory.nextRuntimeId = inventory.at("nextRuntimeId").get<std::uint64_t>();
    state.inventory.capacityWarnings = inventory.value("capacityWarnings", std::uint64_t{0});
    std::set<std::uint64_t> runtimeIds;
    for (const Json& entry : inventory.at("entries")) {
        ItemInventoryEntry parsed = ReadInventoryEntry(entry);
        Require(runtimeIds.insert(parsed.runtimeId).second, "duplicate inventory runtime ID");
        state.inventory.entries.push_back(std::move(parsed));
    }
    Require(state.inventory.nextRuntimeId != 0, "inventory.nextRuntimeId must be positive");
    const Json& weapons = root.at("weapons");
    state.weapons.activeWeaponId = weapons.value("activeWeaponId", std::string{});
    state.weapons.capacityWarnings = weapons.value("capacityWarnings", std::uint64_t{0});
    std::set<std::string> weaponIds;
    for (const Json& magazine : weapons.at("magazines")) {
        PlayerWeaponMagazineState parsed{
                magazine.at("weaponId").get<std::string>(),
                magazine.at("loadedRounds").get<int>()};
        Require(!parsed.weaponId.empty() && parsed.loadedRounds >= 0,
                "weapon magazine is invalid");
        Require(weaponIds.insert(parsed.weaponId).second, "duplicate weapon magazine");
        state.weapons.magazines.push_back(std::move(parsed));
    }
    for (const Json& effect : root.at("healingEffects")) {
        ItemHealingEffect parsed{
                effect.at("totalAmount").get<int>(),
                effect.at("durationSeconds").get<float>(),
                effect.at("elapsedSeconds").get<float>(),
                effect.at("appliedAmount").get<int>()};
        Require(parsed.totalAmount >= 0 && parsed.appliedAmount >= 0
                        && parsed.appliedAmount <= parsed.totalAmount,
                "healing effect amounts are invalid");
        RequireFinite(parsed.durationSeconds, "healing effect.durationSeconds");
        RequireFinite(parsed.elapsedSeconds, "healing effect.elapsedSeconds");
        Require(parsed.durationSeconds >= 0.0f && parsed.elapsedSeconds >= 0.0f,
                "healing effect timing is invalid");
        state.healingEffects.push_back(parsed);
    }
    std::set<std::string> levelIds;
    for (const Json& level : root.at("levels")) {
        ItemLevelCampaignState parsed;
        parsed.levelId = level.at("levelId").get<std::string>();
        Require(!parsed.levelId.empty() && levelIds.insert(parsed.levelId).second,
                "duplicate or empty item campaign level ID");
        parsed.collectedAuthoredItemIds =
                level.at("collectedAuthoredItemIds").get<std::vector<int>>();
        Require(std::all_of(parsed.collectedAuthoredItemIds.begin(),
                            parsed.collectedAuthoredItemIds.end(),
                            [](int id) { return id > 0; }),
                "collected item ID is invalid");
        std::sort(parsed.collectedAuthoredItemIds.begin(),
                parsed.collectedAuthoredItemIds.end());
        Require(std::adjacent_find(parsed.collectedAuthoredItemIds.begin(),
                            parsed.collectedAuthoredItemIds.end())
                        == parsed.collectedAuthoredItemIds.end(),
                "duplicate collected item ID");
        parsed.nextDroppedObjectId = level.at("nextDroppedObjectId").get<int>();
        parsed.capacityWarnings = level.value("capacityWarnings", std::uint64_t{0});
        std::set<int> dropIds;
        for (const Json& drop : level.at("droppedItems")) {
            SectorPlacedRuntimeObject object = ReadDroppedItem(drop);
            Require(dropIds.insert(object.id).second, "duplicate dropped item ID");
            parsed.droppedItems.push_back(std::move(object));
        }
        Require(parsed.nextDroppedObjectId > 0, "nextDroppedObjectId must be positive");
        state.levels.push_back(std::move(parsed));
    }
    state.capacityWarnings = root.value("capacityWarnings", std::uint64_t{0});
    return state;
}

Json PersistentJson(const engine::PersistentScriptStore& store)
{
    Json result{{"bools", Json::object()}, {"ints", Json::object()},
            {"strings", Json::object()}};
    std::map<std::string, bool> bools(store.bools.begin(), store.bools.end());
    std::map<std::string, std::int64_t> ints(store.ints.begin(), store.ints.end());
    std::map<std::string, std::string> strings(store.strings.begin(), store.strings.end());
    for (const auto& entry : bools) result["bools"][entry.first] = entry.second;
    for (const auto& entry : ints) result["ints"][entry.first] = entry.second;
    for (const auto& entry : strings) result["strings"][entry.first] = entry.second;
    return result;
}

engine::PersistentScriptStore ReadPersistent(const Json& root)
{
    Require(root.is_object(), "persistentScripts must be an object");
    engine::PersistentScriptStore result;
    for (const auto& entry : root.at("bools").items()) {
        Require(!entry.key().empty() && entry.value().is_boolean(), "invalid persistent bool");
        result.bools.emplace(entry.key(), entry.value().get<bool>());
    }
    for (const auto& entry : root.at("ints").items()) {
        Require(!entry.key().empty() && entry.value().is_number_integer(), "invalid persistent int");
        result.ints.emplace(entry.key(), entry.value().get<std::int64_t>());
    }
    for (const auto& entry : root.at("strings").items()) {
        Require(!entry.key().empty() && entry.value().is_string(), "invalid persistent string");
        result.strings.emplace(entry.key(), entry.value().get<std::string>());
    }
    return result;
}

Json LevelJson(const GameSaveLevelState& level)
{
    Json root{{"levelId", level.levelId}, {"doors", Json::array()},
            {"props", Json::array()}, {"npcs", Json::array()},
            {"billboards", Json::array()}, {"dynamicLights", Json::array()},
            {"triggers", Json::array()}};
    for (const GameSaveDoorState& value : level.doors) {
        root["doors"].push_back(Json{{"placedObjectId", value.placedObjectId},
                {"instanceId", value.instanceId}, {"openFraction", value.openFraction},
                {"targetOpenFraction", value.targetOpenFraction}, {"enabled", value.enabled}});
    }
    for (const GameSavePropState& value : level.props) {
        Json json{{"placedObjectId", value.placedObjectId}, {"instanceId", value.instanceId},
                {"emissiveScale", value.emissiveScale}, {"opacity", value.opacity},
                {"useConsumed", value.useConsumed}, {"hasAnimator", value.hasAnimator}};
        if (value.hasAnimator) json["animator"] = AnimatorJson(value.animator);
        root["props"].push_back(std::move(json));
    }
    for (const GameSaveNpcState& value : level.npcs) {
        Json json{{"placedObjectId", value.placedObjectId}, {"instanceId", value.instanceId},
                {"position", Vec3(value.position)}, {"yawRadians", value.yawRadians},
                {"health", HealthJson(value.health)}, {"dead", value.dead},
                {"deathAnimationComplete", value.deathAnimationComplete},
                {"despawned", value.despawned},
                {"corpseElapsedSeconds", value.corpseElapsedSeconds},
                {"opacity", value.opacity}, {"hasAnimator", value.hasAnimator},
                {"hasPatrol", value.hasPatrol}};
        if (value.hasAnimator) json["animator"] = AnimatorJson(value.animator);
        if (value.hasPatrol) {
            json["patrol"] = Json{{"patrolEditorId", value.patrolEditorId},
                    {"waypointIndex", value.waypointIndex}, {"direction", value.direction},
                    {"shuffleOrder", value.shuffleOrder}, {"shuffleCursor", value.shuffleCursor},
                    {"randomState", value.randomState}, {"phase", value.phase},
                    {"resumePhase", value.resumePhase},
                    {"waitRemainingSeconds", value.waitRemainingSeconds},
                    {"waypointBaseYawRadians", value.waypointBaseYawRadians},
                    {"lookOffsetRadians", value.lookOffsetRadians},
                    {"lookDirection", value.lookDirection},
                    {"retryRemainingSeconds", value.retryRemainingSeconds},
                    {"destinationXZ", Vec2(value.destinationXZ)},
                    {"stoppedByScript", value.stoppedByScript},
                    {"destinationInitialized", value.destinationInitialized}};
        }
        root["npcs"].push_back(std::move(json));
    }
    for (const GameSaveBillboardState& value : level.billboards) {
        root["billboards"].push_back(Json{{"placedObjectId", value.placedObjectId},
                {"timeSeconds", value.timeSeconds}, {"speed", value.speed},
                {"playing", value.playing}, {"loop", value.loop}, {"finished", value.finished}});
    }
    for (const GameSaveDynamicLightState& value : level.dynamicLights) {
        root["dynamicLights"].push_back(Json{{"instanceId", value.instanceId},
                {"color", ColorJson(value.color)}, {"intensity", value.intensity},
                {"enabled", value.enabled}});
    }
    for (const GameSaveTriggerState& value : level.triggers) {
        root["triggers"].push_back(Json{{"id", value.id}, {"enabled", value.enabled},
                {"inside", value.inside}, {"pending", value.pending},
                {"consumed", value.consumed},
                {"remainingDelayMilliseconds", value.remainingDelayMilliseconds}});
    }
    return root;
}

GameSaveLevelState ReadLevel(const Json& root)
{
    Require(root.is_object(), "level state must be an object");
    GameSaveLevelState level;
    level.levelId = root.at("levelId").get<std::string>();
    Require(!level.levelId.empty(), "level state ID must not be empty");
    std::set<int> objectIds;
    for (const Json& value : root.at("doors")) {
        GameSaveDoorState state{value.at("placedObjectId").get<int>(),
                value.value("instanceId", std::string{}),
                value.at("openFraction").get<float>(),
                value.at("targetOpenFraction").get<float>(),
                value.value("enabled", true)};
        Require(state.placedObjectId > 0
                        && objectIds.insert(state.placedObjectId).second,
                "duplicate or invalid saved door ID");
        RequireFinite(state.openFraction, "door.openFraction");
        RequireFinite(state.targetOpenFraction, "door.targetOpenFraction");
        Require(state.openFraction >= 0.0f && state.openFraction <= 1.0f
                        && state.targetOpenFraction >= 0.0f
                        && state.targetOpenFraction <= 1.0f,
                "door fractions must be between zero and one");
        level.doors.push_back(std::move(state));
    }
    for (const Json& value : root.at("props")) {
        GameSavePropState state;
        state.placedObjectId = value.at("placedObjectId").get<int>();
        state.instanceId = value.value("instanceId", std::string{});
        state.emissiveScale = value.value("emissiveScale", 1.0f);
        state.opacity = value.value("opacity", 1.0f);
        state.useConsumed = value.value("useConsumed", false);
        state.hasAnimator = value.value("hasAnimator", false);
        if (state.hasAnimator) state.animator = ReadAnimator(value.at("animator"), "prop.animator");
        Require(state.placedObjectId > 0
                        && objectIds.insert(state.placedObjectId).second,
                "duplicate or invalid saved prop ID");
        RequireFinite(state.emissiveScale, "prop.emissiveScale");
        RequireFinite(state.opacity, "prop.opacity");
        Require(state.emissiveScale >= 0.0f && state.opacity >= 0.0f && state.opacity <= 1.0f,
                "prop values are outside their valid range");
        level.props.push_back(std::move(state));
    }
    for (const Json& value : root.at("npcs")) {
        GameSaveNpcState state;
        state.placedObjectId = value.at("placedObjectId").get<int>();
        state.instanceId = value.at("instanceId").get<std::string>();
        state.position = ReadVec3(value.at("position"), "npc.position");
        state.yawRadians = value.at("yawRadians").get<float>();
        state.health = ReadHealth(value.at("health"), "npc.health");
        state.dead = value.value("dead", false);
        state.deathAnimationComplete = value.value(
                "deathAnimationComplete", false);
        state.despawned = value.value("despawned", false);
        state.corpseElapsedSeconds = value.value("corpseElapsedSeconds", 0.0f);
        state.opacity = value.value("opacity", 1.0f);
        state.hasAnimator = value.value("hasAnimator", false);
        if (state.hasAnimator) state.animator = ReadAnimator(value.at("animator"), "npc.animator");
        state.hasPatrol = value.value("hasPatrol", false);
        if (state.hasPatrol) {
            const Json& patrol = value.at("patrol");
            state.patrolEditorId = patrol.at("patrolEditorId").get<int>();
            state.waypointIndex = patrol.at("waypointIndex").get<std::size_t>();
            state.direction = patrol.at("direction").get<int>();
            state.shuffleOrder = patrol.at("shuffleOrder").get<std::vector<std::size_t>>();
            state.shuffleCursor = patrol.at("shuffleCursor").get<std::size_t>();
            state.randomState = patrol.at("randomState").get<std::uint32_t>();
            state.phase = patrol.at("phase").get<int>();
            state.resumePhase = patrol.at("resumePhase").get<int>();
            state.waitRemainingSeconds = patrol.value("waitRemainingSeconds", 0.0f);
            state.waypointBaseYawRadians = patrol.value("waypointBaseYawRadians", 0.0f);
            state.lookOffsetRadians = patrol.value("lookOffsetRadians", 0.0f);
            state.lookDirection = patrol.value("lookDirection", 1.0f);
            state.retryRemainingSeconds = patrol.value("retryRemainingSeconds", 0.0f);
            state.destinationXZ = ReadVec2(patrol.at("destinationXZ"), "npc.patrol.destinationXZ");
            state.stoppedByScript = patrol.value("stoppedByScript", false);
            state.destinationInitialized = patrol.value("destinationInitialized", false);
            Require(state.patrolEditorId > 0 && (state.direction == -1 || state.direction == 1),
                    "npc patrol identity or direction is invalid");
            Require(state.shuffleCursor <= state.shuffleOrder.size(),
                    "npc patrol shuffle cursor is invalid");
        }
        Require(state.placedObjectId > 0 && !state.instanceId.empty()
                        && objectIds.insert(state.placedObjectId).second,
                "duplicate or invalid saved NPC identity");
        RequireFinite(state.yawRadians, "npc.yawRadians");
        RequireFinite(state.corpseElapsedSeconds, "npc.corpseElapsedSeconds");
        RequireFinite(state.opacity, "npc.opacity");
        Require(state.corpseElapsedSeconds >= 0.0f && state.opacity >= 0.0f
                        && state.opacity <= 1.0f,
                "npc corpse values are invalid");
        level.npcs.push_back(std::move(state));
    }
    for (const Json& value : root.at("billboards")) {
        GameSaveBillboardState state{value.at("placedObjectId").get<int>(),
                value.value("timeSeconds", 0.0f), value.value("speed", 1.0f),
                value.value("playing", true), value.value("loop", true),
                value.value("finished", false)};
        Require(state.placedObjectId > 0
                        && objectIds.insert(state.placedObjectId).second,
                "duplicate or invalid saved billboard ID");
        RequireFinite(state.timeSeconds, "billboard.timeSeconds");
        RequireFinite(state.speed, "billboard.speed");
        Require(state.timeSeconds >= 0.0f, "billboard time must not be negative");
        level.billboards.push_back(std::move(state));
    }
    std::set<std::string> lightIds;
    for (const Json& value : root.at("dynamicLights")) {
        GameSaveDynamicLightState state{value.at("instanceId").get<std::string>(),
                ReadColor(value.at("color"), "dynamic light.color"),
                value.at("intensity").get<float>(), value.value("enabled", true)};
        Require(!state.instanceId.empty() && lightIds.insert(state.instanceId).second,
                "duplicate or empty dynamic light ID");
        RequireFinite(state.intensity, "dynamic light.intensity");
        Require(state.intensity >= 0.0f, "dynamic light intensity must not be negative");
        level.dynamicLights.push_back(std::move(state));
    }
    std::set<std::string> triggerIds;
    for (const Json& value : root.at("triggers")) {
        GameSaveTriggerState state{value.at("id").get<std::string>(),
                value.value("enabled", true), value.value("inside", false),
                value.value("pending", false), value.value("consumed", false),
                value.value("remainingDelayMilliseconds", 0.0f)};
        Require(!state.id.empty() && triggerIds.insert(state.id).second,
                "duplicate or empty trigger ID");
        RequireFinite(state.remainingDelayMilliseconds, "trigger.remainingDelayMilliseconds");
        Require(state.remainingDelayMilliseconds >= 0.0f,
                "trigger delay must not be negative");
        level.triggers.push_back(std::move(state));
    }
    return level;
}

} // namespace

bool SerializeGameSave(
        const GameSaveData& save,
        std::string& output,
        std::string& error)
{
    try {
        Require(save.formatVersion == GameSaveFormatVersion, "unsupported save format version");
        Require(save.slot >= 1 && save.slot <= GameSaveSlotCount, "save slot is invalid");
        Require(IsValidGameSaveName(save.name), "save name is invalid");
        Require(!save.savedAtUtc.empty(), "save timestamp is missing");
        Require(!save.currentLevelId.empty(), "current level ID is missing");
        Json root{{"formatVersion", save.formatVersion}, {"slot", save.slot},
                {"name", save.name}, {"savedAtUtc", save.savedAtUtc},
                {"currentLevelId", save.currentLevelId}};
        if (!save.thumbnailFile.empty()) root["thumbnailFile"] = save.thumbnailFile;
        root["player"] = Json{{"feetPosition", Vec3(save.player.feetPosition)},
                {"yawRadians", save.player.yawRadians},
                {"pitchRadians", save.player.pitchRadians},
                {"health", HealthJson(save.player.health)},
                {"stamina", Json{{"maximum", save.player.stamina.maximum},
                        {"current", save.player.stamina.current},
                        {"exhausted", save.player.stamina.exhausted}}}};
        root["itemCampaign"] = ItemCampaignJson(save.itemCampaign);
        root["persistentScripts"] = PersistentJson(save.persistentScripts);
        root["levels"] = Json::array();
        std::vector<const GameSaveLevelState*> ordered;
        ordered.reserve(save.levels.size());
        for (const GameSaveLevelState& level : save.levels) ordered.push_back(&level);
        std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
            return left->levelId < right->levelId;
        });
        for (const GameSaveLevelState* level : ordered) root["levels"].push_back(LevelJson(*level));
        std::string candidate = root.dump(2);
        Require(candidate.size() <= GameSaveMaximumJsonBytes, "save JSON exceeds the maximum size");
        output = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string{"Could not serialize game save: "} + exception.what();
        return false;
    }
}

bool DeserializeGameSave(
        const std::string& input,
        GameSaveData& save,
        std::string& error)
{
    try {
        Require(input.size() <= GameSaveMaximumJsonBytes, "save JSON exceeds the maximum size");
        const Json root = Json::parse(input);
        Require(root.is_object(), "save root must be an object");
        GameSaveData candidate;
        candidate.formatVersion = root.at("formatVersion").get<int>();
        Require(candidate.formatVersion == GameSaveFormatVersion,
                "unsupported save format version " + std::to_string(candidate.formatVersion));
        candidate.slot = root.at("slot").get<int>();
        candidate.name = root.at("name").get<std::string>();
        candidate.savedAtUtc = root.at("savedAtUtc").get<std::string>();
        candidate.thumbnailFile = root.value("thumbnailFile", std::string{});
        candidate.currentLevelId = root.at("currentLevelId").get<std::string>();
        Require(candidate.slot >= 1 && candidate.slot <= GameSaveSlotCount, "save slot is invalid");
        Require(IsValidGameSaveName(candidate.name), "save name is invalid");
        Require(!candidate.savedAtUtc.empty() && !candidate.currentLevelId.empty(),
                "save timestamp or current level is missing");
        Require(candidate.thumbnailFile.find('/') == std::string::npos
                        && candidate.thumbnailFile.find('\\') == std::string::npos
                        && candidate.thumbnailFile != "." && candidate.thumbnailFile != "..",
                "thumbnail path is unsafe");
        const Json& player = root.at("player");
        candidate.player.feetPosition = ReadVec3(player.at("feetPosition"), "player.feetPosition");
        candidate.player.yawRadians = player.at("yawRadians").get<float>();
        candidate.player.pitchRadians = player.at("pitchRadians").get<float>();
        RequireFinite(candidate.player.yawRadians, "player.yawRadians");
        RequireFinite(candidate.player.pitchRadians, "player.pitchRadians");
        candidate.player.health = ReadHealth(player.at("health"), "player.health");
        const Json& stamina = player.at("stamina");
        candidate.player.stamina.maximum = stamina.at("maximum").get<float>();
        candidate.player.stamina.current = stamina.at("current").get<float>();
        candidate.player.stamina.exhausted = stamina.value("exhausted", false);
        RequireFinite(candidate.player.stamina.maximum, "player.stamina.maximum");
        RequireFinite(candidate.player.stamina.current, "player.stamina.current");
        Require(candidate.player.stamina.maximum > 0.0f
                        && candidate.player.stamina.current >= 0.0f
                        && candidate.player.stamina.current <= candidate.player.stamina.maximum,
                "player stamina is outside its valid range");
        candidate.itemCampaign = ReadItemCampaign(root.at("itemCampaign"));
        candidate.persistentScripts = ReadPersistent(root.at("persistentScripts"));
        std::set<std::string> levelIds;
        for (const Json& level : root.at("levels")) {
            GameSaveLevelState parsed = ReadLevel(level);
            Require(levelIds.insert(parsed.levelId).second, "duplicate saved level ID");
            candidate.levels.push_back(std::move(parsed));
        }
        save = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string{"Could not parse game save: "} + exception.what();
        return false;
    }
}

} // namespace game
