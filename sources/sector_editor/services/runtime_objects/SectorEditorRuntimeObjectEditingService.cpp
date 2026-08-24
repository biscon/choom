#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"

#include "engine/EngineContext.h"
#include "game/npc/NpcRuntime.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTopologyRenderCache.h"

#include <cmath>
#include <utility>

namespace game {
namespace {

float TriangleCross(Vector2 a, Vector2 b, Vector2 point)
{
    return (b.x - a.x) * (point.y - a.y)
            - (b.y - a.y) * (point.x - a.x);
}

bool PointInOrOnTriangle(Vector2 point, Vector2 a, Vector2 b, Vector2 c)
{
    constexpr float epsilon = 0.0001f;
    const float ab = TriangleCross(a, b, point);
    const float bc = TriangleCross(b, c, point);
    const float ca = TriangleCross(c, a, point);
    const bool hasNegative = ab < -epsilon || bc < -epsilon || ca < -epsilon;
    const bool hasPositive = ab > epsilon || bc > epsilon || ca > epsilon;
    return !(hasNegative && hasPositive);
}

} // namespace

SectorEditorRuntimeObjectEditingService::SectorEditorRuntimeObjectEditingService(
        SectorEditorRuntimeObjectEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorPlacedRuntimeObject* SectorEditorRuntimeObjectEditingService::SelectedObject()
{
    return FindSectorPlacedRuntimeObject(
            context_.map,
            context_.selectionState.selectedRuntimeObjectId);
}

const SectorPlacedRuntimeObject* SectorEditorRuntimeObjectEditingService::SelectedObject() const
{
    return FindSectorPlacedRuntimeObject(
            context_.map,
            context_.selectionState.selectedRuntimeObjectId);
}

void SectorEditorRuntimeObjectEditingService::SelectObject(int objectId)
{
    if (context_.selectionService != nullptr) {
        SelectSectorEditorRuntimeObject(*context_.selectionService, objectId);
        return;
    }
    context_.selectionState.topologySelectionKind = TopologySelectionKind::None;
    context_.selectionState.selectedRuntimeObjectId =
            FindSectorPlacedRuntimeObject(context_.map, objectId) != nullptr
            ? objectId
            : -1;
    ResetInspectorUi();
}

bool SectorEditorRuntimeObjectEditingService::AddBillboard(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddBillboardResult result =
            AddBillboardToSector(context_.map, sectorId, mapPoint);
    if (!result.changed) {
        context_.statusText = result.status;
        return false;
    }
    SelectObject(result.objectId);
    MarkEdited(result.status.c_str());
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::AddDoor(int lineDefId)
{
    const SectorEditorAddDoorResult result = AddDoorToPortal(context_.map, lineDefId);
    if (!result.changed) {
        context_.statusText = result.status;
        return false;
    }
    SelectObject(result.objectId);
    MarkEdited(result.status.c_str());
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::AddStaticModel(Vector2 mapPoint)
{
    if (!context_.authoringDerivationCurrent) {
        context_.statusText =
                "3D prop placement failed: authoring derivation is not current";
        return false;
    }
    if (!context_.topologyRenderCache.valid) {
        context_.statusText =
                "3D prop placement failed: derived sector cache is unavailable";
        return false;
    }
    const int sectorId = FindCachedSectorAt(mapPoint);
    const SectorTopologySector* sector =
            FindSectorTopologySector(context_.map, sectorId);
    if (sector == nullptr) {
        context_.statusText =
                "3D prop placement failed: click inside a derived sector";
        return false;
    }
    const int objectId = AllocateSectorPlacedRuntimeObjectId(context_.map);
    if (!IsValidSectorTopologyId(objectId)) {
        context_.statusText =
                "3D prop placement failed: no runtime object IDs available";
        return false;
    }

    SectorPlacedRuntimeObject object;
    object.id = objectId;
    object.kind = "static_model";
    object.position = Vector3{mapPoint.x, sector->floorZ, mapPoint.y};
    object.staticModel = SectorPlacedStaticModel{};
    context_.map.runtimeObjects.push_back(std::move(object));
    SelectObject(objectId);
    MarkEdited(TextFormat("Added 3D prop %d", objectId));
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::AddDynamicModel(Vector2 mapPoint)
{
    if (!context_.authoringDerivationCurrent || !context_.topologyRenderCache.valid) {
        context_.statusText =
                "Dynamic prop placement failed: derived sector cache is unavailable";
        return false;
    }
    const int sectorId = FindCachedSectorAt(mapPoint);
    const SectorTopologySector* sector = FindSectorTopologySector(context_.map, sectorId);
    if (sector == nullptr) {
        context_.statusText =
                "Dynamic prop placement failed: click inside a derived sector";
        return false;
    }
    const int objectId = AllocateSectorPlacedRuntimeObjectId(context_.map);
    if (!IsValidSectorTopologyId(objectId)) {
        context_.statusText =
                "Dynamic prop placement failed: no runtime object IDs available";
        return false;
    }

    SectorPlacedRuntimeObject object;
    object.id = objectId;
    object.kind = "dynamic_model";
    object.position = Vector3{mapPoint.x, sector->floorZ, mapPoint.y};
    object.dynamicModel = SectorPlacedDynamicModel{};
    object.dynamicModel.instanceId =
            AllocateSectorDynamicModelInstanceId(context_.map, objectId);
    if (object.dynamicModel.instanceId.empty()) {
        context_.statusText =
                "Dynamic prop placement failed: no instance IDs available";
        return false;
    }
    context_.map.runtimeObjects.push_back(std::move(object));
    SelectObject(objectId);
    MarkEdited(TextFormat("Added dynamic prop %d", objectId));
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::AddNpc(
        Vector2 mapPoint,
        const std::string& definitionId)
{
    if (!context_.authoringDerivationCurrent || !context_.topologyRenderCache.valid) {
        context_.statusText =
                "NPC placement failed: derived sector cache is unavailable";
        return false;
    }
    if (FindNpcDefinition(context_.runtimeObjects.npcDefinitionCatalog, definitionId)
            == nullptr) {
        context_.statusText = definitionId.empty()
                ? "NPC placement failed: create an NPC definition first"
                : "NPC placement failed: selected definition is unavailable";
        return false;
    }
    const int sectorId = FindCachedSectorAt(mapPoint);
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.map,
            sectorId);
    if (sector == nullptr) {
        context_.statusText =
                "NPC placement failed: click inside a derived sector";
        return false;
    }
    const int objectId = AllocateSectorPlacedRuntimeObjectId(context_.map);
    if (!IsValidSectorTopologyId(objectId)) {
        context_.statusText = "NPC placement failed: no runtime object IDs available";
        return false;
    }

    SectorPlacedRuntimeObject object;
    object.id = objectId;
    object.kind = "npc";
    object.position = Vector3{mapPoint.x, sector->floorZ, mapPoint.y};
    object.npc.definitionId = definitionId;
    context_.map.runtimeObjects.push_back(std::move(object));
    context_.editingState.npcPlacement.lastDefinitionId = definitionId;
    SelectObject(objectId);
    MarkEdited(TextFormat("Added NPC %d", objectId));
    RefreshPreviewObjects();
    return true;
}

SectorEditorRuntimeObjectDeleteRequest
SectorEditorRuntimeObjectEditingService::RequestDeleteSelected() const
{
    const SectorPlacedRuntimeObject* object = SelectedObject();
    if (object == nullptr) {
        return {};
    }
    return SectorEditorRuntimeObjectDeleteRequest{
            true,
            object->id,
            "Delete Object",
            TextFormat("Delete object %d?", object->id)};
}

bool SectorEditorRuntimeObjectEditingService::DeleteById(int objectId)
{
    if (!RemoveSectorPlacedRuntimeObject(context_.map, objectId)) {
        return false;
    }
    if (context_.editingState.drag.objectId == objectId) {
        context_.editingState.drag = RuntimeObjectDragState{};
    }
    if (context_.selectionState.selectedRuntimeObjectId == objectId) {
        ClearSelection();
    }
    MarkEdited(TextFormat("Deleted object %d", objectId));
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::MutateSelected(
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate)
{
    SectorPlacedRuntimeObject* object = SelectedObject();
    if (object == nullptr || !mutate) {
        return false;
    }
    const Vector3 previousPosition = object->position;
    if (!mutate(*object)) {
        return false;
    }
    if ((object->kind == "static_model"
                || object->kind == "dynamic_model"
                || object->kind == "npc")
            && context_.topologyRenderCache.valid
            && (std::fabs(object->position.x - previousPosition.x) > GeometryEpsilon
                    || std::fabs(object->position.z - previousPosition.z) > GeometryEpsilon)) {
        const int sectorId = FindCachedSectorAt(
                Vector2{object->position.x, object->position.z});
        if (const SectorTopologySector* sector =
                    FindSectorTopologySector(context_.map, sectorId)) {
            object->position.y = sector->floorZ;
        }
    }
    MarkEdited(status);
    RefreshPreviewObjects();
    return true;
}

bool SectorEditorRuntimeObjectEditingService::AssignSelectedStaticModel(
        const std::string& modelPath)
{
    return MutateSelected(
            "Updated 3D prop model",
            [&modelPath](SectorPlacedRuntimeObject& object) {
                if (object.kind != "static_model"
                        || object.staticModel.modelPath == modelPath) {
                    return false;
                }
                object.staticModel.modelPath = modelPath;
                object.staticModel.geometryFingerprint.clear();
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::AssignSelectedDynamicModel(
        const std::string& modelPath)
{
    return MutateSelected(
            "Updated dynamic prop model",
            [&modelPath](SectorPlacedRuntimeObject& object) {
                if (object.kind != "dynamic_model"
                        || object.dynamicModel.modelPath == modelPath) {
                    return false;
                }
                object.dynamicModel.modelPath = modelPath;
                object.dynamicModel.animation.clear();
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::AssignSelectedNpcDefinition(
        const std::string& definitionId)
{
    if (FindNpcDefinition(context_.runtimeObjects.npcDefinitionCatalog, definitionId)
            == nullptr) {
        context_.statusText = "NPC definition is unavailable";
        return false;
    }
    context_.editingState.npcPlacement.lastDefinitionId = definitionId;
    return MutateSelected(
            "Updated NPC definition",
            [&definitionId](SectorPlacedRuntimeObject& object) {
                if (object.kind != "npc"
                        || object.npc.definitionId == definitionId) {
                    return false;
                }
                object.npc.definitionId = definitionId;
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::SetSelectedNpcInstanceId(
        const std::string& instanceId,
        std::string& outError)
{
    const SectorPlacedRuntimeObject* selected = SelectedObject();
    if (selected == nullptr || selected->kind != "npc") {
        outError = "No NPC is selected";
        return false;
    }
    if (!instanceId.empty() && !IsValidNpcInstanceId(instanceId)) {
        outError =
                "Instance ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    if (!instanceId.empty()) {
        for (const SectorPlacedRuntimeObject& object : context_.map.runtimeObjects) {
            if (object.id != selected->id
                    && object.kind == "npc"
                    && object.npc.instanceId == instanceId) {
                outError = "Instance ID must be unique among NPCs in this map";
                return false;
            }
        }
    }
    outError.clear();
    if (selected->npc.instanceId == instanceId) return true;
    return MutateSelected(
            "Updated NPC instance ID",
            [&instanceId](SectorPlacedRuntimeObject& object) {
                if (object.kind != "npc") return false;
                object.npc.instanceId = instanceId;
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::SetSelectedDynamicModelInstanceId(
        const std::string& instanceId,
        std::string& outError)
{
    const SectorPlacedRuntimeObject* selected = SelectedObject();
    if (selected == nullptr || selected->kind != "dynamic_model") {
        outError = "No dynamic prop is selected";
        return false;
    }
    if (!IsValidSectorDynamicModelInstanceId(instanceId)) {
        outError =
                "Instance ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    const SectorPlacedRuntimeObject* existing =
            FindSectorPlacedDynamicModelByInstanceId(context_.map, instanceId);
    if (existing != nullptr && existing->id != selected->id) {
        outError = "Instance ID must be unique among dynamic props in this map";
        return false;
    }
    outError.clear();
    if (selected->dynamicModel.instanceId == instanceId) return true;
    return MutateSelected(
            "Updated dynamic prop instance ID",
            [&instanceId](SectorPlacedRuntimeObject& object) {
                if (object.kind != "dynamic_model") return false;
                object.dynamicModel.instanceId = instanceId;
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::SetSelectedDoorInstanceId(
        const std::string& instanceId,
        std::string& outError)
{
    const SectorPlacedRuntimeObject* selected = SelectedObject();
    if (selected == nullptr || selected->kind != "door") {
        outError = "No door is selected";
        return false;
    }
    if (!IsValidSectorScriptInstanceId(instanceId)) {
        outError =
                "Instance ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    for (const SectorPlacedRuntimeObject& object : context_.map.runtimeObjects) {
        if (object.id != selected->id && object.kind == "door"
                && object.door.instanceId == instanceId) {
            outError = "Instance ID must be unique among doors in this map";
            return false;
        }
    }
    outError.clear();
    if (selected->door.instanceId == instanceId) return true;
    return MutateSelected(
            "Updated door instance ID",
            [&instanceId](SectorPlacedRuntimeObject& object) {
                if (object.kind != "door") return false;
                object.door.instanceId = instanceId;
                return true;
            });
}

bool SectorEditorRuntimeObjectEditingService::SelectedDoorRuntimeTargetOpen(
        bool& outOpen) const
{
    const SectorPlacedRuntimeObject* selected = SelectedObject();
    if (selected == nullptr
            || selected->kind != "door"
            || context_.engineContext == nullptr) {
        return false;
    }
    for (const SectorPlacedRuntimeObjectEntity& entry :
            context_.runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId != selected->id
                || !context_.engineContext->world.IsAlive(entry.entity)
                || !context_.engineContext->world.Has<SectorDoorMotion>(entry.entity)) {
            continue;
        }
        const SectorDoorMotion& motion =
                context_.engineContext->world.Get<SectorDoorMotion>(entry.entity);
        outOpen = std::isfinite(motion.targetOpenFraction)
                && motion.targetOpenFraction > 0.5f;
        return true;
    }
    return false;
}

bool SectorEditorRuntimeObjectEditingService::SetSelectedDoorRuntimeTargetOpen(
        bool open)
{
    const SectorPlacedRuntimeObject* selected = SelectedObject();
    if (selected == nullptr
            || selected->kind != "door"
            || context_.engineContext == nullptr) {
        return false;
    }
    for (const SectorPlacedRuntimeObjectEntity& entry :
            context_.runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId != selected->id
                || !context_.engineContext->world.IsAlive(entry.entity)
                || !context_.engineContext->world.Has<SectorDoorMotion>(entry.entity)) {
            continue;
        }
        SectorDoorMotion& motion =
                context_.engineContext->world.Get<SectorDoorMotion>(entry.entity);
        motion.targetOpenFraction = open ? 1.0f : 0.0f;
        context_.statusText = open
                ? "Door debug runtime target: open"
                : "Door debug runtime target: close";
        return true;
    }
    return false;
}

bool SectorEditorRuntimeObjectEditingService::BeginDrag(int objectId)
{
    const SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context_.map, objectId);
    if (object == nullptr) {
        return false;
    }
    if (object->kind == "door") {
        context_.statusText =
                "Door movement unavailable: doors stay anchored to portal lines";
        return false;
    }
    SelectObject(objectId);
    context_.editingState.drag.active = true;
    context_.editingState.drag.objectId = objectId;
    context_.editingState.drag.originalPosition = object->position;
    context_.editingState.drag.snappedPosition = object->position;
    context_.statusText = TextFormat("Moving object %d", objectId);
    return true;
}

void SectorEditorRuntimeObjectEditingService::UpdateDrag(Vector2 snappedMapPoint)
{
    RuntimeObjectDragState& drag = context_.editingState.drag;
    if (!drag.active) {
        return;
    }
    SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context_.map, drag.objectId);
    if (object == nullptr) {
        drag = {};
        return;
    }

    float baseFloor = object->position.y;
    if ((object->kind == "static_model"
                || object->kind == "dynamic_model"
                || object->kind == "npc")
            && context_.topologyRenderCache.valid) {
        const int sectorId = FindCachedSectorAt(snappedMapPoint);
        if (const SectorTopologySector* sector =
                    FindSectorTopologySector(context_.map, sectorId)) {
            baseFloor = sector->floorZ;
        }
    }
    drag.snappedPosition = Vector3{
            snappedMapPoint.x,
            baseFloor,
            snappedMapPoint.y};
    object->position = drag.snappedPosition;
    UpdateCachedDraw(*object);
    context_.statusText = TextFormat("Moving object %d", object->id);
}

bool SectorEditorRuntimeObjectEditingService::FinishDrag()
{
    RuntimeObjectDragState& drag = context_.editingState.drag;
    if (!drag.active) {
        return false;
    }
    const int objectId = drag.objectId;
    const Vector3 original = drag.originalPosition;
    drag = {};
    SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context_.map, objectId);
    if (object == nullptr) {
        return false;
    }
    SelectObject(objectId);
    const bool moved =
            std::fabs(object->position.x - original.x) > GeometryEpsilon
            || std::fabs(object->position.y - original.y) > GeometryEpsilon
            || std::fabs(object->position.z - original.z) > GeometryEpsilon;
    if (!moved) {
        object->position = original;
        UpdateCachedDraw(*object);
        context_.statusText = TextFormat("Object %d unchanged", objectId);
        return false;
    }
    MarkEdited(TextFormat("Moved object %d", objectId));
    RefreshPreviewObjects();
    return true;
}

void SectorEditorRuntimeObjectEditingService::CancelDrag(const char* message)
{
    RuntimeObjectDragState& drag = context_.editingState.drag;
    if (drag.active) {
        if (SectorPlacedRuntimeObject* object =
                    FindSectorPlacedRuntimeObject(context_.map, drag.objectId)) {
            object->position = drag.originalPosition;
            UpdateCachedDraw(*object);
            SelectObject(object->id);
        }
    }
    drag = {};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
}

int SectorEditorRuntimeObjectEditingService::FindCachedSectorAt(
        Vector2 mapPoint,
        bool* outMultipleMatches) const
{
    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = false;
    }
    if (!context_.topologyRenderCache.valid) {
        return -1;
    }

    int foundSectorId = -1;
    int matches = 0;
    for (const CachedTopologySectorDraw& sector :
            context_.topologyRenderCache.sectors) {
        bool contains = false;
        for (size_t index = 0;
                index + 2 < sector.fillTrianglePoints.size();
                index += 3) {
            if (PointInOrOnTriangle(
                        mapPoint,
                        sector.fillTrianglePoints[index],
                        sector.fillTrianglePoints[index + 1],
                        sector.fillTrianglePoints[index + 2])) {
                contains = true;
                break;
            }
        }
        if (!contains) {
            continue;
        }
        ++matches;
        if (foundSectorId < 0 || sector.sectorId < foundSectorId) {
            foundSectorId = sector.sectorId;
        }
    }
    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = matches > 1;
    }
    return foundSectorId;
}

void SectorEditorRuntimeObjectEditingService::RefreshPreviewObjects()
{
    if (context_.engineContext == nullptr) {
        return;
    }
    SpawnPlacedRuntimeObjects(
            context_.engineContext->world,
            context_.engineContext->assets,
            context_.runtimeObjects,
            context_.map);
}

void SectorEditorRuntimeObjectEditingService::MarkEdited(const char* status)
{
    MarkSectorEditorTopologyDocumentEdited(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.statusText,
            status);
}

void SectorEditorRuntimeObjectEditingService::ResetInspectorUi()
{
    context_.uiState = RuntimeObjectEditingUiState{};
}

void SectorEditorRuntimeObjectEditingService::UpdateCachedDraw(
        const SectorPlacedRuntimeObject& object)
{
    UpdateCachedSectorEditorRuntimeObjectDraw(
            context_.topologyRenderCache,
            object);
}

void SectorEditorRuntimeObjectEditingService::ClearSelection()
{
    if (context_.selectionService != nullptr) {
        ClearSectorEditorSelection(*context_.selectionService);
    } else {
        context_.selectionState.selectedRuntimeObjectId = -1;
    }
    ResetInspectorUi();
}

} // namespace game
