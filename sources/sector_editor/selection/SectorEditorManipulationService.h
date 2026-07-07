#pragma once

#include "engine/input/Input.h"
#include "sector_editor/selection/SectorEditorMoveContext.h"
#include "sector_editor/selection/SectorEditorSelectionTarget.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace game {

struct SectorEditorManipulationServiceContext {
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    LightEditingState& lightState;
    std::string& statusText;
    void* userData = nullptr;

    SectorEditorPickTarget (*currentPickSelectionTarget)(void* userData) = nullptr;
    std::vector<SectorEditorPickCandidate> (*buildSelectPickCandidates)(
            void* userData,
            Vector2 screenPoint) = nullptr;
    bool (*findStaticSpotLightHandle)(
            void* userData,
            Vector2 screenPoint,
            int& outLightId,
            SpotLightHandle& outHandle) = nullptr;
    bool (*findDynamicSpotLightHandle)(
            void* userData,
            Vector2 screenPoint,
            int& outLightId,
            SpotLightHandle& outHandle) = nullptr;

    const SectorEditorMoveProvider* placedObjectMoveProvider = nullptr;
    std::function<Vector2(Vector2)> screenToMap;
    std::function<Vector2(Vector2)> snapMapPoint;
    std::function<void(int)> selectRuntimeObject;
    std::function<void(const SectorPlacedRuntimeObject&)> updateCachedRuntimeObjectDraw;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<void()> refreshRuntimeObjectsAfterAuthoringEdit;

    void (*startAuthoringVertexDrag)(
            void* userData,
            int vertexId,
            SectorTopologyCoordPoint point) = nullptr;
    void (*startRuntimeObjectDrag)(void* userData, int objectId) = nullptr;
    void (*startLightDrag)(
            void* userData,
            int topologyLightId,
            SpotLightHandle spotHandle) = nullptr;

    void (*updateAuthoringVertexDrag)(void* userData, engine::Input& input) = nullptr;
    void (*finishAuthoringVertexDrag)(void* userData) = nullptr;
    void (*cancelAuthoringVertexDrag)(void* userData, const char* message) = nullptr;

    void (*updateRuntimeObjectDrag)(void* userData, engine::Input& input) = nullptr;
    void (*finishRuntimeObjectDrag)(void* userData) = nullptr;
    void (*cancelRuntimeObjectDrag)(void* userData, const char* message) = nullptr;

    void (*updateLightDrag)(void* userData, engine::Input& input) = nullptr;
    void (*finishLightDrag)(void* userData) = nullptr;
    void (*cancelLightDrag)(void* userData, const char* message) = nullptr;
};

bool IsAnySectorEditorManipulationActive(const SectorEditorManipulationServiceContext& context);
void UpdateActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        engine::Input& input);
void FinishActiveSectorEditorManipulation(SectorEditorManipulationServiceContext& context);
bool CancelFirstActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        const char* authoringVertexMessage,
        const char* lightMessage,
        const char* runtimeObjectMessage);
void CancelActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        const char* authoringVertexMessage,
        const char* lightMessage,
        const char* runtimeObjectMessage);

void UpdateSectorEditorSelectDragArm(
        SectorEditorManipulationServiceContext& context,
        engine::Input& input);
void ArmSectorEditorSelectedDrag(
        SectorEditorManipulationServiceContext& context,
        Vector2 pressPosition);
void StartSectorEditorSelectedManipulation(
        SectorEditorManipulationServiceContext& context,
        SectorEditorPickTarget target,
        Vector2 screenPoint);
bool FindSectorEditorSelectedMovablePickTargetAtScreenPoint(
        SectorEditorManipulationServiceContext& context,
        Vector2 screenPoint,
        SectorEditorPickTarget& outTarget);

} // namespace game
