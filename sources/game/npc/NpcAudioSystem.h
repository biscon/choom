#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ecs/Entity.h"
#include "game/SoundSetAudio.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine {
class AssetManager;
class AudioSystem;
class World;
}

namespace game {

struct NpcDefinitionCatalog;

enum class NpcVocalEvent : uint8_t {
    None,
    Hurt,
    Death
};

enum class NpcVocalPlaybackKind : uint8_t {
    None,
    Ambient,
    Hurt,
    Death
};

struct NpcAudioRecord {
    engine::Entity entity = engine::NullEntity();
    engine::SoundHandle hurtSound = engine::NullSoundHandle();
    engine::SoundHandle deathSound = engine::NullSoundHandle();
    LoadedSoundSet ambientSounds;
    SoundSetPlaybackState ambientPlayback;
    engine::SoundPlaybackHandle vocalPlayback =
            engine::NullSoundPlaybackHandle();
    NpcVocalEvent pendingEvent = NpcVocalEvent::None;
    NpcVocalPlaybackKind playbackKind = NpcVocalPlaybackKind::None;
    float minimumAmbientDelaySeconds = 5.0f;
    float maximumAmbientDelaySeconds = 12.0f;
    float ambientDelayRemainingSeconds = 0.0f;
    uint32_t delayRandomState = 0x6d2b79f5u;
    bool ambientDisabled = false;
    bool occupied = false;
};

struct NpcAudioRuntime {
    std::vector<NpcAudioRecord> records;
};

void InitializeNpcAudioRuntime(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AssetScopeHandle assetScope,
        const NpcDefinitionCatalog& definitions,
        NpcAudioRuntime& runtime,
        size_t npcCapacity);
void ShutdownNpcAudioRuntime(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        NpcAudioRuntime& runtime);

bool QueueNpcVocalEvent(
        NpcAudioRuntime& runtime,
        engine::Entity entity,
        NpcVocalEvent event);
float SelectNpcAmbientDelay(NpcAudioRecord& record);
void UpdateNpcAudioSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        NpcAudioRuntime& runtime,
        float dt);

} // namespace game
