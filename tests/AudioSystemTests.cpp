#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"

#include <cassert>
#include <cmath>
#include <iostream>

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

    assets.UnloadScope(level);
    assert(!assets.IsFinished(soundGlobal));
    assert(!assets.IsFinished(musicGlobal));
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
    assert(Near(result.volumeScale, 0.5f));
    assert(Near(result.pan, 0.0f));

    source.position = Vector3{-6.0f, 0.0f, 0.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.5f));
    assert(Near(result.pan, 1.0f));

    source.position = Vector3{6.0f, 0.0f, 0.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.pan, -1.0f));

    source.position = Vector3{0.0f, 0.0f, 10.0f};
    result = engine::ComputeAudioSpatialization(listener, source);
    assert(Near(result.volumeScale, 0.0f));
}

} // namespace

int main()
{
    ScopeDeduplication();
    Spatialization();
    std::cout << "Audio system tests passed\n";
}
