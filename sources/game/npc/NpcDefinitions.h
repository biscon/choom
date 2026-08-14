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

enum class NpcAction {
    Idle,
    Walk,
    Run,
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
};

struct NpcActionDefinition {
    std::string animation;
    float animationSpeed = 1.0f;
    float movementSpeed = 0.0f;
};

struct NpcDefinition {
    std::string id;
    std::string name;
    bool hostile = false;
    std::string modelPath;
    float animationBlendSeconds = kDefaultNpcAnimationBlendSeconds;
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
