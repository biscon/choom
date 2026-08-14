#pragma once

#include "sector_editor/SectorEditorPreviewTypes.h"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace game {

struct SectorEditorPreviewDebugTabDefinition {
    PreviewDebugOverlayTab tab;
    const char* id;
    const char* label;
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
    if (activeTab == PreviewDebugOverlayTab::Navigation) return 520.0f;
    return 390.0f;
}

} // namespace game
