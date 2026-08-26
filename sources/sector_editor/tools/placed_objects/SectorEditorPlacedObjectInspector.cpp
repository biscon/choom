#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

#include "sector_editor/tools/billboards/SectorEditorBillboardInspector.h"
#include "sector_editor/tools/doors/SectorEditorDoorInspector.h"
#include "sector_editor/tools/placed_objects/SectorEditorStaticModelInspector.h"
#include "sector_editor/tools/placed_objects/SectorEditorDynamicModelInspector.h"
#include "sector_editor/tools/placed_objects/SectorEditorNpcInspector.h"
#include "sector_editor/tools/placed_objects/SectorEditorItemInspector.h"

#include <raylib.h>

#include <string>

namespace game {

float MeasureSectorEditorPlacedObjectInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context) {
  const SectorPlacedRuntimeObject *object =
      context.editing.SelectedObject();
  if (object == nullptr) {
    return 42.0f;
  }

  if (object->kind == "door") {
    return MeasureSectorEditorDoorInspectorContentHeight(context, *object);
  }
  if (object->kind == "billboard") {
    return MeasureSectorEditorBillboardInspectorContentHeight(context, *object);
  }
  if (object->kind == "static_model") {
    return MeasureSectorEditorStaticModelInspectorContentHeight(context, *object);
  }
  if (object->kind == "dynamic_model") {
    return MeasureSectorEditorDynamicModelInspectorContentHeight(context, *object);
  }
  if (object->kind == "npc") {
    return MeasureSectorEditorNpcInspectorContentHeight(context, *object);
  }
  if (object->kind == "item") {
    return MeasureSectorEditorItemInspectorContentHeight(context, *object);
  }
  return 72.0f;
}

void DrawSectorEditorPlacedObjectInspector(
    SectorEditorPlacedObjectInspectorContext &context) {
  engine::UIContext &ui = context.ui;
  const engine::UIConfig &config = context.config;
  engine::AssetManager &assets = context.assets;
  const engine::FontHandle font = context.font;
  const float contentW = context.contentW;
  const float rowH = context.rowH;

  float y = 0.0f;
  const SectorPlacedRuntimeObject *selectedObject =
      context.editing.SelectedObject();
  if (selectedObject == nullptr) {
    return;
  }

  engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font,
               TextFormat("Object ID: %d", selectedObject->id),
               engine::UITextJustify::Left, config.textColor);
  y += 38.0f;

  const bool isBillboard = selectedObject->kind == "billboard";
  const bool isDoor = selectedObject->kind == "door";
  const bool isStaticModel = selectedObject->kind == "static_model";
  const bool isDynamicModel = selectedObject->kind == "dynamic_model";
  const bool isNpc = selectedObject->kind == "npc";
  const bool isItem = selectedObject->kind == "item";
  engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font,
               isBillboard ? "Type: Billboard"
               : isStaticModel ? "Type: 3D Prop"
               : isDynamicModel ? "Type: Dynamic Prop"
               : isNpc ? "Type: NPC"
               : isItem ? "Type: Item"
               : isDoor    ? "Type: Door"
                           : "Type: Unsupported object",
               engine::UITextJustify::Left,
               isBillboard || isStaticModel || isDynamicModel || isNpc || isItem || isDoor ? config.mutedTextColor
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
  if (isStaticModel) {
    DrawSectorEditorStaticModelInspector(context, y);
    return;
  }
  if (isDynamicModel) {
    DrawSectorEditorDynamicModelInspector(context, y);
    return;
  }
  if (isNpc) {
    DrawSectorEditorNpcInspector(context, y);
    return;
  }
  if (isItem) {
    DrawSectorEditorItemInspector(context, y);
    return;
  }

  if (engine::Button(ui, config, context.input, assets,
                     "sector_editor_delete_runtime_object",
                     Rectangle{0.0f, y, contentW, rowH}, font, "Delete")) {
    context.deleteRequested = true;
  }
}

} // namespace game
