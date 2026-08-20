#pragma once

#include "sector_editor/SectorEditorPreviewTypes.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace game {

struct SectorEditorPreviewDebugTabDefinition {
    PreviewDebugOverlayTab tab;
    const char* id;
    const char* label;
};

struct SectorEditorPreviewLightStartActionLayout {
    Rectangle pilot = {};
    Rectangle halo = {};
    Rectangle shaft = {};
    float reservedWidth = 0.0f;
};

inline constexpr std::array<SectorEditorPreviewDebugTabDefinition, 10>
        SectorEditorPreviewDebugTabs{{
                {PreviewDebugOverlayTab::View, "sector_editor_preview_tab_view", "View"},
                {PreviewDebugOverlayTab::Render, "sector_editor_preview_tab_render", "Render"},
                {PreviewDebugOverlayTab::Visibility, "sector_editor_preview_tab_visibility", "Visibility"},
                {PreviewDebugOverlayTab::Lighting, "sector_editor_preview_tab_lighting", "Lighting"},
                {PreviewDebugOverlayTab::Pbr, "sector_editor_preview_tab_pbr", "PBR"},
                {PreviewDebugOverlayTab::Objects, "sector_editor_preview_tab_objects", "Objects"},
                {PreviewDebugOverlayTab::Probes, "sector_editor_preview_tab_probes", "Probes"},
                {PreviewDebugOverlayTab::Viewmodel, "sector_editor_preview_tab_viewmodel", "Arms"},
                {PreviewDebugOverlayTab::Navigation, "sector_editor_preview_tab_navigation", "Nav"},
                {PreviewDebugOverlayTab::Controls, "sector_editor_preview_tab_controls", "Controls"}
        }};

inline Rectangle BuildSectorEditorPreviewDebugTabRect(
        Rectangle panel,
        float padding,
        float stripHeight,
        float sectionGap,
        float tabHeight,
        float tabGap,
        size_t index)
{
    const float contentWidth = panel.width - padding * 2.0f;
    const float tabWidth = (contentWidth
            - tabGap * static_cast<float>(SectorEditorPreviewDebugTabs.size() - 1))
            / static_cast<float>(SectorEditorPreviewDebugTabs.size());
    return Rectangle{
            panel.x + padding + static_cast<float>(index) * (tabWidth + tabGap),
            panel.y + padding + stripHeight + sectionGap,
            tabWidth,
            tabHeight};
}

inline float SectorEditorPreviewOverlayExpandedHeight(PreviewDebugOverlayTab activeTab)
{
    if (activeTab == PreviewDebugOverlayTab::Pbr) return 610.0f;
    if (activeTab == PreviewDebugOverlayTab::Navigation) return 760.0f;
    return 390.0f;
}

inline SectorEditorPreviewLightStartActionLayout BuildSectorEditorPreviewLightStartActionLayout(
        Rectangle panel,
        float padding,
        float actionY,
        bool hasHalo,
        bool hasShaft)
{
    SectorEditorPreviewLightStartActionLayout layout;
    const float right = panel.x + panel.width - padding;
    float actionsRight = right;
    layout.pilot = Rectangle{actionsRight - 92.0f, actionY, 92.0f, 28.0f};
    if (hasHalo) {
        actionsRight -= 102.0f;
        layout.halo = Rectangle{actionsRight - 104.0f, actionY, 104.0f, 28.0f};
    }
    if (hasShaft) {
        actionsRight -= 114.0f;
        layout.shaft = Rectangle{actionsRight - 110.0f, actionY, 110.0f, 28.0f};
    }
    layout.reservedWidth = right - std::min(
            layout.pilot.x,
            hasShaft ? layout.shaft.x : (hasHalo ? layout.halo.x : layout.pilot.x));
    return layout;
}

} // namespace game
