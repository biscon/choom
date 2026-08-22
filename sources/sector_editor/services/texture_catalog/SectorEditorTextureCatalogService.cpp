#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorMaterialRegistry.h"
#include "sector_demo/SectorTextureTypes.h"

#include <utility>

namespace game {

SectorEditorTextureCatalogService::SectorEditorTextureCatalogService(
        SectorEditorTextureCatalogServiceContext context)
    : context_(context)
{
}

const SectorMaterialDefinition* SectorEditorTextureCatalogService::FindTexture(
        const std::string& materialId) const
{
    return FindSectorMaterial(context_.registry, materialId);
}

bool SectorEditorTextureCatalogService::HasTexture(const std::string& materialId) const
{
    return FindTexture(materialId) != nullptr;
}

std::vector<std::string> SectorEditorTextureCatalogService::TextureIds() const
{
    return SortedSectorMaterialIds(context_.registry);
}

void SectorEditorTextureCatalogService::PopulatePickerOptions(
        TexturePickerState& picker,
        const std::string& currentTexture) const
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.materialIds.clear();
    picker.optionLabels.clear();

    picker.materialIds = TextureIds();
    picker.optionLabels.reserve(picker.materialIds.size());
    for (size_t i = 0; i < picker.materialIds.size(); ++i) {
        picker.optionLabels.push_back(picker.materialIds[i].c_str());
        if (picker.materialIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

engine::TextureHandle SectorEditorTextureCatalogService::TextureHandleForId(
        const std::string& materialId) const
{
    const auto it = context_.textureState.editorTextureHandlesById.find(materialId);
    return it == context_.textureState.editorTextureHandlesById.end()
            ? engine::NullTextureHandle()
            : it->second;
}

void SectorEditorTextureCatalogService::RefreshTextureHandles(engine::AssetManager& assets)
{
    if (!engine::IsNull(context_.textureState.editorTextureScope)) {
        assets.UnloadScope(context_.textureState.editorTextureScope);
        context_.textureState.editorTextureScope = engine::NullAssetScopeHandle();
    }
    context_.textureState.editorTextureHandlesById.clear();

}

engine::TextureHandle SectorEditorTextureCatalogService::EnsureTextureHandleForId(
        const std::string& materialId,
        engine::AssetManager& assets)
{
    const engine::TextureHandle existing = TextureHandleForId(materialId);
    if (!engine::IsNull(existing)) return existing;
    const SectorMaterialDefinition* material = FindTexture(materialId);
    if (material == nullptr) return engine::NullTextureHandle();
    if (engine::IsNull(context_.textureState.editorTextureScope)) {
        context_.textureState.editorTextureScope =
                assets.CreateScope("sector_editor_material_previews");
    }
    if (engine::IsNull(context_.textureState.editorTextureScope)) {
        return engine::NullTextureHandle();
    }
    const std::string resolvedPath = ResolveEditorAssetPath(material->path);
    const std::string key = resolvedPath + "|preview|"
            + std::to_string(static_cast<int>(material->filter));
    const engine::TextureHandle handle = assets.RequestTexture(
            context_.textureState.editorTextureScope,
            key.c_str(),
            resolvedPath.c_str(),
            engine::TextureColorUsage::DisplaySrgb,
            SectorMaterialTextureLoadFlags(material->filter));
    context_.textureState.editorTextureHandlesById.emplace(materialId, handle);
    return handle;
}

void SectorEditorTextureCatalogService::RefreshDefaultTextureIds()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = context_.registry.materialsById.find(preferred);
        if (preferredIt != context_.registry.materialsById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        const std::vector<std::string> materialIds = TextureIds();
        return materialIds.empty() ? std::string{} : materialIds.front();
    };

    context_.defaultWallTextureId = findTexture("wall");
    context_.defaultFloorTextureId = findTexture("floor");
    context_.defaultCeilingTextureId = findTexture("ceiling");
    context_.defaultLowerWallTextureId =
            findTexture("step_wall", context_.defaultWallTextureId);
    context_.defaultUpperWallTextureId =
            findTexture("upper_wall", context_.defaultWallTextureId);
}

} // namespace game
