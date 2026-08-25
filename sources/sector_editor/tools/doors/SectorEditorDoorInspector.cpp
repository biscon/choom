#include "sector_editor/tools/doors/SectorEditorDoorInspector.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorSwingDoorCatalog.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/tools/doors/SectorEditorDoorModals.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>

namespace game {
namespace {

int DoorMotionOptionIndex(SectorDoorMotionType motion)
{
    switch (motion) {
        case SectorDoorMotionType::SlideVertical: return 0;
        case SectorDoorMotionType::SlideLeft: return 1;
        case SectorDoorMotionType::SlideRight: return 2;
        case SectorDoorMotionType::Swing: return 3;
    }
    return 0;
}

SectorDoorMotionType DoorMotionFromOptionIndex(int index)
{
    switch (index) {
        case 1: return SectorDoorMotionType::SlideLeft;
        case 2: return SectorDoorMotionType::SlideRight;
        case 3: return SectorDoorMotionType::Swing;
        case 0:
        default: return SectorDoorMotionType::SlideVertical;
    }
}

std::string DoorSoundStatus(
        const SectorEditorSoundService& sounds,
        const char* label,
        const std::string& id,
        bool& invalid)
{
    if (id.empty()) {
        return TextFormat("%s sound: <none>", label);
    }
    const SectorSoundDefinition* definition = sounds.Find(id);
    if (definition == nullptr) {
        invalid = true;
        return TextFormat("%s sound missing: %s", label, id.c_str());
    }
    if (definition->type != SectorSoundType::Sound) {
        invalid = true;
        return TextFormat("%s sound is music: %s", label, id.c_str());
    }
    return TextFormat("%s sound: %s", label, id.c_str());
}

void RefreshDoorStyleOptions(
        RuntimeObjectEditingState& editingState,
        const SectorRuntimeObjectState& runtimeObjects)
{
    if (editingState.swingDoorStyleCatalogRevision
                    == runtimeObjects.swingDoorCatalogRevision
            && !editingState.swingDoorStyleLabels.empty()) {
        return;
    }
    editingState.swingDoorStyleCatalogRevision =
            runtimeObjects.swingDoorCatalogRevision;
    editingState.swingDoorStyleIds.clear();
    editingState.swingDoorStyleLabels.clear();
    editingState.swingDoorStyleIds.reserve(
            runtimeObjects.swingDoorCatalog.assets.size() + 1);
    editingState.swingDoorStyleLabels.reserve(
            runtimeObjects.swingDoorCatalog.assets.size() + 1);
    editingState.swingDoorStyleIds.emplace_back();
    editingState.swingDoorStyleLabels.emplace_back("Choose model style");
    if (!runtimeObjects.swingDoorCatalogLoaded) {
        return;
    }
    for (const SectorSwingDoorCatalogAsset& asset :
            runtimeObjects.swingDoorCatalog.assets) {
        editingState.swingDoorStyleIds.push_back(asset.id);
        editingState.swingDoorStyleLabels.push_back(asset.displayName);
    }
}

int DoorStyleOptionIndex(
        const RuntimeObjectEditingState& editingState,
        const std::string& modelAssetId)
{
    const auto found = std::find(
            editingState.swingDoorStyleIds.begin(),
            editingState.swingDoorStyleIds.end(),
            modelAssetId);
    return found == editingState.swingDoorStyleIds.end()
            ? 0
            : static_cast<int>(std::distance(
                    editingState.swingDoorStyleIds.begin(), found));
}

const SectorDoorModelRender* FindRuntimeDoorModel(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        int objectId)
{
    if (context.engineContext == nullptr) {
        return nullptr;
    }
    for (const SectorPlacedRuntimeObjectEntity& entry :
            context.runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId == objectId
                && context.engineContext->world.IsAlive(entry.entity)
                && context.engineContext->world.Has<SectorDoorModelRender>(entry.entity)) {
            return &context.engineContext->world.Get<SectorDoorModelRender>(entry.entity);
        }
    }
    return nullptr;
}

const SectorDoorModelRender* FindRuntimeDoorModel(
        const SectorEditorPlacedObjectInspectorContext& context,
        int objectId)
{
    if (context.engineContext == nullptr) {
        return nullptr;
    }
    for (const SectorPlacedRuntimeObjectEntity& entry :
            context.runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId == objectId
                && context.engineContext->world.IsAlive(entry.entity)
                && context.engineContext->world.Has<SectorDoorModelRender>(entry.entity)) {
            return &context.engineContext->world.Get<SectorDoorModelRender>(entry.entity);
        }
    }
    return nullptr;
}

std::string DoorRuntimeModelStatus(const SectorDoorModelRender* runtimeModel)
{
    if (runtimeModel == nullptr) {
        return "Preview assets: load on runtime rebuild";
    }
    if (runtimeModel->fallbackReason != SectorDoorModelFallbackReason::None) {
        return "Preview leaf: procedural fallback active";
    }
    std::string status = runtimeModel->leafFailed
            ? "Preview leaf: failed; procedural fallback"
            : runtimeModel->leafReady
                    ? "Preview leaf: ready"
                    : "Preview leaf: pending";
    if (runtimeModel->frameDeclared) {
        status += runtimeModel->frameFailed
                ? " | frame failed"
                : runtimeModel->frameReady ? " | frame ready" : " | frame pending";
    }
    return status;
}

struct DoorModelDiagnostic {
    std::string text;
    bool invalid = false;
};

DoorModelDiagnostic BuildDoorModelDiagnostic(
        const SectorTopologyMap& map,
        const SectorRuntimeObjectState& runtimeObjects,
        const SectorPlacedRuntimeObject& object,
        const SectorDoorModelRender* runtimeModel)
{
    DoorModelDiagnostic diagnostic;
    if (!runtimeObjects.swingDoorCatalogLoaded) {
        diagnostic.invalid = true;
        diagnostic.text = runtimeObjects.swingDoorCatalogWarning.empty()
                ? "Model style unavailable: swing-door catalog is not loaded"
                : runtimeObjects.swingDoorCatalogWarning;
        return diagnostic;
    }

    SectorSwingDoorCatalogAsset asset;
    if (!FindSectorSwingDoorCatalogAsset(
                runtimeObjects.swingDoorCatalog,
                object.door.modelAssetId,
                asset)) {
        diagnostic.invalid = true;
        diagnostic.text = object.door.modelAssetId.empty()
                ? "Model style is not selected; procedural fallback is active"
                : "Model style missing: " + object.door.modelAssetId
                        + "; procedural fallback is active";
        return diagnostic;
    }

    const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(map, object.door);
    if (!resolved.valid) {
        diagnostic.invalid = true;
        diagnostic.text = "Model fit unavailable: " + resolved.diagnostic;
        return diagnostic;
    }
    const SectorSwingDoorFitResult fit = ComputeSectorSwingDoorFit(
            asset,
            resolved.width,
            resolved.height,
            object.door.modelFit,
            object.door.modelScale);
    if (fit.status == SectorSwingDoorFitStatus::InvalidInput) {
        diagnostic.invalid = true;
        diagnostic.text = "Model fit is invalid; procedural fallback is active";
        return diagnostic;
    }

    const float sideDifference = fit.widthGap * 0.5f;
    std::ostringstream text;
    text << std::fixed << std::setprecision(3)
         << "Nominal leaf: " << asset.nominalWidth << " W x "
         << asset.nominalHeight << " H x " << asset.nominalThickness << " D\n"
         << "Effective scale: " << fit.effectiveScale << "\n"
         << "Actual leaf: "
         << fit.actualWidth << " W x " << fit.actualHeight << " H x "
         << fit.actualThickness << " D\n";
    if (asset.hasFrame) {
        text << "Fitted assembly: " << fit.assemblyWidth << " W x "
             << fit.assemblyHeight << " H\n";
    }
    text
         << "Target aperture: " << resolved.width << " W x " << resolved.height
         << " H\n"
         << "Portal opening: " << resolved.portalWidth << " W x "
         << resolved.portalHeight << " H\n";
    if (sideDifference >= 0.0f) {
        text << "Side gap: " << sideDifference << " each";
    } else {
        text << "Side overflow: " << -sideDifference << " each";
        diagnostic.invalid = true;
    }
    if (fit.heightGap >= 0.0f) {
        text << "\nAssembly top gap: " << fit.heightGap;
    } else {
        text << "\nAssembly top overflow: " << -fit.heightGap;
        diagnostic.invalid = true;
    }
    text << "\n" << DoorRuntimeModelStatus(runtimeModel);
    diagnostic.text = text.str();
    return diagnostic;
}

std::string BuildDoorAssetStatus(
        const SectorEditorTextureCatalogService& textureCatalog,
        const SectorEditorSoundService& sounds,
        const SectorPlacedDoor& door,
        bool& invalid)
{
    std::string status;
    if (door.visual == SectorDoorVisualType::Procedural) {
        const bool textureMissing = !door.materialId.empty()
                && !textureCatalog.HasTexture(door.materialId);
        invalid = invalid || textureMissing;
        status = door.materialId.empty()
                ? "Material: default"
                : textureMissing
                        ? TextFormat("Material missing: %s", door.materialId.c_str())
                        : TextFormat("Material: %s", door.materialId.c_str());
        status += "\n";
    }
    status += DoorSoundStatus(sounds, "Open", door.openSoundId, invalid);
    status += "\n";
    status += DoorSoundStatus(sounds, "Close", door.closeSoundId, invalid);
    return status;
}

} // namespace

float MeasureSectorEditorDoorInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object)
{
    const SectorResolvedDoorAnchor resolved =
            ResolveSectorDoorAnchor(context.topologyMap, object.door);
    const std::string anchorStatus = resolved.valid
            ? TextFormat(
                    "Anchor valid: line %d, sectors %d -> %d",
                    object.door.anchor.lineDefId,
                    object.door.anchor.frontSectorId,
                    object.door.anchor.backSectorId)
            : TextFormat(
                    "Anchor invalid: %s",
                    resolved.diagnostic.empty()
                            ? "unable to resolve portal"
                            : resolved.diagnostic.c_str());
    const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
            context.smallConfig,
            context.assets,
            context.smallFont,
            anchorStatus.c_str(),
            context.contentW,
            2);
    bool assetInvalid = false;
    const std::string assetStatus = BuildDoorAssetStatus(
            context.textureCatalog,
            context.sounds,
            object.door,
            assetInvalid);
    const float assetStatusHeight = MeasureSectorEditorWrappedTextHeight(
            context.smallConfig,
            context.assets,
            context.smallFont,
            assetStatus.c_str(),
            context.contentW,
            object.door.visual == SectorDoorVisualType::Model ? 2 : 3);
    float modelDiagnosticHeight = 0.0f;
    if (object.door.visual == SectorDoorVisualType::Model) {
        const DoorModelDiagnostic modelDiagnostic = BuildDoorModelDiagnostic(
                context.topologyMap,
                context.runtimeObjects,
                object,
                FindRuntimeDoorModel(context, object.id));
        modelDiagnosticHeight = MeasureSectorEditorWrappedTextHeight(
                context.smallConfig,
                context.assets,
                context.smallFont,
                modelDiagnostic.text.c_str(),
                context.contentW,
                8);
    }
    return SectorEditorDoorInspectorContentHeight(
            context.rowH,
            context.gap,
            anchorStatusHeight,
            assetStatusHeight,
            modelDiagnosticHeight,
            object.door.visual == SectorDoorVisualType::Model,
            object.door.motion == SectorDoorMotionType::Swing);
}

void DrawSectorEditorDoorInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    const engine::FontHandle smallFont = context.smallFont;
    RuntimeObjectEditingUiState& uiState = context.uiState;
    SectorEditorRuntimeObjectEditingService& editing = context.editing;
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig =
            SectorEditorSmallFontConfig(config, assets, smallFont);

    RefreshDoorStyleOptions(context.editingState, context.runtimeObjects);
    const SectorPlacedRuntimeObject* selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr || selectedObject->kind != "door") {
        return;
    }

    const auto selectedDoor = [&]() -> const SectorPlacedRuntimeObject* {
        const SectorPlacedRuntimeObject* selected = editing.SelectedObject();
        return selected != nullptr && selected->kind == "door" ? selected : nullptr;
    };
    const auto drawStackedOption = [&] (
            const char* id,
            const char* label,
            const std::vector<std::string>& options,
            int& selectedIndex) {
        const SectorEditorInspectorStackedOptionRowLayout layout =
                BuildSectorEditorInspectorStackedOptionRowLayout(
                        y, contentW, rowH, gap);
        engine::Text(
                ui, config, assets, layout.labelRect, font, label,
                engine::UITextJustify::Left, config.mutedTextColor);
        const bool changed = engine::Option(
                ui, config, input, assets, id, layout.fieldRect, font,
                options, selectedIndex);
        y += layout.height + gap;
        return changed;
    };
    const auto drawDoorFloat = [&] (
            const char* id,
            const char* label,
            float value,
            engine::UIFloatInputState& inputState,
            float minValue,
            float maxValue,
            int decimals,
            const std::function<bool(SectorPlacedDoor&, float)>& applyValue) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(
                        y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.finite && result.value != value) {
            editing.MutateSelected(
                    "Updated door properties",
                    [applyValue, value = result.value](
                            SectorPlacedRuntimeObject& object) {
                        return object.kind == "door"
                                && applyValue(object.door, value);
                    });
        }
        y += rowH + gap;
    };

    if (uiState.doorScriptFieldsObjectId != selectedObject->id) {
        std::snprintf(
                uiState.doorInstanceIdBuffer,
                sizeof(uiState.doorInstanceIdBuffer),
                "%s",
                selectedObject->door.instanceId.c_str());
        std::snprintf(
                uiState.doorUseTitleBuffer,
                sizeof(uiState.doorUseTitleBuffer),
                "%s",
                selectedObject->door.useTitle.c_str());
        std::snprintf(
                uiState.doorCanOpenScriptBuffer,
                sizeof(uiState.doorCanOpenScriptBuffer),
                "%s",
                selectedObject->door.canOpenScript.c_str());
        std::snprintf(
                uiState.doorCanCloseScriptBuffer,
                sizeof(uiState.doorCanCloseScriptBuffer),
                "%s",
                selectedObject->door.canCloseScript.c_str());
        uiState.doorScriptFieldsObjectId = selectedObject->id;
        uiState.doorInstanceIdError.clear();
    }
    const auto drawDoorText = [&] (
            const char* id,
            const char* label,
            char* buffer,
            size_t capacity,
            const std::function<void(const std::string&)>& submit) {
        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, y, 118.0f, rowH}, font,
                label, engine::UITextJustify::Left, config.mutedTextColor);
        const engine::UITextInputResult result = engine::TextInput(
                ui, config, input, assets,
                id,
                Rectangle{122.0f, y, std::max(0.0f, contentW - 122.0f), rowH},
                font,
                buffer,
                capacity,
                0,
                capacity - 1,
                engine::UITextJustify::Left);
        if (result.submitted) submit(std::string{buffer});
        y += rowH + gap;
    };
    drawDoorText(
            "sector_editor_door_instance_id",
            "Instance ID",
            uiState.doorInstanceIdBuffer,
            sizeof(uiState.doorInstanceIdBuffer),
            [&](const std::string& value) {
                editing.SetSelectedDoorInstanceId(
                        value, uiState.doorInstanceIdError);
            });
    if (!uiState.doorInstanceIdError.empty()) {
        engine::Text(
                ui, smallConfig, assets,
                Rectangle{0.0f, y, contentW, 32.0f}, smallFont,
                uiState.doorInstanceIdError.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor,
                true);
        y += 36.0f;
    }
    drawDoorText(
            "sector_editor_door_use_title",
            "Use Title",
            uiState.doorUseTitleBuffer,
            sizeof(uiState.doorUseTitleBuffer),
            [&](const std::string& value) {
                if (!IsValidSectorUseTitle(value)) return;
                editing.MutateSelected(
                        "Updated door use title",
                        [value](auto& object) {
                            if (object.kind != "door"
                                    || object.door.useTitle == value) return false;
                            object.door.useTitle = value;
                            return true;
                        });
            });
    const auto submitDoorCallback = [&](bool opening, const std::string& value) {
        if (!value.empty() && !IsValidSectorTriggerScriptName(value)) return;
        editing.MutateSelected(
                opening ? "Updated door open permission" : "Updated door close permission",
                [opening, value](auto& object) {
                    if (object.kind != "door") return false;
                    std::string& callback = opening
                            ? object.door.canOpenScript : object.door.canCloseScript;
                    if (callback == value) return false;
                    callback = value;
                    return true;
                });
    };
    drawDoorText(
            "sector_editor_door_can_open_script",
            "Can Open",
            uiState.doorCanOpenScriptBuffer,
            sizeof(uiState.doorCanOpenScriptBuffer),
            [&](const std::string& value) { submitDoorCallback(true, value); });
    drawDoorText(
            "sector_editor_door_can_close_script",
            "Can Close",
            uiState.doorCanCloseScriptBuffer,
            sizeof(uiState.doorCanCloseScriptBuffer),
            [&](const std::string& value) { submitDoorCallback(false, value); });

    const SectorResolvedDoorAnchor resolved =
            ResolveSectorDoorAnchor(context.topologyMap, selectedObject->door);
    const std::string anchorStatus = resolved.valid
            ? TextFormat(
                    "Anchor valid: line %d, sectors %d -> %d",
                    selectedObject->door.anchor.lineDefId,
                    selectedObject->door.anchor.frontSectorId,
                    selectedObject->door.anchor.backSectorId)
            : TextFormat(
                    "Anchor invalid: %s",
                    resolved.diagnostic.empty()
                            ? "unable to resolve portal"
                            : resolved.diagnostic.c_str());
    const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig,
            assets,
            smallFont,
            anchorStatus.c_str(),
            contentW,
            2);
    engine::Text(
            ui,
            smallConfig,
            assets,
            Rectangle{0.0f, y, contentW, anchorStatusHeight},
            smallFont,
            anchorStatus.c_str(),
            engine::UITextJustify::Left,
            resolved.valid ? config.mutedTextColor : config.invalidColor,
            true);
    y += anchorStatusHeight + gap;

    drawDoorFloat(
            "sector_editor_door_width",
            "Target Width",
            selectedObject->door.width,
            uiState.widthInput,
            0.0f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                value = std::max(0.0f, value);
                if (door.width == value) return false;
                door.width = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    drawDoorFloat(
            "sector_editor_door_height",
            "Target Height",
            selectedObject->door.height,
            uiState.heightInput,
            0.0f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                value = std::max(0.0f, value);
                if (door.height == value) return false;
                door.height = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    drawDoorFloat(
            "sector_editor_door_normal_offset",
            "Offset",
            selectedObject->door.normalOffset,
            uiState.normalOffsetInput,
            -100000.0f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                if (door.normalOffset == value) return false;
                door.normalOffset = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    drawDoorFloat(
            "sector_editor_door_height_offset",
            "Offset Height",
            selectedObject->door.heightOffsetWorld,
            uiState.heightOffsetInput,
            -100000.0f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                if (door.heightOffsetWorld == value) return false;
                door.heightOffsetWorld = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;

    static const std::vector<std::string> visualOptions{"Procedural", "Model"};
    int visualIndex = selectedObject->door.visual == SectorDoorVisualType::Model ? 1 : 0;
    const int previousVisualIndex = visualIndex;
    if (drawStackedOption(
                "sector_editor_door_visual",
                "Visual",
                visualOptions,
                visualIndex)
            && visualIndex != previousVisualIndex) {
        if (visualIndex == 1) {
            if (!context.runtimeObjects.swingDoorCatalogLoaded
                    || context.runtimeObjects.swingDoorCatalog.assets.empty()) {
                context.statusText = "Model door unavailable: swing-door catalog has no styles";
            } else {
                editing.MutateSelected(
                        "Selected model swing door",
                        [&catalog = context.runtimeObjects.swingDoorCatalog](
                                SectorPlacedRuntimeObject& object) {
                            if (object.kind != "door") return false;
                            return InitializeSectorEditorModelSwingDoor(
                                    object.door, catalog);
                        });
            }
        } else {
            editing.MutateSelected(
                    "Selected procedural door",
                    [](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door"
                                || object.door.visual
                                        == SectorDoorVisualType::Procedural) {
                            return false;
                        }
                        object.door.visual = SectorDoorVisualType::Procedural;
                        return true;
                    });
        }
    }
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;

    const bool modelVisual =
            !SectorEditorDoorInspectorShowsSlideMotionOptions(selectedObject->door);
    if (modelVisual) {
        int styleIndex = DoorStyleOptionIndex(
                context.editingState, selectedObject->door.modelAssetId);
        const int previousStyleIndex = styleIndex;
        if (drawStackedOption(
                    "sector_editor_door_model_style",
                    "Model Style",
                    context.editingState.swingDoorStyleLabels,
                    styleIndex)
                && styleIndex != previousStyleIndex
                && styleIndex > 0
                && styleIndex < static_cast<int>(
                        context.editingState.swingDoorStyleIds.size())) {
            const std::string styleId =
                    context.editingState.swingDoorStyleIds[
                            static_cast<size_t>(styleIndex)];
            editing.MutateSelected(
                    "Updated door model style",
                    [styleId,
                     &catalog = context.runtimeObjects.swingDoorCatalog](
                            SectorPlacedRuntimeObject& object) {
                        return object.kind == "door"
                                && SelectSectorEditorSwingDoorStyle(
                                        object.door, catalog, styleId);
                    });
        }
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        const DoorModelDiagnostic modelDiagnostic = BuildDoorModelDiagnostic(
                context.topologyMap,
                context.runtimeObjects,
                *selectedObject,
                FindRuntimeDoorModel(context, selectedObject->id));
        const float modelDiagnosticHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                modelDiagnostic.text.c_str(),
                contentW,
                8);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, modelDiagnosticHeight},
                smallFont,
                modelDiagnostic.text.c_str(),
                engine::UITextJustify::Left,
                modelDiagnostic.invalid ? config.invalidColor : config.mutedTextColor,
                true);
        y += modelDiagnosticHeight + gap;

        static const std::vector<std::string> fitOptions{
                "Manual", "Fit Width", "Fit Inside"};
        int fitIndex = static_cast<int>(selectedObject->door.modelFit);
        const int previousFitIndex = fitIndex;
        if (drawStackedOption(
                    "sector_editor_door_model_fit",
                    "Fit",
                    fitOptions,
                    fitIndex)
                && fitIndex != previousFitIndex
                && fitIndex >= 0 && fitIndex < 3) {
            const SectorDoorModelFit fit =
                    static_cast<SectorDoorModelFit>(fitIndex);
            editing.MutateSelected(
                    "Updated door model fit",
                    [fit](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.modelFit == fit) {
                            return false;
                        }
                        object.door.modelFit = fit;
                        return true;
                    });
        }
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        drawDoorFloat(
                "sector_editor_door_model_scale",
                "Scale",
                selectedObject->door.modelScale,
                uiState.modelScaleInput,
                0.001f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    value = std::max(0.001f, value);
                    if (door.modelScale == value) return false;
                    door.modelScale = value;
                    return true;
                });
        engine::Text(
                ui, config, assets, Rectangle{0.0f, y, contentW, rowH}, font,
                "Motion: Swing", engine::UITextJustify::Left,
                config.mutedTextColor);
        y += rowH + gap;
    } else {
        drawDoorFloat(
                "sector_editor_door_thickness",
                "Thickness",
                selectedObject->door.thickness,
                uiState.thicknessInput,
                0.001f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    value = std::max(0.001f, value);
                    if (door.thickness == value) return false;
                    door.thickness = value;
                    return true;
                });
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        static const std::vector<std::string> motionOptions{
                "Slide Vertical", "Slide Left", "Slide Right", "Swing"};
        int motionIndex = DoorMotionOptionIndex(selectedObject->door.motion);
        const int previousMotionIndex = motionIndex;
        if (drawStackedOption(
                    "sector_editor_door_motion",
                    "Motion",
                    motionOptions,
                    motionIndex)
                && motionIndex != previousMotionIndex) {
            const SectorDoorMotionType motion =
                    DoorMotionFromOptionIndex(motionIndex);
            editing.MutateSelected(
                    "Updated door motion",
                    [motion](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.motion == motion) {
                            return false;
                        }
                        object.door.motion = motion;
                        return true;
                    });
        }
    }
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;

    if (selectedObject->door.motion == SectorDoorMotionType::Swing) {
        static const std::vector<std::string> hingeOptions{"Start", "End"};
        int hingeIndex = selectedObject->door.hinge == SectorDoorHinge::End ? 1 : 0;
        const int previousHingeIndex = hingeIndex;
        if (drawStackedOption(
                    "sector_editor_door_hinge",
                    "Hinge",
                    hingeOptions,
                    hingeIndex)
                && hingeIndex != previousHingeIndex) {
            const SectorDoorHinge hinge =
                    hingeIndex == 1 ? SectorDoorHinge::End : SectorDoorHinge::Start;
            editing.MutateSelected(
                    "Updated door hinge",
                    [hinge](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.hinge == hinge) {
                            return false;
                        }
                        object.door.hinge = hinge;
                        return true;
                    });
        }
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        static const std::vector<std::string> swingSideOptions{"Front", "Back"};
        int sideIndex = selectedObject->door.swingSide == SectorDoorSwingSide::Back ? 1 : 0;
        const int previousSideIndex = sideIndex;
        if (drawStackedOption(
                    "sector_editor_door_swing_side",
                    "Swing Into",
                    swingSideOptions,
                    sideIndex)
                && sideIndex != previousSideIndex) {
            const SectorDoorSwingSide side = sideIndex == 1
                    ? SectorDoorSwingSide::Back
                    : SectorDoorSwingSide::Front;
            editing.MutateSelected(
                    "Updated door swing side",
                    [side](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.swingSide == side) {
                            return false;
                        }
                        object.door.swingSide = side;
                        return true;
                    });
        }
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        drawDoorFloat(
                "sector_editor_door_open_angle",
                "Open Angle",
                selectedObject->door.openAngleDegrees,
                uiState.openAngleDegreesInput,
                0.001f,
                170.0f,
                2,
                [](SectorPlacedDoor& door, float value) {
                    value = Clamp(value, 0.001f, 170.0f);
                    if (door.openAngleDegrees == value) return false;
                    door.openAngleDegrees = value;
                    return true;
                });
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        drawDoorFloat(
                "sector_editor_door_angular_speed",
                "Angular Speed",
                selectedObject->door.angularSpeedDegrees,
                uiState.angularSpeedDegreesInput,
                0.0f,
                100000.0f,
                2,
                [](SectorPlacedDoor& door, float value) {
                    value = std::max(0.0f, value);
                    if (door.angularSpeedDegrees == value) return false;
                    door.angularSpeedDegrees = value;
                    return true;
                });
    } else {
        drawDoorFloat(
                "sector_editor_door_open_distance",
                "Open Dist",
                selectedObject->door.openDistance,
                uiState.openDistanceInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    value = std::max(0.0f, value);
                    if (door.openDistance == value) return false;
                    door.openDistance = value;
                    return true;
                });
        selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
        drawDoorFloat(
                "sector_editor_door_speed",
                "Speed",
                selectedObject->door.speed,
                uiState.speedInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    value = std::max(0.0f, value);
                    if (door.speed == value) return false;
                    door.speed = value;
                    return true;
                });
    }
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;

    drawDoorFloat(
            "sector_editor_door_initial_open_fraction",
            "Initial",
            selectedObject->door.initialOpenFraction,
            uiState.initialOpenFractionInput,
            0.0f,
            1.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                value = Clamp(value, 0.0f, 1.0f);
                if (door.initialOpenFraction == value) return false;
                door.initialOpenFraction = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    bool autoOpen = selectedObject->door.autoOpen;
    if (engine::Checkbox(
                ui,
                config,
                input,
                assets,
                "sector_editor_door_auto_open",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Auto Open",
                autoOpen)) {
        editing.MutateSelected(
                "Updated door auto-open",
                [autoOpen](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "door" || object.door.autoOpen == autoOpen) {
                        return false;
                    }
                    object.door.autoOpen = autoOpen;
                    return true;
                });
    }
    y += rowH + gap;
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    drawDoorFloat(
            "sector_editor_door_auto_open_distance",
            "Auto Dist",
            selectedObject->door.autoOpenDistance,
            uiState.autoOpenDistanceInput,
            0.001f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                value = std::max(0.001f, value);
                if (door.autoOpenDistance == value) return false;
                door.autoOpenDistance = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;
    drawDoorFloat(
            "sector_editor_door_interaction_distance",
            "Use Dist",
            selectedObject->door.interactionDistance,
            uiState.interactionDistanceInput,
            0.001f,
            100000.0f,
            3,
            [](SectorPlacedDoor& door, float value) {
                value = std::max(0.001f, value);
                if (door.interactionDistance == value) return false;
                door.interactionDistance = value;
                return true;
            });
    selectedObject = selectedDoor(); if (selectedObject == nullptr) return;

    bool runtimeTargetOpen = false;
    const bool runtimeDoorAvailable =
            editing.SelectedDoorRuntimeTargetOpen(runtimeTargetOpen);
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_door_debug_target",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                !runtimeDoorAvailable
                        ? "Runtime Target Unavailable"
                        : runtimeTargetOpen
                                ? "Debug Target Close"
                                : "Debug Target Open")
            && runtimeDoorAvailable) {
        editing.SetSelectedDoorRuntimeTargetOpen(!runtimeTargetOpen);
    }
    y += rowH + gap;

    bool assetInvalid = false;
    const std::string assetStatus = BuildDoorAssetStatus(
            context.textureCatalog,
            context.sounds,
            selectedObject->door,
            assetInvalid);
    const float assetStatusHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig,
            assets,
            smallFont,
            assetStatus.c_str(),
            contentW,
            modelVisual ? 2 : 3);
    engine::Text(
            ui,
            smallConfig,
            assets,
            Rectangle{0.0f, y, contentW, assetStatusHeight},
            smallFont,
            assetStatus.c_str(),
            engine::UITextJustify::Left,
            assetInvalid ? config.invalidColor : config.mutedTextColor,
            true);
    y += assetStatusHeight + gap;

    if (SectorEditorDoorInspectorShowsProceduralMaterialControls(
                selectedObject->door)) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_pick_texture",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Pick Material")) {
            if (!OpenRuntimeDoorTexturePicker(
                        context.state,
                        context.topologyMap,
                        context.authoringGraph,
                        context.textureCatalog,
                        selectedObject->id)) {
                context.statusText = "No door texture target";
            }
        }
        y += rowH + gap;
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_door_pick_open_sound",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Pick open sound")) {
        if (!context.sounds.OpenDoorPicker(
                    selectedObject->id, SectorEditorDoorSoundTarget::Open)) {
            context.statusText = "No door open sound target";
        }
    }
    y += rowH + gap;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_door_pick_close_sound",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Pick close sound")) {
        if (!context.sounds.OpenDoorPicker(
                    selectedObject->id, SectorEditorDoorSoundTarget::Close)) {
            context.statusText = "No door close sound target";
        }
    }
    y += rowH + gap;
    if (SectorEditorDoorInspectorShowsProceduralMaterialControls(
                selectedObject->door)) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_uv_settings",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "UV Settings...")) {
            OpenSectorEditorDoorTextureSettingsModal(
                    context.state.doorTextureSettingsModal,
                    selectedObject,
                    context.statusText);
        }
        y += rowH + gap;
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_delete_runtime_object",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
