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
    for (auto& bank : voice.banks) for (uint32_t source = 0; source < 8; ++source)
        bank.push_back({engine::SoundHandle{source, 1}, 0.65f + source * 0.2f, source});
    return voice;
}
void Timelines()
{
    auto voice = Voice();
    engine::DialogueSettings settings;
    engine::DialogueTimeline a, b;
    engine::ReserveDialogueTimeline(a); engine::ReserveDialogueTimeline(b);
    const auto* allocation = a.reveals.data();
    const std::string text = "Yes finally another person!!. Come closer. What happened?";
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, 456, settings, &voice, a));
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, 456, settings, &voice, b));
    assert(a.seconds == b.seconds && a.cues.size() == b.cues.size());
    assert(a.reveals.data() == allocation);
    assert(a.reveals.size() == text.size());
    assert(!a.cues.empty());
    for (size_t i = 0; i < a.cues.size(); ++i) {
        const auto& cue = a.cues[i];
        assert(cue.sound == b.cues[i].sound);
        assert(cue.start == b.cues[i].start);
        assert(cue.pitch >= 0.98f && cue.pitch <= 1.02f);
        assert(std::fabs((cue.end-cue.start)*cue.pitch - voice.banks[0][cue.source].seconds) < 0.0001);
        if (cue.beginByte) assert(text[cue.beginByte-1] == ' ');
        if (cue.endByte < text.size()) assert(std::string(" .!?,").find(text[cue.endByte]) != std::string::npos);
        assert(cue.volume >= settings.volume * 0.92f && cue.volume <= settings.volume * 1.08f + 0.00001f);
        assert(cue.end <= a.seconds);
        if (i) { assert(cue.start >= a.cues[i-1].end); assert(cue.source != a.cues[i-1].source); }
    }
    double panic = 0;
    std::array<double, engine::DialogueMoodCount> durations{};
    for (size_t mood = 0; mood < durations.size(); ++mood) {
        assert(engine::BuildDialogueTimeline(text, static_cast<engine::DialogueMood>(mood), 456, settings, &voice, b));
        durations[mood] = b.seconds;
        if (mood == static_cast<size_t>(engine::DialogueMood::Panicked)) panic = b.seconds;
    }
    for (size_t i = 0; i < durations.size(); ++i)
        if (i != static_cast<size_t>(engine::DialogueMood::Panicked))
            assert(settings.charactersPerSecond[static_cast<size_t>(engine::DialogueMood::Panicked)] > settings.charactersPerSecond[i]);
    assert(panic < durations[static_cast<size_t>(engine::DialogueMood::Neutral)]);

    assert(engine::BuildDialogueTimeline("Hi!", engine::DialogueMood::Panicked, 1, settings, &voice, b));
    assert(b.cues.size() == 1); // complete recording; duration no longer biases selection
    assert(engine::BuildDialogueTimeline("Wait...", engine::DialogueMood::Neutral, 1, settings, &voice, a));
    assert(engine::BuildDialogueTimeline("Wait........", engine::DialogueMood::Neutral, 1, settings, &voice, b));
    assert(a.seconds == b.seconds); // repeated dots are one pause
    assert(engine::BuildDialogueTimeline("Wait", engine::DialogueMood::Neutral, 1, settings, &voice, b));
    assert(std::fabs(a.seconds-b.seconds-settings.ellipsisPause) < 0.0001);
    assert(engine::BuildDialogueTimeline(" \t!?...\n", engine::DialogueMood::Afraid, 1, settings, &voice, b));
    assert(b.cues.empty());

    auto noPauses = settings;
    noPauses.commaPause = noPauses.clausePause = noPauses.sentencePause = noPauses.ellipsisPause = 0;
    assert(engine::BuildDialogueTimeline(u8"…—!?", engine::DialogueMood::Neutral, 1, noPauses, &voice, b));
    assert(b.cues.empty() && b.seconds == 0);

    const std::string utf8 = u8"Héllo 世界… êtes-vous là?";
    assert(engine::BuildDialogueTimeline(utf8, engine::DialogueMood::Neutral, 8, settings, &voice, a));
    for (const auto& reveal : a.reveals) {
        assert(reveal.byteCount <= utf8.size());
        if (reveal.byteCount < utf8.size()) assert((static_cast<unsigned char>(utf8[reveal.byteCount]) & 0xc0) != 0x80);
    }
    assert(engine::AdvanceDialogueReveal(a, a.seconds+1) == utf8.size());
    assert(!engine::BuildDialogueTimeline(std::string("\xc0\xaf", 2), engine::DialogueMood::Neutral, 1, settings, &voice, a));
    assert(!engine::BuildDialogueTimeline(std::string(2049, 'a'), engine::DialogueMood::Neutral, 1, settings, &voice, a));
    assert(engine::BuildDialogueTimeline(std::string(2048, 'a'), engine::DialogueMood::Neutral, 1, settings, &voice, a));
    assert(a.reveals.data() == allocation);

    voice.banks[static_cast<size_t>(engine::DialogueMood::Afraid)].clear();
    assert(engine::BuildDialogueTimeline("Neutral fallback.", engine::DialogueMood::Afraid, 7, settings, &voice, b));
    assert(!b.cues.empty());
    assert(engine::BuildDialogueTimeline("Silent but visible.", engine::DialogueMood::Afraid, 7, settings, nullptr, b));
    assert(!b.cues.empty() && engine::IsNull(b.cues[0].sound) && !b.reveals.empty());
}

void SelectionHistoryAndWordDensity()
{
    auto voice = Voice();
    engine::DialogueSelectionHistory history;
    engine::DialogueTimeline timeline;
    engine::ReserveDialogueTimeline(timeline);
    std::vector<uint32_t> order;
    const std::string text = "Yes finally another person! Come closer. Are you hurt?";
    for (uint32_t line = 0; line < 12; ++line) {
        const auto before = history.banks[0].remaining;
        assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, line+1, {}, &voice, timeline, &history));
        assert(history.banks[0].remaining == before); // planning never consumes playback history
        assert(timeline.cues.size() == 8); // nine English words; only 'Are you' share a recording
        for (const auto& cue : timeline.cues) {
            const auto group = text.substr(cue.beginByte, cue.endByte-cue.beginByte);
            assert(std::count(group.begin(), group.end(), ' ') <= 1);
            if (group.find(' ') != std::string::npos) assert(group == "Are you");
            for (size_t n = order.size() > 3 ? order.size()-3 : 0; n < order.size(); ++n)
                assert(cue.source != order[n]);
            order.push_back(cue.source);
            engine::CommitDialogueSelection(history, cue);
        }
    }
    for (size_t n = 0; n < order.size(); n += 8)
        assert(std::set<uint32_t>(order.begin()+n, order.begin()+n+8).size() == 8);
    // Cancelling a line before any audio starts leaves exactly the same choices.
    engine::DialogueTimeline cancelled;
    engine::ReserveDialogueTimeline(cancelled);
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, 123, {}, &voice, cancelled, &history));
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, 123, {}, &voice, timeline, &history));
    assert(cancelled.cues[0].source == timeline.cues[0].source);
    engine::CommitDialogueSelection(history, timeline.cues[0]); // cancellation after one started clip
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Neutral, 123, {}, &voice, timeline, &history));
    assert(cancelled.cues[0].source != timeline.cues[0].source);
    engine::DialogueSelectionHistory skipped;
    engine::CommitDialogueSelection(skipped, cancelled.cues[1]);
    assert(skipped.banks[0].remaining & (uint64_t(1) << cancelled.cues[0].source));
    assert(skipped.banks[0].recent[0] == cancelled.cues[1].source);
    // One- and two-clip fallback banks cannot enforce a three-source cooldown.
    for (size_t count : {size_t(1), size_t(2)}) {
        voice.banks[0].resize(count);
        history = {};
        for (int n = 0; n < 8; ++n) {
            assert(engine::BuildDialogueTimeline("Wait", engine::DialogueMood::Afraid, n+1, {}, &voice, timeline, &history));
            // Explicitly empty the requested bank to exercise neutral fallback.
            voice.banks[static_cast<size_t>(engine::DialogueMood::Afraid)].clear();
            assert(engine::BuildDialogueTimeline("Wait", engine::DialogueMood::Afraid, n+1, {}, &voice, timeline, &history));
            assert(timeline.cues.size() == 1 && timeline.cues[0].bankMood == engine::DialogueMood::Neutral);
            if (count == 2 && n) assert(timeline.cues[0].source != history.banks[0].recent[0]);
            engine::CommitDialogueSelection(history, timeline.cues[0]);
        }
        voice = Voice();
    }
}

void FrameStallsAndUnavailableDevice()
{
    auto voice = Voice();
    engine::DialogueTimeline timeline;
    engine::ReserveDialogueTimeline(timeline);
    const std::string text = "Keep moving, stay quiet and follow the person ahead of you.";
    assert(engine::BuildDialogueTimeline(text, engine::DialogueMood::Panicked, 1, {}, &voice, timeline));
    for (float step : {1.0f/30, 1.0f/60, 1.0f/144, 0.4f}) {
        timeline.revealCursor = 0;
        engine::DialogueSequence sequence;
        size_t previousBytes = 0, started = 0;
        for (int frame = 0; frame < 10000 && !sequence.finished; ++frame) {
            const auto* cue = engine::AdvanceDialogueSequence(timeline, sequence, step, false);
            if (cue) ++started;
            const size_t bytes = engine::AdvanceDialogueReveal(timeline, sequence.position);
            assert(bytes >= previousBytes && bytes <= text.size());
            previousBytes = bytes;
        }
        assert(sequence.finished && started == timeline.cues.size());
        assert(engine::AdvanceDialogueReveal(timeline, sequence.position) == text.size());
    }
    engine::DialogueSequence device;
    assert(engine::AdvanceDialogueSequence(timeline, device, 10, false) == &timeline.cues[0]);
    assert(device.fragmentElapsed == 0); // a late start gets its full duration
    device.usesDevice = true;
    assert(!engine::AdvanceDialogueSequence(timeline, device, 10, true));
    assert(device.nextCue == 1 && device.fragmentActive && !device.finished);
    assert(device.position == timeline.cues[0].end); // text waits while audio still plays
    const auto paused = device;
    assert(!engine::AdvanceDialogueSequence(timeline, device, 10, false, true));
    assert(device.position == paused.position && device.fragmentActive);
    assert(!engine::AdvanceDialogueSequence(timeline, device, 10, false));
    assert(!device.fragmentActive && device.nextCue == 1); // completion cannot start a burst
    assert(engine::AdvanceDialogueSequence(timeline, device, 10, false) == &timeline.cues[1]);
    assert(device.fragmentElapsed == 0);
    engine::AssetManager assets;
    assert(assets.Initialize());
    engine::AudioSystem audio; // No device: all tests remain headless.
    engine::DialoguePlayback playback;
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 1, true, true, {}, 0.01f);
    assert(playback.sequence.nextCue == 1);
    assert(engine::IsNull(playback.handle));
    const double beforeCancel = playback.sequence.position;
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 1, false, false, {}, 10);
    assert(playback.sequence.position == beforeCancel);
    engine::UpdateDialoguePlayback(assets, audio, playback, timeline, 2, true, true, {}, 10);
    assert(playback.token == 2 && playback.sequence.nextCue == 1 && playback.sequence.fragmentElapsed == 0);
    engine::StopDialoguePlayback(assets, audio, playback);
    assert(playback.sequence.nextCue == 0 && playback.token == 0);
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
    Json manifest{{"schema_version", 1}, {"voice_identity_id", "male"}, {"moods", {{"neutral", {"source.wav"}}}}};
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
    assert(library.prepared == 1 && library.voices[0].banks[0].size() == 1);
    assert(library.voices[0].banks[0][0].seconds == 1.0f); // entire one-second fixture, including its tail
    assert(!engine::DialogueVoicesFinished(assets, library)); // queued device upload
    const auto handle = library.voices[0].banks[0][0].sound;
    engine::UnloadDialogueVoices(assets, library);
    assert(assets.IsFinished(handle));
    manifest["moods"]["neutral"] = {"../outside.wav"};
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
    SelectionHistoryAndWordDensity();
    FrameStallsAndUnavailableDevice();
    PcmOwnership();
    LibraryLoading();
}
} // namespace engine_test
