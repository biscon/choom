#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace game {
namespace {

bool IsSupportedStaticModelPath(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
    return extension == ".gltf" || extension == ".glb";
}

std::string NormalizeAssetPath(
        const std::string& assetRelativeRoot,
        const std::filesystem::path& relativePath)
{
    std::filesystem::path combined =
            std::filesystem::path(assetRelativeRoot) / relativePath;
    return combined.lexically_normal().generic_string();
}

} // namespace

SectorEditorStaticModelPickerService::SectorEditorStaticModelPickerService(
        StaticModelPickerState& state,
        std::string& statusText)
    : state_(state)
    , statusText_(statusText)
{
}

void SectorEditorStaticModelPickerService::Open(
        const std::string& currentModelPath,
        ModelPickerTarget target)
{
    state_.target = target;
    state_.open = true;
    state_.requestedModelPath = currentModelPath;
    state_.scroll = engine::UIScrollState{};
    if (!state_.scanned) {
        Refresh();
    } else {
        RestoreRequestedSelection();
    }
    statusText_ = target == ModelPickerTarget::DynamicModel
            ? "Choose a dynamic prop model"
            : "Choose a static prop model";
}

void SectorEditorStaticModelPickerService::Close()
{
    state_.open = false;
    statusText_ = state_.target == ModelPickerTarget::DynamicModel
            ? "Dynamic model selection cancelled"
            : "Static model selection cancelled";
}

bool SectorEditorStaticModelPickerService::Refresh()
{
    return RefreshFromRoot(
            std::filesystem::path(ASSETS_PATH) / "models",
            "assets/models");
}

bool SectorEditorStaticModelPickerService::RefreshFromRoot(
        const std::filesystem::path& modelsRoot,
        const std::string& assetRelativeRoot)
{
    if (HasSelection()) {
        state_.requestedModelPath = SelectedModelPath();
    }
    state_.modelPaths.clear();
    state_.optionLabels.clear();
    state_.selectedModelIndex = -1;
    state_.scanned = true;

    std::error_code error;
    if (!std::filesystem::is_directory(modelsRoot, error)) {
        state_.scanMessage = "Model folder unavailable: "
                + modelsRoot.lexically_normal().generic_string();
        statusText_ = state_.scanMessage;
        return false;
    }

    const std::filesystem::directory_options options =
            std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator it(modelsRoot, options, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && it != end) {
        const std::filesystem::directory_entry& entry = *it;
        std::error_code entryError;
        if (entry.is_regular_file(entryError)
                && !entryError
                && IsSupportedStaticModelPath(entry.path())) {
            const std::filesystem::path relative =
                    std::filesystem::relative(entry.path(), modelsRoot, entryError);
            if (!entryError && !relative.empty()) {
                state_.modelPaths.push_back(
                        NormalizeAssetPath(assetRelativeRoot, relative));
            }
        }
        it.increment(error);
    }

    std::sort(state_.modelPaths.begin(), state_.modelPaths.end());
    state_.modelPaths.erase(
            std::unique(state_.modelPaths.begin(), state_.modelPaths.end()),
            state_.modelPaths.end());
    RebuildOptionLabels();
    RestoreRequestedSelection();
    state_.scanMessage = error
            ? "Model scan completed with filesystem errors"
            : (state_.modelPaths.empty()
                    ? "No .gltf or .glb models found"
                    : "Found " + std::to_string(state_.modelPaths.size()) + " models");
    statusText_ = state_.scanMessage;
    return !error;
}

bool SectorEditorStaticModelPickerService::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.modelPaths.size())) {
        state_.selectedModelIndex = -1;
        return false;
    }
    state_.selectedModelIndex = index;
    return true;
}

bool SectorEditorStaticModelPickerService::HasSelection() const
{
    return state_.selectedModelIndex >= 0
            && state_.selectedModelIndex < static_cast<int>(state_.modelPaths.size());
}

std::string SectorEditorStaticModelPickerService::SelectedModelPath() const
{
    return HasSelection()
            ? state_.modelPaths[static_cast<size_t>(state_.selectedModelIndex)]
            : std::string{};
}

void SectorEditorStaticModelPickerService::RebuildOptionLabels()
{
    state_.optionLabels.clear();
    state_.optionLabels.reserve(state_.modelPaths.size());
    for (const std::string& path : state_.modelPaths) {
        state_.optionLabels.push_back(path.c_str());
    }
}

void SectorEditorStaticModelPickerService::RestoreRequestedSelection()
{
    state_.selectedModelIndex = -1;
    const auto found = std::lower_bound(
            state_.modelPaths.begin(),
            state_.modelPaths.end(),
            state_.requestedModelPath);
    if (found != state_.modelPaths.end() && *found == state_.requestedModelPath) {
        state_.selectedModelIndex = static_cast<int>(
                std::distance(state_.modelPaths.begin(), found));
    }
}

} // namespace game
