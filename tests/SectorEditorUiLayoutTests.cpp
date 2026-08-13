#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"
#include "sector_editor/inspector/SectorEditorInspectorPanel.h"
#include "sector_demo/SectorLightmap.h"

#include <cmath>
#include <array>
#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

bool Overlaps(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width
            && a.x + a.width > b.x
            && a.y < b.y + b.height
            && a.y + a.height > b.y;
}

bool Contains(Rectangle outer, Rectangle inner)
{
    return inner.x >= outer.x
            && inner.y >= outer.y
            && inner.x + inner.width <= outer.x + outer.width
            && inner.y + inner.height <= outer.y + outer.height;
}

void TestModelFilenameExtraction()
{
    Check(game::SectorEditorModelFilename(
                  "assets/models/props/medical_cart.glb")
                    == "medical_cart.glb",
          "model filename removes nested asset directories");
    Check(game::SectorEditorModelFilename("crate.gltf") == "crate.gltf",
          "model filename preserves a bare filename and extension");
    Check(game::SectorEditorModelFilename(
                  "assets\\models\\characters\\guard.glb")
                    == "guard.glb",
          "model filename accepts backslash-separated paths");
    Check(game::SectorEditorModelFilename("").empty(),
          "empty model path produces an empty filename");
}

void TestTextureRowWithoutClear()
{
    const game::SectorEditorInspectorTextureRowLayout layout =
            game::BuildSectorEditorInspectorTextureRowLayout(12.0f, 260.0f, 8.0f, 38.0f, 0.0f);

    Check(Near(layout.pickerButtonRect.x, 222.0f), "picker button is right aligned");
    Check(Near(layout.labelRect.width, 214.0f), "label leaves a gap before picker");
    Check(layout.clearButtonRect.width == 0.0f, "missing clear button has zero width");
    Check(layout.valueRect.y > layout.labelRect.y + layout.labelRect.height,
          "texture value is on its own line");
    Check(!Overlaps(layout.valueRect, layout.pickerButtonRect),
          "texture value line does not overlap picker button");
}

void TestTextureRowWithClear()
{
    const game::SectorEditorInspectorTextureRowLayout layout =
            game::BuildSectorEditorInspectorTextureRowLayout(0.0f, 260.0f, 8.0f, 38.0f, 92.0f);

    Check(Near(layout.clearButtonRect.x, 122.0f), "clear button sits before picker");
    Check(Near(layout.pickerButtonRect.x, 222.0f), "picker remains right aligned with clear button");
    Check(!Overlaps(layout.labelRect, layout.clearButtonRect), "label does not overlap clear button");
    Check(!Overlaps(layout.clearButtonRect, layout.pickerButtonRect), "clear button does not overlap picker");
    Check(layout.valueRect.width > layout.labelRect.width, "texture value line has readable width");
}

void TestCompactNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout narrow =
            game::BuildSectorEditorInspectorCompactNumericRowLayout(4.0f, 150.0f, 40.0f);
    const game::SectorEditorInspectorNumericRowLayout wide =
            game::BuildSectorEditorInspectorCompactNumericRowLayout(4.0f, 320.0f, 40.0f);

    Check(Near(narrow.inputRect.x + narrow.inputRect.width, 150.0f),
          "compact numeric input clamps to narrow content width");
    Check(Near(wide.inputRect.width, game::SectorEditorInspectorCompactInputWidth),
          "compact numeric input keeps fixed width when space allows");
    Check(!Overlaps(wide.labelRect, wide.inputRect), "compact numeric label does not overlap input");
}

void TestRightFloatNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(8.0f, 260.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.width, game::SectorEditorInspectorFloatInputWidth),
          "right float numeric input keeps fixed width when space allows");
    Check(Near(layout.inputRect.x + layout.inputRect.width, 260.0f),
          "right float numeric input is right aligned");
    Check(!Overlaps(layout.labelRect, layout.inputRect),
          "right float numeric label does not overlap input");
    Check(layout.labelRect.width > 120.0f,
          "right float numeric label has room for long labels");
}

void TestRightIntNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightIntRowLayout(8.0f, 260.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.width, game::SectorEditorInspectorIntInputWidth),
          "right int numeric input keeps fixed width when space allows");
    Check(Near(layout.inputRect.x + layout.inputRect.width, 260.0f),
          "right int numeric input is right aligned");
    Check(!Overlaps(layout.labelRect, layout.inputRect),
          "right int numeric label does not overlap input");
}

void TestRightNumericRowClamps()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(8.0f, 72.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.x, 0.0f), "right numeric input clamps to narrow content x");
    Check(Near(layout.inputRect.width, 72.0f), "right numeric input clamps to narrow content width");
    Check(!Overlaps(layout.labelRect, layout.inputRect), "clamped right numeric label does not overlap input");
}

void TestInspectorNumericWidthsMatchControlKinds()
{
    const game::SectorEditorInspectorNumericRowLayout floatLayout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(
                    0.0f, 320.0f, 40.0f, 8.0f);
    const game::SectorEditorInspectorNumericRowLayout intLayout =
            game::BuildSectorEditorInspectorRightIntRowLayout(
                    0.0f, 320.0f, 40.0f, 8.0f);

    Check(Near(floatLayout.inputRect.width, 112.0f),
          "prop float fields use the compact fixed input width");
    Check(floatLayout.labelRect.width >= 200.0f,
          "prop float rows leave room for transform labels");
    Check(Near(intLayout.inputRect.width, 150.0f),
          "integer steppers reserve enough width for buttons and value text");
    Check(intLayout.inputRect.width > floatLayout.inputRect.width,
          "integer steppers are wider than float fields");
}

void TestTextureRowHeight()
{
    Check(Near(game::SectorEditorInspectorTextureRowHeight(), 60.0f),
          "texture row height accounts for action and value lines");
}

void TestStackedOptionRow()
{
    const game::SectorEditorInspectorStackedOptionRowLayout layout =
            game::BuildSectorEditorInspectorStackedOptionRowLayout(12.0f, 260.0f, 40.0f, 8.0f);

    Check(Near(layout.labelRect.x, 0.0f), "stacked option label starts at content x");
    Check(Near(layout.labelRect.width, 260.0f), "stacked option label is full width");
    Check(Near(layout.fieldRect.y, layout.labelRect.y + layout.labelRect.height + 8.0f),
          "stacked option field is below label with gap");
    Check(Near(layout.fieldRect.width, 260.0f), "stacked option field is full width");
    Check(!Overlaps(layout.labelRect, layout.fieldRect), "stacked option label does not overlap field");
    Check(Near(layout.height, 74.0f), "stacked option height accounts for label gap and field");
}

void TestRuntimeObjectInspectorHeightCountsBillboardRows()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float spriteLabelHeight = 54.0f;
    const float aspectWarningHeight = 28.0f;
    const float unsupportedHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            false,
            false,
            false,
            spriteLabelHeight,
            aspectWarningHeight);
    const float singleClipHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            false,
            false,
            spriteLabelHeight,
            aspectWarningHeight);
    const float directionalHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            false,
            true,
            spriteLabelHeight,
            aspectWarningHeight);
    const float warningHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            true,
            true,
            spriteLabelHeight,
            aspectWarningHeight);

    Check(singleClipHeight > unsupportedHeight,
          "billboard inspector height includes billboard controls");
    Check(Near(directionalHeight - singleClipHeight,
               (game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap) * 3.0f),
          "directional billboard height includes three extra stacked clip rows");
    Check(Near(warningHeight - directionalHeight, aspectWarningHeight + gap),
          "aspect warning height includes text row and trailing gap");
}

void TestDoorInspectorHeightCountsConditionalRows()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float anchorStatusHeight = 44.0f;
    const float assetStatusHeight = 20.0f;
    const float modelDiagnosticHeight = 92.0f;
    const float proceduralSlideHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            0.0f,
            false,
            false);
    const float proceduralSwingHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            0.0f,
            false,
            true);
    const float modelSwingHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            modelDiagnosticHeight,
            true,
            true);
    const float stacked =
            game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
    const float expectedProceduralSlideHeight =
            38.0f + 34.0f
            + anchorStatusHeight + gap
            + (rowH + gap) * 4.0f
            + stacked
            + (rowH + gap) + stacked + (rowH + gap) * 2.0f
            + (rowH + gap) * 5.0f
            + assetStatusHeight + gap
            + (rowH + gap) * 5.0f;
    Check(Near(proceduralSlideHeight, expectedProceduralSlideHeight),
          "door inspector height includes target dimensions, normal offset, and height offset rows");
    Check(Near(proceduralSwingHeight - proceduralSlideHeight, stacked * 2.0f),
          "procedural swing inspector reserves two additional stacked hinge/side rows without clipping later controls");
    Check(modelSwingHeight > proceduralSwingHeight
                  && modelSwingHeight >= modelDiagnosticHeight + stacked * 5.0f,
          "model swing inspector reserves style, fit, diagnostics, and swing controls without overlapping the final actions");
}

void TestDoorTextureSettingsModalLayoutDoesNotOverlap()
{
    const Rectangle modal{100.0f, 80.0f, 680.0f, 600.0f};
    const game::SectorEditorDoorTextureSettingsModalLayout layout =
            game::BuildSectorEditorDoorTextureSettingsModalLayout(modal, 26.0f, 10.0f);

    Check(Contains(modal, layout.titleRect), "door texture modal title fits inside modal");
    Check(Contains(modal, layout.statusRect), "door texture modal status fits inside modal");
    Check(Contains(modal, layout.doneButtonRect), "door texture modal done button fits inside modal");

    for (int i = 0; i < 6; ++i) {
        Check(Contains(modal, layout.faceButtonRects[i]), "door texture modal face button fits inside modal");
        Check(Contains(modal, layout.actionButtonRects[i]), "door texture modal action button fits inside modal");
        for (int j = i + 1; j < 6; ++j) {
            Check(!Overlaps(layout.faceButtonRects[i], layout.faceButtonRects[j]),
                  "door texture modal face buttons do not overlap");
            Check(!Overlaps(layout.actionButtonRects[i], layout.actionButtonRects[j]),
                  "door texture modal action buttons do not overlap");
        }
    }

    for (int i = 0; i < 4; ++i) {
        Check(Contains(modal, layout.uvLabelRects[i]), "door texture modal uv label fits inside modal");
        Check(Contains(modal, layout.uvInputRects[i]), "door texture modal uv input fits inside modal");
        Check(!Overlaps(layout.uvLabelRects[i], layout.uvInputRects[i]),
              "door texture modal uv label does not overlap input");
        for (int j = i + 1; j < 4; ++j) {
            Check(!Overlaps(layout.uvInputRects[i], layout.uvInputRects[j]),
                  "door texture modal uv inputs do not overlap");
        }
        for (int j = 0; j < 6; ++j) {
            Check(!Overlaps(layout.faceButtonRects[j], layout.uvInputRects[i]),
                  "door texture modal face buttons do not overlap uv inputs");
            Check(!Overlaps(layout.actionButtonRects[j], layout.uvInputRects[i]),
                  "door texture modal action buttons do not overlap uv inputs");
        }
    }

    for (int i = 0; i < 6; ++i) {
        Check(!Overlaps(layout.actionButtonRects[i], layout.statusRect),
              "door texture modal action buttons do not overlap status");
        Check(!Overlaps(layout.actionButtonRects[i], layout.doneButtonRect),
              "door texture modal action buttons do not overlap done button");
    }
    Check(!Overlaps(layout.statusRect, layout.doneButtonRect),
          "door texture modal status does not overlap done button");
}

void TestPreviewSettingsModalCopiesObjectProbeSettings()
{
    game::SectorTopologyMap map;
    map.lightmapSettings.objectProbeSpacingWorld = 6.5f;
    map.lightmapSettings.objectProbeLowerHeightWorld = 0.75f;
    map.lightmapSettings.objectProbeUpperHeightWorld = 2.25f;

    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings =
            game::NormalizeSectorPreviewObjectProbeSettings(map.lightmapSettings);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 6.5f),
          "preview settings modal draft copies object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeLowerHeightWorld, 0.75f)
                  && Near(modal.draftLightmapSettings.objectProbeUpperHeightWorld, 2.25f),
          "preview settings modal draft copies layered object probe heights");
}

void TestPreviewSettingsScrollableContentHeightsReachLastControls()
{
    const float rowH = 40.0f;
    const float gap = 12.0f;
    const float lightingLastControlBottom =
            16.0f * (rowH + gap)
            + 4.0f * (8.0f + 38.0f)
            + 36.0f + gap;
    const float fogLastControlBottom =
            10.0f * (rowH + gap)
            + 28.0f
            + 36.0f + gap
            + 38.0f
            + 36.0f + gap;

    Check(Near(
                  game::MeasureSectorPreviewSettingsLightingContentHeight(rowH, gap),
                  lightingLastControlBottom + 12.0f),
          "lighting scroll reaches HDR bloom radius with bottom padding");
    Check(Near(
                  game::MeasureSectorPreviewSettingsFogContentHeight(rowH, gap),
                  fogLastControlBottom + 12.0f),
          "fog scroll reaches the complete color swatch with bottom padding");
}

void TestAuthoringFaceInspectorHeightIncludesAllSections()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float anchorSummaryHeight = 28.0f;
    game::SectorAuthoringFaceAnchor anchor;

    const float height =
            game::MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    anchor,
                    rowH,
                    gap,
                    anchorSummaryHeight);
    Check(Near(height, 1434.0f),
          "authoring face height includes merge, ceiling sky, audio, materials, decals, and padding");

    anchor.floorDecal.textureId = "floor_decal";
    anchor.floorDecal.emissive = true;
    const float assignedDecalHeight =
            game::MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    anchor,
                    rowH,
                    gap,
                    anchorSummaryHeight);
    Check(Near(assignedDecalHeight - height, 232.0f),
          "authoring face height includes expanded emissive flat decal controls");
}

void TestPreviewSettingsModalResetPreservesSessionView()
{
    game::SectorPreviewSettingsModalState modal;
    modal.open = true;
    modal.activeTab = game::PreviewSettingsTab::Weapon;
    modal.generalScroll.offset.y = 11.0f;
    modal.skyScroll.offset.y = 22.0f;
    modal.lightingScroll.offset.y = 33.0f;
    modal.fogScroll.offset.y = 44.0f;
    modal.viewmodelScroll.offset.y = 55.0f;
    modal.weaponScroll.offset.y = 66.0f;
    modal.draftConfig.walkSpeed = 123.0f;
    modal.errorMessage = "discard me";

    game::ResetSectorPreviewSettingsModalPreservingView(modal);

    Check(!modal.open, "preview settings reset closes modal");
    Check(modal.activeTab == game::PreviewSettingsTab::Weapon,
          "preview settings reset preserves active tab for the session");
    Check(Near(modal.generalScroll.offset.y, 11.0f)
                  && Near(modal.skyScroll.offset.y, 22.0f)
                  && Near(modal.lightingScroll.offset.y, 33.0f)
                  && Near(modal.fogScroll.offset.y, 44.0f)
                  && Near(modal.viewmodelScroll.offset.y, 55.0f)
                  && Near(modal.weaponScroll.offset.y, 66.0f),
          "preview settings reset preserves every tab scroll offset");
    Check(modal.errorMessage.empty()
                  && !Near(modal.draftConfig.walkSpeed, 123.0f),
          "preview settings reset discards transient drafts and errors");
}

void TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash()
{
    game::SectorTopologyMap map;
    const std::string originalHash = game::ComputeSectorLightmapSourceHash(map);

    game::SectorLightmapBakeSettings draft = map.lightmapSettings;
    draft.objectProbeSpacingWorld = 5.5f;
    draft.objectProbeLowerHeightWorld = 0.7f;
    draft.objectProbeUpperHeightWorld = 1.6f;

    const bool changed = game::ApplySectorPreviewObjectProbeSettings(map, draft);

    Check(changed, "preview settings modal apply reports changed object probe settings");
    Check(Near(map.lightmapSettings.objectProbeSpacingWorld, 5.5f),
          "preview settings modal apply writes object probe spacing");
    Check(Near(map.lightmapSettings.objectProbeLowerHeightWorld, 0.7f)
                  && Near(map.lightmapSettings.objectProbeUpperHeightWorld, 1.6f),
          "preview settings modal apply writes layered object probe heights");
    Check(game::ComputeSectorLightmapSourceHash(map) != originalHash,
          "object probe settings update changes lightmap source hash");
}

void TestPreviewSettingsModalResetsObjectProbeDefaults()
{
    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings.objectProbeSpacingWorld = 9.0f;
    modal.draftLightmapSettings.objectProbeLowerHeightWorld = 3.0f;
    modal.draftLightmapSettings.objectProbeUpperHeightWorld = 0.2f;

    game::ResetSectorPreviewSettingsModalLightingDefaults(modal);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 4.0f),
          "preview settings modal reset restores default object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeLowerHeightWorld, 0.6f)
                  && Near(modal.draftLightmapSettings.objectProbeUpperHeightWorld, 1.5f),
          "preview settings modal reset restores layered object probe height defaults");
}

void TestPreviewSettingsModalNormalizesLayeredProbeSettings()
{
    game::SectorLightmapBakeSettings settings;
    settings.objectProbeSpacingWorld = 0.0f;
    settings.objectProbeLowerHeightWorld = 20.0f;
    settings.objectProbeUpperHeightWorld = -3.0f;

    const game::SectorLightmapBakeSettings normalized =
            game::NormalizeSectorPreviewObjectProbeSettings(settings);
    Check(Near(normalized.objectProbeSpacingWorld, 0.25f),
          "preview settings clamps object probe spacing");
    Check(Near(normalized.objectProbeLowerHeightWorld, 0.0f)
                  && Near(normalized.objectProbeUpperHeightWorld, 16.0f),
          "preview settings clamps and orders layered object probe heights");
}

void TestPreviewSettingsFogTabLayout()
{
    const Rectangle modal{510.0f, 190.0f, 900.0f, 700.0f};
    const std::array<Rectangle, 6> tabs =
            game::BuildSectorPreviewSettingsTabLayout(modal, modal.y + 76.0f, 38.0f);
    for (size_t i = 0; i < tabs.size(); ++i) {
        Check(Contains(modal, tabs[i]), "preview settings tab fits inside expanded modal");
        for (size_t j = i + 1; j < tabs.size(); ++j) {
            Check(!Overlaps(tabs[i], tabs[j]), "preview settings tabs do not overlap");
        }
    }
    Check(tabs[5].x + tabs[5].width <= modal.x + modal.width - 30.0f,
          "weapon tab preserves the modal right margin");
}

void TestPreviewSettingsModalFogDraftApplyAndReset()
{
    game::SectorTopologyMap map;
    map.fogSettings.enabled = true;
    map.fogSettings.density = 0.2f;
    map.fogSettings.color = Color{10, 20, 30, 80};

    game::SectorPreviewSettingsModalState modal;
    modal.draftFogSettings = game::NormalizeSectorTopologyFogSettings(map.fogSettings);
    Check(modal.draftFogSettings.enabled
                  && Near(modal.draftFogSettings.density, 0.2f)
                  && modal.draftFogSettings.color.a == 255,
          "preview settings modal copies normalized fog settings");

    const std::string lightmapHash = game::ComputeSectorLightmapSourceHash(map);
    modal.draftFogSettings.density = 0.35f;
    modal.draftFogSettings.referenceHeightWorld = -3.0f;
    Check(game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal applies changed fog settings");
    Check(Near(map.fogSettings.density, 0.35f)
                  && Near(map.fogSettings.referenceHeightWorld, -3.0f),
          "preview settings modal writes normalized fog settings");
    Check(!game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal reports unchanged fog settings");

    modal.draftFogSettings.localVolumeQuality =
            game::SectorTopologyFogSettings::LocalVolumeQuality::High;
    Check(game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal applies a quality-only fog change");
    Check(map.fogSettings.localVolumeQuality
                  == game::SectorTopologyFogSettings::LocalVolumeQuality::High,
          "preview settings modal writes local fog quality");
    Check(!game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal reports unchanged local fog quality");
    Check(game::ComputeSectorLightmapSourceHash(map) == lightmapHash,
          "preview fog settings do not change the lightmap source hash");

    game::ResetSectorPreviewSettingsModalFogDefaults(modal);
    const game::SectorTopologyFogSettings defaults = game::DefaultSectorTopologyFogSettings();
    Check(modal.draftFogSettings.enabled == defaults.enabled
                  && Near(modal.draftFogSettings.density, defaults.density)
                  && Near(modal.draftFogSettings.heightFalloff, defaults.heightFalloff),
          "preview settings modal resets fog defaults");
}

} // namespace

int main()
{
    TestModelFilenameExtraction();
    TestTextureRowWithoutClear();
    TestTextureRowWithClear();
    TestCompactNumericRow();
    TestRightFloatNumericRow();
    TestRightIntNumericRow();
    TestRightNumericRowClamps();
    TestInspectorNumericWidthsMatchControlKinds();
    TestTextureRowHeight();
    TestStackedOptionRow();
    TestRuntimeObjectInspectorHeightCountsBillboardRows();
    TestDoorInspectorHeightCountsConditionalRows();
    TestDoorTextureSettingsModalLayoutDoesNotOverlap();
    TestPreviewSettingsModalCopiesObjectProbeSettings();
    TestPreviewSettingsScrollableContentHeightsReachLastControls();
    TestAuthoringFaceInspectorHeightIncludesAllSections();
    TestPreviewSettingsModalResetPreservesSessionView();
    TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash();
    TestPreviewSettingsModalResetsObjectProbeDefaults();
    TestPreviewSettingsModalNormalizesLayeredProbeSettings();
    TestPreviewSettingsFogTabLayout();
    TestPreviewSettingsModalFogDraftApplyAndReset();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorUiLayoutTests failure(s)\n";
        return 1;
    }
    return 0;
}
