#pragma once
#include "sector_editor/inspector/SectorEditorParticleEmitterLayout.h"
#include "sector_editor/services/particle_emitters/SectorEditorParticleEmitterEditingService.h"
#include "sector_demo/renderer/SectorParticleRenderer.h"
#include "engine/render/RenderTarget.h"
#include "engine/ui/UI.h"
#include <array>

namespace game {
struct ParticleEmitterEditingUiState {
    int bufferedEmitterId = -1;
    bool advanced = false;
    bool textureAdvanced = false;
    int presetIndex = 0;
    int textureRole = 0;
    char name[64]{};
    char presetName[64]{};
    std::array<std::array<char, 512>, 4> texturePaths{};
    std::array<engine::UIFloatInputState, 48> numbers{};
    std::vector<SectorParticleSettings> presets;
    std::vector<std::string> presetNames;
    std::string message;
};
class SectorEditorParticlePreview {
public:
    void Synchronize(engine::AssetManager& assets, const SectorAuthoringParticleEmitter* emitter,
            uint64_t revision, float dt);
    void Shutdown(engine::AssetManager& assets);
    void Restart() { renderer.Restart(); }
    void Trigger();
    Texture2D Texture() const { return target.native.texture; }
    bool playing = true;
    float yaw = 0.5f;
    float distanceScale = 1.0f;
    const engine::ParticleDiagnostics& Diagnostics() const { return renderer.Diagnostics(); }
private:
    SectorParticleRenderer renderer;
    engine::RenderTarget target;
    engine::AssetScopeHandle scope{};
    std::vector<SectorCompiledParticleEmitter> emitters;
    uint64_t revision = 0;
    int emitterId = -1;
    float scale = 1;
};
void LoadParticleInspectorPresets(ParticleEmitterEditingUiState& ui);
inline float MeasureSectorEditorParticleEmitterInspectorContentHeight(const SectorAuthoringParticleEmitter& emitter,
        const ParticleEmitterEditingUiState& ui, float rowHeight, float gap)
{
    float y = 12;
    WalkSectorParticleInspectorRows(emitter, ui.advanced, ui.textureAdvanced, !ui.message.empty(), [&](SectorParticleInspectorRow row) { y += SectorParticleInspectorRowExtent(row, rowHeight, gap); });
    return y;
}
bool DrawSectorEditorParticleEmitterInspector(engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets, engine::FontHandle font,
        float width, float rowHeight, float gap, const SectorAuthoringParticleEmitter& emitter,
        ParticleEmitterEditingUiState& state, SectorEditorParticleEmitterEditingService& editing,
        SectorEditorParticlePreview& preview);
} // namespace game
