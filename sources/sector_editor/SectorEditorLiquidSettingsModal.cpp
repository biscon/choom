#include "sector_editor/SectorEditorLiquidSettingsModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>

namespace game {
namespace {

enum class LiquidPreset {
    ClearWater,
    DirtyWater,
    Sewage,
    GreenSludge
};

void ApplyPreset(SectorLiquidSettings& liquid, LiquidPreset preset)
{
    const bool enabled = liquid.enabled;
    const SectorLiquidSurfaceReference reference = liquid.surfaceReference;
    const float offset = liquid.surfaceOffset;
    const float direction = liquid.flowDirectionDegrees;
    const float flowSpeed = liquid.flowSpeedWorld;
    liquid = SectorLiquidSettings{};
    liquid.enabled = enabled;
    liquid.surfaceReference = reference;
    liquid.surfaceOffset = offset;
    liquid.flowDirectionDegrees = direction;
    liquid.flowSpeedWorld = flowSpeed;
    switch (preset) {
        case LiquidPreset::ClearWater:
            break;
        case LiquidPreset::DirtyWater:
            liquid.shallowColor = Color{112, 110, 73, 255};
            liquid.deepColor = Color{35, 31, 18, 255};
            liquid.visibilityDepthWorld = 1.8f;
            liquid.roughness = 0.2f;
            liquid.refractionStrength = 0.018f;
            break;
        case LiquidPreset::Sewage:
            liquid.shallowColor = Color{87, 112, 62, 255};
            liquid.deepColor = Color{22, 34, 14, 255};
            liquid.visibilityDepthWorld = 0.9f;
            liquid.roughness = 0.26f;
            liquid.refractionStrength = 0.012f;
            liquid.rippleScaleWorld = 0.7f;
            break;
        case LiquidPreset::GreenSludge:
            liquid.shallowColor = Color{64, 151, 58, 255};
            liquid.deepColor = Color{13, 47, 12, 255};
            liquid.visibilityDepthWorld = 0.4f;
            liquid.roughness = 0.38f;
            liquid.refractionStrength = 0.006f;
            liquid.rippleScaleWorld = 1.4f;
            liquid.rippleStrength = 0.08f;
            liquid.rippleSpeed = 0.15f;
            break;
    }
}

void ResetInputs(SectorEditorLiquidSettingsModalState& state)
{
    state.floatInputs = {};
    state.colorInputs = {};
    state.errorMessage.clear();
}

} // namespace

void OpenSectorEditorLiquidSettingsModal(
        SectorEditorLiquidSettingsModalState& state,
        const SectorAuthoringFaceAnchor& anchor)
{
    state = SectorEditorLiquidSettingsModalState{};
    state.open = true;
    state.faceAnchorId = anchor.id;
    state.draft = anchor.liquid;
    if (!state.draft.enabled) {
        state.draft.enabled = true;
        SectorLiquidSettings disabledDraft = state.draft;
        disabledDraft.enabled = false;
        if (IsDefaultSectorLiquidSettings(disabledDraft)) {
            state.draft.surfaceOffset = std::max(
                    0.0f, (anchor.ceilingZ - anchor.floorZ) * 0.5f);
        }
    }
    state.draft = NormalizeSectorLiquidSettingsForSpan(
            state.draft, anchor.floorZ, anchor.ceilingZ);
}

SectorEditorLiquidSettingsModalAction DrawSectorEditorLiquidSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorLiquidSettingsModalState& state)
{
    if (!state.open) return SectorEditorLiquidSettingsModalAction::None;
    bool cancelRequested = false;
    input.ForEachEvent(engine::InputEventType::KeyPressed, true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                cancelRequested = true;
                engine::ConsumeEvent(event);
            });

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight),
            Color{0, 0, 0, 155});
    const Rectangle modal{
            (EditorWidth - 940.0f) * 0.5f,
            (EditorHeight - 790.0f) * 0.5f,
            940.0f,
            790.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 252});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    constexpr float padding = 28.0f;
    constexpr float rowH = 36.0f;
    constexpr float gap = 8.0f;
    engine::Text(config, assets,
            Rectangle{modal.x + padding, modal.y + 18.0f, modal.width - padding * 2.0f, 38.0f},
            font, "Liquid Settings");
    float y = modal.y + 66.0f;

    const char* presetLabels[] = {"Clear Water", "Dirty Water", "Sewage", "Green Sludge"};
    for (int i = 0; i < 4; ++i) {
        const float buttonW = (modal.width - padding * 2.0f - gap * 3.0f) * 0.25f;
        if (engine::Button(ui, config, input, assets,
                    TextFormat("sector_editor_liquid_preset_%d", i),
                    Rectangle{modal.x + padding + i * (buttonW + gap), y, buttonW, rowH},
                    smallFont, presetLabels[i])) {
            ApplyPreset(state.draft, static_cast<LiquidPreset>(i));
            ResetInputs(state);
        }
    }
    y += rowH + 14.0f;

    const float innerW = modal.width - padding * 2.0f;
    const float columnGap = 28.0f;
    const float columnW = (innerW - columnGap) * 0.5f;
    const float leftX = modal.x + padding;
    const float rightX = leftX + columnW + columnGap;
    const float labelW = 176.0f;

    engine::Text(config, assets, Rectangle{leftX, y, columnW, 30.0f}, smallFont, "Volume and motion");
    engine::Text(config, assets, Rectangle{rightX, y, columnW, 30.0f}, smallFont, "Appearance");
    y += 32.0f;
    float leftY = y;
    float rightY = y;

    auto drawFloat = [&](float x, float& target, int inputIndex, const char* id,
                             const char* label, float minimum, float maximum, int decimals,
                             float& rowY) {
        engine::Text(config, assets, Rectangle{x, rowY, labelW, rowH}, smallFont, label);
        engine::FloatInput(ui, config, input, assets, id,
                Rectangle{x + labelW, rowY, columnW - labelW, rowH}, smallFont,
                target, state.floatInputs[static_cast<size_t>(inputIndex)],
                minimum, maximum, decimals);
        rowY += rowH + gap;
    };

    engine::Text(config, assets, Rectangle{leftX, leftY, labelW, rowH}, smallFont, "Surface reference");
    const char* referenceOptions[] = {"Floor", "Ceiling"};
    int referenceIndex = state.draft.surfaceReference == SectorLiquidSurfaceReference::Ceiling ? 1 : 0;
    if (engine::Option(ui, config, input, assets, "sector_editor_liquid_reference",
                Rectangle{leftX + labelW, leftY, columnW - labelW, rowH}, smallFont,
                referenceOptions, 2, referenceIndex)) {
        state.draft.surfaceReference = referenceIndex == 1
                ? SectorLiquidSurfaceReference::Ceiling
                : SectorLiquidSurfaceReference::Floor;
    }
    leftY += rowH + gap;
    drawFloat(leftX, state.draft.surfaceOffset, 0, "sector_editor_liquid_surface_offset",
            "Surface offset (authored)", 0.0f, 1024.0f, 2, leftY);
    drawFloat(leftX, state.draft.rippleScaleWorld, 1, "sector_editor_liquid_ripple_scale",
            "Ripple scale (m)", SectorLiquidMinRippleScaleWorld,
            SectorLiquidMaxRippleScaleWorld, 2, leftY);
    drawFloat(leftX, state.draft.rippleStrength, 2, "sector_editor_liquid_ripple_strength",
            "Ripple strength", 0.0f, SectorLiquidMaxRippleStrength, 2, leftY);
    drawFloat(leftX, state.draft.rippleSpeed, 3, "sector_editor_liquid_ripple_speed",
            "Ripple speed", 0.0f, SectorLiquidMaxRippleSpeed, 2, leftY);
    drawFloat(leftX, state.draft.flowDirectionDegrees, 4, "sector_editor_liquid_flow_direction",
            "Flow direction (deg)", 0.0f, 360.0f, 1, leftY);
    drawFloat(leftX, state.draft.flowSpeedWorld, 5, "sector_editor_liquid_flow_speed",
            "Flow speed (m/s)", 0.0f, SectorLiquidMaxFlowSpeedWorld, 2, leftY);

    auto drawColor = [&](const char* id, const char* label, Color& color, int stateOffset) {
        engine::Text(config, assets, Rectangle{rightX, rightY, 112.0f, rowH}, smallFont, label);
        DrawColorSwatch(config,
                Rectangle{rightX + 112.0f, rightY + 4.0f, 48.0f, rowH - 8.0f},
                color, 1.0f);
        constexpr float channelGap = 4.0f;
        const float channelsX = rightX + 168.0f;
        const float channelW = (columnW - 168.0f - channelGap * 2.0f) / 3.0f;
        unsigned char* channels[] = {&color.r, &color.g, &color.b};
        for (int channel = 0; channel < 3; ++channel) {
            int value = *channels[channel];
            const engine::UINumericInputResult result = engine::IntInput(
                    ui, config, input, assets,
                    TextFormat("%s_%d", id, channel),
                    Rectangle{channelsX + channel * (channelW + channelGap), rightY, channelW, rowH},
                    smallFont, value,
                    state.colorInputs[static_cast<size_t>(stateOffset + channel)],
                    0, 255, 1);
            if (result.changed) *channels[channel] = static_cast<unsigned char>(value);
        }
        color.a = 255;
        rightY += rowH + gap;
    };
    drawColor("sector_editor_liquid_shallow", "Shallow RGB", state.draft.shallowColor, 0);
    drawColor("sector_editor_liquid_deep", "Deep RGB", state.draft.deepColor, 3);
    drawFloat(rightX, state.draft.visibilityDepthWorld, 6, "sector_editor_liquid_visibility_depth",
            "Visibility depth (m)", SectorLiquidMinVisibilityDepthWorld,
            SectorLiquidMaxVisibilityDepthWorld, 2, rightY);
    drawFloat(rightX, state.draft.roughness, 7, "sector_editor_liquid_roughness",
            "Roughness", 0.0f, 1.0f, 2, rightY);
    drawFloat(rightX, state.draft.refractionStrength, 8, "sector_editor_liquid_refraction",
            "Refraction strength", 0.0f, SectorLiquidMaxRefractionStrength, 3, rightY);

    engine::Text(config, assets,
            Rectangle{leftX, modal.y + modal.height - 154.0f, innerW, 42.0f},
            smallFont,
            "Flow speed 0 keeps the surface gently animated without directional transport. "
            "The first slice renders the upper surface only.",
            engine::UITextJustify::Left, config.mutedTextColor, true);
    if (!state.errorMessage.empty()) {
        engine::Text(config, assets,
                Rectangle{leftX, modal.y + modal.height - 108.0f, innerW - 300.0f, 34.0f},
                smallFont, state.errorMessage.c_str(), engine::UITextJustify::Left,
                config.invalidColor, true);
    }

    constexpr float buttonW = 132.0f;
    const float buttonY = modal.y + modal.height - 58.0f;
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets,
            "sector_editor_liquid_cancel",
            Rectangle{modal.x + modal.width - padding - buttonW * 2.0f - gap,
                    buttonY, buttonW, 40.0f}, smallFont, "Cancel");
    const bool applyRequested = engine::Button(ui, config, input, assets,
            "sector_editor_liquid_apply",
            Rectangle{modal.x + modal.width - padding - buttonW,
                    buttonY, buttonW, 40.0f}, smallFont, "Apply");

    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
    if (cancelRequested) return SectorEditorLiquidSettingsModalAction::Cancel;
    return applyRequested
            ? SectorEditorLiquidSettingsModalAction::Apply
            : SectorEditorLiquidSettingsModalAction::None;
}

} // namespace game
