#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <cstdio>
#include <utility>

namespace game {

SectorEditorTextureCatalogService::SectorEditorTextureCatalogService(
        SectorEditorTextureCatalogServiceContext context)
    : context_(context)
{
}

const SectorTextureDefinition* SectorEditorTextureCatalogService::FindTexture(
        const std::string& textureId) const
{
    return FindSectorTopologyTexture(context_.state.topologyMap, textureId);
}

bool SectorEditorTextureCatalogService::HasTexture(const std::string& textureId) const
{
    return FindTexture(textureId) != nullptr;
}

std::vector<std::string> SectorEditorTextureCatalogService::TextureIds() const
{
    return SortedSectorTopologyTextureIds(context_.state.topologyMap);
}

void SectorEditorTextureCatalogService::PopulatePickerOptions(
        TexturePickerState& picker,
        const std::string& currentTexture) const
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    picker.textureIds = TextureIds();
    picker.optionLabels.reserve(picker.textureIds.size());
    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

engine::TextureHandle SectorEditorTextureCatalogService::TextureHandleForId(
        const std::string& textureId) const
{
    const auto it = context_.state.editorTextureHandlesById.find(textureId);
    return it == context_.state.editorTextureHandlesById.end()
            ? engine::NullTextureHandle()
            : it->second;
}

void SectorEditorTextureCatalogService::RefreshTextureHandles(engine::AssetManager& assets)
{
    if (!engine::IsNull(context_.state.editorTextureScope)) {
        assets.UnloadScope(context_.state.editorTextureScope);
        context_.state.editorTextureScope = engine::NullAssetScopeHandle();
    }
    context_.state.editorTextureHandlesById.clear();

    if (context_.state.topologyMap.texturesById.empty()) {
        return;
    }

    context_.state.editorTextureScope = assets.CreateScope("sector_editor_textures");
    if (engine::IsNull(context_.state.editorTextureScope)) {
        return;
    }

    for (const std::string& textureId : TextureIds()) {
        const SectorTextureDefinition* texture = FindTexture(textureId);
        if (texture == nullptr) {
            continue;
        }

        const std::string resolvedPath = ResolveEditorAssetPath(texture->path);
        context_.state.editorTextureHandlesById.emplace(
                texture->id,
                assets.RequestTexture(
                        context_.state.editorTextureScope,
                        texture->id.c_str(),
                        resolvedPath.c_str(),
                        SectorTextureLoadFlags(texture->filter)
                )
        );
    }
}

void SectorEditorTextureCatalogService::RefreshDefaultTextureIds()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = context_.state.topologyMap.texturesById.find(preferred);
        if (preferredIt != context_.state.topologyMap.texturesById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        const std::vector<std::string> textureIds = TextureIds();
        return textureIds.empty() ? std::string{} : textureIds.front();
    };

    context_.state.defaultWallTextureId = findTexture("wall");
    context_.state.defaultFloorTextureId = findTexture("floor");
    context_.state.defaultCeilingTextureId = findTexture("ceiling");
    context_.state.defaultLowerWallTextureId =
            findTexture("step_wall", context_.state.defaultWallTextureId);
    context_.state.defaultUpperWallTextureId =
            findTexture("upper_wall", context_.state.defaultWallTextureId);
}

void SectorEditorTextureCatalogService::SelectAddMapTexturePath(
        AddMapTextureState& modalState,
        int pathIndex) const
{
    if (pathIndex < 0 || pathIndex >= static_cast<int>(modalState.paths.size())) {
        modalState.selectedPathIndex = -1;
        modalState.textureIdBuffer[0] = '\0';
        return;
    }

    modalState.selectedPathIndex = pathIndex;
    const std::string base = GeneratedTextureIdBase(modalState.paths[static_cast<size_t>(pathIndex)]);
    std::string uniqueId = base;
    int suffix = 1;
    while (HasTexture(uniqueId)) {
        char suffixBuffer[16] = {};
        std::snprintf(suffixBuffer, sizeof(suffixBuffer), "_%03d", suffix);
        uniqueId = base + suffixBuffer;
        ++suffix;
    }

    std::snprintf(modalState.textureIdBuffer, sizeof(modalState.textureIdBuffer), "%s", uniqueId.c_str());
    modalState.previewPath.clear();
    modalState.previewTexture = engine::NullTextureHandle();
}

bool SectorEditorTextureCatalogService::ValidateAddMapTextureId(
        const AddMapTextureState& modalState,
        std::string& error) const
{
    error.clear();
    if (modalState.selectedPathIndex < 0 || modalState.selectedPathIndex >= static_cast<int>(modalState.paths.size())) {
        error = "Select a PNG file";
        return false;
    }

    const std::string id = modalState.textureIdBuffer;
    if (id.empty()) {
        error = "Texture ID is required";
        return false;
    }
    if (!IsValidTextureId(id)) {
        error = "Texture ID may only contain letters, digits, underscores, and dashes";
        return false;
    }
    return true;
}

SectorEditorAddTextureResult SectorEditorTextureCatalogService::RegisterSelectedMapTexture(
        AddMapTextureState& modalState)
{
    SectorEditorAddTextureResult result;
    if (!ValidateAddMapTextureId(modalState, result.error)) {
        modalState.validationMessage = result.error;
        return result;
    }

    const std::string id = modalState.textureIdBuffer;
    const std::string path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    result.replacing = HasTexture(id);
    result.textureId = id;

    SectorTextureDefinition definition;
    definition.id = id;
    definition.path = path;
    definition.filter = modalState.filter;
    context_.state.topologyMap.texturesById[id] = std::move(definition);

    result.success = true;
    return result;
}

} // namespace game
