#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "game/FootstepAudio.h"
#include "game/PlayerAudio.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace {

bool Near(float lhs, float rhs, float tolerance = 0.0001f)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

void ScopeDeduplication()
{
    engine::AssetManager assets;
    assert(assets.Initialize());
    const engine::AssetScopeHandle global = assets.GlobalScope();
    const engine::AssetScopeHandle level = assets.CreateScope("audio_test_level");

    const engine::SoundHandle soundGlobal = assets.RequestSound(
            global, "/tmp/audio_test/../audio_test/shot.wav");
    const engine::SoundHandle soundLevel = assets.RequestSound(
            level, "/tmp/audio_test/shot.wav");
    assert(!engine::IsNull(soundGlobal));
    assert(soundGlobal == soundLevel);

    const engine::MusicHandle musicGlobal = assets.RequestMusic(
            global, "/tmp/audio_test/../audio_test/music.ogg");
    const engine::MusicHandle musicLevel = assets.RequestMusic(
            level, "/tmp/audio_test/music.ogg");
    assert(!engine::IsNull(musicGlobal));
    assert(musicGlobal == musicLevel);

    const engine::MusicHandle emitterA = assets.RequestMusicInstance(
            level, "emitter_a", "/tmp/audio_test/music.ogg");
    const engine::MusicHandle emitterARepeat = assets.RequestMusicInstance(
            level, "emitter_a", "/tmp/audio_test/alternate.ogg");
    const engine::MusicHandle emitterB = assets.RequestMusicInstance(
            level, "emitter_b", "/tmp/audio_test/music.ogg");
    assert(!engine::IsNull(emitterA));
    assert(emitterA == emitterARepeat);
    assert(emitterA != emitterB);
    assert(emitterA != musicGlobal && emitterB != musicGlobal);

    assets.UnloadScope(level);
    assert(!assets.IsFinished(soundGlobal));
    assert(!assets.IsFinished(musicGlobal));
    assert(assets.IsFinished(emitterA));
    assert(assets.IsFinished(emitterB));
    assets.UnloadScope(global);
    assert(assets.IsFinished(soundGlobal));
    assert(assets.IsFinished(musicGlobal));
    assets.Shutdown();
}

void Spatialization()
{
    engine::AudioListener listener;
    listener.position = Vector3{};
    listener.forward = Vector3{0.0f, 0.0f, 1.0f};
    listener.up = Vector3{0.0f, 1.0f, 0.0f};

    engine::PositionalSoundSettings source;
    source.minimumDistanceWorld = 2.0f;
    source.maximumDistanceWorld = 10.0f;

    source.position = Vector3{0.0f, 0.0f, 1.0f};
    engine::AudioSpatialization result =
            engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 1.0f));
    assert(Near(result.pan, 0.0f));

    source.position = Vector3{0.0f, 0.0f, 6.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 1.0f / 3.0f));
    assert(Near(result.pan, 0.0f));

    source.position = Vector3{-6.0f, 0.0f, 0.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 1.0f / 3.0f));
    assert(Near(result.pan, 1.0f));

    source.position = Vector3{6.0f, 0.0f, 0.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.pan, -1.0f));

    source.position = Vector3{0.0f, 0.0f, 10.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.0f));

    source.minimumDistanceWorld = 1.0f;
    source.maximumDistanceWorld = 25.0f;
    source.position = Vector3{0.0f, 0.0f, 10.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.1f));
    source.position = Vector3{0.0f, 0.0f, 20.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.05f));
    source.position = Vector3{0.0f, 0.0f, 22.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(result.volumeScale > 0.03f && result.volumeScale < 0.032f);
    source.position = Vector3{0.0f, 0.0f, 25.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.0f));
}

void NpcFootstepSpatialization()
{
    engine::AudioListener listener;
    listener.position = Vector3{};

    engine::PositionalSoundSettings source =
            game::MakeNpcFootstepPositionalSettings(
                    Vector3{0.0f, 1.7f, 0.0f});
    assert(Near(source.minimumDistanceWorld, 4.0f));
    assert(Near(source.maximumDistanceWorld, 25.0f));
    engine::AudioSpatialization result =
            engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 1.0f));

    source.position = Vector3{0.0f, 0.0f, 10.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.4f));

    source.position = Vector3{0.0f, 0.0f, 25.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.0f));
}

void FootstepCatalogDiscovery()
{
    const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "engine_footstep_catalog_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "Tile_Mono_01.wav").put('\0');
    std::ofstream(root / "Tile_Mono_10.wav").put('\0');
    std::ofstream(root / "Plastic_001.mp3").put('\0');
    std::ofstream(root / "nested" / "DirtRoad_Mono_5.ogg").put('\0');
    for (int variation = 1; variation <= 10; ++variation) {
        const std::string name = "BootsLinoleum_"
                + (variation < 10 ? std::string{"0"} : std::string{})
                + std::to_string(variation)
                + ".wav";
        std::ofstream(root / name).put('\0');
    }
    std::ofstream(root / "not_a_variation.wav").put('\0');
    std::ofstream(root / "Ignored_01.flac").put('\0');

    const game::FootstepCatalog catalog = game::DiscoverFootstepCatalog(root.string());
    assert(catalog.sets.size() == 4);
    const game::FootstepCatalogSet* tile = game::FindFootstepCatalogSet(catalog, "Tile_Mono");
    assert(tile != nullptr && tile->relativePaths.size() == 2);
    assert(tile->relativePaths[0] == "footsteps/Tile_Mono_01.wav");
    const game::FootstepCatalogSet* nested = game::FindFootstepCatalogSet(
            catalog,
            "nested/DirtRoad_Mono");
    assert(nested != nullptr && nested->relativePaths.size() == 1);
    const game::FootstepCatalogSet* boots = game::FindFootstepCatalogSet(
            catalog,
            "BootsLinoleum");
    assert(boots != nullptr && boots->relativePaths.size() == 10);
    assert(game::IsValidFootstepSetId("Tile_Mono"));
    assert(game::IsValidFootstepSetId("nested/Tile_Mono"));
    assert(!game::IsValidFootstepSetId("../Tile_Mono"));
    assert(!game::IsValidFootstepSetId("nested\\Tile_Mono"));
    std::filesystem::remove_all(root, ignored);
}

void FootstepShuffleAndPitch()
{
    game::FootstepPlaybackState state;
    game::ReserveFootstepPlaybackState(state, 10);
    std::set<size_t> firstCycle;
    size_t previous = static_cast<size_t>(-1);
    for (size_t i = 0; i < 10; ++i) {
        const size_t selected = game::SelectFootstepVariation(state, "set", 10);
        assert(selected < 10);
        assert(selected != previous);
        firstCycle.insert(selected);
        previous = selected;
    }
    assert(firstCycle.size() == 10);
    const size_t nextCycle = game::SelectFootstepVariation(state, "set", 10);
    assert(nextCycle != previous);

    for (int i = 0; i < 100; ++i) {
        const float pitch = game::SelectFootstepPitch(state);
        assert(pitch >= 0.96f && pitch <= 1.04f);
    }
    assert(game::SelectFootstepVariation(state, "single", 1) == 0);
    assert(game::SelectFootstepVariation(state, "single", 1) == 0);
}

void PlayerSoundCatalogAndPlaybackSelection()
{
    const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "engine_player_sound_catalog_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "future");
    for (int variation = 1; variation <= 3; ++variation) {
        std::ofstream(root / ("Jump_0" + std::to_string(variation) + ".wav"))
                .put('\0');
    }
    for (int variation = 1; variation <= 6; ++variation) {
        std::ofstream(root / ("human_pain_0"
                + std::to_string(variation) + ".wav")).put('\0');
    }
    std::ofstream(root / "Land_01.wav").put('\0');
    for (int variation = 1; variation <= 12; ++variation) {
        std::ofstream(root / "future" / ("WallImpact_"
                + std::to_string(variation) + ".ogg")).put('\0');
    }
    std::ofstream(root / "Breathing_001.mp3").put('\0');
    std::ofstream(root / "Ignored.wav").put('\0');

    const game::SoundSetCatalog catalog = game::DiscoverSoundSetCatalog(
            root.string(),
            "player");
    assert(catalog.sets.size() == 5);
    const game::SoundSetCatalogSet* jump = game::FindSoundSetCatalogSet(
            catalog,
            "Jump");
    assert(jump != nullptr && jump->relativePaths.size() == 3);
    assert(jump->relativePaths.front() == "player/Jump_01.wav");
    const game::SoundSetCatalogSet* land = game::FindSoundSetCatalogSet(
            catalog,
            "Land");
    assert(land != nullptr && land->relativePaths.size() == 1);
    const game::SoundSetCatalogSet* pain = game::FindSoundSetCatalogSet(
            catalog,
            "human_pain");
    assert(pain != nullptr && pain->relativePaths.size() == 6);
    assert(pain->relativePaths.front() == "player/human_pain_01.wav");
    assert(pain->relativePaths.back() == "player/human_pain_06.wav");
    const game::SoundSetCatalogSet* future = game::FindSoundSetCatalogSet(
            catalog,
            "future/WallImpact");
    assert(future != nullptr && future->relativePaths.size() == 12);

    game::SoundSetPlaybackState jumpState;
    game::SoundSetPlaybackState landState;
    game::ReserveSoundSetPlaybackState(jumpState, 3, 4);
    game::ReserveSoundSetPlaybackState(landState, 1, 4);
    const size_t firstJump = game::SelectSoundSetVariation(
            jumpState, "Jump", 3);
    assert(game::SelectSoundSetVariation(landState, "Land", 1) == 0);
    const size_t secondJump = game::SelectSoundSetVariation(
            jumpState, "Jump", 3);
    assert(firstJump != secondJump);
    game::SoundSetPlaybackState painState;
    game::ReserveSoundSetPlaybackState(painState, 6, 10);
    size_t previousPain = static_cast<size_t>(-1);
    for (int selection = 0; selection < 100; ++selection) {
        const size_t painVariation = game::SelectSoundSetVariation(
                painState, "human_pain", 6);
        assert(painVariation < 6);
        assert(painVariation != previousPain);
        previousPain = painVariation;
    }
    for (int i = 0; i < 100; ++i) {
        const float pitch = game::SelectSoundSetPitch(
                jumpState,
                0.98f,
                1.02f);
        assert(pitch >= 0.98f && pitch <= 1.02f);
    }
    std::filesystem::remove_all(root, ignored);
}

void ListenerLowPassMappingAndTransition()
{
    assert(Near(
            engine::AudioListenerLowPassCutoffHz(0.0f),
            engine::AudioUnfilteredLowPassCutoffHz));
    assert(Near(
            engine::AudioListenerLowPassCutoffHz(1.0f),
            engine::AudioMinimumListenerLowPassCutoffHz));
    const float defaultCutoff = engine::AudioListenerLowPassCutoffHz(0.65f);
    assert(defaultCutoff > 1150.0f && defaultCutoff < 1170.0f);
    assert(Near(
            engine::CombineAudioLowPassCutoffs(2000.0f, defaultCutoff),
            defaultCutoff));
    assert(Near(
            engine::CombineAudioLowPassCutoffs(1000.0f, defaultCutoff),
            1000.0f));

    engine::AssetManager assets;
    engine::AudioSystem audio;
    audio.SetListenerLowPassStrength(1.0f);
    audio.Update(assets, 0.1f);
    assert(Near(audio.ListenerLowPassStrength(), 0.5f));
    audio.Update(assets, 0.1f);
    assert(Near(audio.ListenerLowPassStrength(), 1.0f));
    audio.SetListenerLowPassStrength(0.0f);
    audio.Update(assets, 0.05f);
    assert(Near(audio.ListenerLowPassStrength(), 0.75f));
}

void PlayerLiquidAudioTransitions()
{
    game::PlayerLiquidAudioFrameState state;
    game::PlayerLiquidAudioFrameDecision decision =
            game::AdvancePlayerLiquidAudioFrame(
                    state, true, false, false);
    assert(!decision.playEntrySplash && !decision.playExitSound);
    assert(decision.loopShouldExist && !decision.loopShouldPlay);

    decision = game::AdvancePlayerLiquidAudioFrame(
            state, true, false, true);
    assert(!decision.playEntrySplash && !decision.playExitSound);
    assert(decision.loopShouldExist && decision.loopShouldPlay);

    decision = game::AdvancePlayerLiquidAudioFrame(
            state, true, true, true);
    assert(!decision.playEntrySplash && !decision.playExitSound);
    assert(!decision.loopShouldExist && !decision.loopShouldPlay);

    decision = game::AdvancePlayerLiquidAudioFrame(
            state, true, false, true);
    assert(!decision.playEntrySplash && !decision.playExitSound);
    assert(decision.loopShouldPlay);

    decision = game::AdvancePlayerLiquidAudioFrame(
            state, false, false, false);
    assert(!decision.playEntrySplash && decision.playExitSound);
    assert(!decision.loopShouldExist);

    decision = game::AdvancePlayerLiquidAudioFrame(
            state, true, false, true);
    assert(decision.playEntrySplash && !decision.playExitSound);
    assert(decision.loopShouldPlay);
}

} // namespace

int main()
{
    ScopeDeduplication();
    Spatialization();
    NpcFootstepSpatialization();
    FootstepCatalogDiscovery();
    FootstepShuffleAndPitch();
    PlayerSoundCatalogAndPlaybackSelection();
    ListenerLowPassMappingAndTransition();
    PlayerLiquidAudioTransitions();
    std::cout << "Audio system tests passed\n";
}
