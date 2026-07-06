#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

#include <cstddef>

namespace game {

void CloseSectorEditorTexturePicker(TexturePickerState& picker)
{
    picker = TexturePickerState{};
}

void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const std::vector<std::string>& textureIds,
        const std::string& currentTexture)
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());
    picker.optionLabels.reserve(picker.textureIds.size());

    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

void OpenSectorEditorTexturePicker(
        TexturePickerState& picker,
        const std::vector<std::string>& textureIds,
        const std::string& currentTexture)
{
    picker.open = true;
    PopulateSectorEditorTexturePickerOptions(picker, textureIds, currentTexture);
}

SectorEditorSelectedTexture CurrentSectorEditorTexturePickerSelection(const TexturePickerState& picker)
{
    SectorEditorSelectedTexture selected;
    if (!picker.open
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::None
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        return selected;
    }

    selected.valid = true;
    selected.textureId = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    return selected;
}

} // namespace game
