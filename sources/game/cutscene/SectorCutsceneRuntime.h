#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/audio/DialogueSpeech.h"
#include "engine/ecs/Entity.h"
#include "engine/scripting/ScriptData.h"
#include "game/navigation/SectorNavigationTypes.h"
#include "game/npc/NpcRuntime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <raylib.h>

namespace engine {
class AssetManager;
class World;
struct ScriptRuntime;
}

namespace game {

class SectorCollisionWorld;
class SectorNavigationWorld;
struct SectorDynamicDoorCollider;
struct SectorFpsControllerConfig;
struct SectorFpsControllerState;

inline constexpr size_t kSectorCutsceneMaximumCaptionBytes = 8192;
inline constexpr size_t kSectorCutsceneMaximumCaptionCodepoints = 2048;

enum class SectorCutsceneLookTargetKind : uint8_t {
    Npc,
    Prop
};

enum class SectorCutsceneCaptionKind : uint8_t {
    Say,
    Text
};

enum class SectorCutsceneTextPosition : int {
    Top = 1,
    Center = 2,
    Bottom = 3
};

struct SectorCutsceneLookState {
    uint64_t token = 0;
    engine::ScriptOperationHandle operation{};
    engine::Entity entity = engine::NullEntity();
    SectorCutsceneLookTargetKind targetKind =
            SectorCutsceneLookTargetKind::Npc;
    float targetHeight = 0.5f;
    float startYawRadians = 0.0f;
    float startPitchRadians = 0.0f;
    float targetYawRadians = 0.0f;
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.0;
    bool active = false;
};

struct SectorCutscenePlayerMoveState {
    uint64_t token = 0;
    engine::ScriptOperationHandle operation{};
    SectorNavigationPathHandle pathHandle{};
    Vector2 requestedDestinationXZ{};
    Vector3 projectedDestination{};
    std::array<Vector3, SectorNavigationMaximumStraightPathCorners> corners{};
    std::array<int, SectorNavigationMaximumStraightPathCorners> cornerDoorIds{};
    std::array<SectorNavigationDoorDirection,
            SectorNavigationMaximumStraightPathCorners> cornerDoorDirections{};
    std::array<Vector3,
            SectorNavigationMaximumStraightPathCorners> cornerDoorLandings{};
    std::array<SectorNavigationTileKey,
            SectorNavigationMaximumCorridorTiles> corridorTiles{};
    size_t corridorTileCount = 0;
    size_t cornerCount = 0;
    size_t nextCorner = 0;
    uint64_t pathTileRevision = 0;
    NpcMoveGait gait = NpcMoveGait::Walk;
    float movementSpeed = 0.0f;
    float stallSeconds = 0.0f;
    float facingVelocity = 0.0f;
    SectorCutsceneLookState arrivalLook;
    bool arrivalLookStarted = false;
    bool arrived = false;
    uint32_t replanCount = 0;
    NpcDoorTraversalPhase doorPhase = NpcDoorTraversalPhase::None;
    int doorId = 0;
    SectorNavigationDoorDirection doorDirection =
            SectorNavigationDoorDirection::None;
    Vector3 doorLanding{};
    float doorWaitSeconds = 0.0f;
    bool holdsDoor = false;
    bool doorReplanRequested = false;
    bool active = false;
};

struct SectorCutsceneCaptionState {
    uint64_t token = 0;
    engine::ScriptOperationHandle operation{};
    SectorCutsceneCaptionKind kind = SectorCutsceneCaptionKind::Say;
    SectorCutsceneTextPosition position = SectorCutsceneTextPosition::Bottom;
    std::string text;
    size_t codepointCount = 0;
    size_t visibleByteCount = 0;
    double revealSeconds = 0.0;
    double holdSeconds = 0.0;
    bool explicitHold = false;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.35;
    double elapsedSeconds = 0.0;
    float opacity = 0.0f;
    bool active = false;
    engine::Entity speaker = engine::NullEntity();
    bool playerSpeaker = false;
    engine::DialogueMood mood = engine::DialogueMood::Neutral;
    engine::DialogueTimeline speechTimeline;
    bool voiceTiming = true;
    bool speechDriven = false;
    bool speechFinished = false;
};

// Borrowed load-time data used only while constructing a caption.
struct SectorCutsceneSpeechOptions {
    engine::Entity speaker = engine::NullEntity();
    bool playerSpeaker = false;
    engine::DialogueMood mood = engine::DialogueMood::Neutral;
    const engine::DialogueVoice* voice = nullptr;
    const engine::DialogueSelectionHistory* history = nullptr;
    const engine::DialogueSettings* settings = nullptr;
    uint32_t seed = 1;
};

struct SectorCutsceneFadeState {
    uint64_t token = 0;
    engine::ScriptOperationHandle operation{};
    float opacity = 0.0f;
    float startOpacity = 0.0f;
    float targetOpacity = 0.0f;
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.0;
    bool active = false;
};

struct SectorCutscenePresentationState {
    double progress = 0.0;
    bool active = false;
};

struct SectorCutscenePresentationLayout {
    Rectangle topBar{};
    Rectangle bottomBar{};
    float captionY = 0.0f;
};

struct SectorCutsceneRuntime {
    SectorCutscenePlayerMoveState playerMove;
    SectorCutsceneLookState look;
    SectorCutsceneCaptionState caption;
    SectorCutsceneFadeState fade;
    SectorCutscenePresentationState presentation;
    engine::DialoguePlayback speechPlayback;
    engine::Entity speechSpeaker = engine::NullEntity();
    engine::DialogueSelectionHistory playerSpeechHistory;
    engine::ScriptTaskHandle controlsOwnerTask{};
    uint64_t nextToken = 1;
    bool controlsEnabled = true;
};

void InitializeSectorCutsceneRuntime(SectorCutsceneRuntime& runtime);
void ResetSectorCutsceneRuntime(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld* navigation = nullptr);

bool BeginSectorCutscenePlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const SectorFpsControllerState& player,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        float movementSpeed,
        uint64_t& outToken,
        std::string& error);
void BindSectorCutscenePlayerMoveOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation);
void CancelSectorCutscenePlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld* navigation,
        uint64_t token);
void PrepareSectorCutscenePlayerDoorTraversal(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 playerFeetPosition,
        float dt);
int SectorCutscenePlayerDoorHoldId(const SectorCutsceneRuntime& runtime);
Vector2 BuildSectorCutscenePlayerMoveDelta(
        SectorCutsceneRuntime& runtime,
        const SectorFpsControllerState& player,
        float dt,
        float* outFacingYawRadians);
// Camera-only backend; arrivalTarget is a resolved world-space visual target.
// Returns false if an active arrival look cannot aim at that point.
bool AdvanceSectorCutscenePlayerCamera(
        SectorCutsceneRuntime& runtime,
        SectorFpsControllerState& player,
        Vector3 eyePosition,
        const Vector3* arrivalTarget,
        float dt);
void UpdateSectorCutscenePlayerCamera(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        engine::World& world,
        engine::AssetManager& assets,
        SectorFpsControllerState& player,
        const SectorFpsControllerConfig& playerConfig,
        engine::ScriptRuntime& scripts,
        float dt);
void FinishSectorCutscenePlayerMoveFrame(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const SectorFpsControllerState& player,
        Vector2 previousPositionXZ,
        engine::ScriptRuntime& scripts,
        float dt);

bool BeginSectorCutsceneLook(
        SectorCutsceneRuntime& runtime,
        engine::Entity entity,
        SectorCutsceneLookTargetKind kind,
        float targetHeight,
        double durationSeconds,
        const SectorFpsControllerState& player,
        uint64_t& outToken,
        std::string& error);
void BindSectorCutsceneLookOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation);
void CancelSectorCutsceneLook(SectorCutsceneRuntime& runtime, uint64_t token);
void UpdateSectorCutsceneLook(
        SectorCutsceneRuntime& runtime,
        engine::World& world,
        engine::AssetManager& assets,
        SectorFpsControllerState& player,
        const SectorFpsControllerConfig& playerConfig,
        engine::ScriptRuntime& scripts,
        float dt);

bool BeginSectorCutsceneCaption(
        SectorCutsceneRuntime& runtime,
        SectorCutsceneCaptionKind kind,
        SectorCutsceneTextPosition position,
        std::string_view text,
        const double* holdSeconds,
        uint64_t& outToken,
        std::string& error,
        const SectorCutsceneSpeechOptions* speech = nullptr);
void BindSectorCutsceneCaptionOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation);
void CancelSectorCutsceneCaption(SectorCutsceneRuntime& runtime, uint64_t token);
bool AdvanceSectorCutsceneSpeech(SectorCutsceneRuntime& runtime, engine::ScriptRuntime& scripts);
void SetSectorCutsceneCaptionVoiceTiming(SectorCutsceneRuntime& runtime, bool enabled);
void UpdateSectorCutsceneSpeech(SectorCutsceneRuntime& runtime, engine::World& world,
        engine::AssetManager& assets, engine::AudioSystem& audio, float dt,
        bool voicesEnabled = true);
void StopSectorCutsceneSpeech(SectorCutsceneRuntime& runtime, engine::World& world,
        engine::AssetManager& assets, engine::AudioSystem& audio);

bool BeginSectorCutsceneFade(
        SectorCutsceneRuntime& runtime,
        float targetOpacity,
        double durationSeconds,
        uint64_t& outToken,
        std::string& error);
void BindSectorCutsceneFadeOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation);
void CancelSectorCutsceneFade(SectorCutsceneRuntime& runtime, uint64_t token);

void UpdateSectorCutsceneTimelines(
        SectorCutsceneRuntime& runtime,
        engine::ScriptRuntime& scripts,
        float dt);
SectorCutscenePresentationLayout BuildSectorCutscenePresentationLayout(
        const SectorCutscenePresentationState& presentation,
        Rectangle viewport,
        SectorCutsceneTextPosition captionPosition,
        float captionBlockHeight);
void DrawSectorCutsceneLetterbox(
        const SectorCutsceneRuntime& runtime,
        Rectangle viewport);
void DrawSectorCutsceneCaption(
        const SectorCutsceneRuntime& runtime,
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle playableViewport);

} // namespace game
