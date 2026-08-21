#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <functional>

namespace game {

inline std::array<Rectangle, 4> BuildSectorPreviewSettingsTabLayout(
        Rectangle modal,
        float y,
        float tabHeight)
{
    constexpr float margin = 30.0f;
    constexpr float gap = 8.0f;
    const float tabWidth = (modal.width - margin * 2.0f - gap * 3.0f) / 4.0f;
    std::array<Rectangle, 4> tabs{};
    for (size_t i = 0; i < tabs.size(); ++i) {
        tabs[i] = Rectangle{
                modal.x + margin + (tabWidth + gap) * static_cast<float>(i),
                y,
                tabWidth,
                tabHeight
        };
    }
    return tabs;
}

inline float MeasureSectorPreviewSettingsLightingContentHeight(
        float rowHeight,
        float gap)
{
    constexpr float sectionLead = 8.0f;
    constexpr float sectionTitleHeight = 38.0f;
    constexpr float swatchHeight = 36.0f;
    constexpr float trailingPadding = 12.0f;
    return 16.0f * (rowHeight + gap)
            + 4.0f * (sectionLead + sectionTitleHeight)
            + swatchHeight + gap
            + trailingPadding;
}

inline float MeasureSectorPreviewSettingsFogContentHeight(
        float rowHeight,
        float gap)
{
    constexpr float noteHeight = 36.0f;
    constexpr float colorTitleHeight = 38.0f;
    constexpr float swatchHeight = 36.0f;
    constexpr float trailingPadding = 12.0f;
    return 10.0f * (rowHeight + gap)
            + noteHeight + gap
            + colorTitleHeight
            + swatchHeight + gap
            + trailingPadding;
}

inline void ResetSectorPreviewSettingsModalPreservingView(
        SectorPreviewSettingsModalState& modalState)
{
    const PreviewSettingsTab activeTab = modalState.activeTab;
    const engine::UIScrollState generalScroll = modalState.generalScroll;
    const engine::UIScrollState skyScroll = modalState.skyScroll;
    const engine::UIScrollState lightingScroll = modalState.lightingScroll;
    const engine::UIScrollState fogScroll = modalState.fogScroll;

    modalState = SectorPreviewSettingsModalState{};
    modalState.activeTab = activeTab;
    modalState.generalScroll = generalScroll;
    modalState.skyScroll = skyScroll;
    modalState.lightingScroll = lightingScroll;
    modalState.fogScroll = fogScroll;
}

inline SectorLightmapBakeSettings NormalizeSectorPreviewObjectProbeSettings(
        SectorLightmapBakeSettings settings)
{
    settings.objectProbeSpacingWorld = std::clamp(settings.objectProbeSpacingWorld, 0.25f, 128.0f);
    settings.objectProbeLowerHeightWorld = std::clamp(
            settings.objectProbeLowerHeightWorld, 0.0f, 16.0f);
    settings.objectProbeUpperHeightWorld = std::clamp(
            settings.objectProbeUpperHeightWorld, 0.0f, 16.0f);
    if (settings.objectProbeLowerHeightWorld
            > settings.objectProbeUpperHeightWorld) {
        std::swap(
                settings.objectProbeLowerHeightWorld,
                settings.objectProbeUpperHeightWorld);
    }
    return settings;
}

inline void ResetSectorPreviewSettingsModalLightingDefaults(
        SectorPreviewSettingsModalState& modalState)
{
    modalState.draftDirectionalLight = DefaultSectorTopologyDirectionalLightSettings();
    modalState.draftLightmapSettings = SectorLightmapBakeSettings{};
    modalState.draftHdrBloom = engine::HdrBloomSettings{};
    modalState.lightDirectionXInput = engine::UIFloatInputState{};
    modalState.lightDirectionYInput = engine::UIFloatInputState{};
    modalState.lightDirectionZInput = engine::UIFloatInputState{};
    modalState.lightIntensityInput = engine::UIFloatInputState{};
    modalState.objectProbeSpacingInput = engine::UIFloatInputState{};
    modalState.objectProbeLowerHeightInput = engine::UIFloatInputState{};
    modalState.objectProbeUpperHeightInput = engine::UIFloatInputState{};
    modalState.bloomThresholdInput = {};
    modalState.bloomSoftKneeInput = {};
    modalState.bloomIntensityInput = {};
    modalState.bloomRadiusInput = {};
    modalState.lightColorRedInput = engine::UIIntInputState{};
    modalState.lightColorGreenInput = engine::UIIntInputState{};
    modalState.lightColorBlueInput = engine::UIIntInputState{};
}

inline void ResetSectorPreviewSettingsModalFogDefaults(
        SectorPreviewSettingsModalState& modalState)
{
    modalState.draftFogSettings = DefaultSectorTopologyFogSettings();
    modalState.fogStartDistanceInput = engine::UIFloatInputState{};
    modalState.fogEndDistanceInput = engine::UIFloatInputState{};
    modalState.fogFalloffExponentInput = engine::UIFloatInputState{};
    modalState.fogBrightnessInput = engine::UIFloatInputState{};
    modalState.fogDensityInput = engine::UIFloatInputState{};
    modalState.fogMaxOpacityInput = engine::UIFloatInputState{};
    modalState.fogReferenceHeightInput = engine::UIFloatInputState{};
    modalState.fogHeightFalloffInput = engine::UIFloatInputState{};
    modalState.fogColorRedInput = engine::UIIntInputState{};
    modalState.fogColorGreenInput = engine::UIIntInputState{};
    modalState.fogColorBlueInput = engine::UIIntInputState{};
}

inline bool ApplySectorPreviewFogSettings(
        SectorTopologyMap& map,
        const SectorTopologyFogSettings& draftSettings)
{
    const SectorTopologyFogSettings draft = NormalizeSectorTopologyFogSettings(draftSettings);
    const SectorTopologyFogSettings current = NormalizeSectorTopologyFogSettings(map.fogSettings);
    const bool same = current.enabled == draft.enabled
            && current.mode == draft.mode
            && current.color.r == draft.color.r
            && current.color.g == draft.color.g
            && current.color.b == draft.color.b
            && current.color.a == draft.color.a
            && current.startDistanceWorld == draft.startDistanceWorld
            && current.endDistanceWorld == draft.endDistanceWorld
            && current.falloffExponent == draft.falloffExponent
            && current.brightness == draft.brightness
            && current.density == draft.density
            && current.maxOpacity == draft.maxOpacity
            && current.referenceHeightWorld == draft.referenceHeightWorld
            && current.heightFalloff == draft.heightFalloff;
    if (same) {
        return false;
    }
    map.fogSettings = draft;
    return true;
}

inline bool ApplySectorPreviewObjectProbeSettings(
        SectorTopologyMap& map,
        const SectorLightmapBakeSettings& draftSettings)
{
    const SectorLightmapBakeSettings normalizedDraft =
            NormalizeSectorPreviewObjectProbeSettings(draftSettings);
    const SectorLightmapBakeSettings normalizedCurrent =
            NormalizeSectorPreviewObjectProbeSettings(map.lightmapSettings);
    if (normalizedCurrent.objectProbeSpacingWorld == normalizedDraft.objectProbeSpacingWorld
            && normalizedCurrent.objectProbeLowerHeightWorld
                    == normalizedDraft.objectProbeLowerHeightWorld
            && normalizedCurrent.objectProbeUpperHeightWorld
                    == normalizedDraft.objectProbeUpperHeightWorld) {
        return false;
    }

    map.lightmapSettings.objectProbeSpacingWorld = normalizedDraft.objectProbeSpacingWorld;
    map.lightmapSettings.objectProbeLowerHeightWorld =
            normalizedDraft.objectProbeLowerHeightWorld;
    map.lightmapSettings.objectProbeUpperHeightWorld =
            normalizedDraft.objectProbeUpperHeightWorld;
    return true;
}

struct SectorEditorPreviewSettingsModalCallbacks {
    std::function<void()> close;
    std::function<void()> apply;
    std::function<void()> openSkyTexturePicker;
};

void DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorPreviewSettingsModalState& modalState,
        bool texturePickerOpen,
        const SectorEditorPreviewSettingsModalCallbacks& callbacks);

} // namespace game
