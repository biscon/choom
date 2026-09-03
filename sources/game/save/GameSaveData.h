#pragma once

#include "engine/scripting/ScriptData.h"
#include "game/Health.h"
#include "game/PlayerStamina.h"
#include "game/PlayerOxygen.h"
#include "game/items/ItemInventory.h"
#include "sector_demo/SectorTopologyTypes.h"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

inline constexpr int GameSaveFormatVersion = 1;
inline constexpr int GameSaveSlotCount = 12;
inline constexpr int GameSaveThumbnailWidth = 320;
inline constexpr int GameSaveThumbnailHeight = 180;
inline constexpr std::size_t GameSaveMaximumJsonBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t GameSaveMaximumNameCharacters = 48;

struct GameSaveAnimatorState {
    std::string animationName;
    std::string targetAnimationName;
    float frame = 0.0f;
    float targetFrame = 0.0f;
    float speed = 1.0f;
    float transitionDurationSeconds = 0.0f;
    float transitionElapsedSeconds = 0.0f;
    bool playing = true;
    bool loop = true;
    bool reverse = false;
    bool finished = false;
    bool paused = false;
    bool targetLoop = true;
    bool targetFinished = false;
};

struct GameSaveDoorState {
    int placedObjectId = 0;
    std::string instanceId;
    float openFraction = 0.0f;
    float targetOpenFraction = 0.0f;
    bool enabled = true;
};

struct GameSaveDuctAccessState {
    int placedObjectId = 0;
    bool coverRemoved = false;
    SectorDuctCoverRemovalSide removalSide =
            SectorDuctCoverRemovalSide::Outside;
};

struct GameSavePropState {
    int placedObjectId = 0;
    std::string instanceId;
    float emissiveScale = 1.0f;
    float opacity = 1.0f;
    bool useConsumed = false;
    bool hasAnimator = false;
    GameSaveAnimatorState animator;
};

struct GameSaveNpcState {
    int placedObjectId = 0;
    std::string instanceId;
    Vector3 position = {};
    float yawRadians = 0.0f;
    Health health;
    bool dead = false;
    bool deathAnimationComplete = false;
    bool despawned = false;
    float corpseElapsedSeconds = 0.0f;
    float opacity = 1.0f;
    bool hasAnimator = false;
    GameSaveAnimatorState animator;

    bool hasPatrol = false;
    int patrolEditorId = 0;
    std::size_t waypointIndex = 0;
    int direction = 1;
    std::vector<std::size_t> shuffleOrder;
    std::size_t shuffleCursor = 0;
    std::uint32_t randomState = 0;
    int phase = 0;
    int resumePhase = 0;
    float waitRemainingSeconds = 0.0f;
    float waypointBaseYawRadians = 0.0f;
    float lookOffsetRadians = 0.0f;
    float lookDirection = 1.0f;
    float retryRemainingSeconds = 0.0f;
    Vector2 destinationXZ = {};
    bool stoppedByScript = false;
    bool destinationInitialized = false;
};

struct GameSaveBillboardState {
    int placedObjectId = 0;
    float timeSeconds = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool finished = false;
};

struct GameSaveDynamicLightState {
    std::string instanceId;
    Color color = WHITE;
    float intensity = 1.0f;
    bool enabled = true;
};

struct GameSaveTriggerState {
    std::string id;
    bool enabled = true;
    bool inside = false;
    bool pending = false;
    bool consumed = false;
    float remainingDelayMilliseconds = 0.0f;
};

struct GameSaveLevelState {
    std::string levelId;
    std::vector<GameSaveDoorState> doors;
    std::vector<GameSaveDuctAccessState> ductAccesses;
    std::vector<GameSavePropState> props;
    std::vector<GameSaveNpcState> npcs;
    std::vector<GameSaveBillboardState> billboards;
    std::vector<GameSaveDynamicLightState> dynamicLights;
    std::vector<GameSaveTriggerState> triggers;
};

struct GameSavePlayerState {
    Vector3 feetPosition = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    Health health;
    PlayerStamina stamina;
    PlayerOxygen oxygen;
    bool hasOxygenState = false;
    bool flashlightEnabled = false;
};

struct GameSaveData {
    int formatVersion = GameSaveFormatVersion;
    int slot = 0;
    std::string name;
    std::string savedAtUtc;
    std::string thumbnailFile;
    std::string currentLevelId;
    GameSavePlayerState player;
    ItemCampaignState itemCampaign;
    engine::PersistentScriptStore persistentScripts;
    std::vector<GameSaveLevelState> levels;
};

enum class GameSaveSlotStatus {
    Empty,
    Ready,
    Corrupt,
    Incompatible
};

struct GameSaveSlotInfo {
    int slot = 0;
    GameSaveSlotStatus status = GameSaveSlotStatus::Empty;
    std::string name;
    std::string savedAtUtc;
    std::string displayTimestamp;
    std::string currentLevelId;
    std::string thumbnailPath;
    std::string error;
};

} // namespace game
