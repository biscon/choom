#include "game/cutscene/SectorCutsceneRuntime.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {
void ClearSpeaker(engine::World& world, engine::Entity entity)
{
    if (world.IsAlive(entity) && world.Has<NpcRuntimeInstance>(entity))
        world.Get<NpcRuntimeInstance>(entity).dialogueSpeaking = false;
}
}

void StopSectorCutsceneSpeech(SectorCutsceneRuntime& runtime, engine::World& world,
        engine::AssetManager& assets, engine::AudioSystem& audio)
{
    ClearSpeaker(world, runtime.speechSpeaker);
    runtime.speechSpeaker = engine::NullEntity();
    engine::StopDialoguePlayback(assets, audio, runtime.speechPlayback);
}

void UpdateSectorCutsceneSpeech(SectorCutsceneRuntime& runtime, engine::World& world,
        engine::AssetManager& assets, engine::AudioSystem& audio, float dt, bool voicesEnabled)
{
    auto& caption = runtime.caption;
    const engine::Entity speaker = caption.speaker;
    const bool active = caption.active && caption.kind == SectorCutsceneCaptionKind::Say;
    SetSectorCutsceneCaptionVoiceTiming(runtime, voicesEnabled);
    bool speaking = voicesEnabled && active && !caption.speechFinished
            && world.IsAlive(speaker) && world.Has<NpcRuntimeInstance>(speaker)
            && world.Has<SectorObjectTransform>(speaker);
    if (speaking && world.Has<NpcCombatState>(speaker))
        speaking = !world.Get<NpcCombatState>(speaker).dead;
    if (runtime.speechSpeaker != speaker || !speaking) {
        ClearSpeaker(world, runtime.speechSpeaker);
        runtime.speechSpeaker = engine::NullEntity();
    }
    Vector3 position{};
    if (speaking) {
        world.Get<NpcRuntimeInstance>(speaker).dialogueSpeaking = true;
        runtime.speechSpeaker = speaker;
        position = world.Get<SectorObjectTransform>(speaker).position;
        // A stable upper-body emitter avoids skeleton evaluation for audio.
        position.y += 1.35f;
    }
    const auto* started = engine::UpdateDialoguePlayback(assets, audio, runtime.speechPlayback,
            caption.speechTimeline, caption.token, active && caption.voiceTiming, speaking, position, dt);
    if (started && speaking)
        engine::CommitDialogueSelection(world.Get<NpcRuntimeInstance>(speaker).dialogueHistory, *started);
    if (active && !caption.voiceTiming) {
        // The regular caption update owns the steady typewriter clock. Audio
        // release and speech punctuation must not hold up silent text.
        caption.speechDriven = audio.IsPaused();
        return;
    }
    if (active && caption.speechFinished && !audio.IsPaused()) {
        caption.elapsedSeconds += std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    } else if (active && !caption.speechFinished) {
        caption.speechDriven = true;
        caption.elapsedSeconds = runtime.speechPlayback.sequence.position;
        caption.speechFinished = runtime.speechPlayback.sequence.finished;
        caption.visibleByteCount = engine::AdvanceDialogueReveal(caption.speechTimeline, caption.elapsedSeconds);
    }
}
} // namespace game
