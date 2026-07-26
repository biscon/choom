#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

#include "sector_editor/tools/billboards/SectorEditorBillboardInspector.h"
#include "sector_editor/tools/doors/SectorEditorDoorInspector.h"

#include <raylib.h>

#include <string>

namespace game {

float MeasureSectorEditorPlacedObjectInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context) {
  const SectorPlacedRuntimeObject *object =
      context.callbacks.selectedRuntimeObject();
  if (object == nullptr) {
    return 42.0f;
  }

  if (object->kind == "door") {
    return MeasureSectorEditorDoorInspectorContentHeight(context, *object);
  }
  if (object->kind == "billboard") {
    return MeasureSectorEditorBillboardInspectorContentHeight(context, *object);
  }
  return 72.0f;
}

void DrawSectorEditorPlacedObjectInspector(
    SectorEditorPlacedObjectInspectorContext &context) {
  engine::UIContext &ui = context.ui;
  const engine::UIConfig &config = context.config;
  engine::AssetManager &assets = context.assets;
  const engine::FontHandle font = context.font;
  const SectorEditorPlacedObjectInspectorCallbacks &callbacks =
      context.callbacks;
  const float contentW = context.contentW;
  const float rowH = context.rowH;

  float y = 0.0f;
  const SectorPlacedRuntimeObject *selectedObject =
      callbacks.selectedRuntimeObject();
  if (selectedObject == nullptr) {
    return;
  }

  engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font,
               TextFormat("Object ID: %d", selectedObject->id),
               engine::UITextJustify::Left, config.textColor);
  y += 38.0f;

  const bool isBillboard = selectedObject->kind == "billboard";
  const bool isDoor = selectedObject->kind == "door";
  engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font,
               isBillboard ? "Type: Billboard"
               : isDoor    ? "Type: Door"
                           : "Type: Unsupported object",
               engine::UITextJustify::Left,
               isBillboard || isDoor ? config.mutedTextColor
                                     : config.invalidColor);
  y += 34.0f;

  if (isDoor) {
    DrawSectorEditorDoorInspector(context, y);
    return;
  }
  if (isBillboard) {
    DrawSectorEditorBillboardInspector(context, y);
    return;
  }

  if (engine::Button(ui, config, context.input, assets,
                     "sector_editor_delete_runtime_object",
                     Rectangle{0.0f, y, contentW, rowH}, font, "Delete")) {
    callbacks.deleteSelectedRuntimeObject();
  }
}

} // namespace game
