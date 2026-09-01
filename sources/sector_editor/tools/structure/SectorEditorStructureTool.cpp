#include "sector_editor/tools/structure/SectorEditorStructureTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace game {
namespace {

constexpr float PaletteButtonWidth = 62.0f;
constexpr float PaletteButtonHeight = 30.0f;
constexpr float PaletteGap = 4.0f;
constexpr float PaletteInset = 10.0f;

constexpr std::array<SectorStructuralPrimitiveKind, 5> PaletteKinds{
        SectorStructuralPrimitiveKind::Box,
        SectorStructuralPrimitiveKind::Ramp,
        SectorStructuralPrimitiveKind::Stairs,
        SectorStructuralPrimitiveKind::Cylinder,
        SectorStructuralPrimitiveKind::Sphere};

Rectangle PaletteButtonRect(const SectorEditorToolContext& context, size_t index)
{
    return Rectangle{
            context.canvasRect.x + PaletteInset
                    + static_cast<float>(index) * (PaletteButtonWidth + PaletteGap),
            context.canvasRect.y + PaletteInset,
            PaletteButtonWidth,
            PaletteButtonHeight};
}

bool PaletteContains(const SectorEditorToolContext& context, Vector2 point)
{
    return CheckCollisionPointRec(point, Rectangle{
            context.canvasRect.x + PaletteInset,
            context.canvasRect.y + PaletteInset,
            PaletteButtonWidth * static_cast<float>(PaletteKinds.size())
                    + PaletteGap * static_cast<float>(PaletteKinds.size() - 1),
            PaletteButtonHeight});
}

const char* PaletteLabel(SectorStructuralPrimitiveKind kind)
{
    switch (kind) {
        case SectorStructuralPrimitiveKind::Box: return "Box";
        case SectorStructuralPrimitiveKind::Ramp: return "Ramp";
        case SectorStructuralPrimitiveKind::Stairs: return "Stairs";
        case SectorStructuralPrimitiveKind::Cylinder: return "Cylinder";
        case SectorStructuralPrimitiveKind::Sphere: return "Sphere";
    }
    return "?";
}

bool CancelStructureTool(SectorEditorToolContext& context, const char* message)
{
    if (context.structuralPrimitiveState == nullptr) return false;
    const bool wasActive = context.structuralPrimitiveState->pendingPlacement.active;
    context.structuralPrimitiveState->pendingPlacement = PendingStructuralPrimitivePlacement{};
    if (wasActive && message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
    return wasActive;
}

bool CurrentCoordPoint(
        SectorEditorToolContext& context,
        SectorTopologyCoordPoint& outPoint,
        std::string& outError)
{
    return context.currentSnappedSectorPoint && context.toTopologyCoordPoint
            && context.toTopologyCoordPoint(
                    context.currentSnappedSectorPoint(), outPoint, outError);
}

bool HandleStructureMousePress(
        SectorEditorToolContext& context,
        const engine::InputEvent& event)
{
    if (event.mouseButton.button != MOUSE_LEFT_BUTTON
            || context.structuralPrimitiveState == nullptr
            || context.structuralPrimitiveEditing == nullptr) return false;

    if (PaletteContains(context, event.mouseButton.position)) {
        for (size_t index = 0; index < PaletteKinds.size(); ++index) {
            if (!CheckCollisionPointRec(
                        event.mouseButton.position,
                        PaletteButtonRect(context, index))) continue;
            context.structuralPrimitiveState->placementKind = PaletteKinds[index];
            context.statusText = TextFormat(
                    "Structure %s: drag in the canvas to place",
                    PaletteLabel(PaletteKinds[index]));
            return true;
        }
        return true;
    }

    SectorTopologyCoordPoint point;
    std::string error;
    if (!CurrentCoordPoint(context, point, error)) {
        context.statusText = error.empty()
                ? "Structure placement is outside the authoring coordinate range"
                : error;
        return true;
    }
    float floor = 0.0f;
    if (!context.structuralPrimitiveEditing->ResolvePlacementFloor(point, floor)) {
        context.statusText = "Structure placement must start strictly inside a derived sector";
        return true;
    }
    PendingStructuralPrimitivePlacement& pending =
            context.structuralPrimitiveState->pendingPlacement;
    pending = PendingStructuralPrimitivePlacement{};
    pending.active = true;
    pending.kind = context.structuralPrimitiveState->placementKind;
    pending.start = point;
    pending.current = point;
    pending.seedFloor = floor;
    context.statusText = "Structure: drag to size, right click/Esc cancels";
    return true;
}

bool UpdateStructureTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.structuralPrimitiveState == nullptr
            || context.structuralPrimitiveEditing == nullptr) return false;
    PendingStructuralPrimitivePlacement& pending =
            context.structuralPrimitiveState->pendingPlacement;
    if (pending.active) {
        std::string error;
        SectorTopologyCoordPoint current;
        if (CurrentCoordPoint(context, current, error)) {
            pending.current = current;
            pending.errorMessage.clear();
        } else {
            pending.errorMessage = error;
        }
    }

    bool handled = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseButtonReleased,
            true,
            [&context, &pending, &handled](engine::InputEvent& event) {
                if (!pending.active || event.mouseButton.button != MOUSE_LEFT_BUTTON) return;
                const PendingStructuralPrimitivePlacement placement = pending;
                pending = PendingStructuralPrimitivePlacement{};
                int primitiveId = -1;
                if (context.structuralPrimitiveEditing->CreateFromDrag(
                            placement.kind,
                            placement.start,
                            placement.current,
                            placement.seedFloor,
                            context.state.defaultWallTextureId,
                            &primitiveId)) {
                    context.currentTool = SectorEditorTool::Select;
                }
                engine::ConsumeEvent(event);
                handled = true;
            });
    context.input->ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [&context, &handled](engine::InputEvent& event) {
                if (event.mouseButton.button == MOUSE_RIGHT_BUTTON
                        && CancelStructureTool(context, "Structure placement cancelled")) {
                    engine::ConsumeEvent(event);
                    handled = true;
                }
            });
    return handled;
}

void DrawFootprint(
        SectorEditorToolContext& context,
        const SectorAuthoringStructuralPrimitive& primitive,
        Color color)
{
    if (!context.mapToScreen) return;
    const SectorStructuralFootprint footprint = BuildSectorStructuralFootprint(primitive);
    if (footprint.circular) {
        const Vector2 center = context.mapToScreen(
                SectorWorldToAuthoringPosition(footprint.centerWorld));
        const Vector2 radiusPoint = context.mapToScreen(
                SectorWorldToAuthoringPosition(Vector2{
                        footprint.centerWorld.x + footprint.radiusWorld,
                        footprint.centerWorld.y}));
        DrawCircleLinesV(center, std::fabs(radiusPoint.x - center.x), color);
        return;
    }
    for (size_t index = 0; index < footprint.pointsWorld.size(); ++index) {
        const Vector2 a = context.mapToScreen(
                SectorWorldToAuthoringPosition(footprint.pointsWorld[index]));
        const Vector2 b = context.mapToScreen(
                SectorWorldToAuthoringPosition(
                        footprint.pointsWorld[(index + 1) % footprint.pointsWorld.size()]));
        DrawLineEx(a, b, 2.0f, color);
    }
}

void DrawStructureToolOverlay(SectorEditorToolContext& context)
{
    if (context.currentTool != SectorEditorTool::Structure) return;
    if (context.structuralPrimitiveState == nullptr) return;
    for (size_t index = 0; index < PaletteKinds.size(); ++index) {
        const Rectangle rect = PaletteButtonRect(context, index);
        const bool active = context.structuralPrimitiveState->placementKind
                == PaletteKinds[index];
        DrawRectangleRec(rect, active ? Color{58, 105, 148, 245}
                                      : Color{34, 39, 48, 240});
        DrawRectangleLinesEx(rect, active ? 2.0f : 1.0f,
                active ? SKYBLUE : Color{110, 120, 134, 255});
        const char* label = PaletteLabel(PaletteKinds[index]);
        const int fontSize = 14;
        DrawText(label,
                static_cast<int>(rect.x + (rect.width - MeasureText(label, fontSize)) * 0.5f),
                static_cast<int>(rect.y + 8.0f), fontSize, RAYWHITE);
    }

    const PendingStructuralPrimitivePlacement& pending =
            context.structuralPrimitiveState->pendingPlacement;
    if (!pending.active || context.structuralPrimitiveEditing == nullptr) return;
    SectorAuthoringStructuralPrimitive preview;
    std::string error;
    const bool valid = context.structuralPrimitiveEditing->BuildPlacementValue(
            pending.kind, pending.start, pending.current, pending.seedFloor,
            context.state.defaultWallTextureId, 1, preview, error);
    DrawFootprint(context, preview, valid ? YELLOW : RED);
    const int64_t dx = static_cast<int64_t>(pending.current.x) - pending.start.x;
    const int64_t dz = static_cast<int64_t>(pending.current.y) - pending.start.y;
    const char* dimensions = (pending.kind == SectorStructuralPrimitiveKind::Cylinder
                    || pending.kind == SectorStructuralPrimitiveKind::Sphere)
            ? TextFormat("r %.2f", SectorCoordToVisibleAuthoring(
                    static_cast<SectorCoord>(std::llround(std::hypot(
                            static_cast<double>(dx), static_cast<double>(dz))))))
            : TextFormat("%.2f x %.2f",
                    SectorCoordToVisibleAuthoring(static_cast<SectorCoord>(std::llabs(dx))),
                    SectorCoordToVisibleAuthoring(static_cast<SectorCoord>(std::llabs(dz))));
    DrawText(dimensions,
            static_cast<int>(context.canvasRect.x + PaletteInset),
            static_cast<int>(context.canvasRect.y + PaletteInset
                    + PaletteButtonHeight + 8.0f),
            16, valid ? YELLOW : RED);
}

const SectorEditorToolModule StructureModule{
        SectorEditorTool::Structure,
        "Structure",
        nullptr,
        nullptr,
        HandleStructureMousePress,
        UpdateStructureTool,
        DrawStructureToolOverlay,
        CancelStructureTool};

} // namespace

namespace {

constexpr float HandleRadiusPixels = 7.0f;
constexpr float RotationHandleOffsetPixels = 28.0f;

float Distance2(Vector2 a, Vector2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vector2 PrimitiveCenterMap(const SectorAuthoringStructuralPrimitive& primitive)
{
    return Vector2{
            SectorCoordToVisibleAuthoring(primitive.x),
            SectorCoordToVisibleAuthoring(primitive.z)};
}

std::vector<Vector2> FootprintMap(const SectorAuthoringStructuralPrimitive& primitive)
{
    const SectorStructuralFootprint footprint = BuildSectorStructuralFootprint(primitive);
    std::vector<Vector2> result;
    result.reserve(footprint.pointsWorld.size());
    for (Vector2 point : footprint.pointsWorld) {
        result.push_back(SectorWorldToAuthoringPosition(point));
    }
    return result;
}

Vector2 RotationHandleScreen(
        SectorEditorToolContext& context,
        const SectorAuthoringStructuralPrimitive& primitive)
{
    const Vector2 center = context.mapToScreen(PrimitiveCenterMap(primitive));
    const SectorStructuralFootprint footprint = BuildSectorStructuralFootprint(primitive);
    Vector2 directionMap = SectorWorldToAuthoringPosition(Vector2{
            footprint.centerWorld.x + footprint.ascentDirectionWorld.x,
            footprint.centerWorld.y + footprint.ascentDirectionWorld.y});
    const Vector2 centerMap = SectorWorldToAuthoringPosition(footprint.centerWorld);
    const Vector2 directionScreen = context.mapToScreen(directionMap);
    Vector2 direction{directionScreen.x - center.x, directionScreen.y - center.y};
    const float length = std::max(0.0001f, std::hypot(direction.x, direction.y));
    direction.x /= length;
    direction.y /= length;
    float extent = 16.0f;
    for (Vector2 point : FootprintMap(primitive)) {
        const Vector2 screen = context.mapToScreen(point);
        const float projected = (screen.x - center.x) * direction.x
                + (screen.y - center.y) * direction.y;
        extent = std::max(extent, projected);
    }
    return Vector2{
            center.x + direction.x * (extent + RotationHandleOffsetPixels),
            center.y + direction.y * (extent + RotationHandleOffsetPixels)};
}

bool PointInPolygon(const std::vector<Vector2>& polygon, Vector2 point)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vector2 a = polygon[i];
        const Vector2 b = polygon[j];
        if (((a.y > point.y) != (b.y > point.y))
                && point.x < (b.x - a.x) * (point.y - a.y)
                                / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

bool PointInsidePrimitive(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector2 mapPoint)
{
    const SectorStructuralFootprint footprint = BuildSectorStructuralFootprint(primitive);
    if (footprint.circular) {
        const Vector2 center = SectorWorldToAuthoringPosition(footprint.centerWorld);
        const float radius = SectorWorldToAuthoringDistance(footprint.radiusWorld);
        return Distance2(center, mapPoint) <= radius * radius;
    }
    const std::vector<Vector2> points = FootprintMap(primitive);
    return points.size() >= 3 && PointInPolygon(points, mapPoint);
}

SectorEditorStructuralHandleKind HitStructuralHandle(
        SectorEditorToolContext& context,
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector2 screenPoint)
{
    if (Distance2(RotationHandleScreen(context, primitive), screenPoint)
            <= HandleRadiusPixels * HandleRadiusPixels * 2.0f) {
        return SectorEditorStructuralHandleKind::Rotate;
    }
    if (SectorStructuralPrimitiveHasTilt(primitive)) {
        return SectorEditorStructuralHandleKind::None;
    }
    const std::vector<Vector2> points = FootprintMap(primitive);
    if (primitive.kind == SectorStructuralPrimitiveKind::Cylinder
            || primitive.kind == SectorStructuralPrimitiveKind::Sphere) {
        const Vector2 center = context.mapToScreen(PrimitiveCenterMap(primitive));
        const float radius = primitive.kind == SectorStructuralPrimitiveKind::Cylinder
                ? SectorCoordToVisibleAuthoring(primitive.cylinder.radius)
                : SectorCoordToVisibleAuthoring(primitive.sphere.radius);
        constexpr std::array<Vector2, 4> directions{{
                {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}}};
        for (Vector2 direction : directions) {
            const Vector2 handle = context.mapToScreen(Vector2{
                    PrimitiveCenterMap(primitive).x + direction.x * radius,
                    PrimitiveCenterMap(primitive).y + direction.y * radius});
            if (Distance2(handle, screenPoint)
                    <= HandleRadiusPixels * HandleRadiusPixels * 2.0f) {
                return SectorEditorStructuralHandleKind::Radius;
            }
        }
        (void)center;
    } else if (points.size() == 4) {
        for (size_t index = 0; index < 4; ++index) {
            if (Distance2(context.mapToScreen(points[index]), screenPoint)
                    <= HandleRadiusPixels * HandleRadiusPixels * 2.0f) {
                return static_cast<SectorEditorStructuralHandleKind>(
                        static_cast<int>(SectorEditorStructuralHandleKind::Corner0)
                                + static_cast<int>(index));
            }
        }
        for (size_t index = 0; index < 4; ++index) {
            const Vector2 midpoint{
                    (points[index].x + points[(index + 1) % 4].x) * 0.5f,
                    (points[index].y + points[(index + 1) % 4].y) * 0.5f};
            if (Distance2(context.mapToScreen(midpoint), screenPoint)
                    <= HandleRadiusPixels * HandleRadiusPixels * 2.0f) {
                return static_cast<SectorEditorStructuralHandleKind>(
                        static_cast<int>(SectorEditorStructuralHandleKind::Edge0)
                                + static_cast<int>(index));
            }
        }
    }
    return SectorEditorStructuralHandleKind::None;
}

SectorCoord ToCoord(float authored)
{
    return static_cast<SectorCoord>(std::llround(
            static_cast<double>(authored) * SectorCoordSubdivisions));
}

void SetRectDimensions(
        SectorAuthoringStructuralPrimitive& primitive,
        SectorCoord width,
        SectorCoord depth)
{
    if (primitive.kind == SectorStructuralPrimitiveKind::Box) {
        primitive.box.width = width;
        primitive.box.depth = depth;
    } else if (primitive.kind == SectorStructuralPrimitiveKind::Ramp) {
        primitive.ramp.width = width;
        primitive.ramp.run = depth;
    } else if (primitive.kind == SectorStructuralPrimitiveKind::Stairs) {
        primitive.stairs.width = width;
        primitive.stairs.run = depth;
    }
}

bool UpdatePreviewValue(
        SectorEditorToolContext& context,
        StructuralPrimitiveManipulationState& drag)
{
    SectorAuthoringStructuralPrimitive candidate = drag.original;
    const Vector2 center = PrimitiveCenterMap(drag.original);
    const Vector2 snapped = context.state.snappedMouseMap;
    if (drag.handle == SectorEditorStructuralHandleKind::Move) {
        candidate.x += ToCoord(snapped.x - drag.pressMap.x);
        candidate.z += ToCoord(snapped.y - drag.pressMap.y);
    } else if (drag.handle == SectorEditorStructuralHandleKind::Rotate) {
        const Vector2 raw = context.screenToMap(context.input->MousePosition());
        float yaw = std::atan2(-(raw.x - center.x), raw.y - center.y)
                * 180.0f / PI;
        if (context.input->IsKeyDown(KEY_LEFT_SHIFT)
                || context.input->IsKeyDown(KEY_RIGHT_SHIFT)) {
            yaw = std::round(yaw / 15.0f) * 15.0f;
        }
        if (yaw < 0.0f) yaw += 360.0f;
        candidate.yawDegrees = yaw;
    } else if (drag.handle == SectorEditorStructuralHandleKind::Radius) {
        const SectorCoord radius = ToCoord(std::hypot(
                snapped.x - center.x, snapped.y - center.y));
        if (candidate.kind == SectorStructuralPrimitiveKind::Cylinder) {
            candidate.cylinder.radius = radius;
        } else {
            candidate.sphere.radius = radius;
        }
    } else {
        const int handle = static_cast<int>(drag.handle);
        const bool corner = handle >= static_cast<int>(SectorEditorStructuralHandleKind::Corner0)
                && handle <= static_cast<int>(SectorEditorStructuralHandleKind::Corner3);
        const int index = corner
                ? handle - static_cast<int>(SectorEditorStructuralHandleKind::Corner0)
                : handle - static_cast<int>(SectorEditorStructuralHandleKind::Edge0);
        const float radians = drag.original.yawDegrees * PI / 180.0f;
        const Vector2 axisX{std::cos(radians), std::sin(radians)};
        const Vector2 axisZ{-std::sin(radians), std::cos(radians)};
        const auto dot = [](Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; };
        const Vector2 relative{snapped.x - center.x, snapped.y - center.y};
        float localX = dot(relative, axisX);
        float localZ = dot(relative, axisZ);
        SectorCoord oldWidth = candidate.kind == SectorStructuralPrimitiveKind::Box
                ? candidate.box.width
                : candidate.kind == SectorStructuralPrimitiveKind::Ramp
                        ? candidate.ramp.width : candidate.stairs.width;
        SectorCoord oldDepth = candidate.kind == SectorStructuralPrimitiveKind::Box
                ? candidate.box.depth
                : candidate.kind == SectorStructuralPrimitiveKind::Ramp
                        ? candidate.ramp.run : candidate.stairs.run;
        const float halfWidth = SectorCoordToVisibleAuthoring(oldWidth) * 0.5f;
        const float halfDepth = SectorCoordToVisibleAuthoring(oldDepth) * 0.5f;
        float fixedX = 0.0f;
        float fixedZ = 0.0f;
        if (corner) {
            fixedX = (index == 0 || index == 3) ? halfWidth : -halfWidth;
            fixedZ = (index == 0 || index == 1) ? halfDepth : -halfDepth;
        } else if (index == 0 || index == 2) {
            fixedZ = index == 0 ? halfDepth : -halfDepth;
            localX = 0.0f;
        } else {
            fixedX = index == 1 ? -halfWidth : halfWidth;
            localZ = 0.0f;
        }
        const bool adjustWidth = corner || index == 1 || index == 3;
        const bool adjustDepth = corner || index == 0 || index == 2;
        const float centerLocalX = adjustWidth ? (localX + fixedX) * 0.5f : 0.0f;
        const float centerLocalZ = adjustDepth ? (localZ + fixedZ) * 0.5f : 0.0f;
        candidate.x = ToCoord(center.x + axisX.x * centerLocalX + axisZ.x * centerLocalZ);
        candidate.z = ToCoord(center.y + axisX.y * centerLocalX + axisZ.y * centerLocalZ);
        SetRectDimensions(candidate,
                adjustWidth ? ToCoord(std::fabs(localX - fixedX)) : oldWidth,
                adjustDepth ? ToCoord(std::fabs(localZ - fixedZ)) : oldDepth);
    }

    const std::vector<SectorStructuralDiagnostic> diagnostics =
            ValidateSectorAuthoringStructuralPrimitives({candidate});
    drag.preview = candidate;
    drag.valid = diagnostics.empty();
    drag.errorMessage = diagnostics.empty() ? std::string{} : diagnostics.front().message;
    context.statusText = drag.valid
            ? TextFormat("Adjusting structure %d", drag.primitiveId)
            : TextFormat("Structure adjustment rejected: %s", drag.errorMessage.c_str());
    return true;
}

} // namespace

bool BeginSectorEditorStructuralManipulation(
        SectorEditorToolContext& context,
        Vector2 screenPoint)
{
    if (context.structuralPrimitiveState == nullptr
            || context.structuralPrimitiveEditing == nullptr
            || !context.mapToScreen || !context.screenToMap) return false;
    const SectorAuthoringStructuralPrimitive* selected =
            context.structuralPrimitiveEditing->Selected();
    if (selected == nullptr) return false;
    SectorEditorStructuralHandleKind handle = HitStructuralHandle(
            context, *selected, screenPoint);
    if (handle == SectorEditorStructuralHandleKind::None
            && PointInsidePrimitive(*selected, context.screenToMap(screenPoint))) {
        handle = SectorEditorStructuralHandleKind::Move;
    }
    if (handle == SectorEditorStructuralHandleKind::None) return false;
    StructuralPrimitiveManipulationState& drag =
            context.structuralPrimitiveState->manipulation;
    drag = StructuralPrimitiveManipulationState{};
    drag.active = true;
    drag.primitiveId = selected->id;
    drag.handle = handle;
    drag.original = *selected;
    drag.preview = *selected;
    drag.pressMap = context.state.snappedMouseMap;
    drag.pressYawDegrees = selected->yawDegrees;
    drag.valid = true;
    context.statusText = TextFormat("Adjusting structure %d", selected->id);
    return true;
}

bool UpdateSectorEditorStructuralManipulation(SectorEditorToolContext& context)
{
    if (context.structuralPrimitiveState == nullptr || context.input == nullptr) return false;
    StructuralPrimitiveManipulationState& drag =
            context.structuralPrimitiveState->manipulation;
    if (!drag.active) return false;
    UpdatePreviewValue(context, drag);
    bool released = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseButtonReleased,
            true,
            [&context, &drag, &released](engine::InputEvent& event) {
                if (event.mouseButton.button != MOUSE_LEFT_BUTTON) return;
                const StructuralPrimitiveManipulationState completed = drag;
                drag = StructuralPrimitiveManipulationState{};
                if (completed.valid) {
                    if (!context.structuralPrimitiveEditing->CommitPreviewValue(
                                completed.primitiveId,
                                completed.preview,
                                TextFormat("Adjusted structure %d", completed.primitiveId))) {
                        context.statusText = "Structure unchanged";
                    }
                } else {
                    context.statusText = completed.errorMessage.empty()
                            ? "Structure adjustment rejected"
                            : completed.errorMessage;
                }
                engine::ConsumeEvent(event);
                released = true;
            });
    return released;
}

bool CancelSectorEditorStructuralManipulation(
        SectorEditorToolContext& context,
        const char* message)
{
    if (context.structuralPrimitiveState == nullptr
            || !context.structuralPrimitiveState->manipulation.active) return false;
    context.structuralPrimitiveState->manipulation =
            StructuralPrimitiveManipulationState{};
    if (message != nullptr) context.statusText = message;
    return true;
}

void DrawSectorEditorStructuralSelectionOverlay(SectorEditorToolContext& context)
{
    if (context.currentTool != SectorEditorTool::Select
            || context.structuralPrimitiveEditing == nullptr
            || context.structuralPrimitiveState == nullptr
            || !context.mapToScreen) return;
    const StructuralPrimitiveManipulationState& drag =
            context.structuralPrimitiveState->manipulation;
    const SectorAuthoringStructuralPrimitive* selected = drag.active
            ? &drag.preview : context.structuralPrimitiveEditing->Selected();
    if (selected == nullptr) return;
    const Color color = drag.active && !drag.valid ? RED : YELLOW;
    DrawFootprint(context, *selected, color);
    const std::vector<Vector2> points = FootprintMap(*selected);
    if (!SectorStructuralPrimitiveHasTilt(*selected)
            && (selected->kind == SectorStructuralPrimitiveKind::Cylinder
                    || selected->kind == SectorStructuralPrimitiveKind::Sphere)) {
        const float radius = selected->kind == SectorStructuralPrimitiveKind::Cylinder
                ? SectorCoordToVisibleAuthoring(selected->cylinder.radius)
                : SectorCoordToVisibleAuthoring(selected->sphere.radius);
        const Vector2 center = PrimitiveCenterMap(*selected);
        for (Vector2 direction : std::array<Vector2, 4>{{
                    {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}}}) {
            DrawCircleV(context.mapToScreen(Vector2{
                    center.x + direction.x * radius,
                    center.y + direction.y * radius}), HandleRadiusPixels, color);
        }
    } else if (!SectorStructuralPrimitiveHasTilt(*selected)
            && points.size() == 4) {
        for (size_t index = 0; index < 4; ++index) {
            DrawCircleV(context.mapToScreen(points[index]), HandleRadiusPixels, color);
            const Vector2 midpoint{
                    (points[index].x + points[(index + 1) % 4].x) * 0.5f,
                    (points[index].y + points[(index + 1) % 4].y) * 0.5f};
            DrawCircleV(context.mapToScreen(midpoint), HandleRadiusPixels - 1.0f, color);
        }
    }
    const Vector2 centerScreen = context.mapToScreen(PrimitiveCenterMap(*selected));
    const Vector2 rotation = RotationHandleScreen(context, *selected);
    DrawLineEx(centerScreen, rotation, 1.5f, color);
    DrawCircleV(rotation, HandleRadiusPixels, color);
    if (drag.active) {
        const char* feedback = drag.handle == SectorEditorStructuralHandleKind::Rotate
                ? TextFormat("yaw %.1f", selected->yawDegrees)
                : selected->kind == SectorStructuralPrimitiveKind::Cylinder
                        ? TextFormat("radius %.2f", SectorCoordToVisibleAuthoring(
                                selected->cylinder.radius))
                        : selected->kind == SectorStructuralPrimitiveKind::Sphere
                                ? TextFormat("radius %.2f", SectorCoordToVisibleAuthoring(
                                        selected->sphere.radius))
                                : selected->kind == SectorStructuralPrimitiveKind::Box
                                        ? TextFormat("%.2f x %.2f",
                                                SectorCoordToVisibleAuthoring(selected->box.width),
                                                SectorCoordToVisibleAuthoring(selected->box.depth))
                                        : selected->kind == SectorStructuralPrimitiveKind::Ramp
                                                ? TextFormat("%.2f x %.2f",
                                                        SectorCoordToVisibleAuthoring(selected->ramp.width),
                                                        SectorCoordToVisibleAuthoring(selected->ramp.run))
                                                : TextFormat("%.2f x %.2f",
                                                        SectorCoordToVisibleAuthoring(selected->stairs.width),
                                                        SectorCoordToVisibleAuthoring(selected->stairs.run));
        DrawText(feedback, static_cast<int>(rotation.x + 10.0f),
                static_cast<int>(rotation.y - 10.0f), 16, color);
    }
}

const SectorEditorToolModule& SectorEditorStructureToolModule()
{
    return StructureModule;
}

} // namespace game
