#include "sector_editor/SectorEditorMaterialActions.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

namespace {

bool IsFlatTarget(TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            || kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

SectorEditorMaterialActionResult Failure(const char* status)
{
    SectorEditorMaterialActionResult result;
    result.status = status == nullptr ? "" : status;
    return result;
}

SectorEditorMaterialActionResult Changed(std::string status)
{
    SectorEditorMaterialActionResult result;
    result.changed = true;
    result.status = std::move(status);
    return result;
}

bool ValidateUvScale(float scale)
{
    return std::isfinite(scale)
            && scale >= TopologyUvScaleMin
            && scale <= TopologyUvScaleMax;
}

} // namespace

bool IsValidMaterialSurfaceTarget(const SectorTopologyMap& map, TopologySurfaceEditTarget target)
{
    switch (target.kind) {
        case TopologySurfaceEditTargetKind::SectorFloor:
        case TopologySurfaceEditTargetKind::SectorCeiling:
            return FindSectorTopologySector(map, target.sectorId) != nullptr;
        case TopologySurfaceEditTargetKind::SideDefWall:
        case TopologySurfaceEditTargetKind::SideDefLower:
        case TopologySurfaceEditTargetKind::SideDefUpper:
        case TopologySurfaceEditTargetKind::SideDefMiddle: {
            const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
            if (sideDef == nullptr
                    || sideDef->lineDefId != target.lineDefId
                    || sideDef->side != target.side) {
                return false;
            }
            return target.kind != TopologySurfaceEditTargetKind::SideDefMiddle
                    || IsTopologyMiddleEligible(map, sideDef);
        }
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return false;
}

const SectorTopologyDecalLayer* DecalForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    if (!IsValidMaterialSurfaceTarget(map, target) || IsMiddleTopologyEditTarget(target.kind)) {
        return nullptr;
    }

    if (IsFlatTarget(target.kind)) {
        const SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorDecal
                : &sector->ceilingDecal;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).decal;
}

SectorTopologyDecalLayer* MutableDecalForMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    if (!IsValidMaterialSurfaceTarget(map, target) || IsMiddleTopologyEditTarget(target.kind)) {
        return nullptr;
    }

    if (IsFlatTarget(target.kind)) {
        SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorDecal
                : &sector->ceilingDecal;
    }

    SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).decal;
}

const SectorTopologyUvSettings* UvForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return nullptr;
    }
    if (layer == TopologyMaterialLayer::Decal) {
        const SectorTopologyDecalLayer* decal = DecalForMaterialSurface(map, target);
        return decal == nullptr || decal->textureId.empty() ? nullptr : &decal->uv;
    }

    if (IsFlatTarget(target.kind)) {
        const SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorUv
                : &sector->ceilingUv;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).uv;
}

SectorTopologyUvSettings* MutableUvForMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return nullptr;
    }
    if (layer == TopologyMaterialLayer::Decal) {
        SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
        return decal == nullptr || decal->textureId.empty() ? nullptr : &decal->uv;
    }

    if (IsFlatTarget(target.kind)) {
        SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorUv
                : &sector->ceilingUv;
    }

    SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).uv;
}

bool IsMaterialDecalAssigned(const SectorTopologyMap& map, TopologySurfaceEditTarget target)
{
    const SectorTopologyDecalLayer* decal = DecalForMaterialSurface(map, target);
    return decal != nullptr && !decal->textureId.empty();
}

std::string CurrentTextureForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return std::string{"<none>"};
    }
    if (layer == TopologyMaterialLayer::Decal) {
        const SectorTopologyDecalLayer* decal = DecalForMaterialSurface(map, target);
        return decal == nullptr ? std::string{} : decal->textureId;
    }

    if (IsFlatTarget(target.kind)) {
        const SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return std::string{};
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? sector->floorTextureId
                : sector->ceilingTextureId;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return std::string{};
    }
    switch (target.kind) {
        case TopologySurfaceEditTargetKind::SideDefWall:
            return sideDef->wall.textureId;
        case TopologySurfaceEditTargetKind::SideDefLower:
            return sideDef->lower.textureId;
        case TopologySurfaceEditTargetKind::SideDefUpper:
            return sideDef->upper.textureId;
        case TopologySurfaceEditTargetKind::SideDefMiddle:
            return sideDef->middle.textureId;
        case TopologySurfaceEditTargetKind::SectorFloor:
        case TopologySurfaceEditTargetKind::SectorCeiling:
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return std::string{"<none>"};
}

bool CopyMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialPayload& outPayload,
        std::string& status)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        status = "Selected material target is no longer valid.";
        return false;
    }

    TopologyMaterialPayload payload;
    payload.valid = true;
    payload.kind = target.kind;

    if (IsFlatTarget(target.kind)) {
        const SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            status = "Selected material target is no longer valid.";
            return false;
        }
        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
            payload.textureId = sector->floorTextureId;
            payload.uv = sector->floorUv;
        } else {
            payload.textureId = sector->ceilingTextureId;
            payload.uv = sector->ceilingUv;
        }
    } else {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
        if (sideDef == nullptr) {
            status = "Selected material target is no longer valid.";
            return false;
        }
        const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                TopologyEditTargetWallPart(target.kind));
        payload.textureId = part.textureId;
        payload.uv = part.uv;
    }

    outPayload = payload;
    status = TextFormat("Copied %s material.", TopologyMaterialKindName(payload.kind));
    return true;
}

SectorEditorMaterialActionResult PasteMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        const TopologyMaterialPayload& payload)
{
    if (!payload.valid) {
        return Failure("No copied material.");
    }
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected material target is no longer valid.");
    }
    if (payload.kind != target.kind) {
        return Failure(TextFormat(
                "Copied material is for %s; selected target is %s.",
                TopologyMaterialKindName(payload.kind),
                TopologyMaterialKindName(target.kind)));
    }

    if (IsFlatTarget(target.kind)) {
        SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
        if (sector == nullptr) {
            return Failure("Selected material target is no longer valid.");
        }
        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
            sector->floorTextureId = payload.textureId;
            sector->floorUv = payload.uv;
        } else {
            sector->ceilingTextureId = payload.textureId;
            sector->ceilingUv = payload.uv;
        }
    } else {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
        if (sideDef == nullptr) {
            return Failure("Selected material target is no longer valid.");
        }
        SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                TopologyEditTargetWallPart(target.kind));
        part.textureId = payload.textureId;
        part.uv = payload.uv;
    }

    return Changed(TextFormat("Pasted %s material.", TopologyMaterialKindName(target.kind)));
}

SectorEditorMaterialActionResult ApplySurfaceUvValue(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        int component,
        float value)
{
    if (!IsValidMaterialSurfaceTarget(map, target) || !std::isfinite(value)) {
        return {};
    }
    if ((component == 0 || component == 1) && !ValidateUvScale(value)) {
        return {};
    }

    SectorTopologyUvSettings* uv = MutableUvForMaterialSurface(map, target, layer);
    if (uv == nullptr) {
        return Failure(layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.");
    }

    switch (component) {
        case 0: uv->scale.x = value; break;
        case 1: uv->scale.y = value; break;
        case 2: uv->offset.x = value; break;
        case 3: uv->offset.y = value; break;
        default: return {};
    }

    return Changed(TextFormat(
            "Updated 3D %s %s UV",
            SurfaceKindName(surfaceKind),
            TopologyMaterialLayerStatusName(layer)));
}

SectorEditorMaterialActionResult ApplySurfaceDecalOpacity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float opacity)
{
    if (!IsValidMaterialSurfaceTarget(map, target) || !std::isfinite(opacity)) {
        return {};
    }
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
    if (decal == nullptr || decal->textureId.empty()) {
        return Failure("No decal assigned.");
    }
    if (decal->opacity == opacity) {
        return {};
    }

    decal->opacity = opacity;
    return Changed(TextFormat("Set %s decal opacity.", TopologyMaterialKindName(target.kind)));
}

SectorEditorMaterialActionResult ApplySurfaceDecalEmissive(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        bool emissive)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected material target is no longer valid.");
    }
    SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
    if (decal == nullptr || decal->textureId.empty()) {
        return Failure("No decal assigned.");
    }
    if (decal->emissive == emissive) {
        return {};
    }

    decal->emissive = emissive;
    return Changed(TextFormat("Set %s decal emissive.", TopologyMaterialKindName(target.kind)));
}

SectorEditorMaterialActionResult ApplySurfaceDecalTint(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        Vector3 tint)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected material target is no longer valid.");
    }
    if (!IsValidDecalTint(tint)) {
        return Failure("Invalid decal tint.");
    }
    SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
    if (decal == nullptr || decal->textureId.empty()) {
        return Failure("No decal assigned.");
    }
    if (SameTint(decal->tint, tint)) {
        return {};
    }

    decal->tint = tint;
    return Changed(TextFormat("Set %s decal tint.", TopologyMaterialKindName(target.kind)));
}

SectorEditorMaterialActionResult ApplySurfaceDecalBloomIntensity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float bloomIntensity)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected material target is no longer valid.");
    }
    bloomIntensity = ClampDecalBloomIntensity(bloomIntensity);
    SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
    if (decal == nullptr || decal->textureId.empty()) {
        return Failure("No decal assigned.");
    }
    if (decal->bloomIntensity == bloomIntensity) {
        return {};
    }

    decal->bloomIntensity = bloomIntensity;
    return Changed(TextFormat("Set %s decal bloom intensity.", TopologyMaterialKindName(target.kind)));
}

bool BuildDecalTintModal(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        DecalTintModalState& outModal,
        std::string& status)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        status = "Selected material target is no longer valid.";
        return false;
    }
    const SectorTopologyDecalLayer* decal = DecalForMaterialSurface(map, target);
    if (decal == nullptr || decal->textureId.empty()) {
        status = "No decal assigned.";
        return false;
    }

    DecalTintModalState modal;
    modal.open = true;
    modal.target = target;
    modal.tint = ClampDecalTint(decal->tint);
    outModal = modal;
    status.clear();
    return true;
}

SectorEditorMaterialActionResult ClearSurfaceDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    SectorTopologyDecalLayer* decal = MutableDecalForMaterialSurface(map, target);
    if (decal == nullptr) {
        return Failure("Selected material target is no longer valid.");
    }
    if (IsDefaultDecalLayer(*decal)) {
        return Failure("No decal assigned.");
    }

    ResetDecalLayer(*decal);
    SectorEditorMaterialActionResult result = Changed(
            TextFormat("Cleared %s decal.", TopologyMaterialKindName(target.kind)));
    result.resetSurface3DUi = true;
    result.resetSectorUvInputs = true;
    result.resetSideDefUvInputs = true;
    result.resetDecalInputs = true;
    result.closeDecalTintModal = true;
    return result;
}

SectorEditorMaterialActionResult ClearMiddleTexture(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    if (target.kind != TopologySurfaceEditTargetKind::SideDefMiddle
            || !IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected middle texture target is no longer valid.");
    }

    SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr) {
        return Failure("Selected middle texture target is no longer valid.");
    }
    if (IsDefaultWallPartSettings(sideDef->middle)) {
        return Failure("No middle texture assigned.");
    }

    sideDef->middle = SectorTopologyWallPartSettings{};
    SectorEditorMaterialActionResult result = Changed("Cleared middle texture.");
    result.resetSurface3DUi = true;
    result.resetSideDefUvInputs = true;
    result.closeDecalTintModal = true;
    return result;
}

SectorEditorMaterialActionResult ResetSurfaceUv(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return {};
    }

    SectorTopologyUvSettings* uv = MutableUvForMaterialSurface(map, target, layer);
    if (uv == nullptr) {
        return Failure(layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.");
    }

    const bool changed = uv->scale.x != 1.0f
            || uv->scale.y != 1.0f
            || uv->offset.x != 0.0f
            || uv->offset.y != 0.0f;
    if (!changed) {
        return {};
    }

    ResetTopologyUv(*uv);
    SectorEditorMaterialActionResult result = Changed(TextFormat(
            "Reset 3D %s %s UV",
            SurfaceKindName(surfaceKind),
            TopologyMaterialLayerStatusName(layer)));
    result.resetSurface3DUi = true;
    return result;
}

SectorEditorMaterialActionResult FitSelectedDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    if (!IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Selected material target is no longer valid.");
    }
    if (!IsMaterialDecalAssigned(map, target)) {
        return Failure("No decal assigned.");
    }
    if (IsFlatTarget(target.kind)) {
        return FitSelectedFlatDecal(map, target);
    }
    if (IsWallTopologyEditTarget(target.kind)) {
        return FitSelectedWallMaterial(map, target, TopologyUvFitMode::Both, TopologyMaterialLayer::Decal);
    }

    return Failure("Selected material target cannot be fit.");
}

SectorEditorMaterialActionResult FitSelectedFlatDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target)
{
    if (!IsFlatTarget(target.kind)) {
        return Failure("Select a floor or ceiling surface before fitting decal UVs.");
    }
    if (!IsMaterialDecalAssigned(map, target)) {
        return Failure("No decal assigned.");
    }

    const SectorTopologySector* sector = FindSectorTopologySector(map, target.sectorId);
    if (sector == nullptr) {
        return Failure("Selected sector is no longer valid.");
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(map, sector->id, loops)) {
        return Failure("Selected sector has invalid loops.");
    }

    bool hasPoint = false;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    auto visitLoop = [&](const SectorTopologyLoop& loop) {
        if (loop.vertexIds.empty()) {
            return false;
        }
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
            if (vertex == nullptr) {
                return false;
            }
            const Vector2 world = SectorCoordToWorldPosition2(vertex->x, vertex->y);
            if (!std::isfinite(world.x) || !std::isfinite(world.y)) {
                return false;
            }
            if (!hasPoint) {
                minX = maxX = world.x;
                minY = maxY = world.y;
                hasPoint = true;
            } else {
                minX = std::min(minX, world.x);
                maxX = std::max(maxX, world.x);
                minY = std::min(minY, world.y);
                maxY = std::max(maxY, world.y);
            }
        }
        return true;
    };

    if (!visitLoop(loops.outer)) {
        return Failure("Selected sector has invalid outer loop.");
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (!visitLoop(hole)) {
            return Failure("Selected sector has invalid hole loop.");
        }
    }
    if (!hasPoint) {
        return Failure("Selected sector has no vertices.");
    }

    const float widthWorld = maxX - minX;
    const float heightWorld = maxY - minY;
    if (!(widthWorld > 0.0f) || !(heightWorld > 0.0f)
            || !std::isfinite(widthWorld) || !std::isfinite(heightWorld)) {
        return Failure("Selected sector has invalid flat bounds.");
    }

    const Vector2 fittedScale{
            kSectorGeneratedTextureWorldSize / widthWorld,
            kSectorGeneratedTextureWorldSize / heightWorld};
    if (!ValidateUvScale(fittedScale.x) || !ValidateUvScale(fittedScale.y)) {
        return Failure("Fit decal requires a UV scale outside the editable range.");
    }

    SectorTopologyUvSettings* uv = MutableUvForMaterialSurface(map, target, TopologyMaterialLayer::Decal);
    if (uv == nullptr) {
        return Failure("No decal assigned.");
    }
    uv->scale = fittedScale;
    uv->offset = Vector2{0.0f, 0.0f};

    SectorEditorMaterialActionResult result = Changed(
            TextFormat("Fit %s decal.", TopologyMaterialKindName(target.kind)));
    result.resetSurface3DUi = true;
    result.resetSectorUvInputs = true;
    return result;
}

SectorEditorMaterialActionResult FitSelectedWallMaterial(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Select a wall, lower, upper, or middle surface before fitting UVs.");
    }
    if (layer == TopologyMaterialLayer::Decal && !IsMaterialDecalAssigned(map, target)) {
        return Failure("No decal assigned.");
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        return Failure("Selected sidedef is no longer valid.");
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return Failure("Selected sidedef references a missing linedef.");
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *lineDef, start, end) || start == nullptr || end == nullptr) {
        return Failure("Selected linedef endpoints are missing.");
    }

    const double dx = static_cast<double>(end->x) - static_cast<double>(start->x);
    const double dy = static_cast<double>(end->y) - static_cast<double>(start->y);
    const double coordLength = std::sqrt(dx * dx + dy * dy);
    const float wallLengthWorld = SectorCoordDistanceToWorldDistance(coordLength);
    if (!(wallLengthWorld > 0.0f) || !std::isfinite(wallLengthWorld)) {
        return Failure("Selected wall has invalid length.");
    }

    const SectorTopologySector* sector = FindSectorTopologySector(map, sideDef->sectorId);
    if (sector == nullptr) {
        return Failure("Selected sidedef references a missing sector.");
    }

    const int oppositeSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(map, sideDef->id);
    if (oppositeSideDefId != -1 && opposite == nullptr) {
        return Failure("Selected sidedef's opposite side is no longer valid.");
    }

    const SectorTopologySector* oppositeSector = nullptr;
    if (opposite != nullptr) {
        oppositeSector = FindSectorTopologySector(map, opposite->sectorId);
        if (oppositeSector == nullptr) {
            return Failure("Selected sidedef's opposite sector is missing.");
        }
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    if (wallPart == TopologyWallPart::Middle
            && layer == TopologyMaterialLayer::Base
            && sideDef->middle.textureId.empty()) {
        return Failure("No middle texture assigned.");
    }

    float heightAuthoring = 0.0f;
    switch (wallPart) {
        case TopologyWallPart::Wall:
            if (opposite != nullptr) {
                return Failure("Selected wall part has no visible solid wall span.");
            }
            heightAuthoring = std::fabs(sector->ceilingZ - sector->floorZ);
            break;
        case TopologyWallPart::Lower:
            if (oppositeSector == nullptr) {
                return Failure("Lower wall fit needs an opposite sector.");
            }
            if (!(oppositeSector->floorZ > sector->floorZ)) {
                return Failure("Selected lower wall has no visible height span.");
            }
            heightAuthoring = oppositeSector->floorZ - sector->floorZ;
            break;
        case TopologyWallPart::Upper:
            if (oppositeSector == nullptr) {
                return Failure("Upper wall fit needs an opposite sector.");
            }
            if (!(sector->ceilingZ > oppositeSector->ceilingZ)) {
                return Failure("Selected upper wall has no visible height span.");
            }
            heightAuthoring = sector->ceilingZ - oppositeSector->ceilingZ;
            break;
        case TopologyWallPart::Middle: {
            if (oppositeSector == nullptr) {
                return Failure("Middle texture fit needs an opposite sector.");
            }
            const float bottom = std::max(sector->floorZ, oppositeSector->floorZ);
            const float top = std::min(sector->ceilingZ, oppositeSector->ceilingZ);
            if (!(top > bottom) || !std::isfinite(bottom) || !std::isfinite(top)) {
                return Failure("Selected middle texture has no visible height span.");
            }
            heightAuthoring = top - bottom;
            break;
        }
    }

    const float wallHeightWorld = SectorAuthoringToWorldDistance(std::fabs(heightAuthoring));
    const bool fitWidth = mode == TopologyUvFitMode::Width || mode == TopologyUvFitMode::Both;
    const bool fitHeight = mode == TopologyUvFitMode::Height || mode == TopologyUvFitMode::Both;
    if (fitHeight && (!(wallHeightWorld > 0.0f) || !std::isfinite(wallHeightWorld))) {
        return Failure("Selected wall has invalid height.");
    }

    const float widthScale = kSectorGeneratedTextureWorldSize / wallLengthWorld;
    const float heightScale = fitHeight ? kSectorGeneratedTextureWorldSize / wallHeightWorld : 1.0f;
    if (fitWidth && !ValidateUvScale(widthScale)) {
        return Failure("Fit width requires a UV scale outside the editable range.");
    }
    if (fitHeight && !ValidateUvScale(heightScale)) {
        return Failure("Fit height requires a UV scale outside the editable range.");
    }

    SectorTopologyUvSettings* uv = MutableUvForMaterialSurface(map, target, layer);
    if (uv == nullptr) {
        return Failure(layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.");
    }
    if (fitWidth) {
        uv->scale.x = widthScale;
        uv->offset.x = 0.0f;
    }
    if (fitHeight) {
        uv->scale.y = heightScale;
        uv->offset.y = 0.0f;
    }

    SectorEditorMaterialActionResult result = Changed(
            wallPart == TopologyWallPart::Middle
                    ? TextFormat("Fit middle texture %s.", TopologyUvFitModeStatusName(mode))
                    : TextFormat(
                            "Fit %s %s %s.",
                            TopologyWallPartStatusName(wallPart),
                            TopologyMaterialLayerStatusName(layer),
                            TopologyUvFitModeStatusName(mode)));
    result.resetSurface3DUi = true;
    result.resetSideDefUvInputs = true;
    return result;
}

SectorEditorMaterialActionResult AlignSelectedWallMaterialVertical(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Select a wall, lower, or upper surface before aligning UVs.");
    }
    if (layer == TopologyMaterialLayer::Decal && !IsMaterialDecalAssigned(map, target)) {
        return Failure("No decal assigned.");
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        return Failure("Selected sidedef is no longer valid.");
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return Failure("Selected sidedef references a missing linedef.");
    }

    const SectorTopologySector* sector = FindSectorTopologySector(map, sideDef->sectorId);
    if (sector == nullptr) {
        return Failure("Selected sidedef references a missing sector.");
    }

    const int oppositeSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(map, sideDef->id);
    if (oppositeSideDefId != -1 && opposite == nullptr) {
        return Failure("Selected sidedef's opposite side is no longer valid.");
    }

    const SectorTopologySector* oppositeSector = nullptr;
    if (opposite != nullptr) {
        oppositeSector = FindSectorTopologySector(map, opposite->sectorId);
        if (oppositeSector == nullptr) {
            return Failure("Selected sidedef's opposite sector is missing.");
        }
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    if (wallPart == TopologyWallPart::Middle) {
        return Failure("Middle texture vertical alignment is not available yet.");
    }

    float spanBottomAuthoring = 0.0f;
    float spanTopAuthoring = 0.0f;
    switch (wallPart) {
        case TopologyWallPart::Wall:
            if (opposite != nullptr) {
                return Failure("Selected wall part has no visible solid wall span.");
            }
            spanBottomAuthoring = sector->floorZ;
            spanTopAuthoring = sector->ceilingZ;
            break;
        case TopologyWallPart::Lower:
            if (oppositeSector == nullptr) {
                return Failure("Lower wall alignment needs an opposite sector.");
            }
            if (!(oppositeSector->floorZ > sector->floorZ)) {
                return Failure("Selected lower wall has no visible height span.");
            }
            spanBottomAuthoring = sector->floorZ;
            spanTopAuthoring = oppositeSector->floorZ;
            break;
        case TopologyWallPart::Upper:
            if (oppositeSector == nullptr) {
                return Failure("Upper wall alignment needs an opposite sector.");
            }
            if (!(sector->ceilingZ > oppositeSector->ceilingZ)) {
                return Failure("Selected upper wall has no visible height span.");
            }
            spanBottomAuthoring = oppositeSector->ceilingZ;
            spanTopAuthoring = sector->ceilingZ;
            break;
        case TopologyWallPart::Middle:
            return Failure("Middle texture vertical alignment is not available yet.");
    }

    const float spanBottomWorld = SectorAuthoringToWorldDistance(spanBottomAuthoring);
    const float spanTopWorld = SectorAuthoringToWorldDistance(spanTopAuthoring);
    const float spanHeightWorld = spanTopWorld - spanBottomWorld;
    if (!(spanHeightWorld > 0.0f)
            || !std::isfinite(spanBottomWorld)
            || !std::isfinite(spanTopWorld)
            || !std::isfinite(spanHeightWorld)) {
        return Failure("Selected wall has invalid height.");
    }

    SectorTopologyUvSettings* uv = MutableUvForMaterialSurface(map, target, layer);
    if (uv == nullptr) {
        return Failure(layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.");
    }
    if (!std::isfinite(uv->scale.y)) {
        return Failure("Selected wall has invalid V scale.");
    }

    const float alignedOffsetY = -(spanTopWorld / kSectorGeneratedTextureWorldSize) * uv->scale.y;
    if (!std::isfinite(alignedOffsetY)) {
        return Failure("Aligned V offset is invalid.");
    }

    uv->offset.y = alignedOffsetY;
    SectorEditorMaterialActionResult result = Changed(TextFormat(
            "Aligned %s %s vertically.",
            TopologyWallPartStatusName(wallPart),
            TopologyMaterialLayerStatusName(layer)));
    result.resetSurface3DUi = true;
    result.resetSideDefUvInputs = true;
    return result;
}

SectorEditorMaterialActionResult AlignSelectedWallMaterialU(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidMaterialSurfaceTarget(map, target)) {
        return Failure("Select a wall, lower, or upper surface before aligning U.");
    }
    if (layer == TopologyMaterialLayer::Decal && !IsMaterialDecalAssigned(map, target)) {
        return Failure("No decal assigned.");
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        return Failure("Selected sidedef is no longer valid.");
    }

    const SectorTopologySector* sector = FindSectorTopologySector(map, sideDef->sectorId);
    if (sector == nullptr) {
        return Failure("Selected sidedef references a missing sector.");
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return Failure("Selected sidedef references a missing linedef.");
    }

    float selectedLengthWorld = 0.0f;
    if (!SectorTopologyWallLengthWorld(map, *lineDef, selectedLengthWorld)) {
        return Failure("Selected wall has invalid length.");
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(map, sideDef->sectorId, loops)) {
        return Failure("Could not extract selected sector boundary loop.");
    }

    const SectorTopologyLoop* selectedLoop = nullptr;
    size_t selectedEdgeIndex = 0;
    if (!FindUniqueSectorLoopEdgeForSideDef(loops, sideDef->id, selectedLoop, selectedEdgeIndex)
            || selectedLoop == nullptr
            || selectedLoop->edges.empty()) {
        return Failure("Selected sidedef was not found in exactly one sector loop.");
    }

    const size_t edgeCount = selectedLoop->edges.size();
    if (edgeCount < 2) {
        return Failure("Selected sector loop has no neighboring wall segment.");
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    if (wallPart == TopologyWallPart::Middle) {
        return Failure("Middle texture U alignment is not available yet.");
    }
    const SectorTopologyLoopEdge& selectedEdge = selectedLoop->edges[selectedEdgeIndex];

    if (selectedEdge.sideDefId != sideDef->id
            || selectedEdge.lineDefId != sideDef->lineDefId
            || selectedEdge.side != sideDef->side) {
        return Failure("Selected sidedef loop edge is stale.");
    }

    if (!TopologyWallPartHasVisibleSpan(map, *sideDef, wallPart)) {
        switch (wallPart) {
            case TopologyWallPart::Wall:
                return Failure("Selected wall part has no visible solid wall span.");
            case TopologyWallPart::Lower:
                return Failure("Selected lower wall has no visible height span.");
            case TopologyWallPart::Upper:
                return Failure("Selected upper wall has no visible height span.");
            case TopologyWallPart::Middle:
                return Failure("Selected middle texture has no visible height span.");
        }
    }

    std::string neighborError;
    const SectorTopologyLoopEdge* neighborEdge = FindVisibleTopologyWallPartNeighborEdge(
            map,
            *selectedLoop,
            selectedEdgeIndex,
            sideDef->sectorId,
            direction,
            wallPart,
            layer,
            neighborError);
    if (neighborEdge == nullptr) {
        if (!neighborError.empty()) {
            return Failure(neighborError.c_str());
        }
        return Failure(TextFormat(
                "No %s visible %s %s in this sector loop.",
                TopologyUAlignDirectionStatusName(direction),
                TopologyWallPartSurfaceStatusName(wallPart),
                TopologyMaterialLayerStatusName(layer)));
    }

    const SectorTopologySideDef* neighborSideDef = FindSectorTopologySideDef(map, neighborEdge->sideDefId);
    if (neighborSideDef == nullptr
            || neighborSideDef->lineDefId != neighborEdge->lineDefId
            || neighborSideDef->sectorId != sideDef->sectorId
            || neighborSideDef->side != neighborEdge->side) {
        return Failure(TextFormat(
                "%s sidedef is no longer valid.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next"));
    }

    const SectorTopologyLineDef* neighborLineDef = FindSectorTopologyLineDef(map, neighborSideDef->lineDefId);
    if (neighborLineDef == nullptr) {
        return Failure(TextFormat(
                "%s sidedef references a missing linedef.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next"));
    }

    float neighborLengthWorld = 0.0f;
    if (!SectorTopologyWallLengthWorld(map, *neighborLineDef, neighborLengthWorld)) {
        return Failure(TextFormat(
                "%s wall has invalid length.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next"));
    }

    const SectorTopologyWallPartSettings& neighborPart = TopologyWallPartSettingsFor(*neighborSideDef, wallPart);
    const SectorTopologyUvSettings& neighborUv = layer == TopologyMaterialLayer::Decal
            ? neighborPart.decal.uv
            : neighborPart.uv;
    SectorTopologyUvSettings* selectedUv = MutableUvForMaterialSurface(map, target, layer);
    if (selectedUv == nullptr) {
        return Failure(layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.");
    }

    if (!std::isfinite(selectedUv->scale.x)
            || !std::isfinite(selectedUv->offset.x)
            || !std::isfinite(neighborUv.scale.x)
            || !std::isfinite(neighborUv.offset.x)) {
        return Failure("Wall U alignment requires finite U scale and offset values.");
    }

    float alignedOffsetX = 0.0f;
    if (direction == TopologyUAlignDirection::Previous) {
        const float previousBaseEndU = neighborLengthWorld / kSectorGeneratedTextureWorldSize;
        alignedOffsetX = previousBaseEndU * neighborUv.scale.x + neighborUv.offset.x;
    } else {
        const float selectedBaseEndU = selectedLengthWorld / kSectorGeneratedTextureWorldSize;
        alignedOffsetX = neighborUv.offset.x - selectedBaseEndU * selectedUv->scale.x;
    }

    if (!std::isfinite(alignedOffsetX)) {
        return Failure("Aligned U offset is invalid.");
    }

    selectedUv->offset.x = alignedOffsetX;
    SectorEditorMaterialActionResult result = Changed(TextFormat(
            "Aligned %s %s U from %s.",
            TopologyWallPartStatusName(wallPart),
            TopologyMaterialLayerStatusName(layer),
            TopologyUAlignDirectionStatusName(direction)));
    result.resetSurface3DUi = true;
    result.resetSideDefUvInputs = true;
    return result;
}

} // namespace game
