#include "sector_editor/inspector/SectorEditorParticleEmitterInspector.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorAssetPaths.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {
std::string PresetPath() { return ResolveSectorAssetPath("assets/particles/presets.json"); }
}
void LoadParticleInspectorPresets(ParticleEmitterEditingUiState& ui)
{
    if (!LoadSectorParticlePresets(PresetPath(), ui.presets, ui.message)) {
        ui.presets.clear();
        for (int i = 0; i < 6; ++i) ui.presets.push_back(MakeSectorParticlePreset(static_cast<SectorParticlePreset>(i)));
    }
    ui.presetNames.clear(); ui.presetNames.reserve(ui.presets.size());
    for (const auto& p : ui.presets) ui.presetNames.push_back(p.presetName);
}
void SectorEditorParticlePreview::Shutdown(engine::AssetManager& assets)
{
    renderer.Shutdown();
    if (target.native.id) engine::UnloadRenderTarget(target);
    if (!engine::IsNull(scope)) assets.UnloadScope(scope);
    scope = {}; emitterId = -1; revision = 0; emitters.clear();
}
void SectorEditorParticlePreview::Synchronize(engine::AssetManager& assets,
        const SectorAuthoringParticleEmitter* emitter, uint64_t newRevision, float dt)
{
    if (!emitter) { if (emitterId != -1) Shutdown(assets); return; }
    if (emitter->id != emitterId || revision != newRevision) {
        emitterId = emitter->id; revision = newRevision;
        scale = emitter->settings.scale;
        SectorCompiledParticleEmitter e;
        e.sourceAuthoringEmitterId = emitter->id; e.id = emitter->referenceId;
        e.sectorId = 1; e.enabled = true; e.settings = emitter->settings;
        e.yawDegrees = emitter->yawDegrees; e.pitchDegrees = emitter->pitchDegrees;
        e.positionWorld = {0, 0.1f, 0};
        emitters.assign(1,e);
        if (!renderer.Reconfigure(emitters)) {
            renderer.Shutdown();
            if (!engine::IsNull(scope)) assets.UnloadScope(scope);
            scope = assets.CreateScope("particle_inspector");
            renderer.Initialize(assets, scope, emitters, 1024);
        }
        engine::RenderTargetDescriptor descriptor;
        descriptor.debugName = "particle_inspector"; descriptor.width = 384; descriptor.height = 256;
        descriptor.filter = engine::RenderTargetFilter::Bilinear;
        descriptor.wrap = engine::RenderTargetWrap::Clamp;
        if (!target.native.id) engine::LoadRenderTarget(descriptor, target);
    }
    if (!target.native.id) return;
    const float distance = std::max(1.0f, scale * 4) * distanceScale;
    Camera3D camera{{std::sin(yaw) * distance, distance * 0.5f + scale, std::cos(yaw) * distance},
            {0, scale, 0}, {0,1,0}, 45, CAMERA_PERSPECTIVE};
    if (playing) renderer.Update(dt, emitters, nullptr, camera, true, true);
    BeginTextureMode(target.native); ClearBackground({31,34,39,255}); BeginMode3D(camera);
    DrawPlane({0,0,0}, {20 * scale, 20 * scale}, {46,49,54,255});
    DrawGrid(10, std::max(0.1f, scale)); EndMode3D(); EndTextureMode();
    renderer.Draw(assets, target.native, nullptr, camera, nullptr, nullptr, nullptr, nullptr, nullptr, true);
}
void SectorEditorParticlePreview::Trigger()
{
    if (!emitters.empty()) { emitters[0].pendingBurst = 1; playing = true; }
}
bool DrawSectorEditorParticleEmitterInspector(engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets, engine::FontHandle font,
        float width, float rowHeight, float gap, const SectorAuthoringParticleEmitter& source,
        ParticleEmitterEditingUiState& state, SectorEditorParticleEmitterEditingService& editing,
        SectorEditorParticlePreview& preview)
{
    SectorAuthoringParticleEmitter e = source;
    if (state.bufferedEmitterId != e.id) {
        state.bufferedEmitterId = e.id; state.numbers = {};
        std::snprintf(state.name, sizeof(state.name), "%s", e.referenceId.c_str());
        std::snprintf(state.presetName, sizeof(state.presetName), "%s", e.settings.presetName.c_str());
        state.presetIndex = 0;
        for (size_t i = 0; i < state.presets.size(); ++i) if (state.presets[i].presetName == e.settings.presetName) state.presetIndex = static_cast<int>(i);
        for (int i = 0; i < 4; ++i) std::snprintf(state.texturePaths[i].data(), 512, "%s", e.settings.textures[i].path.c_str());
    }
    float y = 0; bool changed = false, remove = false;
    const auto text = [&](const char* label, float yy, float height) {
        engine::Text(ui, config, assets, {0,yy,width,height}, font, label,
                engine::UITextJustify::Left, config.textColor, true);
    };
    const auto button = [&](const char* id, const char* label, float yy, float x = 0, float w = -1) {
        return engine::Button(ui, config, input, assets, id, {x,yy,w < 0 ? width : w,rowHeight}, font, label);
    };
    // Snapshot conditions avoid changing the row sequence midway through a draw.
    WalkSectorParticleInspectorRows(source, state.advanced, state.textureAdvanced, !state.message.empty(), [&](SectorParticleInspectorRow row) {
        char id[64]; std::snprintf(id, sizeof(id), "particle_field_%d", static_cast<int>(row));
        float fieldY = y;
        const auto label = [&](const char* value) { text(value, y, SectorParticleInspectorLabelHeight(rowHeight, row)); fieldY = y + SectorParticleInspectorLabelHeight(rowHeight, row) + gap; };
        const auto number = [&](const char* name, float& value, float lo, float hi, int decimals = 2) {
            label(name);
            auto& buffer = state.numbers[static_cast<size_t>(row - ParticleInspectorPositionX)];
            const auto result = engine::FloatInput(ui, config, input, assets, id,
                    {0,fieldY,width,rowHeight}, font, value, buffer, lo, hi, decimals);
            if (result.changed && std::isfinite(value)) changed = true;
        };
        const auto option = [&](const char* name, const char* const* values, size_t count, int& selected) {
            label(name); return engine::Option(ui, config, input, assets, id,
                    {0,fieldY,width,rowHeight}, font, values, count, selected);
        };
        switch (row) {
            case ParticleInspectorTitle: text("Particle Emitter", y, rowHeight); break;
            case ParticleInspectorPreview: {
                Rectangle bounds{0,y,width,180};
                if (ui.inScrollArea) { bounds.x += ui.scrollViewport.x - ui.scrollOffset.x; bounds.y += ui.scrollViewport.y - ui.scrollOffset.y; }
                const auto texture = preview.Texture();
                if (texture.id) DrawTexturePro(texture, {0,0,static_cast<float>(texture.width),-static_cast<float>(texture.height)}, bounds, {}, 0, WHITE);
                break;
            }
            case ParticleInspectorPlayback:
                if (button("particle_play", preview.playing ? "Pause preview" : "Play preview", y)) preview.playing = !preview.playing;
                if (button("particle_restart", "Restart preview", y + rowHeight + gap)) preview.Restart();
                if (button("particle_trigger", "Trigger preview burst", y + 2 * (rowHeight + gap))) preview.Trigger();
                break;
            case ParticleInspectorViewControls: {
                const float half = (width - gap) * 0.5f;
                if (button("particle_orbit_left", "Orbit -", y, 0, half)) preview.yaw -= 0.3f;
                if (button("particle_orbit_right", "Orbit +", y, half + gap, half)) preview.yaw += 0.3f;
                if (button("particle_zoom_in", "Zoom +", y + rowHeight + gap, 0, half)) preview.distanceScale = std::max(0.2f, preview.distanceScale * 0.8f);
                if (button("particle_zoom_out", "Zoom -", y + rowHeight + gap, half + gap, half)) preview.distanceScale = std::min(5.0f, preview.distanceScale * 1.25f);
                break;
            }
            case ParticleInspectorPreviewStats: {
                char stats[96]; const auto& d = preview.Diagnostics();
                std::snprintf(stats, sizeof(stats), "%zu live / %zu draws / %.2f ms", d.active, d.drawCalls, d.updateMilliseconds);
                text(stats, y, rowHeight); break;
            }
            case ParticleInspectorName: {
                label("Script name");
                auto r = engine::TextInput(ui, config, input, assets, id, {0,fieldY,width,rowHeight}, font,
                        state.name, sizeof(state.name), 0, 63, engine::UITextJustify::Left);
                if (r.submitted || r.focusLost) { e.referenceId = state.name; changed |= e.referenceId != source.referenceId; }
                break;
            }
            case ParticleInspectorEnabled: changed |= engine::Checkbox(ui, config, input, assets, id, {0,y,width,rowHeight}, font, "Enabled at start", e.enabled); break;
            case ParticleInspectorPreset:
                label("Preset");
                if (engine::Option(ui, config, input, assets, id, {0,fieldY,width,rowHeight}, font, state.presetNames, state.presetIndex)
                        && state.presetIndex >= 0 && static_cast<size_t>(state.presetIndex) < state.presets.size()) {
                    e.settings = state.presets[state.presetIndex]; changed = true; state.bufferedEmitterId = -1;
                } break;
            case ParticleInspectorReapply:
                if (button(id, "Reapply selected preset", y) && state.presetIndex >= 0 && static_cast<size_t>(state.presetIndex) < state.presets.size()) {
                    e.settings = state.presets[state.presetIndex]; changed = true; state.bufferedEmitterId = -1;
                } break;
            case ParticleInspectorPresetName:
                label("Custom preset name"); engine::TextInput(ui, config, input, assets, id, {0,fieldY,width,rowHeight}, font,
                        state.presetName, sizeof(state.presetName), 0, 63, engine::UITextJustify::Left); break;
            case ParticleInspectorSavePreset:
                if (button(id, "Save custom preset", y)) {
                    auto preset = e.settings; preset.presetName = state.presetName;
                    if (SaveSectorParticlePreset(PresetPath(), preset, state.message)) {
                        LoadParticleInspectorPresets(state); e.settings.presetName = preset.presetName; changed = true;
                        state.bufferedEmitterId = -1;
                        state.message = "Saved preset; existing placements keep their settings.";
                    }
                } break;
            case ParticleInspectorPositionX: { float n = SectorCoordToVisibleAuthoring(e.x); number("X (map units)",n,-8192,8192,3); VisibleAuthoringToSectorCoord(n,e.x); break; }
            case ParticleInspectorPositionZ: { float n = SectorCoordToVisibleAuthoring(e.z); number("Z (map units)",n,-8192,8192,3); VisibleAuthoringToSectorCoord(n,e.z); break; }
            case ParticleInspectorHeight: number("Height above floor (m)",e.heightWorld,-100,100); break;
            case ParticleInspectorYaw: number("Yaw (degrees)",e.yawDegrees,-360,360); break;
            case ParticleInspectorPitch: number("Pitch (90 = up)",e.pitchDegrees,-90,90); break;
            case ParticleInspectorScale: number("Scale",e.settings.scale,0.01f,100); break;
            case ParticleInspectorIntensity: number("Intensity",e.settings.intensity,0,100); break;
            case ParticleInspectorTimeScale: number("Playback speed",e.settings.timeScale,0,4); break;
            case ParticleInspectorSpeed: number("Initial speed / jet strength",e.settings.speed,0,100); break;
            case ParticleInspectorLifetime: number("Lifetime multiplier",e.settings.lifetime,0.01f,20); break;
            case ParticleInspectorSpread: number("Spread",e.settings.spread,0,10); break;
            case ParticleInspectorTintR: case ParticleInspectorTintG: case ParticleInspectorTintB: case ParticleInspectorTintA: {
                unsigned char* values[]{&e.settings.tint.r,&e.settings.tint.g,&e.settings.tint.b,&e.settings.tint.a};
                const char* names[]{"Tint red","Tint green","Tint blue","Opacity"};
                const int i = row - ParticleInspectorTintR; float n = *values[i]; number(names[i],n,0,255,0); *values[i] = static_cast<unsigned char>(n); break;
            }
            case ParticleInspectorSmoke: number("Smoke amount",e.settings.smokeAmount,0,10); break;
            case ParticleInspectorEmbers: number("Ember amount",e.settings.emberAmount,0,10); break;
            case ParticleInspectorSwirl: number("Swirl strength",e.settings.swirl,-20,20); break;
            case ParticleInspectorBurstInterval: number("Burst interval (seconds)",e.settings.burstInterval,0.02f,3600); break;
            case ParticleInspectorBurstCount: number("Burst count",e.settings.burstCount,1,1024,0); break;
            case ParticleInspectorAdvanced: engine::Checkbox(ui, config, input, assets, id, {0,y,width,rowHeight}, font, "Advanced settings", state.advanced); break;
            case ParticleInspectorShape: { const char* names[]{"Point","Disc","Box"}; int n = static_cast<int>(e.settings.shape);
                if (option("Source shape",names,3,n)) { e.settings.shape = static_cast<engine::ParticleShape>(n); changed = true; } break; }
            case ParticleInspectorWidth: number("Source width (m)",e.settings.dimensions.x,0,100); break;
            case ParticleInspectorDepth: number("Source depth (m)",e.settings.dimensions.z,0,100); break;
            case ParticleInspectorBoxHeight: number("Source height (m)",e.settings.dimensions.y,0,100); break;
            case ParticleInspectorDriftX: number("Drift X (m/s)",e.settings.drift.x,-100,100); break;
            case ParticleInspectorDriftY: number("Drift Y (m/s)",e.settings.drift.y,-100,100); break;
            case ParticleInspectorDriftZ: number("Drift Z (m/s)",e.settings.drift.z,-100,100); break;
            case ParticleInspectorTurbulence: number("Turbulence",e.settings.turbulence,0,20); break;
            case ParticleInspectorGravity: number("Gravity (m/s squared)",e.settings.gravity,-100,100); break;
            case ParticleInspectorDrag: number("Drag",e.settings.drag,0,20); break;
            case ParticleInspectorEmission: { const char* names[]{"Continuous","Repeating burst","Triggered burst"}; int n = static_cast<int>(e.settings.emission);
                if (option("Emission",names,3,n)) { e.settings.emission = static_cast<engine::ParticleEmission>(n); changed = true; } break; }
            case ParticleInspectorCollision: { const char* names[]{"None","Die on contact","Bounce"}; int n = static_cast<int>(e.settings.collision);
                if (option("Static-world collision",names,3,n)) { e.settings.collision = static_cast<engine::ParticleCollision>(n); changed = true; } break; }
            case ParticleInspectorRestitution: number("Bounce restitution",e.settings.restitution,0,1); break;
            case ParticleInspectorDistance: number("Maximum distance (m)",e.settings.maxDistance,1,1000); break;
            case ParticleInspectorTextures: engine::Checkbox(ui, config, input, assets, id, {0,y,width,rowHeight}, font, "Texture overrides / flipbooks",state.textureAdvanced); break;
            case ParticleInspectorTextureRole: { const char* names[]{"Flame","Smoke","Vapor","Spark"}; option("Texture role",names,4,state.textureRole); break; }
            case ParticleInspectorTexturePath: {
                label("Texture path (empty = supplied)");
                auto& path = state.texturePaths[state.textureRole];
                auto r = engine::TextInput(ui, config, input, assets, id, {0,fieldY,width,rowHeight}, font,
                        path.data(),path.size(),0,path.size()-1,engine::UITextJustify::Left);
                if (r.submitted || r.focusLost) { auto& t = e.settings.textures[state.textureRole]; changed |= t.path != path.data(); t.path = path.data(); } break;
            }
            case ParticleInspectorColumns: case ParticleInspectorRows: case ParticleInspectorFrames: {
                auto& t = e.settings.textures[state.textureRole]; int* value = row == ParticleInspectorColumns ? &t.columns : row == ParticleInspectorRows ? &t.rows : &t.frames;
                float n = static_cast<float>(*value); number(row == ParticleInspectorColumns ? "Flipbook columns" : row == ParticleInspectorRows ? "Flipbook rows" : "Flipbook frame count",n,1,row == ParticleInspectorFrames ? 4096 : 64,0);
                *value = static_cast<int>(n); if (row != ParticleInspectorFrames) t.frames = std::min(t.frames,t.columns*t.rows); break;
            }
            case ParticleInspectorFps: number("Flipbook FPS",e.settings.textures[state.textureRole].fps,0,240); break;
            case ParticleInspectorRandomStart: changed |= engine::Checkbox(ui,config,input,assets,id,{0,y,width,rowHeight},font,"Random starting frame",e.settings.textures[state.textureRole].randomStart); break;
            case ParticleInspectorMessage: text(state.message.c_str(),y,72); break;
            case ParticleInspectorDuplicate: if (button(id,"Duplicate emitter",y)) { editing.DuplicateSelected(); changed = false; } break;
            case ParticleInspectorDelete: remove = button(id,"Delete emitter",y); break;
        }
        y += SectorParticleInspectorRowExtent(row,rowHeight,gap);
    });
    if (changed && !remove) {
        std::string error;
        if (!editing.ValidateSelectedReferenceId(e.referenceId,error) || !ValidateSectorParticleSettings(e.settings,error)) state.message = error;
        else if (editing.Apply(e)) state.message.clear();
    }
    return remove;
}
} // namespace game
