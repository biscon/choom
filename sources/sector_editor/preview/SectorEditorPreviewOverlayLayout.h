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

inline constexpr std::array<SectorEditorPreviewDebugTabDefinition, 11>
        SectorEditorPreviewDebugTabs{{
                {PreviewDebugOverlayTab::View, "sector_editor_preview_tab_view", "View"},
                {PreviewDebugOverlayTab::Render, "sector_editor_preview_tab_render", "Render"},
                {PreviewDebugOverlayTab::Visibility, "sector_editor_preview_tab_visibility", "Visibility"},
                {PreviewDebugOverlayTab::Lighting, "sector_editor_preview_tab_lighting", "Lighting"},
                {PreviewDebugOverlayTab::Atmosphere, "sector_editor_preview_tab_atmosphere", "Atmo"},
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

inline float SectorEditorPreviewOverlayExpandedHeight(
        PreviewDebugOverlayTab activeTab,
        float viewportHeight = 0.0f)
{
    if (activeTab == PreviewDebugOverlayTab::Pbr) return 610.0f;
    if (activeTab == PreviewDebugOverlayTab::Atmosphere) {
        const float desired = 600.0f;
        return viewportHeight > 0.0f
                ? std::min(desired, std::max(240.0f, viewportHeight - 64.0f))
                : desired;
    }
    if (activeTab == PreviewDebugOverlayTab::Navigation) return 760.0f;
    return 390.0f;
}

struct SectorEditorPreviewAtmosphereOverlayLayout {
    Rectangle backend;
    Rectangle debugView;
    Rectangle freezeHistory;
    Rectangle capture;
    Rectangle copyReport;
    Rectangle diagnosticsScroll;
};

inline SectorEditorPreviewAtmosphereOverlayLayout
BuildSectorEditorPreviewAtmosphereOverlayLayout(
        Rectangle panel,
        float padding,
        float contentTop,
        float rowHeight,
        float gap)
{
    const float contentWidth = std::max(0.0f, panel.width - padding * 2.0f);
    const float controlGap = 8.0f;
    const float backendWidth = 180.0f;
    const float debugWidth = 190.0f;
    const float freezeWidth = 170.0f;
    const float actionWidth = 132.0f;
    const float actionTop = contentTop + rowHeight + gap;
    const float scrollTop = actionTop + rowHeight + gap;
    return SectorEditorPreviewAtmosphereOverlayLayout{
            Rectangle{panel.x + padding, contentTop, backendWidth, rowHeight},
            Rectangle{panel.x + padding + backendWidth + controlGap,
                    contentTop, debugWidth, rowHeight},
            Rectangle{panel.x + padding + backendWidth + controlGap
                            + debugWidth + controlGap,
                    contentTop, freezeWidth, rowHeight},
            Rectangle{panel.x + padding, actionTop, actionWidth, rowHeight},
            Rectangle{panel.x + padding + actionWidth + controlGap,
                    actionTop, actionWidth, rowHeight},
            Rectangle{panel.x + padding, scrollTop, contentWidth,
                    std::max(0.0f, panel.y + panel.height - padding - scrollTop)}};
}

} // namespace game
