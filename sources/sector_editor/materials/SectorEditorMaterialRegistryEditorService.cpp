#include "sector_editor/materials/SectorEditorMaterialRegistryEditorService.h"

#include "engine/assets/AssetManager.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorMaterialRefactor.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

void VisitWallPart(
        SectorTopologyWallPartSettings& part,
        const std::function<void(std::string&)>& visitor)
{
    visitor(part.materialId);
    visitor(part.decal.materialId);
}

void VisitMapMaterials(
        SectorTopologyMap& map,
        const std::function<void(std::string&)>& visitor)
{
    for (SectorTopologySideDef& side : map.sideDefs) {
        VisitWallPart(side.wall, visitor);
        VisitWallPart(side.lower, visitor);
        VisitWallPart(side.upper, visitor);
        VisitWallPart(side.middle, visitor);
    }
    for (SectorTopologySector& sector : map.sectors) {
        visitor(sector.floorMaterialId);
        visitor(sector.ceilingMaterialId);
        visitor(sector.floorDecal.materialId);
        visitor(sector.ceilingDecal.materialId);
    }
    visitor(map.skySettings.materialId);
    for (SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "door") visitor(object.door.materialId);
    }
}

void VisitAuthoringMaterials(
        SectorAuthoringGraph& graph,
        const std::function<void(std::string&)>& visitor)
{
    for (SectorAuthoringLineSide& side : graph.lineSides) {
        VisitWallPart(side.wall, visitor);
        VisitWallPart(side.lower, visitor);
        VisitWallPart(side.upper, visitor);
        VisitWallPart(side.middle, visitor);
    }
    for (SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
        visitor(anchor.floorMaterialId);
        visitor(anchor.ceilingMaterialId);
        visitor(anchor.floorDecal.materialId);
        visitor(anchor.ceilingDecal.materialId);
        VisitWallPart(anchor.defaultWall, visitor);
        VisitWallPart(anchor.defaultLower, visitor);
        VisitWallPart(anchor.defaultUpper, visitor);
    }
}

std::string UniqueMaterialId(
        const std::vector<SectorEditorMaterialRegistryDraft>& drafts,
        const char* base)
{
    std::string result = base;
    int suffix = 1;
    const auto exists = [&](const std::string& id) {
        return std::any_of(drafts.begin(), drafts.end(), [&](const auto& draft) {
            return draft.definition.id == id;
        });
    };
    while (exists(result)) result = std::string(base) + "_" + std::to_string(suffix++);
    return result;
}

} // namespace

SectorEditorMaterialRegistryEditorService::SectorEditorMaterialRegistryEditorService(
        SectorEditorMaterialRegistryEditorState& state,
        SectorMaterialRegistry& registry,
        SectorAuthoringGraph& authoringGraph,
        SectorTopologyMap& topologyMap,
        SectorEditorDerivationDocumentAccess derivation,
        SectorEditorDocumentLifecycleAccess lifecycle,
        std::string& statusText,
        std::filesystem::path registryPath,
        std::filesystem::path levelsRoot)
    : state_(state), registry_(registry), authoringGraph_(authoringGraph),
      topologyMap_(topologyMap), derivation_(derivation), lifecycle_(lifecycle),
      statusText_(statusText), registryPath_(std::move(registryPath)),
      levelsRoot_(std::move(levelsRoot))
{
}

void SectorEditorMaterialRegistryEditorService::Open()
{
    state_ = SectorEditorMaterialRegistryEditorState{};
    state_.open = true;
    for (const std::string& id : SortedSectorMaterialIds(registry_)) {
        state_.drafts.push_back({registry_.materialsById.at(id), id, false});
    }
    state_.selectedIndex = state_.drafts.empty() ? -1 : 0;
    RebuildListLabels();
    SyncBuffers();
    statusText_ = "Material Editor opened";
}

void SectorEditorMaterialRegistryEditorService::Close(engine::AssetManager* assets)
{
    if (assets != nullptr && !engine::IsNull(state_.previewScope)) {
        assets->UnloadScope(state_.previewScope);
    }
    state_ = SectorEditorMaterialRegistryEditorState{};
}

void SectorEditorMaterialRegistryEditorService::Cancel(engine::AssetManager* assets)
{
    Close(assets);
    statusText_ = "Material changes cancelled";
}

void SectorEditorMaterialRegistryEditorService::Shutdown(engine::AssetManager& assets)
{
    Close(&assets);
}

SectorEditorMaterialRegistryDraft* SectorEditorMaterialRegistryEditorService::SelectedDraft()
{
    if (state_.selectedIndex < 0
            || state_.selectedIndex >= static_cast<int>(state_.drafts.size())) return nullptr;
    return &state_.drafts[static_cast<size_t>(state_.selectedIndex)];
}

bool SectorEditorMaterialRegistryEditorService::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.drafts.size())) return false;
    state_.selectedIndex = index;
    state_.validationMessage.clear();
    SyncBuffers();
    return true;
}

void SectorEditorMaterialRegistryEditorService::AddMaterial()
{
    SectorEditorMaterialRegistryDraft draft;
    draft.definition.path = "assets/images/wall.png";
    const std::string generatedId = GeneratedTextureIdBase(draft.definition.path);
    draft.definition.id = UniqueMaterialId(state_.drafts, generatedId.c_str());
    draft.definition.filter = SectorMaterialFilter::Anisotropic8x;
    draft.definition.roughnessFactor = 0.8f;
    state_.drafts.push_back(std::move(draft));
    state_.selectedIndex = static_cast<int>(state_.drafts.size()) - 1;
    RebuildListLabels();
    SyncBuffers();
}

void SectorEditorMaterialRegistryEditorService::ApplyIdBuffer()
{
    SectorEditorMaterialRegistryDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    const std::string id = state_.idBuffer;
    if (!IsValidSectorMaterialId(id)) {
        state_.validationMessage = "ID must use 1-95 letters, digits, underscores, or dashes";
        return;
    }
    for (const auto& other : state_.drafts) {
        if (&other != draft && other.definition.id == id) {
            state_.validationMessage = "Material ID already exists";
            return;
        }
    }
    draft->definition.id = id;
    draft->idWasEdited = true;
    state_.validationMessage.clear();
    RebuildListLabels();
}

void SectorEditorMaterialRegistryEditorService::ApplyPathBuffer()
{
    SectorEditorMaterialRegistryDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    const std::string previous = draft->definition.path;
    draft->definition.path = state_.pathBuffer;
    std::string error;
    if (!ValidateSectorMaterialDefinition(draft->definition, error)) {
        draft->definition.path = previous;
        state_.validationMessage = error;
        SyncBuffers();
        return;
    }
    if (draft->originalId.empty() && !draft->idWasEdited) {
        const std::string generatedId = GeneratedTextureIdBase(draft->definition.path);
        draft->definition.id = UniqueMaterialId(state_.drafts, generatedId.c_str());
        RebuildListLabels();
        std::snprintf(
                state_.idBuffer,
                sizeof(state_.idBuffer),
                "%s",
                draft->definition.id.c_str());
    }
    state_.validationMessage.clear();
}

bool SectorEditorMaterialRegistryEditorService::CurrentDocumentReferences(std::string_view id) const
{
    bool found = false;
    const auto inspect = [&](std::string& value) { found = found || value == id; };
    VisitAuthoringMaterials(authoringGraph_, inspect);
    VisitMapMaterials(topologyMap_, inspect);
    return found;
}

bool SectorEditorMaterialRegistryEditorService::RequestDeleteSelected()
{
    const SectorEditorMaterialRegistryDraft* draft = SelectedDraft();
    if (draft == nullptr) return false;
    const std::string referenceId = draft->originalId.empty()
            ? draft->definition.id : draft->originalId;
    if (CurrentDocumentReferences(referenceId)) {
        state_.validationMessage = "Cannot delete '" + referenceId
                + "': it is referenced by the open level";
        return false;
    }
    std::unordered_map<std::string, size_t> counts;
    std::string error;
    if (!CountSectorMaterialReferencesInLevels(
                levelsRoot_, {referenceId}, counts, error)) {
        state_.validationMessage = error;
        return false;
    }
    if (counts[referenceId] != 0) {
        state_.validationMessage = "Cannot delete '" + referenceId
                + "': it is referenced by a saved level";
        return false;
    }
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationId = draft->definition.id;
    return true;
}

void SectorEditorMaterialRegistryEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationId.clear();
}

void SectorEditorMaterialRegistryEditorService::ConfirmDeleteSelected()
{
    if (!state_.deleteConfirmationOpen || SelectedDraft() == nullptr) return;
    state_.drafts.erase(state_.drafts.begin() + state_.selectedIndex);
    state_.selectedIndex = state_.drafts.empty()
            ? -1 : std::min(state_.selectedIndex, static_cast<int>(state_.drafts.size()) - 1);
    CancelDelete();
    RebuildListLabels();
    SyncBuffers();
}

bool SectorEditorMaterialRegistryEditorService::ValidateDrafts(std::string& error) const
{
    if (state_.drafts.empty()) {
        error = "Material registry must contain at least one material";
        return false;
    }
    std::unordered_set<std::string> ids;
    for (const auto& draft : state_.drafts) {
        if (!ValidateSectorMaterialDefinition(draft.definition, error)) return false;
        if (!ids.insert(draft.definition.id).second) {
            error = "Duplicate material ID '" + draft.definition.id + "'";
            return false;
        }
    }
    return true;
}

bool SectorEditorMaterialRegistryEditorService::SaveAndClose(engine::AssetManager& assets)
{
    ApplyPathBuffer();
    ApplyIdBuffer();
    std::string error;
    if (!state_.validationMessage.empty() || !ValidateDrafts(error)) {
        if (state_.validationMessage.empty()) state_.validationMessage = error;
        return false;
    }
    SectorMaterialRegistry edited;
    std::unordered_map<std::string, std::string> renamed;
    std::unordered_set<std::string> retainedOriginals;
    for (const auto& draft : state_.drafts) {
        edited.materialsById.emplace(draft.definition.id, draft.definition);
        if (!draft.originalId.empty()) {
            retainedOriginals.insert(draft.originalId);
            if (draft.originalId != draft.definition.id) {
                renamed.emplace(draft.originalId, draft.definition.id);
            }
        }
    }
    std::unordered_set<std::string> deleted;
    for (const auto& entry : registry_.materialsById) {
        if (retainedOriginals.find(entry.first) == retainedOriginals.end()) {
            deleted.insert(entry.first);
        }
    }
    if (!SaveSectorMaterialRegistryWithLevelRefactors(
                registryPath_, levelsRoot_, edited, renamed, deleted, error)) {
        state_.validationMessage = error;
        return false;
    }

    const auto renameValue = [&](std::string& value) {
        const auto it = renamed.find(value);
        if (it != renamed.end()) value = it->second;
    };
    VisitAuthoringMaterials(authoringGraph_, renameValue);
    VisitMapMaterials(topologyMap_, renameValue);
    VisitMapMaterials(derivation_.authoringDerivation.topology, renameValue);
    if (derivation_.lastValidAuthoringDerivedTopology.has_value()) {
        VisitMapMaterials(*derivation_.lastValidAuthoringDerivedTopology, renameValue);
    }
    if (!renamed.empty()) {
        lifecycle_.topologyDocumentDirty = true;
        lifecycle_.hasUnsavedChanges = true;
    }
    registry_ = std::move(edited);
    Close(&assets);
    statusText_ = "Saved global material registry";
    return true;
}

void SectorEditorMaterialRegistryEditorService::EnsurePreview(engine::AssetManager& assets)
{
    const SectorEditorMaterialRegistryDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    if (state_.previewPath == draft->definition.path
            && state_.previewFilter == draft->definition.filter
            && !engine::IsNull(state_.previewTexture)) return;
    if (!engine::IsNull(state_.previewScope)) assets.UnloadScope(state_.previewScope);
    state_.previewScope = assets.CreateScope("sector_editor_material_registry_preview");
    state_.previewPath = draft->definition.path;
    state_.previewFilter = draft->definition.filter;
    state_.previewTexture = assets.RequestTexture(
            state_.previewScope,
            (draft->definition.path + "|material-editor").c_str(),
            ResolveEditorAssetPath(draft->definition.path).c_str(),
            engine::TextureColorUsage::DisplaySrgb,
            SectorMaterialTextureLoadFlags(draft->definition.filter));
}

void SectorEditorMaterialRegistryEditorService::SyncBuffers()
{
    const SectorEditorMaterialRegistryDraft* draft = SelectedDraft();
    const std::string id = draft == nullptr ? std::string{} : draft->definition.id;
    const std::string path = draft == nullptr ? std::string{} : draft->definition.path;
    std::snprintf(state_.idBuffer, sizeof(state_.idBuffer), "%s", id.c_str());
    std::snprintf(state_.pathBuffer, sizeof(state_.pathBuffer), "%s", path.c_str());
    state_.metallicInput = engine::UIFloatInputState{};
    state_.roughnessInput = engine::UIFloatInputState{};
    state_.normalStrengthInput = engine::UIFloatInputState{};
    state_.previewPath.clear();
    state_.previewTexture = engine::NullTextureHandle();
}

void SectorEditorMaterialRegistryEditorService::RebuildListLabels()
{
    state_.listLabelStorage.clear();
    state_.listLabels.clear();
    state_.listLabelStorage.reserve(state_.drafts.size());
    for (const auto& draft : state_.drafts) {
        state_.listLabelStorage.push_back(draft.definition.id);
    }
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
}

} // namespace game
