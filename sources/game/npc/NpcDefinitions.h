#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace game {

inline constexpr int kNpcDefinitionFormatVersion = 1;
inline constexpr const char* kNpcDefinitionsAssetRoot = "assets/npcs";
inline constexpr const char* kNpcCharacterModelsAssetRoot =
        "assets/models/characters";
inline constexpr float kDefaultNpcAnimationBlendSeconds = 0.2f;
inline constexpr float kMinimumNpcAnimationBlendSeconds = 0.01f;
inline constexpr float kMaximumNpcAnimationBlendSeconds = 2.0f;
inline constexpr int kDefaultNpcBaseHealth = 100;
inline constexpr int kMinimumNpcBaseHealth = 1;
inline constexpr int kMaximumNpcBaseHealth = 1000000;
inline constexpr float kDefaultNpcCorpseDespawnDelaySeconds = 2.0f;
inline constexpr float kDefaultNpcCorpseFadeDurationSeconds = 0.75f;
inline constexpr float kMaximumNpcCorpseDespawnDelaySeconds = 600.0f;
inline constexpr float kMinimumNpcCorpseFadeDurationSeconds = 0.001f;
inline constexpr float kMaximumNpcCorpseFadeDurationSeconds = 60.0f;
inline constexpr float kDefaultNpcAmbientMinimumDelaySeconds = 5.0f;
inline constexpr float kDefaultNpcAmbientMaximumDelaySeconds = 12.0f;
inline constexpr float kMaximumNpcAmbientDelaySeconds = 600.0f;
inline constexpr float kDefaultNpcVisionRangeWorld = 15.0f;
inline constexpr float kDefaultNpcVisionAngleDegrees = 120.0f;
inline constexpr float kDefaultNpcHearingRangeWorld = 12.0f;
inline constexpr int kDefaultNpcInvestigationDurationMilliseconds = 4000;
inline constexpr float kDefaultNpcAttackHitPhase = 0.55f;
inline constexpr float kDefaultNpcAttackRangeWorld = 1.0f;
inline constexpr int kDefaultNpcAttackDamage = 15;
inline constexpr float kDefaultNpcAttackKnockbackImpulseWorldPerSecond = 0.0f;
inline constexpr int kDefaultNpcAttackStunMilliseconds = 0;
inline constexpr bool kDefaultNpcAttackCameraImpactEnabled = true;
inline constexpr float kDefaultNpcAttackCameraImpactPitchKickDegrees = 2.5f;
inline constexpr float kDefaultNpcAttackCameraImpactRollKickDegrees = 3.5f;
inline constexpr float kDefaultNpcAttackCameraImpactSpringFrequencyHz = 4.0f;
inline constexpr float kDefaultNpcAttackCameraImpactSpringDampingRatio = 0.75f;
inline constexpr float kDefaultNpcAttackCameraImpactMaxPitchDegrees = 7.5f;
inline constexpr float kDefaultNpcAttackCameraImpactMaxRollDegrees = 10.0f;
inline constexpr float kMinimumNpcAttackCameraImpactSpringFrequencyHz = 0.5f;
inline constexpr float kMaximumNpcAttackCameraImpactSpringFrequencyHz = 40.0f;
inline constexpr float kMinimumNpcAttackCameraImpactSpringDampingRatio = 0.1f;
inline constexpr float kMaximumNpcAttackCameraImpactSpringDampingRatio = 3.0f;
inline constexpr float kMaximumNpcAttackCameraImpactKickDegrees = 45.0f;
inline constexpr float kMaximumNpcAttackCameraImpactLimitDegrees = 90.0f;

enum class NpcAction {
    Idle,
    Walk,
    Run,
    Attack,
    Hurt,
    Death,
    Count
};

inline constexpr size_t kNpcActionCount =
        static_cast<size_t>(NpcAction::Count);

struct NpcActionMetadata {
    NpcAction action = NpcAction::Idle;
    const char* jsonKey = "idle";
    const char* displayName = "Idle";
    bool hasMovementSpeed = false;
    float defaultMovementSpeed = 0.0f;
    bool hasSound = false;
};

struct NpcAttackCameraImpactDefinition {
    bool enabled = kDefaultNpcAttackCameraImpactEnabled;
    float pitchKickDegrees = kDefaultNpcAttackCameraImpactPitchKickDegrees;
    float rollKickDegrees = kDefaultNpcAttackCameraImpactRollKickDegrees;
    float springFrequencyHz =
            kDefaultNpcAttackCameraImpactSpringFrequencyHz;
    float springDampingRatio =
            kDefaultNpcAttackCameraImpactSpringDampingRatio;
    float maxPitchDegrees = kDefaultNpcAttackCameraImpactMaxPitchDegrees;
    float maxRollDegrees = kDefaultNpcAttackCameraImpactMaxRollDegrees;
};

struct NpcActionDefinition {
    std::string animation;
    std::string soundPath;
    std::string attackSoundPath;
    float animationSpeed = 1.0f;
    float movementSpeed = 0.0f;
    float hitPhase = kDefaultNpcAttackHitPhase;
    float rangeWorld = kDefaultNpcAttackRangeWorld;
    int damage = kDefaultNpcAttackDamage;
    float knockbackImpulseWorldPerSecond =
            kDefaultNpcAttackKnockbackImpulseWorldPerSecond;
    int stunMilliseconds = kDefaultNpcAttackStunMilliseconds;
    NpcAttackCameraImpactDefinition cameraImpact;
};

struct NpcPerceptionDefinition {
    float visionRangeWorld = kDefaultNpcVisionRangeWorld;
    float visionAngleDegrees = kDefaultNpcVisionAngleDegrees;
    float hearingRangeWorld = kDefaultNpcHearingRangeWorld;
    int investigationDurationMilliseconds =
            kDefaultNpcInvestigationDurationMilliseconds;
};

struct NpcAmbientVocalizationDefinition {
    std::vector<std::string> soundPaths;
    float minimumDelaySeconds = kDefaultNpcAmbientMinimumDelaySeconds;
    float maximumDelaySeconds = kDefaultNpcAmbientMaximumDelaySeconds;
};

struct NpcDefinition {
    std::string id;
    std::string name;
    bool hostile = false;
    std::string aiType;
    bool canOpenDoors = true;
    int baseHealth = kDefaultNpcBaseHealth;
    bool despawnOnDeath = false;
    float corpseDespawnDelaySeconds = kDefaultNpcCorpseDespawnDelaySeconds;
    float corpseFadeDurationSeconds = kDefaultNpcCorpseFadeDurationSeconds;
    std::string modelPath;
    float animationBlendSeconds = kDefaultNpcAnimationBlendSeconds;
    NpcPerceptionDefinition perception;
    NpcAmbientVocalizationDefinition ambientVocalizations;
    std::array<NpcActionDefinition, kNpcActionCount> actions;
};

struct NpcDefinitionCatalogError {
    std::string path;
    std::string message;
};

struct NpcDefinitionCatalog {
    std::vector<NpcDefinition> definitions;
    std::vector<NpcDefinitionCatalogError> errors;
};

const std::array<NpcActionMetadata, kNpcActionCount>& NpcActionMetadataTable();
const NpcActionMetadata& GetNpcActionMetadata(NpcAction action);
NpcDefinition MakeDefaultNpcDefinition();
NpcActionDefinition& GetNpcAction(NpcDefinition& definition, NpcAction action);
const NpcActionDefinition& GetNpcAction(
        const NpcDefinition& definition,
        NpcAction action);

bool IsValidNpcDefinitionId(std::string_view id);
bool IsValidNpcCharacterModelPath(std::string_view path);
bool IsValidNpcAudioPath(std::string_view path);
bool ValidateNpcDefinition(
        const NpcDefinition& definition,
        std::string& outError);

bool ParseNpcDefinitionJson(
        std::string_view jsonText,
        NpcDefinition& outDefinition,
        std::string& outError);
bool SerializeNpcDefinitionJson(
        const NpcDefinition& definition,
        std::string& outJson,
        std::string& outError);
bool LoadNpcDefinition(
        const std::filesystem::path& path,
        NpcDefinition& outDefinition,
        std::string& outError);
bool SaveNpcDefinition(
        const std::filesystem::path& path,
        const NpcDefinition& definition,
        std::string& outError);
bool DiscoverNpcDefinitions(
        const std::filesystem::path& root,
        NpcDefinitionCatalog& outCatalog);

const NpcDefinition* FindNpcDefinition(
        const NpcDefinitionCatalog& catalog,
        std::string_view id);

} // namespace game
