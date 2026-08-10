#include "sector_editor/services/authoring_faces/SectorEditorAuthoringFaceMergeService.h"

#include "sector_editor/document/SectorEditorDocumentActions.h"
#include "sector_editor/services/runtime_objects/SectorEditorAuthoringDoorReconciliation.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

namespace game {
namespace {

struct FaceBoundaryEdgeInfo {
    int planarEdgeId = -1;
    int authoringLineId = -1;
    std::set<int> faceAnchorIds;
};

const SectorAuthoringResolvedFaceMapping* FindResolvedFace(
        const SectorAuthoringDerivationResult& derivation,
        int faceAnchorId)
{
    const SectorAuthoringResolvedFaceMapping* found = nullptr;
    for (const SectorAuthoringResolvedFaceMapping& mapping
            : derivation.mapping.resolvedFaces) {
        if (mapping.faceAnchorId != faceAnchorId) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &mapping;
    }
    return found;
}

const SectorAuthoringExtractedFace* FindExtractedFace(
        const SectorAuthoringDerivationResult& derivation,
        int extractedFaceId)
{
    for (const SectorAuthoringExtractedFace& face : derivation.faces.faces) {
        if (face.id == extractedFaceId) {
            return &face;
        }
    }
    return nullptr;
}

int FindFaceAnchorIdForTopologySector(
        const SectorAuthoringDerivationResult& derivation,
        int topologySectorId)
{
    int found = -1;
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : derivation.mapping.sectors) {
        if (mapping.topologySectorId != topologySectorId
                || !IsValidSectorAuthoringId(mapping.faceAnchorId)) {
            continue;
        }
        if (found >= 0 && found != mapping.faceAnchorId) {
            return -1;
        }
        found = mapping.faceAnchorId;
    }
    return found;
}

std::map<int, FaceBoundaryEdgeInfo> BuildFaceBoundaryEdges(
        const SectorAuthoringDerivationResult& derivation)
{
    std::map<int, FaceBoundaryEdgeInfo> edges;
    for (const SectorAuthoringDerivedLineMapping& mapping : derivation.mapping.lines) {
        FaceBoundaryEdgeInfo& edge = edges[mapping.planarEdgeId];
        edge.planarEdgeId = mapping.planarEdgeId;
        edge.authoringLineId = mapping.authoringLineId;
    }

    for (const SectorAuthoringDerivedSideMapping& side : derivation.mapping.sides) {
        const SectorAuthoringDerivedLineMapping* line = nullptr;
        for (const SectorAuthoringDerivedLineMapping& candidate
                : derivation.mapping.lines) {
            if (candidate.topologyLineDefId != side.topologyLineDefId) {
                continue;
            }
            if (line != nullptr) {
                line = nullptr;
                break;
            }
            line = &candidate;
        }
        const int faceAnchorId =
                FindFaceAnchorIdForTopologySector(derivation, side.topologySectorId);
        if (line == nullptr || !IsValidSectorAuthoringId(faceAnchorId)) {
            continue;
        }
        FaceBoundaryEdgeInfo& edge = edges[line->planarEdgeId];
        edge.planarEdgeId = line->planarEdgeId;
        edge.authoringLineId = line->authoringLineId;
        edge.faceAnchorIds.insert(faceAnchorId);
    }

    for (const SectorAuthoringResolvedFaceMapping& mapping
            : derivation.mapping.resolvedFaces) {
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)) {
            continue;
        }
        const SectorAuthoringExtractedFace* face =
                FindExtractedFace(derivation, mapping.extractedFaceId);
        if (face == nullptr) {
            continue;
        }
        for (const SectorAuthoringFaceBoundaryEdge& boundary : face->boundary) {
            FaceBoundaryEdgeInfo& edge = edges[boundary.planarEdgeId];
            edge.planarEdgeId = boundary.planarEdgeId;
            edge.authoringLineId = boundary.sourceLineId;
            edge.faceAnchorIds.insert(mapping.faceAnchorId);
        }
    }
    return edges;
}

std::vector<int> SortedUniqueFaceIds(const SelectionState& selection)
{
    std::vector<int> ids = selection.selectedAuthoringFaceAnchorIds;
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

bool AllSelectedFacesReachTarget(
        const std::map<int, FaceBoundaryEdgeInfo>& edges,
        const std::set<int>& selected,
        int targetFaceAnchorId)
{
    std::map<int, std::vector<int>> adjacency;
    std::set<int> region = selected;
    region.insert(targetFaceAnchorId);
    for (const auto& entry : edges) {
        const std::set<int>& faces = entry.second.faceAnchorIds;
        if (faces.size() != 2) {
            continue;
        }
        auto it = faces.begin();
        const int first = *it++;
        const int second = *it;
        if (region.find(first) == region.end()
                || region.find(second) == region.end()
                || (selected.find(first) == selected.end()
                        && selected.find(second) == selected.end())) {
            continue;
        }
        adjacency[first].push_back(second);
        adjacency[second].push_back(first);
    }

    std::set<int> visited;
    std::queue<int> pending;
    pending.push(targetFaceAnchorId);
    visited.insert(targetFaceAnchorId);
    while (!pending.empty()) {
        const int current = pending.front();
        pending.pop();
        for (int neighbor : adjacency[current]) {
            if (visited.insert(neighbor).second) {
                pending.push(neighbor);
            }
        }
    }
    return std::all_of(
            selected.begin(),
            selected.end(),
            [&visited](int id) { return visited.find(id) != visited.end(); });
}

bool DerivationHasCompleteFaceAnchors(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationResult& derivation)
{
    if (!derivation.success) {
        return false;
    }
    for (const SectorAuthoringExtractedFace& face : derivation.faces.faces) {
        int mappingCount = 0;
        for (const SectorAuthoringResolvedFaceMapping& mapping
                : derivation.mapping.resolvedFaces) {
            if (mapping.extractedFaceId != face.id
                    || !IsValidSectorAuthoringId(mapping.faceAnchorId)
                    || FindSectorAuthoringFaceAnchor(graph, mapping.faceAnchorId) == nullptr) {
                continue;
            }
            ++mappingCount;
        }
        if (mappingCount != 1) {
            return false;
        }
    }
    return true;
}

} // namespace

SectorEditorAuthoringFaceMergeService::SectorEditorAuthoringFaceMergeService(
        SectorEditorAuthoringFaceMergeServiceContext context)
    : context_(std::move(context))
{
}

bool SectorEditorAuthoringFaceMergeService::BeginTargetPick()
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText =
                "Merge Selected Into requires current valid derived topology";
        return false;
    }
    const std::vector<int> selected = SortedUniqueFaceIds(context_.selectionState);
    if (selected.empty()) {
        context_.statusText = "Select one or more authoring faces to merge";
        return false;
    }
    for (int faceAnchorId : selected) {
        if (FindSectorAuthoringFaceAnchor(context_.authoringGraph, faceAnchorId)
                == nullptr) {
            context_.statusText = "Merge selection contains a stale face anchor";
            return false;
        }
    }
    context_.mergeState.choosingTarget = true;
    context_.mergeState.hoveredTargetFaceAnchorId = -1;
    context_.state.currentTool = SectorEditorTool::Select;
    context_.state.pendingAuthoringLine = PendingAuthoringLineDraw{};
    context_.state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    context_.state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    context_.statusText =
            "Merge Selected Into: click the surviving adjacent face; Esc cancels";
    return true;
}

void SectorEditorAuthoringFaceMergeService::CancelTargetPick(const char* status)
{
    context_.mergeState = SectorEditorAuthoringFaceMergeState{};
    if (status != nullptr && status[0] != '\0') {
        context_.statusText = status;
    }
}

bool SectorEditorAuthoringFaceMergeService::IsChoosingTarget() const
{
    return context_.mergeState.choosingTarget;
}

int SectorEditorAuthoringFaceMergeService::FindTargetAtMapPoint(
        Vector2 mapPoint,
        bool requireEligible) const
{
    int faceAnchorId = -1;
    if (!FindSectorEditorAuthoringFaceAnchorAtMapPoint(
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
                IsSectorEditorAuthoringDerivationCurrent(context_.derivation),
                mapPoint,
                &faceAnchorId)) {
        return -1;
    }
    if (IsSectorEditorAuthoringFaceSelected(context_.selectionState, faceAnchorId)) {
        return -1;
    }
    const SectorAuthoringFaceAnchor* anchor =
            FindSectorAuthoringFaceAnchor(context_.authoringGraph, faceAnchorId);
    const SectorAuthoringResolvedFaceMapping* resolved =
            FindResolvedFace(context_.derivation.authoringDerivation, faceAnchorId);
    if (anchor == nullptr || anchor->isVoid || resolved == nullptr
            || resolved->kind != SectorAuthoringFaceResolutionKind::DerivedSector) {
        return -1;
    }
    if (!requireEligible) {
        return faceAnchorId;
    }

    const std::vector<int> selectedIds =
            SortedUniqueFaceIds(context_.selectionState);
    const std::set<int> selected(selectedIds.begin(), selectedIds.end());
    return AllSelectedFacesReachTarget(
                   BuildFaceBoundaryEdges(context_.derivation.authoringDerivation),
                   selected,
                   faceAnchorId)
            ? faceAnchorId
            : -1;
}

void SectorEditorAuthoringFaceMergeService::UpdateTargetHover(Vector2 mapPoint)
{
    context_.mergeState.hoveredTargetFaceAnchorId =
            FindTargetAtMapPoint(mapPoint, false);
}

bool SectorEditorAuthoringFaceMergeService::RequestMergeAtTarget(
        int targetFaceAnchorId)
{
    SectorEditorAuthoringFaceMergePlan plan = BuildPlan(targetFaceAnchorId);
    if (!plan.valid) {
        context_.statusText = plan.status;
        return false;
    }
    const std::string message =
            BuildSectorEditorAuthoringFaceMergeConfirmationMessage(
                    plan,
                    context_.authoringGraph);
    context_.mergeState = SectorEditorAuthoringFaceMergeState{};
    OpenConfirmationModal(
            context_.state.confirmationModal,
            "Merge Selected Faces",
            message.c_str(),
            [this, confirmedPlan = std::move(plan)]() mutable {
                CommitPlan(std::move(confirmedPlan));
            });
    return true;
}

SectorEditorAuthoringFaceMergePlan
SectorEditorAuthoringFaceMergeService::BuildPlan(int targetFaceAnchorId) const
{
    SectorEditorAuthoringFaceMergePlan plan;
    plan.targetFaceAnchorId = targetFaceAnchorId;
    plan.selectedFaceAnchorIds = SortedUniqueFaceIds(context_.selectionState);
    const auto fail = [&plan](std::string status) {
        plan.status = std::move(status);
        return plan;
    };

    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        return fail("Merge Selected Into requires current valid derived topology");
    }
    if (plan.selectedFaceAnchorIds.empty()) {
        return fail("Select one or more authoring faces to merge");
    }
    const std::set<int> selected(
            plan.selectedFaceAnchorIds.begin(),
            plan.selectedFaceAnchorIds.end());
    if (selected.find(targetFaceAnchorId) != selected.end()) {
        return fail("The surviving face cannot also be selected for removal");
    }
    const SectorAuthoringFaceAnchor* target =
            FindSectorAuthoringFaceAnchor(context_.authoringGraph, targetFaceAnchorId);
    const SectorAuthoringResolvedFaceMapping* targetResolved =
            FindResolvedFace(context_.derivation.authoringDerivation, targetFaceAnchorId);
    if (target == nullptr || target->isVoid || targetResolved == nullptr
            || targetResolved->kind != SectorAuthoringFaceResolutionKind::DerivedSector) {
        return fail("Choose a current non-void authoring face as the survivor");
    }
    for (int faceAnchorId : selected) {
        if (FindSectorAuthoringFaceAnchor(context_.authoringGraph, faceAnchorId)
                == nullptr) {
            return fail("Merge selection contains a stale face anchor");
        }
    }

    const std::map<int, FaceBoundaryEdgeInfo> edges =
            BuildFaceBoundaryEdges(context_.derivation.authoringDerivation);
    if (!AllSelectedFacesReachTarget(edges, selected, targetFaceAnchorId)) {
        return fail(
                "Every selected face must connect to the survivor through selected faces");
    }

    std::set<int> region = selected;
    region.insert(targetFaceAnchorId);
    std::set<int> removableEdgeIds;
    std::set<int> removableLineIds;
    for (const auto& entry : edges) {
        const FaceBoundaryEdgeInfo& edge = entry.second;
        if (edge.faceAnchorIds.size() != 2
                || !IsValidSectorAuthoringId(edge.authoringLineId)) {
            continue;
        }
        bool allInRegion = true;
        bool touchesSelected = false;
        for (int faceAnchorId : edge.faceAnchorIds) {
            allInRegion = allInRegion
                    && region.find(faceAnchorId) != region.end();
            touchesSelected = touchesSelected
                    || selected.find(faceAnchorId) != selected.end();
        }
        if (allInRegion && touchesSelected) {
            removableEdgeIds.insert(edge.planarEdgeId);
            removableLineIds.insert(edge.authoringLineId);
        }
    }
    if (removableLineIds.empty()) {
        return fail("Selected faces share no removable boundary with the survivor");
    }

    std::map<int, std::set<int>> planarEdgesByAuthoringLine;
    for (const SectorAuthoringDerivedLineMapping& mapping
            : context_.derivation.authoringDerivation.mapping.lines) {
        planarEdgesByAuthoringLine[mapping.authoringLineId].insert(mapping.planarEdgeId);
    }
    for (int lineId : removableLineIds) {
        const auto found = planarEdgesByAuthoringLine.find(lineId);
        if (found == planarEdgesByAuthoringLine.end()) {
            return fail("Merge boundary is not mapped to a current authoring line");
        }
        for (int planarEdgeId : found->second) {
            if (removableEdgeIds.find(planarEdgeId) == removableEdgeIds.end()) {
                return fail(TextFormat(
                        "Authoring line %d spans both removed and retained boundaries; split it first",
                        lineId));
            }
        }
    }

    plan.removedAuthoringLineIds.assign(
            removableLineIds.begin(),
            removableLineIds.end());
    plan.candidateGraph = context_.authoringGraph;
    std::set<int> removedEndpointIds;
    for (const SectorAuthoringLine& line : plan.candidateGraph.lines) {
        if (removableLineIds.find(line.id) != removableLineIds.end()) {
            removedEndpointIds.insert(line.startVertexId);
            removedEndpointIds.insert(line.endVertexId);
        }
    }
    plan.candidateGraph.lines.erase(
            std::remove_if(
                    plan.candidateGraph.lines.begin(),
                    plan.candidateGraph.lines.end(),
                    [&removableLineIds](const SectorAuthoringLine& line) {
                        return removableLineIds.find(line.id) != removableLineIds.end();
                    }),
            plan.candidateGraph.lines.end());
    const std::size_t lineSideCountBefore = plan.candidateGraph.lineSides.size();
    plan.candidateGraph.lineSides.erase(
            std::remove_if(
                    plan.candidateGraph.lineSides.begin(),
                    plan.candidateGraph.lineSides.end(),
                    [&removableLineIds](const SectorAuthoringLineSide& side) {
                        return removableLineIds.find(side.id.lineId)
                                != removableLineIds.end();
                    }),
            plan.candidateGraph.lineSides.end());
    plan.removedLineSideCount = static_cast<int>(
            lineSideCountBefore - plan.candidateGraph.lineSides.size());
    plan.candidateGraph.faceAnchors.erase(
            std::remove_if(
                    plan.candidateGraph.faceAnchors.begin(),
                    plan.candidateGraph.faceAnchors.end(),
                    [&selected](const SectorAuthoringFaceAnchor& anchor) {
                        return selected.find(anchor.id) != selected.end();
                    }),
            plan.candidateGraph.faceAnchors.end());

    std::set<int> referencedVertexIds;
    for (const SectorAuthoringLine& line : plan.candidateGraph.lines) {
        referencedVertexIds.insert(line.startVertexId);
        referencedVertexIds.insert(line.endVertexId);
    }
    plan.candidateGraph.vertices.erase(
            std::remove_if(
                    plan.candidateGraph.vertices.begin(),
                    plan.candidateGraph.vertices.end(),
                    [&removedEndpointIds, &referencedVertexIds, &plan](
                            const SectorAuthoringVertex& vertex) {
                        const bool remove = removedEndpointIds.find(vertex.id)
                                        != removedEndpointIds.end()
                                && referencedVertexIds.find(vertex.id)
                                        == referencedVertexIds.end();
                        if (remove) {
                            plan.removedAuthoringVertexIds.push_back(vertex.id);
                        }
                        return remove;
                    }),
            plan.candidateGraph.vertices.end());
    std::sort(
            plan.removedAuthoringVertexIds.begin(),
            plan.removedAuthoringVertexIds.end());

    plan.candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(plan.candidateGraph);
    if (!plan.candidateDerivation.success) {
        return fail("Merge candidate does not produce valid derived topology");
    }
    if (!DerivationHasCompleteFaceAnchors(
                plan.candidateGraph,
                plan.candidateDerivation)) {
        return fail("Merge candidate leaves a derived face without exactly one anchor");
    }

    plan.candidateMapData = context_.topologyMap;
    std::string doorError;
    const std::set<int> rejectDoorAuthoringLineIds;
    if (!ReconcileSectorEditorAuthoringCandidateDoors(
                context_.topologyMap,
                context_.derivation.authoringDerivation,
                plan.candidateDerivation,
                removableLineIds,
                rejectDoorAuthoringLineIds,
                plan.candidateMapData,
                plan.removedDoorObjectIds,
                doorError)) {
        return fail(std::move(doorError));
    }

    plan.valid = true;
    plan.status = TextFormat(
            "Merged %d authoring face%s into face %d",
            static_cast<int>(plan.selectedFaceAnchorIds.size()),
            plan.selectedFaceAnchorIds.size() == 1 ? "" : "s",
            targetFaceAnchorId);
    return plan;
}

bool SectorEditorAuthoringFaceMergeService::CommitPlan(
        SectorEditorAuthoringFaceMergePlan plan)
{
    if (!plan.valid) {
        context_.statusText = plan.status.empty()
                ? "Merge Selected Into failed"
                : plan.status;
        return false;
    }
    const int targetFaceAnchorId = plan.targetFaceAnchorId;
    const std::string status = plan.status;
    if (!CommitSectorEditorAuthoringGraphCandidate(
                context_.state,
                context_.lifecycle,
                context_.topologyMap,
                context_.authoringGraph,
                context_.derivation,
                context_.selectionState,
                std::move(plan.candidateGraph),
                std::move(plan.candidateDerivation),
                plan.candidateMapData,
                status.c_str())) {
        context_.statusText = "Merge Selected Into failed during commit";
        return false;
    }
    SelectSectorEditorAuthoringFaceAnchor(
            context_.authoringGraph,
            context_.selectionState,
            targetFaceAnchorId);
    context_.mergeState = SectorEditorAuthoringFaceMergeState{};
    context_.statusText = status;
    return true;
}

std::string BuildSectorEditorAuthoringFaceMergeConfirmationMessage(
        const SectorEditorAuthoringFaceMergePlan& plan,
        const SectorAuthoringGraph& graph)
{
    const SectorAuthoringFaceAnchor* target =
            FindSectorAuthoringFaceAnchor(graph, plan.targetFaceAnchorId);
    std::ostringstream message;
    message << "Merge face" << (plan.selectedFaceAnchorIds.size() == 1 ? " " : "s ");
    for (std::size_t index = 0; index < plan.selectedFaceAnchorIds.size(); ++index) {
        if (index > 0) {
            message << ", ";
        }
        message << plan.selectedFaceAnchorIds[index];
    }
    message << " into ";
    if (target != nullptr && !target->name.empty()) {
        message << target->name << " (" << target->id << ")";
    } else {
        message << "face " << plan.targetFaceAnchorId;
    }
    message << "?\n\nThis removes "
            << plan.removedAuthoringLineIds.size() << " line(s), "
            << plan.removedLineSideCount << " side record(s), "
            << plan.removedAuthoringVertexIds.size() << " unused vertex/vertices, and "
            << plan.removedDoorObjectIds.size() << " dependent door(s).";
    return message.str();
}

} // namespace game
