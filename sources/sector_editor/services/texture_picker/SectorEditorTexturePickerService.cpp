#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <cstddef>
#include <vector>

namespace game {

void CloseSectorEditorTexturePicker(TexturePickerState& picker)
{
    picker = TexturePickerState{};
}

void PopulateSectorEditorTexturePickerOptions(
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const std::string& currentTexture)
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(map);
    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());

    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

void OpenSectorEditorTexturePicker(
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const std::string& currentTexture)
{
    picker.open = true;
    PopulateSectorEditorTexturePickerOptions(picker, map, currentTexture);
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
