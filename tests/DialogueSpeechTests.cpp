#include "engine/audio/DialogueSpeech.h"
#include "engine/assets/AssetManager.h"
#include "util/json.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

namespace engine_test {
namespace {
engine::DialogueVoice Voice()
{
    engine::DialogueVoice voice;
    voice.id = "fixture";
    for (auto& bank : voice.banks) for (uint32_t v = 0; v < engine::DialogueVowelCount; ++v)
        bank[v] = {engine::SoundHandle{v, 1}, 0.14f, v};
    return voice;
}
void Timelines()
{
    auto voice = Voice();
    engine::DialogueSettings settings;
    engine::DialogueTimeline a, b;
    engine::ReserveDialogueTimeline(a); engine::ReserveDialogueTimeline(b);
    const auto* allocation = a.reveals.data();
    const std::string text = "Moon, meet Elin! Are you hurt?";
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, settings, &voice, a));
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, settings, &voice, b));
    assert(a.seconds == b.seconds && a.cues.size() == b.cues.size());
    assert(a.reveals.data() == allocation && a.reveals.size() == text.size());
    assert(a.cues[0].source == static_cast<uint32_t>(engine::DialogueVowel::Oo));
    assert(a.cues[1].source == static_cast<uint32_t>(engine::DialogueVowel::Ee));
    for (size_t i = 0; i < a.cues.size(); ++i) {
        assert(a.cues[i].start == b.cues[i].start && a.cues[i].pitch == b.cues[i].pitch);
        assert(a.cues[i].sound == b.cues[i].sound);
    }
    for (auto& bank : voice.banks) for (auto& clip : bank) clip.seconds = 5;
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, settings, &voice, b));
    assert(a.seconds == b.seconds); // clip duration cannot slow text
    for (size_t i = 0; i < a.reveals.size(); ++i) assert(a.reveals[i].seconds == b.reveals[i].seconds);
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, settings, nullptr, b));
    assert(a.seconds == b.seconds && a.cues.size() == b.cues.size());
    assert(engine::BuildDialogueTimeline("bcdf", engine::DialogueMood::Neutral, settings, &voice, a));
    assert(a.cues.empty() && std::abs(a.seconds - 4.0/30) < 1e-6);
    assert(engine::BuildDialogueTimeline("Wait... Next", engine::DialogueMood::Neutral, settings, &voice, a));
    assert(engine::BuildDialogueTimeline("Wait........ Next", engine::DialogueMood::Neutral, settings, &voice, b));
    assert(a.seconds == b.seconds);
    assert(engine::BuildDialogueTimeline("a,b", engine::DialogueMood::Neutral, settings, &voice, a));
    assert(std::abs(a.reveals[2].seconds-a.reveals[1].seconds-.1) < 1e-6);
    assert(engine::BuildDialogueTimeline("a?b", engine::DialogueMood::Neutral, settings, &voice, a));
    assert(std::abs(a.reveals[2].seconds-a.reveals[1].seconds-.2) < 1e-6);
    const std::string utf8 = u8"Héllo 世界… êtes-vous là?";
    assert(engine::BuildDialogueTimeline(utf8, engine::DialogueMood::Neutral, settings, &voice, a));
    for (const auto& reveal : a.reveals)
        if (reveal.byteCount < utf8.size()) assert((static_cast<unsigned char>(utf8[reveal.byteCount]) & 0xc0) != 0x80);
    assert(engine::AdvanceDialogueReveal(a, a.seconds+1) == utf8.size());
    assert(!engine::BuildDialogueTimeline(std::string("\xc0\xaf", 2), engine::DialogueMood::Neutral, settings, &voice, a));
    assert(!engine::BuildDialogueTimeline(std::string(2049, 'a'), engine::DialogueMood::Neutral, settings, &voice, a));
    assert(engine::BuildDialogueTimeline(std::string(2048, 'a'), engine::DialogueMood::Neutral, settings, &voice, a));
    assert(a.reveals.data() == allocation);
    voice.banks[static_cast<size_t>(engine::DialogueMood::Afraid)] = {};
    assert(engine::BuildDialogueTimeline("a", engine::DialogueMood::Afraid, settings, &voice, a));
    assert(a.cues[0].bankMood == engine::DialogueMood::Neutral && !engine::IsNull(a.cues[0].sound));
}
void DeterministicWordsAndExpression()
{
    auto voice = Voice();
    engine::DialogueTimeline a, b;
    engine::ReserveDialogueTimeline(a); engine::ReserveDialogueTimeline(b);
    assert(engine::BuildDialogueTimeline("Elin Elin", engine::DialogueMood::Neutral, {}, &voice, a));
    assert(a.cues.size() == 4);
    assert(a.cues[0].source == a.cues[2].source && a.cues[1].source == a.cues[3].source);
    for (auto text : {"Elin", "Elin?", "ELIN!", "Elin..."}) {
        assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, {}, &voice, b));
        assert(b.cues.size() == 2);
    }
    assert(engine::BuildDialogueTimeline("Elin", engine::DialogueMood::Neutral, {}, &voice, a));
    assert(b.cues.back().volume < a.cues.back().volume); // ellipsis fades final word
    assert(engine::BuildDialogueTimeline("Elin?", engine::DialogueMood::Neutral, {}, &voice, b));
    assert(b.cues.back().pitch > a.cues.back().pitch);
    assert(engine::BuildDialogueTimeline("ELIN!", engine::DialogueMood::Neutral, {}, &voice, b));
    assert(b.cues.front().volume > a.cues.front().volume);
    assert(engine::BuildDialogueTimeline("a a a a a", engine::DialogueMood::Neutral, {}, &voice, a));
    assert(a.cues[1].start < a.cues[0].end); // intentional overlap
    auto cue = a.cues.front(); cue.wobbleHz = 2; cue.wobbleSemitones = .3f;
    assert(engine::DialogueCuePitch(cue, .125) > cue.pitch);
    assert(engine::DialogueCuePitch(cue, .375) < cue.pitch);
}
void FrameStallsAndUnavailableDevice()
{
    auto voice = Voice();
    engine::DialogueTimeline timeline;
    engine::ReserveDialogueTimeline(timeline);
    assert(engine::BuildDialogueTimeline("a a a a a a a a a a", engine::DialogueMood::Neutral, {}, &voice, timeline));
    for (float step : {1.0f/30, 1.0f/60, 1.0f/144, 0.4f}) {
        engine::DialogueSequence sequence;
        double elapsed = 0;
        size_t started = 0;
        while (!sequence.finished) {
            if (engine::AdvanceDialogueSequence(timeline, sequence, step)) ++started;
            elapsed += step;
        }
        assert(elapsed >= timeline.seconds && elapsed < timeline.seconds + step + 1e-6);
        assert(sequence.nextCue == timeline.cues.size());
        if (step < .1f) assert(started == timeline.cues.size());
        else assert(started < timeline.cues.size());
    }
    engine::DialogueSequence sequence;
    assert(!engine::AdvanceDialogueSequence(timeline, sequence, 10, true));
    assert(sequence.position == 0);
    assert(!engine::AdvanceDialogueSequence(timeline, sequence, 10)); // no stale burst
    assert(sequence.finished);
    engine::AssetManager assets;
    assert(assets.Initialize());
    engine::AudioSystem audio;
    engine::DialoguePlayback playback;
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 1, true, false, {}, .1f);
    const auto cursor = playback.sequence.nextCue;
    assert(cursor > 0); // muted events are consumed
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 1, true, true, {}, 0);
    assert(playback.sequence.nextCue == cursor);
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 2, true, true, {}, 0);
    assert(playback.sequence.position == 0 && playback.sequence.nextCue == 0);
    engine::StopDialoguePlayback(assets, audio, playback);
    for (const auto& slot : playback.slots) assert(engine::IsNull(slot.handle));
    assets.Shutdown();
}

std::vector<int16_t> Pcm()
{
    std::vector<int16_t> samples(48000, 0);
    for (size_t i = 6000; i < 40000; ++i)
        samples[i] = static_cast<int16_t>(9000 * std::sin(i * 6.283185307179586 * 230 / 48000));
    return samples;
}
void PcmOwnership()
{
    const auto samples = Pcm();
    engine::AssetManager assets;
    assert(assets.Initialize());
    const auto first = assets.CreateScope("dialogue_test_a"), second = assets.CreateScope("dialogue_test_b");
    auto handle = assets.CreateSoundFromPcm(first, "fixture", samples.data(), samples.size());
    assert(!engine::IsNull(handle));
    assert(assets.CreateSoundFromPcm(first, "fixture", samples.data(), samples.size()) == handle);
    assert(assets.CreateSoundFromPcm(second, "fixture", samples.data(), samples.size()) == handle);
    assert(engine::IsNull(assets.CreateSoundFromPcm(first, "invalid", nullptr, 0)));
    assets.UnloadScope(first);
    assert(!assets.IsFinished(handle));
    assets.UnloadScope(second);
    assert(assets.IsFinished(handle));
    assert(engine::IsNull(assets.CreateSoundFromPcm(first, "stale_scope", samples.data(), samples.size())));
    assets.Shutdown();
}

void LibraryLoading()
{
    using Json = nlohmann::ordered_json;
    const auto root = std::filesystem::temp_directory_path() / ("dialogue_tests_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "config");
    std::filesystem::create_directories(root / "voice");
    auto samples = Pcm();
    Wave wave{static_cast<unsigned int>(samples.size()), 48000, 16, 1, samples.data()};
    assert(ExportWave(wave, (root / "voice/source.wav").string().c_str()));
    Json manifest{{"schema_version", 2}, {"voice_identity_id", "male"}, {"moods", {{"neutral", {{"ah", "source.wav"}}}}}};
    std::ofstream(root / "voice/manifest.json") << manifest.dump();
    Json config{{"characters_per_second", Json::object()}, {"voices", {{"male", "voice/manifest.json"}, {"female", "missing.json"}}}};
    std::ofstream(root / "config/dialogue_speech.json") << config.dump();
    engine::AssetManager assets;
    assert(assets.Initialize());
    engine::DialogueVoiceLibrary library;
    engine::BeginLoadDialogueVoices(assets, library, root);
    assert(library.pending.size() == 1 && library.voices.size() == 1);
    assert(!engine::DialogueVoicesFinished(assets, library));
    engine::PrepareDialogueVoices(assets, library, 1);
    assert(library.prepared == 1 && library.voices[0].banks[0].size() == engine::DialogueVowelCount);
    assert(library.voices[0].banks[0][0].seconds == 1.0f); // entire one-second fixture, including its tail
    assert(!engine::DialogueVoicesFinished(assets, library)); // queued device upload
    const auto handle = library.voices[0].banks[0][0].sound;
    engine::UnloadDialogueVoices(assets, library);
    assert(assets.IsFinished(handle));
    manifest["moods"]["neutral"]["ah"] = "../outside.wav";
    std::ofstream(root / "voice/manifest.json") << manifest.dump();
    engine::BeginLoadDialogueVoices(assets, library, root);
    assert(library.pending.empty() && engine::DialogueVoicesFinished(assets, library));
    engine::UnloadDialogueVoices(assets, library);
    assets.Shutdown();
    std::filesystem::remove_all(root);
}
} // namespace

void TestDialogueSpeech()
{
    Timelines();
    DeterministicWordsAndExpression();
    FrameStallsAndUnavailableDevice();
    PcmOwnership();
    LibraryLoading();
}
} // namespace engine_test
