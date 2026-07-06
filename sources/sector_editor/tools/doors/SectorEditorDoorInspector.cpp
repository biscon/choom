#include "sector_editor/tools/doors/SectorEditorDoorInspector.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"

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

} // namespace

float MeasureSectorEditorDoorInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object) {
  const SectorResolvedDoorAnchor resolved =
      ResolveSectorDoorAnchor(context.state.topologyMap, object.door);
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
  const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
      context.smallConfig, context.assets, context.smallFont,
      textureStatus.c_str(), context.contentW, 1);
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
  SectorEditorState &state = context.state;
  SectorEditorUiState &uiState = context.uiState;
  const SectorEditorPlacedObjectInspectorCallbacks &callbacks =
      context.callbacks;
  const float contentW = context.contentW;
  const float rowH = context.rowH;
  const float gap = context.gap;
  const engine::UIConfig smallConfig =
      SectorEditorSmallFontConfig(config, assets, smallFont);

  const SectorPlacedRuntimeObject *selectedObject =
      callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr || selectedObject->kind != "door") {
    return;
  }

  const SectorResolvedDoorAnchor resolved =
      ResolveSectorDoorAnchor(state.topologyMap, selectedObject->door);
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
          callbacks.mutateSelectedRuntimeObject(
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
                uiState.runtimeObjectWidthInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float width = std::max(0.0f, value);
                  if (door.width == width) {
                    return false;
                  }
                  door.width = width;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_height", "Height",
                selectedObject->door.height, uiState.runtimeObjectHeightInput,
                0.0f, 100000.0f, 3, [](SectorPlacedDoor &door, float value) {
                  const float height = std::max(0.0f, value);
                  if (door.height == height) {
                    return false;
                  }
                  door.height = height;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_thickness", "Thickness",
                selectedObject->door.thickness,
                uiState.runtimeObjectThicknessInput, 0.001f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float thickness = std::max(0.001f, value);
                  if (door.thickness == thickness) {
                    return false;
                  }
                  door.thickness = thickness;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_normal_offset", "Offset",
                selectedObject->door.normalOffset,
                uiState.runtimeObjectNormalOffsetInput, -100000.0f, 100000.0f,
                3, [](SectorPlacedDoor &door, float value) {
                  if (door.normalOffset == value) {
                    return false;
                  }
                  door.normalOffset = value;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
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
    callbacks.mutateSelectedRuntimeObject(
        "Updated door motion", [motion](SectorPlacedRuntimeObject &object) {
          if (object.kind != "door" || object.door.motion == motion) {
            return false;
          }
          object.door.motion = motion;
          return true;
        });
  }
  y += motionLayout.height + gap;
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_open_distance", "Open Dist",
                selectedObject->door.openDistance,
                uiState.runtimeObjectOpenDistanceInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.0f, value);
                  if (door.openDistance == distance) {
                    return false;
                  }
                  door.openDistance = distance;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_speed", "Speed", selectedObject->door.speed,
                uiState.runtimeObjectSpeedInput, 0.0f, 100000.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float speed = std::max(0.0f, value);
                  if (door.speed == speed) {
                    return false;
                  }
                  door.speed = speed;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_initial_open_fraction", "Initial",
                selectedObject->door.initialOpenFraction,
                uiState.runtimeObjectInitialOpenFractionInput, 0.0f, 1.0f, 3,
                [](SectorPlacedDoor &door, float value) {
                  const float fraction = Clamp(value, 0.0f, 1.0f);
                  if (door.initialOpenFraction == fraction) {
                    return false;
                  }
                  door.initialOpenFraction = fraction;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  bool autoOpen = selectedObject->door.autoOpen;
  if (engine::Checkbox(
          ui, config, input, assets, "sector_editor_door_auto_open",
          Rectangle{0.0f, y, contentW, rowH}, font, "Auto Open", autoOpen)) {
    callbacks.mutateSelectedRuntimeObject(
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
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_auto_open_distance", "Auto Dist",
                selectedObject->door.autoOpenDistance,
                uiState.runtimeObjectAutoOpenDistanceInput, 0.001f, 100000.0f,
                3, [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.001f, value);
                  if (door.autoOpenDistance == distance) {
                    return false;
                  }
                  door.autoOpenDistance = distance;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  drawDoorFloat("sector_editor_door_interaction_distance", "Use Dist",
                selectedObject->door.interactionDistance,
                uiState.runtimeObjectInteractionDistanceInput, 0.001f,
                100000.0f, 3, [](SectorPlacedDoor &door, float value) {
                  const float distance = std::max(0.001f, value);
                  if (door.interactionDistance == distance) {
                    return false;
                  }
                  door.interactionDistance = distance;
                  return true;
                });
  selectedObject = callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  bool runtimeTargetOpen = false;
  const bool runtimeDoorAvailable =
      callbacks.selectedDoorRuntimeTargetOpen(runtimeTargetOpen);
  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_debug_target",
          Rectangle{0.0f, y, contentW, rowH}, font,
          !runtimeDoorAvailable ? "Runtime Target Unavailable"
                                : (runtimeTargetOpen ? "Debug Target Close"
                                                     : "Debug Target Open"))) {
    if (runtimeDoorAvailable) {
      callbacks.setSelectedDoorRuntimeTargetOpen(!runtimeTargetOpen);
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
  const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
      smallConfig, assets, smallFont, textureStatus.c_str(), contentW, 1);
  engine::Text(ui, smallConfig, assets,
               Rectangle{0.0f, y, contentW, textureStatusHeight}, smallFont,
               textureStatus.c_str(), engine::UITextJustify::Left,
               textureMissing ? config.invalidColor : config.mutedTextColor,
               true);
  y += textureStatusHeight + gap;

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_pick_texture",
          Rectangle{0.0f, y, contentW, rowH}, font, "Pick Texture")) {
    callbacks.openDoorTexturePicker();
  }
  y += rowH + gap;

  if (engine::Button(
          ui, config, input, assets, "sector_editor_door_uv_settings",
          Rectangle{0.0f, y, contentW, rowH}, font, "UV Settings...")) {
    callbacks.openDoorTextureSettingsModal();
  }
  y += rowH + gap;

  if (engine::Button(ui, config, input, assets,
                     "sector_editor_delete_runtime_object",
                     Rectangle{0.0f, y, contentW, rowH}, font, "Delete")) {
    callbacks.deleteSelectedRuntimeObject();
    return;
  }
  return;
}

} // namespace game
