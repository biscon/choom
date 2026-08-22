#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

#include <cstddef>

namespace game {

void CloseSectorEditorTexturePicker(TexturePickerState& picker)
{
    picker = TexturePickerState{};
}

void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const std::vector<std::string>& materialIds,
        const std::string& currentTexture)
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.materialIds.clear();
    picker.optionLabels.clear();

    picker.materialIds.insert(picker.materialIds.end(), materialIds.begin(), materialIds.end());
    picker.optionLabels.reserve(picker.materialIds.size());

    for (size_t i = 0; i < picker.materialIds.size(); ++i) {
        picker.optionLabels.push_back(picker.materialIds[i].c_str());
        if (picker.materialIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
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
