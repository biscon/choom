#pragma once

#include "sector_editor/npcs/SectorEditorNpcEditorState.h"

#include <filesystem>
#include <string>

namespace engine { class AssetManager; }

namespace game {

class SectorEditorNpcEditorService {
public:
    SectorEditorNpcEditorService(
            SectorEditorNpcEditorState& state,
            SectorEditorNpcEditorSessionState& session,
            std::string& statusText,
            std::filesystem::path definitionsRoot);

    bool Open();
    void Cancel(engine::AssetManager* assets);
    bool SaveAndClose(engine::AssetManager* assets);
    void Shutdown(engine::AssetManager& assets);

    SectorEditorNpcDefinitionDraft* SelectedDraft();
    const SectorEditorNpcDefinitionDraft* SelectedDraft() const;
    bool SelectIndex(int index);
    void AddDefinition();
    void RequestDeleteSelected();
    void CancelDelete();
    void ConfirmDeleteSelected();

    void ApplyIdBuffer();
    void ApplyNameBuffer();
    void SetSelectedHostile(bool hostile);
    void SetSelectedAiType(const std::string& aiType);
    void SetSelectedPerception(
            float visionRangeWorld,
            float visionAngleDegrees,
            float hearingRangeWorld,
            int investigationDurationMilliseconds);
    void SetSelectedCanOpenDoors(bool canOpenDoors);
    void SetSelectedBaseHealth(int health);
    void SetSelectedDespawnOnDeath(bool despawn);
    void SetSelectedCorpseDespawnDelayMilliseconds(int milliseconds);
    void SetSelectedCorpseFadeDurationMilliseconds(int milliseconds);
    void SetSelectedAnimationBlendSeconds(float seconds);
    void SetSelectedModelPath(
            const std::string& modelPath,
            engine::AssetManager& assets);
    void SetSelectedAnimation(NpcAction action, const std::string& animation);
    void SetSelectedPlayerDetectedSound(const std::string& soundPath);
    void SetSelectedActionSound(NpcAction action, const std::string& soundPath);
    void SetSelectedAttackSound(const std::string& soundPath);
    void SetSelectedAnimationSpeed(NpcAction action, float speed);
    void SetSelectedMovementSpeed(NpcAction action, float speed);
    void SetSelectedAttack(
            float hitPhase,
            float rangeWorld,
            int damage,
            float knockbackImpulseWorldPerSecond,
            int stunMilliseconds);
    void SetSelectedAttackCameraImpact(
            const NpcAttackCameraImpactDefinition& cameraImpact);
    void SetSelectedAmbientDelayRange(float minimumSeconds, float maximumSeconds);
    bool AddSelectedAmbientSound(const std::string& soundPath);
    bool ReplaceSelectedAmbientSound(size_t index, const std::string& soundPath);
    bool RemoveSelectedAmbientSound(size_t index);

    void EnsureSelectedModelRequested(engine::AssetManager& assets);
    void RefreshAnimationOptions(engine::AssetManager& assets);
    bool SelectedModelReady(const engine::AssetManager& assets) const;
    bool SelectedModelFailed(const engine::AssetManager& assets) const;
    bool SelectedAnimationExists(
            const engine::AssetManager& assets,
            std::string_view animation) const;

    SectorEditorNpcEditorState& State() { return state_; }
    const SectorEditorNpcEditorState& State() const { return state_; }
    SectorEditorNpcEditorSessionState& Session() { return session_; }

private:
    void Close(engine::AssetManager* assets);
    void ResetTransientStatePreservingSession(engine::AssetManager* assets);
    void SyncBuffersFromSelection();
    void RebuildListLabels();
    bool ValidateDrafts(std::string& outError) const;
    std::filesystem::path DefinitionPath(std::string_view id) const;

    SectorEditorNpcEditorState& state_;
    SectorEditorNpcEditorSessionState& session_;
    std::string& statusText_;
    std::filesystem::path definitionsRoot_;
};

} // namespace game
