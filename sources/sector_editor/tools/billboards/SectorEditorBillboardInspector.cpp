#include "sector_editor/tools/billboards/SectorEditorBillboardInspector.h"

#include "sector_demo/SectorBillboardRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

namespace game {
namespace {

constexpr float BillboardSizeMin = 0.001f;
constexpr float BillboardSizeMax = 100000.0f;

std::string RuntimeObjectSpriteLabel(const SectorPlacedRuntimeObject &object) {
  return object.billboard.spriteAnimationPath.empty()
             ? std::string("Sprite: None selected")
             : std::string(TextFormat(
                   "Sprite: %s", object.billboard.spriteAnimationPath.c_str()));
}

bool ResolveBillboardAspectFromAnimation(
    const engine::AssetManager &assets,
    const engine::SpriteAnimationHandle animation, uint32_t clipIndex,
    float &outAspect) {
  const engine::SpriteAnimationAsset *asset =
      assets.GetSpriteAnimation(animation);
  if (asset == nullptr || asset->frames.empty()) {
    return false;
  }

  uint32_t frameIndex = 0;
  if (clipIndex != engine::InvalidSpriteClipIndex &&
      clipIndex < asset->clips.size()) {
    const engine::SpriteClip &clip = asset->clips[clipIndex];
    if (clip.frameCount > 0 && clip.firstFrame < asset->frames.size()) {
      frameIndex = clip.firstFrame;
    }
  }

  const engine::SpriteFrame &frame = asset->frames[frameIndex];
  Vector2 frameSize = frame.sourceSize;
  if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
    frameSize =
        Vector2{std::abs(frame.source.width), std::abs(frame.source.height)};
  }
  if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
    return false;
  }

  outAspect = frameSize.x / frameSize.y;
  return std::isfinite(outAspect) && outAspect > 0.0f;
}

float ResolveBillboardAspect(const SectorRuntimeObjectState &runtimeObjects,
                             engine::EngineContext *engineContext,
                             const engine::AssetManager &assets,
                             const SectorPlacedRuntimeObject &object) {
  if (engineContext == nullptr || object.kind != "billboard") {
    return 0.0f;
  }

  engine::SpriteAnimationHandle animation = engine::NullSpriteAnimationHandle();
  uint32_t clipIndex = engine::InvalidSpriteClipIndex;
  for (const SectorPlacedRuntimeObjectEntity &entry :
       runtimeObjects.placedObjectEntities) {
    if (entry.placedObjectId != object.id ||
        !engineContext->world.IsAlive(entry.entity) ||
        !engineContext->world.Has<SectorBillboardSprite>(entry.entity)) {
      continue;
    }

    const SectorBillboardSprite &sprite =
        engineContext->world.Get<SectorBillboardSprite>(entry.entity);
    animation = sprite.animation;
    clipIndex = sprite.clipIndex;
    break;
  }

  if (engine::IsNull(animation)) {
    return 0.0f;
  }

  if (object.billboard.directional) {
    const uint32_t frontClipIndex = assets.FindSpriteClipIndex(
        animation, object.billboard.frontClip.c_str());
    if (frontClipIndex != engine::InvalidSpriteClipIndex) {
      clipIndex = frontClipIndex;
    }
  } else if (!object.billboard.clip.empty()) {
    const uint32_t selectedClipIndex =
        assets.FindSpriteClipIndex(animation, object.billboard.clip.c_str());
    if (selectedClipIndex != engine::InvalidSpriteClipIndex) {
      clipIndex = selectedClipIndex;
    }
  }

  float aspect = 0.0f;
  return ResolveBillboardAspectFromAnimation(assets, animation, clipIndex,
                                             aspect)
             ? aspect
             : 0.0f;
}

} // namespace

float MeasureSectorEditorBillboardInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object) {
  const std::string spriteLabel = RuntimeObjectSpriteLabel(object);
  const float spriteLabelHeight = MeasureSectorEditorWrappedTextHeight(
      context.smallConfig, context.assets, context.smallFont,
      spriteLabel.c_str(), context.contentW, 1);
  const bool hasBillboardAspect =
      ResolveBillboardAspect(context.runtimeObjects, context.engineContext,
                             context.assets, object) > 0.0f;
  const bool keepAspectWarningVisible =
      object.billboard.keepAspectRatio && !hasBillboardAspect;
  return SectorEditorRuntimeObjectInspectorContentHeight(
      context.rowH, context.gap, true, keepAspectWarningVisible,
      object.billboard.directional, spriteLabelHeight, 28.0f);
}

void DrawSectorEditorBillboardInspector(
    SectorEditorPlacedObjectInspectorContext &context, float &y) {
  engine::UIContext &ui = context.ui;
  const engine::UIConfig &config = context.config;
  engine::Input &input = context.input;
  engine::AssetManager &assets = context.assets;
  const engine::FontHandle font = context.font;
  const engine::FontHandle smallFont = context.smallFont;
  RuntimeObjectEditingState &state = context.editingState;
  RuntimeObjectEditingUiState &uiState = context.uiState;
  SectorEditorRuntimeObjectEditingService &editing = context.editing;
  const float contentW = context.contentW;
  const float rowH = context.rowH;
  const float gap = context.gap;
  const engine::UIConfig smallConfig =
      SectorEditorSmallFontConfig(config, assets, smallFont);

  const bool isBillboard = true;
  const SectorPlacedRuntimeObject *selectedObject =
      editing.SelectedObject();
  if (selectedObject == nullptr || selectedObject->kind != "billboard") {
    return;
  }

  const std::string spriteLabel = RuntimeObjectSpriteLabel(*selectedObject);
  const float spriteLabelHeight = MeasureSectorEditorWrappedTextHeight(
      smallConfig, assets, smallFont, spriteLabel.c_str(), contentW, 1);
  engine::Text(ui, smallConfig, assets,
               Rectangle{0.0f, y, contentW, spriteLabelHeight}, smallFont,
               spriteLabel.c_str(), engine::UITextJustify::Left,
               selectedObject->billboard.spriteAnimationPath.empty()
                   ? config.invalidColor
                   : config.textColor,
               true);
  y += spriteLabelHeight + gap;

  const float selectedBillboardAspect = ResolveBillboardAspect(
      context.runtimeObjects, context.engineContext, assets, *selectedObject);
  const bool hasBillboardAspect = selectedBillboardAspect > 0.0f;

  auto drawObjectFloat =
      [&](const char *id, const char *label, float value,
          engine::UIFloatInputState &inputState, float minValue, float maxValue,
          int decimals,
          const std::function<bool(SectorPlacedRuntimeObject &, float)>
              &applyValue) {
        const float labelW = 92.0f;
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
            ui, config, input, assets, font, id, label,
            Rectangle{0.0f, y, labelW, rowH},
            Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap),
                      rowH},
            engine::UITextJustify::Left, value, inputState, minValue, maxValue,
            decimals);
        if (result.changed && result.finite && result.value != value) {
          editing.MutateSelected(
              "Updated object transform",
              [applyValue,
               value = result.value](SectorPlacedRuntimeObject &object) {
                return applyValue(object, value);
              });
        }
        y += rowH + gap;
      };

  auto drawBillboardFloat =
      [&](const char *id, const char *label, float value,
          engine::UIFloatInputState &inputState, float minValue, float maxValue,
          int decimals,
          const std::function<bool(SectorPlacedRuntimeObject &, float)>
              &applyValue) {
        const float labelW = 92.0f;
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
            ui, config, input, assets, font, id, label,
            Rectangle{0.0f, y, labelW, rowH},
            Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap),
                      rowH},
            engine::UITextJustify::Left, value, inputState, minValue, maxValue,
            decimals);
        if (result.changed && result.finite && result.value != value) {
          editing.MutateSelected(
              "Updated billboard properties",
              [applyValue,
               value = result.value](SectorPlacedRuntimeObject &object) {
                return applyValue(object, value);
              });
        }
        y += rowH + gap;
      };

  auto openBillboardPickerButton = [&](const char *id, const char *label) {
    if (engine::Button(ui, config, input, assets, id,
                       Rectangle{0.0f, y, contentW, rowH}, font, label)) {
      OpenBillboardSpritePicker(
          state.spritePicker,
          selectedObject != nullptr
              ? selectedObject->billboard.spriteAnimationPath
              : std::string{});
      context.statusText = "Pick a billboard sprite";
    }
    y += rowH + gap;
  };

  const SectorSpriteMetadata *selectedBillboardMetadata = nullptr;
  std::string selectedBillboardMetadataStatus;
  auto resolveSelectedBillboardMetadata =
      [&]() -> const SectorSpriteMetadata * {
    if (!isBillboard || selectedObject == nullptr) {
      return nullptr;
    }
    const std::string spritePath =
        selectedObject->billboard.spriteAnimationPath;
    if (spritePath.empty()) {
      selectedBillboardMetadataStatus = "Pick a sprite first";
      return nullptr;
    }

    const bool targetChanged =
        state.billboardMetadataObjectId != selectedObject->id ||
        state.billboardMetadataSpriteAnimationPath != spritePath;
    if (targetChanged) {
      state.billboardMetadataObjectId = selectedObject->id;
      state.billboardMetadataSpriteAnimationPath = spritePath;
      state.billboardMetadataInitialRepairAttempted = false;
      if (!state.spriteMetadataCatalog.scanned ||
          FindSpriteMetadata(state.spriteMetadataCatalog, spritePath) ==
              nullptr) {
        RefreshSpriteMetadataCatalog(state.spriteMetadataCatalog);
      }
    }

    const SectorSpriteMetadata *metadata =
        FindSpriteMetadata(state.spriteMetadataCatalog, spritePath);
    if (metadata == nullptr) {
      selectedBillboardMetadataStatus =
          state.spriteMetadataCatalog.scanMessage.empty()
              ? "Sprite metadata unavailable"
              : state.spriteMetadataCatalog.scanMessage;
      return nullptr;
    }
    if (metadata->clipNames.empty()) {
      selectedBillboardMetadataStatus = "Sprite has no clips";
      return nullptr;
    }

    if (!state.billboardMetadataInitialRepairAttempted) {
      state.billboardMetadataInitialRepairAttempted = true;
      const int objectId = selectedObject->id;
      editing.MutateSelected(
          "Updated billboard clip defaults",
          [objectId, spritePath, metadata](SectorPlacedRuntimeObject &object) {
            if (object.id != objectId || object.kind != "billboard" ||
                object.billboard.spriteAnimationPath != spritePath) {
              return false;
            }
            return RepairBillboardClipsForSpriteMetadata(object.billboard,
                                                         *metadata, false);
          });
      selectedObject = editing.SelectedObject();
    }
    return metadata;
  };

  auto drawBillboardClipOptionRow =
      [&](const char *optionId, const char *label, const std::string &clipName,
          const std::function<bool(SectorPlacedRuntimeObject &,
                                   const std::string &)> &applyClip) {
        const SectorEditorInspectorStackedOptionRowLayout layout =
            BuildSectorEditorInspectorStackedOptionRowLayout(y, contentW, rowH,
                                                             gap);
        engine::Text(ui, config, assets, layout.labelRect, font, label,
                     engine::UITextJustify::Left, config.mutedTextColor);
        if (selectedBillboardMetadata == nullptr ||
            selectedBillboardMetadata->clipNames.empty()) {
          engine::Text(ui, config, assets, layout.fieldRect, font,
                       selectedBillboardMetadataStatus.empty()
                           ? "Sprite metadata unavailable"
                           : selectedBillboardMetadataStatus.c_str(),
                       engine::UITextJustify::Left, config.mutedTextColor);
          y += layout.height + gap;
          return;
        }

        int selectedIndex = 0;
        const auto it =
            std::find(selectedBillboardMetadata->clipNames.begin(),
                      selectedBillboardMetadata->clipNames.end(), clipName);
        if (it != selectedBillboardMetadata->clipNames.end()) {
          selectedIndex = static_cast<int>(
              std::distance(selectedBillboardMetadata->clipNames.begin(), it));
        }
        const int previousIndex = selectedIndex;
        if (engine::Option(
                ui, config, input, assets, optionId, layout.fieldRect, font,
                selectedBillboardMetadata->clipNames, selectedIndex) &&
            selectedIndex >= 0 &&
            selectedIndex <
                static_cast<int>(selectedBillboardMetadata->clipNames.size()) &&
            selectedIndex != previousIndex) {
          const std::string selectedClip =
              selectedBillboardMetadata
                  ->clipNames[static_cast<size_t>(selectedIndex)];
          editing.MutateSelected(
              "Updated billboard clip",
              [applyClip, selectedClip](SectorPlacedRuntimeObject &object) {
                if (object.kind != "billboard") {
                  return false;
                }
                return applyClip(object, selectedClip);
              });
        }
        y += layout.height + gap;
      };

  if (isBillboard) {
    openBillboardPickerButton("sector_editor_runtime_object_pick_sprite",
                              "Pick Sprite");
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }
  }

  drawObjectFloat("sector_editor_runtime_object_x", "Position X",
                  selectedObject->position.x, uiState.xInput,
                  -100000.0f, 100000.0f, 3,
                  [](SectorPlacedRuntimeObject &object, float value) {
                    if (object.position.x == value) {
                      return false;
                    }
                    object.position.x = value;
                    return true;
                  });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }
  drawObjectFloat("sector_editor_runtime_object_y", "Position Y",
                  selectedObject->position.y, uiState.yInput,
                  -100000.0f, 100000.0f, 3,
                  [](SectorPlacedRuntimeObject &object, float value) {
                    if (object.position.y == value) {
                      return false;
                    }
                    object.position.y = value;
                    return true;
                  });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }
  drawObjectFloat("sector_editor_runtime_object_z", "Position Z",
                  selectedObject->position.z, uiState.zInput,
                  -100000.0f, 100000.0f, 3,
                  [](SectorPlacedRuntimeObject &object, float value) {
                    if (object.position.z == value) {
                      return false;
                    }
                    object.position.z = value;
                    return true;
                  });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }
  constexpr float radiansToDegrees = 180.0f / PI;
  constexpr float degreesToRadians = PI / 180.0f;
  drawObjectFloat(
      "sector_editor_runtime_object_yaw", "Yaw",
      selectedObject->yawRadians * radiansToDegrees,
      uiState.yawInput, -3600.0f, 3600.0f, 2,
      [degreesToRadians](SectorPlacedRuntimeObject &object, float value) {
        const float radians = value * degreesToRadians;
        if (object.yawRadians == radians) {
          return false;
        }
        object.yawRadians = radians;
        return true;
      });
  selectedObject = editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  if (isBillboard) {
    drawBillboardFloat(
        "sector_editor_runtime_object_width", "Width",
        selectedObject->billboard.sizeWorld.x, uiState.widthInput,
        BillboardSizeMin, BillboardSizeMax, 3,
        [hasBillboardAspect, selectedBillboardAspect](
            SectorPlacedRuntimeObject &object, float value) {
          const float width = std::max(BillboardSizeMin, value);
          bool changed = object.billboard.sizeWorld.x != width;
          object.billboard.sizeWorld.x = width;
          if (object.billboard.keepAspectRatio && hasBillboardAspect) {
            const float height =
                std::max(BillboardSizeMin, width / selectedBillboardAspect);
            changed = changed || object.billboard.sizeWorld.y != height;
            object.billboard.sizeWorld.y = height;
          }
          return changed;
        });
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    drawBillboardFloat(
        "sector_editor_runtime_object_height", "Height",
        selectedObject->billboard.sizeWorld.y, uiState.heightInput,
        BillboardSizeMin, BillboardSizeMax, 3,
        [hasBillboardAspect, selectedBillboardAspect](
            SectorPlacedRuntimeObject &object, float value) {
          const float height = std::max(BillboardSizeMin, value);
          bool changed = object.billboard.sizeWorld.y != height;
          object.billboard.sizeWorld.y = height;
          if (object.billboard.keepAspectRatio && hasBillboardAspect) {
            const float width =
                std::max(BillboardSizeMin, height * selectedBillboardAspect);
            changed = changed || object.billboard.sizeWorld.x != width;
            object.billboard.sizeWorld.x = width;
          }
          return changed;
        });
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    bool keepAspect = selectedObject->billboard.keepAspectRatio;
    if (engine::Checkbox(ui, config, input, assets,
                         "sector_editor_runtime_object_keep_aspect",
                         Rectangle{0.0f, y, contentW, rowH}, font,
                         "Keep aspect ratio", keepAspect)) {
      editing.MutateSelected(
          keepAspect && !hasBillboardAspect
              ? "Billboard aspect unavailable until sprite metadata loads"
              : "Updated billboard aspect mode",
          [keepAspect](SectorPlacedRuntimeObject &object) {
            if (object.billboard.keepAspectRatio == keepAspect) {
              return false;
            }
            object.billboard.keepAspectRatio = keepAspect;
            return true;
          });
    }
    y += rowH + gap;
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }
    if (selectedObject->billboard.keepAspectRatio && !hasBillboardAspect) {
      engine::Text(ui, smallConfig, assets, Rectangle{0.0f, y, contentW, 28.0f},
                   smallFont, "Aspect unavailable until sprite metadata loads",
                   engine::UITextJustify::Left, config.mutedTextColor);
      y += 28.0f + gap;
    }

    drawBillboardFloat("sector_editor_runtime_object_origin_x", "Origin X",
                       selectedObject->billboard.originNormalized.x,
                       uiState.originXInput, 0.0f, 1.0f, 3,
                       [](SectorPlacedRuntimeObject &object, float value) {
                         const float origin = Clamp(value, 0.0f, 1.0f);
                         if (object.billboard.originNormalized.x == origin) {
                           return false;
                         }
                         object.billboard.originNormalized.x = origin;
                         return true;
                       });
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    drawBillboardFloat("sector_editor_runtime_object_origin_y", "Origin Y",
                       selectedObject->billboard.originNormalized.y,
                       uiState.originYInput, 0.0f, 1.0f, 3,
                       [](SectorPlacedRuntimeObject &object, float value) {
                         const float origin = Clamp(value, 0.0f, 1.0f);
                         if (object.billboard.originNormalized.y == origin) {
                           return false;
                         }
                         object.billboard.originNormalized.y = origin;
                         return true;
                       });
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    bool directional = selectedObject->billboard.directional;
    if (engine::Checkbox(ui, config, input, assets,
                         "sector_editor_runtime_object_directional",
                         Rectangle{0.0f, y, contentW, rowH}, font,
                         "Directional", directional)) {
      editing.MutateSelected(
          "Updated billboard directional mode",
          [directional](SectorPlacedRuntimeObject &object) {
            if (object.billboard.directional == directional) {
              return false;
            }
            object.billboard.directional = directional;
            return true;
          });
    }
    y += rowH + gap;
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    selectedBillboardMetadata = resolveSelectedBillboardMetadata();
    selectedObject = editing.SelectedObject();
    if (selectedObject == nullptr) {
      return;
    }

    if (selectedObject->billboard.directional) {
      drawBillboardClipOptionRow(
          "sector_editor_billboard_clip_front_option", "Front Clip",
          selectedObject->billboard.frontClip,
          [](SectorPlacedRuntimeObject &object, const std::string &clip) {
            if (object.billboard.frontClip == clip) {
              return false;
            }
            object.billboard.frontClip = clip;
            return true;
          });
      selectedObject = editing.SelectedObject();
      if (selectedObject == nullptr) {
        return;
      }
      drawBillboardClipOptionRow(
          "sector_editor_billboard_clip_back_option", "Back Clip",
          selectedObject->billboard.backClip,
          [](SectorPlacedRuntimeObject &object, const std::string &clip) {
            if (object.billboard.backClip == clip) {
              return false;
            }
            object.billboard.backClip = clip;
            return true;
          });
      selectedObject = editing.SelectedObject();
      if (selectedObject == nullptr) {
        return;
      }
      drawBillboardClipOptionRow(
          "sector_editor_billboard_clip_left_option", "Left Clip",
          selectedObject->billboard.leftClip,
          [](SectorPlacedRuntimeObject &object, const std::string &clip) {
            if (object.billboard.leftClip == clip) {
              return false;
            }
            object.billboard.leftClip = clip;
            return true;
          });
      selectedObject = editing.SelectedObject();
      if (selectedObject == nullptr) {
        return;
      }
      drawBillboardClipOptionRow(
          "sector_editor_billboard_clip_right_option", "Right Clip",
          selectedObject->billboard.rightClip,
          [](SectorPlacedRuntimeObject &object, const std::string &clip) {
            if (object.billboard.rightClip == clip) {
              return false;
            }
            object.billboard.rightClip = clip;
            return true;
          });
      selectedObject = editing.SelectedObject();
      if (selectedObject == nullptr) {
        return;
      }
    } else {
      drawBillboardClipOptionRow(
          "sector_editor_billboard_clip_single_option", "Clip",
          selectedObject->billboard.clip,
          [](SectorPlacedRuntimeObject &object, const std::string &clip) {
            if (object.billboard.clip == clip) {
              return false;
            }
            object.billboard.clip = clip;
            return true;
          });
      selectedObject = editing.SelectedObject();
      if (selectedObject == nullptr) {
        return;
      }
    }

    bool playing = selectedObject->billboard.playing;
    if (engine::Checkbox(
            ui, config, input, assets, "sector_editor_runtime_object_playing",
            Rectangle{0.0f, y, contentW, rowH}, font, "Playing", playing)) {
      editing.MutateSelected(
          "Updated billboard playback",
          [playing](SectorPlacedRuntimeObject &object) {
            if (object.billboard.playing == playing) {
              return false;
            }
            object.billboard.playing = playing;
            return true;
          });
    }
    y += rowH + gap;
  }

  if (engine::Button(ui, config, input, assets,
                     "sector_editor_delete_runtime_object",
                     Rectangle{0.0f, y, contentW, rowH}, font, "Delete")) {
    context.deleteRequested = true;
  }
}

} // namespace game
