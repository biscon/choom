#include "sector_editor/services/structural_primitives/SectorEditorStructuralPrimitiveEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace game {
namespace {

float WrapDegrees(float degrees)
{
    degrees = std::fmod(degrees, 360.0f);
    if (degrees < 0.0f) degrees += 360.0f;
    return degrees;
}

SectorCoord RoundedCoordDelta(float worldDelta)
{
    const float authored = SectorWorldToAuthoringDistance(worldDelta);
    const double ticks = static_cast<double>(authored)
            * static_cast<double>(SectorCoordSubdivisions);
    return static_cast<SectorCoord>(std::llround(ticks));
}

bool SamePrimitive(
        const SectorAuthoringStructuralPrimitive& a,
        const SectorAuthoringStructuralPrimitive& b)
{
    const auto sameUv = [](const SectorTopologyUvSettings& lhs,
                           const SectorTopologyUvSettings& rhs) {
        return lhs.scale.x == rhs.scale.x && lhs.scale.y == rhs.scale.y
                && lhs.offset.x == rhs.offset.x && lhs.offset.y == rhs.offset.y;
    };
    const auto sameSettings = [&sameUv](
            const SectorStructuralMaterialSettings& lhs,
            const SectorStructuralMaterialSettings& rhs) {
        return lhs.materialId == rhs.materialId && sameUv(lhs.uv, rhs.uv);
    };
    if (!sameSettings(a.materials.defaultSurface, b.materials.defaultSurface)) return false;
    for (size_t index = 0; index < a.materials.overrides.size(); ++index) {
        const auto& lhs = a.materials.overrides[index];
        const auto& rhs = b.materials.overrides[index];
        if (lhs.enabled != rhs.enabled
                || !sameSettings(lhs.settings, rhs.settings)) return false;
    }
    return a.id == b.id && a.kind == b.kind && a.enabled == b.enabled
            && a.x == b.x && a.z == b.z && a.yawDegrees == b.yawDegrees
            && a.pitchDegrees == b.pitchDegrees
            && a.rollDegrees == b.rollDegrees
            && a.collision == b.collision
            && a.receivesLightmap == b.receivesLightmap
            && a.castsBakedShadow == b.castsBakedShadow
            && a.castsDynamicShadow == b.castsDynamicShadow
            && a.box.width == b.box.width && a.box.depth == b.box.depth
            && a.box.bottom == b.box.bottom && a.box.top == b.box.top
            && a.ramp.width == b.ramp.width && a.ramp.run == b.ramp.run
            && a.ramp.solidBottom == b.ramp.solidBottom
            && a.ramp.low == b.ramp.low && a.ramp.high == b.ramp.high
            && a.stairs.width == b.stairs.width && a.stairs.run == b.stairs.run
            && a.stairs.bottom == b.stairs.bottom && a.stairs.rise == b.stairs.rise
            && a.stairs.stepCount == b.stairs.stepCount
            && a.cylinder.radius == b.cylinder.radius
            && a.cylinder.bottom == b.cylinder.bottom
            && a.cylinder.top == b.cylinder.top
            && a.cylinder.radialSegments == b.cylinder.radialSegments
            && a.sphere.radius == b.sphere.radius
            && a.sphere.centerHeight == b.sphere.centerHeight
            && a.sphere.latitudeSegments == b.sphere.latitudeSegments
            && a.sphere.longitudeSegments == b.sphere.longitudeSegments;
}

} // namespace

void TranslateSectorStructuralPrimitiveHeight(
        SectorAuthoringStructuralPrimitive& primitive,
        float deltaAuthored)
{
    switch (primitive.kind) {
        case SectorStructuralPrimitiveKind::Box:
            primitive.box.bottom += deltaAuthored;
            primitive.box.top += deltaAuthored;
            break;
        case SectorStructuralPrimitiveKind::Ramp:
            primitive.ramp.solidBottom += deltaAuthored;
            primitive.ramp.low += deltaAuthored;
            primitive.ramp.high += deltaAuthored;
            break;
        case SectorStructuralPrimitiveKind::Stairs:
            primitive.stairs.bottom += deltaAuthored;
            break;
        case SectorStructuralPrimitiveKind::Cylinder:
            primitive.cylinder.bottom += deltaAuthored;
            primitive.cylinder.top += deltaAuthored;
            break;
        case SectorStructuralPrimitiveKind::Sphere:
            primitive.sphere.centerHeight += deltaAuthored;
            break;
    }
}

SectorEditorStructuralPrimitiveEditingService::
SectorEditorStructuralPrimitiveEditingService(
        SectorEditorStructuralPrimitiveEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringStructuralPrimitive*
SectorEditorStructuralPrimitiveEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::StructuralPrimitive) return nullptr;
    return FindSectorAuthoringStructuralPrimitive(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.structuralPrimitiveId);
}

const SectorAuthoringStructuralPrimitive*
SectorEditorStructuralPrimitiveEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::StructuralPrimitive) return nullptr;
    return FindSectorAuthoringStructuralPrimitive(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.structuralPrimitiveId);
}

bool SectorEditorStructuralPrimitiveEditingService::Select(int primitiveId)
{
    return SelectSectorEditorAuthoringStructuralPrimitive(
            context_.authoringGraph, context_.selectionState, primitiveId);
}

bool SectorEditorStructuralPrimitiveEditingService::ResolvePlacementFloor(
        SectorTopologyCoordPoint point,
        float& outFloor) const
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) return false;
    int sectorId = -1;
    if (!ResolveSectorAuthoringPointToDerivedSector(
                context_.derivation.authoringDerivation, point, &sectorId)) return false;
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.derivation.authoringDerivation.topology, sectorId);
    if (sector == nullptr) return false;
    outFloor = sector->floorZ;
    return true;
}

bool SectorEditorStructuralPrimitiveEditingService::BuildPlacementValue(
        SectorStructuralPrimitiveKind kind,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        float seedFloor,
        const std::string& defaultMaterialId,
        int primitiveId,
        SectorAuthoringStructuralPrimitive& primitive,
        std::string& outError) const
{
    const int64_t dx64 = static_cast<int64_t>(end.x) - start.x;
    const int64_t dz64 = static_cast<int64_t>(end.y) - start.y;
    const int64_t absDx = std::llabs(dx64);
    const int64_t absDz = std::llabs(dz64);
    if (absDx > std::numeric_limits<SectorCoord>::max()
            || absDz > std::numeric_limits<SectorCoord>::max()) {
        outError = "Structure placement rejected: footprint is outside coordinate range";
        return false;
    }
    const SectorCoord dx = static_cast<SectorCoord>(absDx);
    const SectorCoord dz = static_cast<SectorCoord>(absDz);
    primitive = DefaultSectorAuthoringStructuralPrimitive(kind);
    primitive.id = primitiveId;
    primitive.materials.defaultSurface.materialId = defaultMaterialId;

    if (kind == SectorStructuralPrimitiveKind::Cylinder
            || kind == SectorStructuralPrimitiveKind::Sphere) {
        const double radius = std::hypot(
                static_cast<double>(dx64), static_cast<double>(dz64));
        if (!std::isfinite(radius)
                || radius > std::numeric_limits<SectorCoord>::max()) {
            outError = "Structure placement rejected: radius is outside coordinate range";
            return false;
        }
        const SectorCoord authoredRadius =
                static_cast<SectorCoord>(std::llround(radius));
        if (authoredRadius < SectorStructuralMinimumPlanarExtent) {
            outError = "Structure placement rejected: radius is too small";
            return false;
        }
        primitive.x = start.x;
        primitive.z = start.y;
        if (kind == SectorStructuralPrimitiveKind::Cylinder) {
            primitive.cylinder.radius = authoredRadius;
            const float span = primitive.cylinder.top - primitive.cylinder.bottom;
            primitive.cylinder.bottom = seedFloor;
            primitive.cylinder.top = seedFloor + span;
        } else {
            primitive.sphere.radius = authoredRadius;
            primitive.sphere.centerHeight = seedFloor
                    + SectorCoordToVisibleAuthoring(authoredRadius);
        }
    } else {
        if (dx < SectorStructuralMinimumPlanarExtent
                || dz < SectorStructuralMinimumPlanarExtent) {
            outError = "Structure placement rejected: footprint is too small";
            return false;
        }
        primitive.x = static_cast<SectorCoord>(
                (static_cast<int64_t>(start.x) + end.x) / 2);
        primitive.z = static_cast<SectorCoord>(
                (static_cast<int64_t>(start.y) + end.y) / 2);
        if (kind == SectorStructuralPrimitiveKind::Box) {
            primitive.box.width = dx;
            primitive.box.depth = dz;
            const float span = primitive.box.top - primitive.box.bottom;
            primitive.box.bottom = seedFloor;
            primitive.box.top = seedFloor + span;
        } else {
            const bool runAlongZ = dz >= dx;
            const SectorCoord width = runAlongZ ? dx : dz;
            const SectorCoord run = runAlongZ ? dz : dx;
            primitive.yawDegrees = runAlongZ
                    ? (dz64 >= 0 ? 0.0f : 180.0f)
                    : (dx64 >= 0 ? 270.0f : 90.0f);
            if (kind == SectorStructuralPrimitiveKind::Ramp) {
                primitive.ramp.width = width;
                primitive.ramp.run = run;
                const float rise = primitive.ramp.high - primitive.ramp.low;
                primitive.ramp.solidBottom = seedFloor;
                primitive.ramp.low = seedFloor;
                primitive.ramp.high = seedFloor + rise;
            } else {
                primitive.stairs.width = width;
                primitive.stairs.run = run;
                primitive.stairs.bottom = seedFloor;
            }
        }
    }
    outError.clear();
    return true;
}

bool SectorEditorStructuralPrimitiveEditingService::CommitGraphCandidate(
        SectorAuthoringGraph candidateGraph,
        const char* successStatus,
        bool preserveSelection)
{
    SectorAuthoringDerivationResult candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidateGraph);
    if (!candidateDerivation.success) {
        context_.statusText = "Structural primitive edit rejected by authoring derivation";
        return false;
    }
    const SectorAuthoringSelectionTarget previousSelection =
            context_.selectionState.selectedAuthoring;
    const bool committed = CommitSectorEditorAuthoringGraphCandidate(
            context_.state,
            context_.lifecycle,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            context_.selectionState,
            std::move(candidateGraph),
            std::move(candidateDerivation),
            context_.topologyMap,
            successStatus);
    if (committed && preserveSelection
            && previousSelection.kind
                    == SectorAuthoringSelectionKind::StructuralPrimitive) {
        Select(previousSelection.structuralPrimitiveId);
    }
    context_.statusText = context_.derivation.authoringDerivationStatus;
    return committed;
}

bool SectorEditorStructuralPrimitiveEditingService::CreateFromDrag(
        SectorStructuralPrimitiveKind kind,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        float seedFloor,
        const std::string& defaultMaterialId,
        int* outPrimitiveId)
{
    const int id = AllocateSectorAuthoringStructuralPrimitiveId(
            context_.authoringGraph);
    if (!IsValidSectorAuthoringId(id)) {
        context_.statusText = "Structure placement failed: no authoring ID is available";
        return false;
    }
    SectorAuthoringStructuralPrimitive primitive;
    std::string error;
    if (!BuildPlacementValue(
                kind, start, end, seedFloor, defaultMaterialId, id,
                primitive, error)) {
        context_.statusText = error;
        return false;
    }

    SectorAuthoringGraph candidate = context_.authoringGraph;
    candidate.structuralPrimitives.push_back(primitive);
    if (!CommitGraphCandidate(
                std::move(candidate),
                TextFormat("Created %s structure %d",
                        SectorStructuralPrimitiveKindName(kind), id),
                false)) return false;
    Select(id);
    if (outPrimitiveId != nullptr) *outPrimitiveId = id;
    return true;
}

bool SectorEditorStructuralPrimitiveEditingService::MutateById(
        int primitiveId,
        const char* status,
        const std::function<bool(SectorAuthoringStructuralPrimitive&)>& mutate)
{
    if (!mutate) return false;
    SectorAuthoringGraph candidate = context_.authoringGraph;
    SectorAuthoringStructuralPrimitive* primitive =
            FindSectorAuthoringStructuralPrimitive(candidate, primitiveId);
    if (primitive == nullptr || !mutate(*primitive)) return false;
    primitive->yawDegrees = WrapDegrees(primitive->yawDegrees);
    primitive->pitchDegrees = WrapDegrees(primitive->pitchDegrees);
    primitive->rollDegrees = WrapDegrees(primitive->rollDegrees);
    return CommitGraphCandidate(std::move(candidate), status);
}

bool SectorEditorStructuralPrimitiveEditingService::CommitPreviewValue(
        int primitiveId,
        const SectorAuthoringStructuralPrimitive& value,
        const char* status)
{
    return MutateById(primitiveId, status,
            [&value](SectorAuthoringStructuralPrimitive& target) {
                if (SamePrimitive(target, value)) return false;
                target = value;
                return true;
            });
}

bool SectorEditorStructuralPrimitiveEditingService::DeleteSelected()
{
    const SectorAuthoringStructuralPrimitive* selected = Selected();
    if (selected == nullptr) return false;
    const int id = selected->id;
    SectorAuthoringGraph candidate = context_.authoringGraph;
    candidate.structuralPrimitives.erase(
            std::remove_if(candidate.structuralPrimitives.begin(),
                    candidate.structuralPrimitives.end(),
                    [id](const SectorAuthoringStructuralPrimitive& primitive) {
                        return primitive.id == id;
                    }),
            candidate.structuralPrimitives.end());
    if (!CommitGraphCandidate(std::move(candidate),
                TextFormat("Deleted structure %d", id), false)) return false;
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return true;
}

bool SectorEditorStructuralPrimitiveEditingService::BeginPreviewAdjustment()
{
    PreviewStructuralPrimitiveAdjustmentState& adjustment =
            context_.editingState.previewAdjustment;
    const SectorAuthoringStructuralPrimitive* selected = Selected();
    if (adjustment.active || selected == nullptr
            || !IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Select a structure with current authoring derivation";
        return false;
    }
    const int preset = adjustment.preset;
    adjustment = PreviewStructuralPrimitiveAdjustmentState{};
    adjustment.active = true;
    adjustment.primitiveId = selected->id;
    adjustment.preset = preset;
    adjustment.stagedGraph = context_.authoringGraph;
    adjustment.stagedDerivation = context_.derivation.authoringDerivation;
    context_.statusText = TextFormat("Adjusting structure %d", selected->id);
    return true;
}

bool SectorEditorStructuralPrimitiveEditingService::BuildPreviewNudge(
        float deltaXWorld,
        float deltaZWorld,
        float deltaHeightWorld,
        float deltaYawDegrees,
        float deltaPitchDegrees,
        float deltaRollDegrees,
        SectorEditorStructuralPreviewCandidate& outCandidate)
{
    outCandidate = SectorEditorStructuralPreviewCandidate{};
    PreviewStructuralPrimitiveAdjustmentState& adjustment =
            context_.editingState.previewAdjustment;
    if (!adjustment.active) return false;
    SectorAuthoringGraph candidate = adjustment.stagedGraph;
    SectorAuthoringStructuralPrimitive* primitive =
            FindSectorAuthoringStructuralPrimitive(candidate, adjustment.primitiveId);
    const SectorAuthoringStructuralPrimitive* original =
            FindSectorAuthoringStructuralPrimitive(
                    context_.authoringGraph, adjustment.primitiveId);
    if (primitive == nullptr || original == nullptr) return false;
    primitive->x += RoundedCoordDelta(deltaXWorld);
    primitive->z += RoundedCoordDelta(deltaZWorld);
    TranslateSectorStructuralPrimitiveHeight(
            *primitive, SectorWorldToAuthoringDistance(deltaHeightWorld));
    primitive->yawDegrees = WrapDegrees(primitive->yawDegrees + deltaYawDegrees);
    primitive->pitchDegrees = WrapDegrees(
            primitive->pitchDegrees + deltaPitchDegrees);
    primitive->rollDegrees = WrapDegrees(
            primitive->rollDegrees + deltaRollDegrees);
    SectorAuthoringDerivationResult derivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidate);
    if (!derivation.success) {
        context_.statusText = "Structure nudge rejected by authoring derivation";
        return false;
    }
    CopySectorEditorMapLevelFields(derivation.topology, context_.topologyMap);
    outCandidate.valid = true;
    outCandidate.changedFromOriginal = !SamePrimitive(*primitive, *original);
    outCandidate.graph = std::move(candidate);
    outCandidate.derivation = std::move(derivation);
    return true;
}

void SectorEditorStructuralPrimitiveEditingService::AcceptPreviewNudge(
        SectorEditorStructuralPreviewCandidate candidate)
{
    if (!candidate.valid || !context_.editingState.previewAdjustment.active) return;
    PreviewStructuralPrimitiveAdjustmentState& adjustment =
            context_.editingState.previewAdjustment;
    adjustment.changed = candidate.changedFromOriginal;
    adjustment.stagedGraph = std::move(candidate.graph);
    adjustment.stagedDerivation = std::move(candidate.derivation);
    context_.statusText = TextFormat("Adjusting structure %d", adjustment.primitiveId);
}

bool SectorEditorStructuralPrimitiveEditingService::ApplyPreviewAdjustment()
{
    PreviewStructuralPrimitiveAdjustmentState& adjustment =
            context_.editingState.previewAdjustment;
    if (!adjustment.active) return false;
    const int id = adjustment.primitiveId;
    const int preset = adjustment.preset;
    if (!adjustment.changed) {
        adjustment = PreviewStructuralPrimitiveAdjustmentState{};
        adjustment.preset = preset;
        context_.statusText = TextFormat("Structure %d unchanged", id);
        return false;
    }
    const bool committed = CommitSectorEditorAuthoringGraphCandidate(
            context_.state, context_.lifecycle, context_.topologyMap,
            context_.authoringGraph, context_.derivation,
            context_.selectionState, std::move(adjustment.stagedGraph),
            std::move(adjustment.stagedDerivation), context_.topologyMap,
            TextFormat("Adjusted structure %d in 3D", id));
    adjustment = PreviewStructuralPrimitiveAdjustmentState{};
    adjustment.preset = preset;
    if (committed) Select(id);
    return committed;
}

bool SectorEditorStructuralPrimitiveEditingService::CancelPreviewAdjustment(
        const char* message)
{
    PreviewStructuralPrimitiveAdjustmentState& adjustment =
            context_.editingState.previewAdjustment;
    if (!adjustment.active) return false;
    const bool changed = adjustment.changed;
    const int preset = adjustment.preset;
    adjustment = PreviewStructuralPrimitiveAdjustmentState{};
    adjustment.preset = preset;
    context_.statusText = message != nullptr && message[0] != '\0'
            ? message : "Structure adjustment cancelled";
    return changed;
}

} // namespace game
