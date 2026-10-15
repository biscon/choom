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
    SetSectorCutsceneCaptionVoicesEnabled(runtime, voicesEnabled);
    const bool playerAudible = voicesEnabled && active && !caption.skippedForReading && caption.playerSpeaker;
    bool npcAudible = voicesEnabled && active && !caption.skippedForReading
            && world.IsAlive(speaker) && world.Has<NpcRuntimeInstance>(speaker)
            && world.Has<SectorObjectTransform>(speaker);
    if (npcAudible && world.Has<NpcCombatState>(speaker))
        npcAudible = !world.Get<NpcCombatState>(speaker).dead;
    const bool speaking = npcAudible && !caption.speechFinished;
    if (runtime.speechSpeaker != speaker || !speaking) {
        ClearSpeaker(world, runtime.speechSpeaker);
        runtime.speechSpeaker = engine::NullEntity();
    }
    Vector3 position{};
    if (npcAudible) {
        if (speaking) {
            world.Get<NpcRuntimeInstance>(speaker).dialogueSpeaking = true;
            runtime.speechSpeaker = speaker;
        }
        position = world.Get<SectorObjectTransform>(speaker).position;
        // A stable upper-body emitter avoids skeleton evaluation for audio.
        position.y += 1.35f;
    }
    engine::UpdateDialoguePlayback(assets, audio, runtime.speechPlayback,
            caption.speechTimeline, caption.token,
            active && !caption.skippedForReading,
            npcAudible || playerAudible, position, dt, !caption.playerSpeaker);
    if (active && caption.skippedForReading) {
        // The regular caption update owns the skipped reading hold.
        // Audio release must not hold up its clock or advance it twice.
        caption.speechDriven = audio.IsPaused();
        return;
    }
    if (active) {
        // Playback's logical clock also owns the reading hold and fade. Audio
        // availability and natural vowel tails never alter this position.
        caption.speechDriven = true;
        caption.elapsedSeconds = runtime.speechPlayback.sequence.position;
        caption.speechFinished = runtime.speechPlayback.sequence.finished;
        caption.visibleByteCount = engine::AdvanceDialogueReveal(caption.speechTimeline, caption.elapsedSeconds);
    }
}
} // namespace game
