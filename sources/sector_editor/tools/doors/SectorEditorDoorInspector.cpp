#include "sector_editor/tools/doors/SectorEditorDoorInspector.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
#include "sector_editor/tools/doors/SectorEditorDoorModals.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace game {
namespace {

int DoorMotionOptionIndex(SectorDoorMotionType motion) {
  switch (motion) {
  case SectorDoorMotionType::SlideVertical:
    return 0;
  case SectorDoorMotionType::SlideLeft:
    return 1;
  case SectorDoorMotionType::SlideRight:
    return 2;
  case SectorDoorMotionType::Swing:
    // Slice 5 adds the dedicated swing authoring controls. Until then the
    // runtime diagnostic/fallback remains authoritative for loaded records.
    return 0;
  }
  return 0;
}

SectorDoorMotionType DoorMotionFromOptionIndex(int index) {
  switch (index) {
  case 1:
    return SectorDoorMotionType::SlideLeft;
  case 2:
    return SectorDoorMotionType::SlideRight;
  case 0:
  default:
    return SectorDoorMotionType::SlideVertical;
  }
}

std::string DoorSoundStatus(
    const SectorEditorSoundService &sounds, const char *label,
    const std::string &id, bool &invalid) {
  if (id.empty()) {
    return TextFormat("%s sound: <none>", label);
  }
  const SectorSoundDefinition *definition = sounds.Find(id);
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

} // namespace

float MeasureSectorEditorDoorInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object) {
  const SectorResolvedDoorAnchor resolved =
      ResolveSectorDoorAnchor(context.topologyMap, object.door);
  const std::string anchorStatus =
      resolved.valid
          ? TextFormat("Anchor valid: line %d, sectors %d -> %d",
                       object.door.anchor.lineDefId,
                       object.door.anchor.frontSectorId,
                       object.door.anchor.backSectorId)
          : TextFormat("Anchor invalid: %s", resolved.diagnostic.empty()
                                                 ? "unable to resolve portal"
                                                 : resolved.diagnostic.c_str());
  const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
      context.smallConfig, context.assets, context.smallFont,
      anchorStatus.c_str(), context.contentW, 2);
  const bool textureMissing =
      !object.door.textureId.empty() &&
      !context.textureCatalog.HasTexture(object.door.textureId);
  const std::string textureStatus =
      object.door.textureId.empty() ? "Texture: default material"
      : textureMissing
          ? TextFormat("Texture missing: %s", object.door.textureId.c_str())
          : TextFormat("Texture: %s", object.door.textureId.c_str());
  bool soundInvalid = false;
  const std::string assetStatus = textureStatus + "\n"
      + DoorSoundStatus(context.sounds, "Open", object.door.openSoundId, soundInvalid)
      + "\n"
      + DoorSoundStatus(context.sounds, "Close", object.door.closeSoundId, soundInvalid);
  const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
      context.smallConfig, context.assets, context.smallFont,
      assetStatus.c_str(), context.contentW, 3);
  return SectorEditorDoorInspectorContentHeight(
      context.rowH, context.gap, anchorStatusHeight, textureStatusHeight);
}

void DrawSectorEditorDoorInspector(
    SectorEditorPlacedObjectInspectorContext &context, float &y) {
  engine::UIContext &ui = context.ui;
  const engine::UIConfig &config = context.config;
  engine::Input &input = context.input;
  engine::AssetManager &assets = context.assets;
  const engine::FontHandle font = context.font;
  const engine::FontHandle smallFont = context.smallFont;
  RuntimeObjectEditingUiState &uiState = context.uiState;
  SectorEditorRuntimeObjectEditingService &editing = context.editing;
  const float contentW = context.contentW;
  const float rowH = context.rowH;
  const float gap = context.gap;
  const engine::UIConfig smallConfig =
      SectorEditorSmallFontConfig(config, assets, smallFont);

  const SectorPlacedRuntimeObject *selectedObject =
      editing.SelectedObject();
  if (selectedObject == nullptr || selectedObject->kind != "door") {
    return;
  }

  const SectorResolvedDoorAnchor resolved =
      ResolveSectorDoorAnchor(context.topologyMap, selectedObject->door);
  const std::string anchorStatus =
      resolved.valid
          ? TextFormat("Anchor valid: line %d, sectors %d -> %d",
                       selectedObject->door.anchor.lineDefId,
                       selectedObject->door.anchor.frontSectorId,
                       selectedObject->door.anchor.backSectorId)
          : TextFormat("Anchor invalid: %s", resolved.diagnostic.empty()
                                                 ? "unable to resolve portal"
                                                 : resolved.diagnostic.c_str());
  const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
      smallConfig, assets, smallFont, anchorStatus.c_str(), contentW, 2);
  engine::Text(
      ui, smallConfig, assets, Rectangle{0.0f, y, contentW, anchorStatusHeight},
      smallFont, anchorStatus.c_str(), engine::UITextJustify::Left,
      resolved.valid ? config.mutedTextColor : config.invalidColor, true);
  y += anchorStatusHeight + gap;

  auto drawDoorFloat =
      [&](const char *id, const char *label, float value,
          engine::UIFloatInputState &inputState, float minValue, float maxValue,
          int decimals,
          const std::function<bool(SectorPlacedDoor &, float)> &applyValue) {
        const float labelW = 104.0f;
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
            ui, config, input, assets, font, id, label,
            Rectangle{0.0f, y, labelW, rowH},
            Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap),
                      rowH},
            engine::UITextJustify::Left, value, inputState, minValue, maxValue,
            decimals);
        if (result.changed && result.finite && result.value != value) {
          editing.MutateSelected(
              "Updated door properties",
              [applyValue,
               value = result.value](SectorPlacedRuntimeObject &object) {
                if (object.kind != "door") {
                  return false;
                }
                return applyValue(object.door, value);
              });
        }
        y += rowH + gap;
      };

  drawDoorFloat("sector_editor_door_width", "Width", selectedObject->door.width,
                uiState.widthInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float width = std::max(0.0f, value);
                  if (door.width == width) {
                    return false;
                  }
                  door.width = width;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_height", "Height",
                selectedObject->door.height, uiState.heightInput,
                0.0f, 100000.0f, 3, [](SectorPlacedDoor &door, float value) {
                  const float height = std::max(0.0f, value);
                  if (door.height == height) {
                    return false;
                  }
                  door.height = height;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_thickness", "Thickness",
                selectedObject->door.thickness,
                uiState.thicknessInput, 0.001f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float thickness = std::max(0.001f, value);
                  if (door.thickness == thickness) {
                    return false;
                  }
                  door.thickness = thickness;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_normal_offset", "Offset",
                selectedObject->door.normalOffset,
                uiState.normalOffsetInput, -100000.0f, 100000.0f,
                3, [](SectorPlacedDoor &door, float value) {
                  if (door.normalOffset == value) {
                    return false;
                  }
                  door.normalOffset = value;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  const char *motionOptions[] = {"Slide Vertical", "Slide Left", "Slide Right"};
  int motionIndex = DoorMotionOptionIndex(selectedObject->door.motion);
  const int previousMotionIndex = motionIndex;
  const SectorEditorInspectorStackedOptionRowLayout motionLayout =
      BuildSectorEditorInspectorStackedOptionRowLayout(y, contentW, rowH, gap);
  engine::Text(ui, config, assets, motionLayout.labelRect, font, "Motion",
               engine::UITextJustify::Left, config.mutedTextColor);
  if (engine::Option(ui, config, input, assets, "sector_editor_door_motion",
                     motionLayout.fieldRect, font, motionOptions, 3,
                     motionIndex) &&
      motionIndex != previousMotionIndex) {
    const SectorDoorMotionType motion = DoorMotionFromOptionIndex(motionIndex);
    editing.MutateSelected(
        "Updated door motion", [motion](SectorPlacedRuntimeObject &object) {
          if (object.kind != "door" || object.door.motion == motion) {
            return false;
          }
          object.door.motion = motion;
          return true;
        });
  }
  y += motionLayout.height + gap;
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_open_distance", "Open Dist",
                selectedObject->door.openDistance,
                uiState.openDistanceInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.0f, value);
                  if (door.openDistance == distance) {
                    return false;
                  }
                  door.openDistance = distance;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_speed", "Speed", selectedObject->door.speed,
                uiState.speedInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float speed = std::max(0.0f, value);
                  if (door.speed == speed) {
                    return false;
                  }
                  door.speed = speed;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_initial_open_fraction", "Initial",
                selectedObject->door.initialOpenFraction,
                uiState.initialOpenFractionInput, 0.0f, 1.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float fraction = Clamp(value, 0.0f, 1.0f);
                  if (door.initialOpenFraction == fraction) {
                    return false;
                  }
                  door.initialOpenFraction = fraction;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  bool autoOpen = selectedObject->door.autoOpen;
  if (engine::Checkbox(
          ui, config, input, assets, "sector_editor_door_auto_open",
          Rectangle{0.0f, y, contentW, rowH}, font, "Auto Open", autoOpen)) {
    editing.MutateSelected(
        "Updated door auto-open",
        [autoOpen](SectorPlacedRuntimeObject &object) {
          if (object.kind != "door" || object.door.autoOpen == autoOpen) {
            return false;
          }
          object.door.autoOpen = autoOpen;
          return true;
        });
  }
  y += rowH + gap;
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_auto_open_distance", "Auto Dist",
                selectedObject->door.autoOpenDistance,
                uiState.autoOpenDistanceInput, 0.001f, 100000.0f,
                3, [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.001f, value);
                  if (door.autoOpenDistance == distance) {
                    return false;
                  }
                  door.autoOpenDistance = distance;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_interaction_distance", "Use Dist",
                selectedObject->door.interactionDistance,
                uiState.interactionDistanceInput, 0.001f,
                100000.0f, 3, [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.001f, value);
                  if (door.interactionDistance == distance) {
                    return false;
                  }
                  door.interactionDistance = distance;
                  return true;
                });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  bool runtimeTargetOpen = false;
  const bool runtimeDoorAvailable =
      editing.SelectedDoorRuntimeTargetOpen(runtimeTargetOpen);
  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_debug_target",
          Rectangle{0.0f, y, contentW, rowH}, font,
          !runtimeDoorAvailable ? "Runtime Target Unavailable"
                                : (runtimeTargetOpen ? "Debug Target Close"
                                                     : "Debug Target Open"))) {
    if (runtimeDoorAvailable) {
      editing.SetSelectedDoorRuntimeTargetOpen(!runtimeTargetOpen);
    }
  }
  y += rowH + gap;

  const bool textureMissing =
      !selectedObject->door.textureId.empty() &&
      !context.textureCatalog.HasTexture(selectedObject->door.textureId);
  const std::string textureStatus =
      selectedObject->door.textureId.empty() ? "Texture: default material"
      : textureMissing
          ? TextFormat("Texture missing: %s",
                       selectedObject->door.textureId.c_str())
          : TextFormat("Texture: %s", selectedObject->door.textureId.c_str());
  bool soundInvalid = false;
  const std::string assetStatus = textureStatus + "\n"
      + DoorSoundStatus(context.sounds, "Open", selectedObject->door.openSoundId, soundInvalid)
      + "\n"
      + DoorSoundStatus(context.sounds, "Close", selectedObject->door.closeSoundId, soundInvalid);
  const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
      smallConfig, assets, smallFont, assetStatus.c_str(), contentW, 3);
  engine::Text(ui, smallConfig, assets,
               Rectangle{0.0f, y, contentW, textureStatusHeight}, smallFont,
               assetStatus.c_str(), engine::UITextJustify::Left,
               textureMissing || soundInvalid ? config.invalidColor : config.mutedTextColor,
               true);
  y += textureStatusHeight + gap;

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_pick_texture",
          Rectangle{0.0f, y, contentW, rowH}, font, "Pick Texture")) {
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

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_pick_open_sound",
          Rectangle{0.0f, y, contentW, rowH}, font, "Pick open sound")) {
    if (!context.sounds.OpenDoorPicker(
            selectedObject->id, SectorEditorDoorSoundTarget::Open)) {
      context.statusText = "No door open sound target";
    }
  }
  y += rowH + gap;

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_pick_close_sound",
          Rectangle{0.0f, y, contentW, rowH}, font, "Pick close sound")) {
    if (!context.sounds.OpenDoorPicker(
            selectedObject->id, SectorEditorDoorSoundTarget::Close)) {
      context.statusText = "No door close sound target";
    }
  }
  y += rowH + gap;

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_uv_settings",
          Rectangle{0.0f, y, contentW, rowH}, font, "UV Settings...")) {
    OpenSectorEditorDoorTextureSettingsModal(
        context.state.doorTextureSettingsModal,
        selectedObject,
        context.statusText);
  }
  y += rowH + gap;

  if (engine::Button(ui, config, input, assets,
                     "sector_editor_delete_runtime_object",
                     Rectangle{0.0f, y, contentW, rowH}, font, "Delete")) {
    context.deleteRequested = true;
    return;
  }
  return;
}

} // namespace game
