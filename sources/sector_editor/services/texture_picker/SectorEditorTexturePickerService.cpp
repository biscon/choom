#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>

namespace game {

namespace {

bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
{
    if (filter.empty()) return true;
    return std::search(
            text.begin(), text.end(),
            filter.begin(), filter.end(),
            [](char lhs, char rhs) {
                return std::tolower(static_cast<unsigned char>(lhs))
                        == std::tolower(static_cast<unsigned char>(rhs));
            }) != text.end();
}

void RebuildSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const std::string& preferredMaterialId)
{
    picker.materialIds.clear();
    picker.optionLabels.clear();

    const std::string_view filter = picker.filterBuffer;
    picker.materialIds.reserve(picker.allMaterialIds.size());
    for (const std::string& materialId : picker.allMaterialIds) {
        if (ContainsCaseInsensitive(materialId, filter)) {
            picker.materialIds.push_back(materialId);
        }
    }

    picker.optionLabels.reserve(picker.materialIds.size());
    picker.selectedTextureIndex = picker.materialIds.empty() ? -1 : 0;
    for (size_t i = 0; i < picker.materialIds.size(); ++i) {
        picker.optionLabels.push_back(picker.materialIds[i].c_str());
        if (picker.materialIds[i] == preferredMaterialId) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
    picker.filterMessage = picker.materialIds.empty()
            ? "No materials match the filter"
            : std::string{};
}

} // namespace

void CloseSectorEditorTexturePicker(TexturePickerState& picker)
{
    picker = TexturePickerState{};
}

void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const std::vector<std::string>& materialIds,
        const std::string& currentTexture)
{
    picker.scroll = engine::UIScrollState{};
    picker.allMaterialIds = materialIds;
    picker.filterBuffer[0] = '\0';
    RebuildSectorEditorTexturePickerOptions(picker, currentTexture);
}

void ApplySectorEditorTexturePickerFilter(TexturePickerState& picker)
{
    std::string selectedMaterialId;
    if (picker.selectedTextureIndex >= 0
            && picker.selectedTextureIndex < static_cast<int>(picker.materialIds.size())) {
        selectedMaterialId = picker.materialIds[static_cast<size_t>(picker.selectedTextureIndex)];
    }
    picker.scroll = engine::UIScrollState{};
    RebuildSectorEditorTexturePickerOptions(picker, selectedMaterialId);
}

void OpenSectorEditorTexturePicker(
        TexturePickerState& picker,
        const std::vector<std::string>& materialIds,
        const std::string& currentTexture)
{
    picker.open = true;
    PopulateSectorEditorTexturePickerOptions(picker, materialIds, currentTexture);
}

SectorEditorSelectedTexture CurrentSectorEditorTexturePickerSelection(const TexturePickerState& picker)
{
    SectorEditorSelectedTexture selected;
    if (!picker.open
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::None
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.materialIds.size())) {
        return selected;
    }

    selected.valid = true;
    selected.materialId = picker.materialIds[static_cast<size_t>(picker.selectedTextureIndex)];
    return selected;
}

} // namespace game
