#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/structural_primitives/SectorEditorStructuralPrimitiveEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorStructuralPrimitiveEditingServiceContext {
    SectorEditorState& state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState& selectionState;
    SectorEditorStructuralPrimitiveEditingState& editingState;
    std::string& statusText;
};

struct SectorEditorStructuralPreviewCandidate {
    bool valid = false;
    bool changedFromOriginal = false;
    SectorAuthoringGraph graph;
    SectorAuthoringDerivationResult derivation;
};

class SectorEditorStructuralPrimitiveEditingService {
public:
    explicit SectorEditorStructuralPrimitiveEditingService(
            SectorEditorStructuralPrimitiveEditingServiceContext context);

    SectorAuthoringStructuralPrimitive* Selected();
    const SectorAuthoringStructuralPrimitive* Selected() const;
    bool Select(int primitiveId);

    bool ResolvePlacementFloor(SectorTopologyCoordPoint point, float& outFloor) const;
    bool BuildPlacementValue(
            SectorStructuralPrimitiveKind kind,
            SectorTopologyCoordPoint start,
            SectorTopologyCoordPoint end,
            float seedFloor,
            const std::string& defaultMaterialId,
            int primitiveId,
            SectorAuthoringStructuralPrimitive& outPrimitive,
            std::string& outError) const;

    bool CreateFromDrag(
            SectorStructuralPrimitiveKind kind,
            SectorTopologyCoordPoint start,
            SectorTopologyCoordPoint end,
            float seedFloor,
            const std::string& defaultMaterialId,
            int* outPrimitiveId = nullptr);
    bool MutateById(
            int primitiveId,
            const char* status,
            const std::function<bool(SectorAuthoringStructuralPrimitive&)>& mutate);
    bool CommitPreviewValue(
            int primitiveId,
            const SectorAuthoringStructuralPrimitive& value,
            const char* status);
    bool DeleteSelected();

    bool BeginPreviewAdjustment();
    bool BuildPreviewNudge(
            float deltaXWorld,
            float deltaZWorld,
            float deltaHeightWorld,
            float deltaYawDegrees,
            SectorEditorStructuralPreviewCandidate& outCandidate);
    void AcceptPreviewNudge(SectorEditorStructuralPreviewCandidate candidate);
    bool ApplyPreviewAdjustment();
    bool CancelPreviewAdjustment(const char* message = nullptr);

private:
    bool CommitGraphCandidate(
            SectorAuthoringGraph candidateGraph,
            const char* successStatus,
            bool preserveSelection = true);

    SectorEditorStructuralPrimitiveEditingServiceContext context_;
};

void TranslateSectorStructuralPrimitiveHeight(
        SectorAuthoringStructuralPrimitive& primitive,
        float deltaAuthored);

} // namespace game
