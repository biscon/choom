#include "game/npc/NpcAudioSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/ecs/World.h"
#include "game/npc/NpcDefinitions.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace game {
namespace {

constexpr float NpcVocalVolume = 1.0f;
constexpr float NpcVocalMinimumDistanceWorld = 1.0f;
constexpr float NpcVocalMaximumDistanceWorld = 25.0f;

uint32_t NextRandom(uint32_t& state)
{
    if (state == 0) state = 0x6d2b79f5u;
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

uint32_t HashNpcSeed(
        const NpcRuntimeInstance& npc,
        engine::Entity entity)
{
    uint32_t hash = 2166136261u;
    const auto append = [&hash](const std::string& text) {
        for (const unsigned char value : text) {
            hash ^= value;
            hash *= 16777619u;
        }
    };
    append(npc.definitionId);
    append(npc.instanceId);
    hash ^= entity.index;
    hash *= 16777619u;
    return hash != 0 ? hash : 0x6d2b79f5u;
}

NpcAudioRecord* FindRecord(
        NpcAudioRuntime& runtime,
        engine::Entity entity)
{
    for (NpcAudioRecord& record : runtime.records) {
        if (record.occupied && record.entity == entity) return &record;
    }
    return nullptr;
}

engine::SoundHandle RequestNpcSound(
        engine::AssetManager& assets,
        engine::AssetScopeHandle assetScope,
        const std::string& relativePath)
{
    if (relativePath.empty()) return engine::NullSoundHandle();
    const std::string path = ResolveSectorAudioAssetPath(relativePath);
    return assets.RequestSound(assetScope, path.c_str());
}

engine::PositionalSoundSettings NpcVocalPosition(Vector3 position)
{
    engine::PositionalSoundSettings positional;
    positional.position = position;
    positional.minimumDistanceWorld = NpcVocalMinimumDistanceWorld;
    positional.maximumDistanceWorld = NpcVocalMaximumDistanceWorld;
    return positional;
}

void StopVocalPlayback(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        NpcAudioRecord& record)
{
    if (!engine::IsNull(record.vocalPlayback)) {
        audio.StopSound(assets, record.vocalPlayback);
    }
    record.vocalPlayback = engine::NullSoundPlaybackHandle();
    record.playbackKind = NpcVocalPlaybackKind::None;
}

engine::SoundPlaybackHandle PlayActionSound(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        engine::SoundHandle sound,
        Vector3 position)
{
    if (engine::IsNull(sound)) return engine::NullSoundPlaybackHandle();
    return audio.PlaySoundAt(
            assets,
            sound,
            NpcVocalPosition(position),
            engine::SoundPlaybackSettings{NpcVocalVolume, 1.0f, 0.0f});
}

void ScheduleAmbientDelay(NpcAudioRecord& record)
{
    record.ambientDelayRemainingSeconds = SelectNpcAmbientDelay(record);
}

} // namespace

void InitializeNpcAudioRuntime(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AssetScopeHandle assetScope,
        const NpcDefinitionCatalog& definitions,
        NpcAudioRuntime& runtime,
        size_t npcCapacity)
{
    runtime.records.clear();
    runtime.records.reserve(npcCapacity);
    if (engine::IsNull(assetScope)) return;

    world.ForEach<NpcRuntimeInstance>(
            [&](engine::Entity entity, NpcRuntimeInstance& npc) {
                const NpcDefinition* definition = FindNpcDefinition(
                        definitions, npc.definitionId);
                if (definition == nullptr) return;
                const NpcActionDefinition& hurt = GetNpcAction(
                        *definition, NpcAction::Hurt);
                const NpcActionDefinition& death = GetNpcAction(
                        *definition, NpcAction::Death);
                const bool hasAmbient = !definition->ambientVocalizations
                        .soundPaths.empty();
                if (hurt.soundPath.empty()
                        && death.soundPath.empty()
                        && definition->playerDetectedSoundPath.empty()
                        && !hasAmbient) {
                    return;
                }

                NpcAudioRecord record;
                record.entity = entity;
                record.occupied = true;
                record.delayRandomState = HashNpcSeed(npc, entity);
                record.ambientPlayback.randomState = record.delayRandomState;
                record.minimumAmbientDelaySeconds = definition
                        ->ambientVocalizations.minimumDelaySeconds;
                record.maximumAmbientDelaySeconds = definition
                        ->ambientVocalizations.maximumDelaySeconds;
                record.hurtSound = RequestNpcSound(
                        assets, assetScope, hurt.soundPath);
                record.deathSound = RequestNpcSound(
                        assets, assetScope, death.soundPath);
                record.playerDetectedSound = RequestNpcSound(
                        assets,
                        assetScope,
                        definition->playerDetectedSoundPath);
                record.ambientSounds.id = npc.definitionId + ":ambient";
                record.ambientSounds.sounds.reserve(
                        definition->ambientVocalizations.soundPaths.size());
                for (const std::string& path :
                        definition->ambientVocalizations.soundPaths) {
                    const engine::SoundHandle sound = RequestNpcSound(
                            assets, assetScope, path);
                    if (!engine::IsNull(sound)) {
                        record.ambientSounds.sounds.push_back(sound);
                    }
                }
                ReserveSoundSetPlaybackState(
                        record.ambientPlayback,
                        record.ambientSounds.sounds.size(),
                        record.ambientSounds.id.size());
                ScheduleAmbientDelay(record);
                runtime.records.push_back(std::move(record));
            });
}

void ShutdownNpcAudioRuntime(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        NpcAudioRuntime& runtime)
{
    for (NpcAudioRecord& record : runtime.records) {
        StopVocalPlayback(assets, audio, record);
    }
    runtime.records.clear();
}

bool QueueNpcVocalEvent(
        NpcAudioRuntime& runtime,
        engine::Entity entity,
        NpcVocalEvent event)
{
    NpcAudioRecord* record = FindRecord(runtime, entity);
    if (record == nullptr || event == NpcVocalEvent::None) return false;
    if (event == NpcVocalEvent::Death) {
        if (record->ambientDisabled) return false;
        record->ambientDisabled = true;
        record->pendingEvent = NpcVocalEvent::Death;
        return true;
    }
    if (event == NpcVocalEvent::Hurt) {
        if (record->ambientDisabled
                || record->pendingEvent == NpcVocalEvent::Death
                || engine::IsNull(record->hurtSound)
                || record->pendingEvent == NpcVocalEvent::Hurt
                || record->playbackKind == NpcVocalPlaybackKind::Hurt) {
            return false;
        }
        record->pendingEvent = NpcVocalEvent::Hurt;
        return true;
    }
    if (record->ambientDisabled
            || engine::IsNull(record->playerDetectedSound)
            || record->pendingEvent == NpcVocalEvent::Death
            || record->pendingEvent == NpcVocalEvent::Hurt
            || record->pendingEvent == NpcVocalEvent::PlayerDetected
            || record->playbackKind == NpcVocalPlaybackKind::Death
            || record->playbackKind == NpcVocalPlaybackKind::Hurt
            || record->playbackKind
                    == NpcVocalPlaybackKind::PlayerDetected) {
        return false;
    }
    record->pendingEvent = NpcVocalEvent::PlayerDetected;
    return true;
}

float SelectNpcAmbientDelay(NpcAudioRecord& record)
{
    float minimum = std::isfinite(record.minimumAmbientDelaySeconds)
            ? std::max(0.0f, record.minimumAmbientDelaySeconds)
            : 0.0f;
    float maximum = std::isfinite(record.maximumAmbientDelaySeconds)
            ? std::max(minimum, record.maximumAmbientDelaySeconds)
            : minimum;
    if (maximum <= minimum) return minimum;
    const float unit = static_cast<float>(
            NextRandom(record.delayRandomState) & 0xffffu) / 65535.0f;
    return minimum + unit * (maximum - minimum);
}

void UpdateNpcAudioSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        NpcAudioRuntime& runtime,
        float rawDt)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    world.ForEach<NpcAiState>(
            [&](engine::Entity entity, NpcAiState& ai) {
                if (!ai.playerDetectionAudioPending) return;
                if (world.Has<NpcCombatState>(entity)
                        && world.Get<NpcCombatState>(entity).dead) {
                    ai.playerDetectionAudioPending = false;
                    return;
                }
                QueueNpcVocalEvent(
                        runtime, entity, NpcVocalEvent::PlayerDetected);
                ai.playerDetectionAudioPending = false;
            });
    for (NpcAudioRecord& record : runtime.records) {
        if (!record.occupied) continue;
        if (!world.IsAlive(record.entity)
                || !world.Has<SectorObjectTransform>(record.entity)) {
            StopVocalPlayback(assets, audio, record);
            record.occupied = false;
            continue;
        }
        if (world.Has<NpcCombatState>(record.entity)
                && world.Get<NpcCombatState>(record.entity).dead
                && !record.ambientDisabled) {
            StopVocalPlayback(assets, audio, record);
            record.pendingEvent = NpcVocalEvent::None;
            record.ambientDisabled = true;
            continue;
        }
        const Vector3 position =
                world.Get<SectorObjectTransform>(record.entity).position;
        if (!engine::IsNull(record.vocalPlayback)) {
            if (audio.IsSoundPlaying(record.vocalPlayback)) {
                audio.SetSoundPosition(record.vocalPlayback, position);
            } else {
                record.vocalPlayback = engine::NullSoundPlaybackHandle();
                const NpcVocalPlaybackKind finishedKind = record.playbackKind;
                record.playbackKind = NpcVocalPlaybackKind::None;
                if (finishedKind != NpcVocalPlaybackKind::Death
                        && !record.ambientDisabled) {
                    ScheduleAmbientDelay(record);
                }
            }
        }

        if (record.pendingEvent == NpcVocalEvent::Death) {
            StopVocalPlayback(assets, audio, record);
            record.pendingEvent = NpcVocalEvent::None;
            record.vocalPlayback = PlayActionSound(
                    assets, audio, record.deathSound, position);
            record.playbackKind = engine::IsNull(record.vocalPlayback)
                    ? NpcVocalPlaybackKind::None
                    : NpcVocalPlaybackKind::Death;
            continue;
        }
        if (record.pendingEvent == NpcVocalEvent::Hurt) {
            record.pendingEvent = NpcVocalEvent::None;
            if (record.playbackKind == NpcVocalPlaybackKind::Hurt
                    && audio.IsSoundPlaying(record.vocalPlayback)) {
                continue;
            }
            StopVocalPlayback(assets, audio, record);
            record.vocalPlayback = PlayActionSound(
                    assets, audio, record.hurtSound, position);
            record.playbackKind = engine::IsNull(record.vocalPlayback)
                    ? NpcVocalPlaybackKind::None
                    : NpcVocalPlaybackKind::Hurt;
            if (engine::IsNull(record.vocalPlayback)
                    && !record.ambientDisabled) {
                ScheduleAmbientDelay(record);
            }
            continue;
        }
        if (record.pendingEvent == NpcVocalEvent::PlayerDetected) {
            record.pendingEvent = NpcVocalEvent::None;
            StopVocalPlayback(assets, audio, record);
            record.vocalPlayback = PlayActionSound(
                    assets,
                    audio,
                    record.playerDetectedSound,
                    position);
            record.playbackKind = engine::IsNull(record.vocalPlayback)
                    ? NpcVocalPlaybackKind::None
                    : NpcVocalPlaybackKind::PlayerDetected;
            if (engine::IsNull(record.vocalPlayback)
                    && !record.ambientDisabled) {
                ScheduleAmbientDelay(record);
            }
            continue;
        }
        if (!engine::IsNull(record.vocalPlayback)
                || record.ambientDisabled
                || record.ambientSounds.sounds.empty()) {
            continue;
        }
        record.ambientDelayRemainingSeconds -= dt;
        if (record.ambientDelayRemainingSeconds > 0.0f) continue;
        record.vocalPlayback = PlaySoundSetAt(
                assets,
                audio,
                record.ambientSounds,
                record.ambientPlayback,
                NpcVocalVolume,
                1.0f,
                1.0f,
                NpcVocalPosition(position));
        record.playbackKind = engine::IsNull(record.vocalPlayback)
                ? NpcVocalPlaybackKind::None
                : NpcVocalPlaybackKind::Ambient;
        if (engine::IsNull(record.vocalPlayback)) ScheduleAmbientDelay(record);
    }
}

} // namespace game
