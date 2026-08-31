#pragma once

#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingState.h"
#include "sector_editor/services/config_clipboard/SectorEditorConfigClipboardTypes.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "game/items/ItemDefinitions.h"

#include <functional>
#include <string>

namespace engine {
struct EngineContext;
}

namespace game {

struct SectorEditorRuntimeObjectEditingServiceContext {
    SectorTopologyMap& map;
    SectorRuntimeObjectState& runtimeObjects;
    RuntimeObjectEditingState& editingState;
    RuntimeObjectEditingUiState& uiState;
    SelectionState& selectionState;
    SectorEditorSelectionServiceContext* selectionService = nullptr;
    SectorEditorDocumentLifecycleAccess lifecycle;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    std::string& statusText;
    engine::EngineContext* engineContext = nullptr;
    bool authoringDerivationCurrent = false;
    const ItemRegistry* itemRegistry = nullptr;
};

struct SectorEditorRuntimeObjectDeleteRequest {
    bool requested = false;
    int objectId = -1;
    std::string title;
    std::string message;
};

struct SectorEditorPreviewObjectAdjustmentResult {
    bool changed = false;
    bool bakedStatusRefreshNeeded = false;
    bool restoreBakedStatus = false;
    bool commitBakedStatus = false;
    bool staticNavigationRebuildNeeded = false;
};

bool IsSectorEditorPreviewAdjustableObject(
        const SectorPlacedRuntimeObject& object);
const char* SectorEditorPreviewObjectKindName(
        const SectorPlacedRuntimeObject& object);
float SectorEditorPreviewObjectHeightOffsetWorld(
        const SectorPlacedRuntimeObject& object);
float SectorEditorPreviewObjectTranslationStepWorld(
        PreviewObjectNudgePreset preset);
float SectorEditorPreviewObjectYawStepDegrees(
        PreviewObjectNudgePreset preset);

class SectorEditorRuntimeObjectEditingService {
public:
    explicit SectorEditorRuntimeObjectEditingService(
            SectorEditorRuntimeObjectEditingServiceContext context);

    SectorPlacedRuntimeObject* SelectedObject();
    const SectorPlacedRuntimeObject* SelectedObject() const;
    const ItemRegistry* ItemRegistryView() const { return context_.itemRegistry; }
    void SelectObject(int objectId);

    bool AddBillboard(int sectorId, Vector2 mapPoint);
    bool AddDoor(int lineDefId);
    bool AddWindow(int lineDefId);
    bool AddStaticModel(Vector2 mapPoint);
    bool AddDynamicModel(Vector2 mapPoint);
    bool AddItem(Vector2 mapPoint, const std::string& definitionId);
    bool AddNpc(Vector2 mapPoint, const std::string& definitionId);
    bool CopySelectedConfig(
            SectorEditorConfigClipboardState& clipboard) const;
    bool PasteSelectedConfig(
            const SectorEditorConfigClipboardState& clipboard);
    SectorEditorRuntimeObjectDeleteRequest RequestDeleteSelected() const;
    bool DeleteById(int objectId);

    bool MutateSelected(
            const char* status,
            const std::function<bool(SectorPlacedRuntimeObject&)>& mutate);
    bool AssignSelectedStaticModel(const std::string& modelPath);
    bool SetSelectedStaticModelInstanceId(
            const std::string& instanceId,
            std::string& outError);
    bool AssignSelectedDynamicModel(const std::string& modelPath);
    bool AssignSelectedItemDefinition(const std::string& definitionId);
    bool SetSelectedItemInstanceId(
            const std::string& instanceId,
            std::string& outError);
    bool SetSelectedDynamicModelInstanceId(
            const std::string& instanceId,
            std::string& outError);
    bool SetSelectedDoorInstanceId(
            const std::string& instanceId,
            std::string& outError);
    bool AssignSelectedNpcDefinition(const std::string& definitionId);
    bool SetSelectedNpcInstanceId(
            const std::string& instanceId,
            std::string& outError);
    bool SelectedDoorRuntimeTargetOpen(bool& outOpen) const;
    bool SetSelectedDoorRuntimeTargetOpen(bool open);

    bool BeginPreviewAdjustment();
    SectorEditorPreviewObjectAdjustmentResult PreviewNudge(
            float deltaXWorld,
            float deltaZWorld,
            float deltaHeightWorld,
            float deltaYawDegrees);
    SectorEditorPreviewObjectAdjustmentResult ApplyPreviewAdjustment();
    SectorEditorPreviewObjectAdjustmentResult CancelPreviewAdjustment(
            const char* message);
    void SetPreviewAdjustmentPreset(PreviewObjectNudgePreset preset);

    bool BeginDrag(int objectId);
    void UpdateDrag(Vector2 snappedMapPoint);
    bool FinishDrag();
    void CancelDrag(const char* message);

    int FindCachedSectorAt(Vector2 mapPoint, bool* outMultipleMatches = nullptr) const;
    void RefreshPreviewObjects();
    void MarkEdited(const char* status);

private:
    void ResetInspectorUi();
    void UpdateCachedDraw(const SectorPlacedRuntimeObject& object);
    void ClearSelection();

    SectorEditorRuntimeObjectEditingServiceContext context_;
};

} // namespace game
