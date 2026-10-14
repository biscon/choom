#include "engine/audio/DialogueSpeech.h"
#include "engine/assets/AssetManager.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>

namespace game {
namespace {
constexpr unsigned Rate = 48000;
using Pcm = std::vector<int16_t>;
std::string Csv(std::string value)
{
    size_t pos = 0;
    while ((pos = value.find('"', pos)) != std::string::npos) { value.insert(pos, 1, '"'); pos += 2; }
    return '"' + value + '"';
}
void WriteWave(const std::filesystem::path& path, Pcm& pcm)
{
    Wave wave{static_cast<unsigned>(pcm.size()), Rate, 16, 1, pcm.data()};
    if (!ExportWave(wave, path.string().c_str())) throw std::runtime_error("Cannot write " + path.string());
}
const Pcm& ReadPcm(std::map<std::filesystem::path, Pcm>& cache, const std::filesystem::path& path)
{
    auto found = cache.find(path);
    if (found != cache.end()) return found->second;
    Wave wave = LoadWave(path.string().c_str());
    if (!IsWaveValid(wave)) throw std::runtime_error("Cannot decode " + path.string());
    WaveFormat(&wave, Rate, 16, 1);
    if (!IsWaveValid(wave)) throw std::runtime_error("Cannot convert " + path.string());
    const auto* samples = static_cast<const int16_t*>(wave.data);
    Pcm pcm(samples, samples + wave.frameCount);
    UnloadWave(wave);
    return cache.emplace(path, std::move(pcm)).first->second;
}

} // namespace

int PreviewDialogue(const std::filesystem::path& assetsRoot, const std::filesystem::path& output,
        bool continuousPitch, bool stress, bool fixedRate = false)
{
    engine::AssetManager assets;
    if (!assets.Initialize()) throw std::runtime_error("AssetManager initialization failed");
    engine::DialogueVoiceLibrary library;
    engine::BeginLoadDialogueVoices(assets, library, assetsRoot);
    engine::PrepareDialogueVoices(assets, library, library.pending.size());
    if (fixedRate) library.settings.charactersPerSecond.fill(30);
    if (stress) library.settings.charactersPerSecond.fill(60);
    if (continuousPitch) for (auto& voice : library.voices) voice.continuousPitch = true;
    // Never initialize an audio device or upload the queued sounds. The same
    // loaded durations/manifest settings and pure planner drive offline mixing.
    if (library.voices.empty()) throw std::runtime_error("No vowel profiles available");
    std::filesystem::create_directories(output);
    std::map<std::filesystem::path, Pcm> cache;
    std::array<std::string, 4> lines{
            "Wait!", "Yes finally another person! Come closer. Are you hurt?",
            "Elin, Elin. Moon, moon. Keep moving, stay with me!",
            "Are you there? STOP! I thought... never mind."};
    if (stress) {
        const std::array<std::string, 4> syllables{"AT", "SHA", "AE ", "OO "};
        for (size_t i = 0; i < lines.size(); ++i) {
            lines[i].clear();
            for (size_t n = 0; n < 32; ++n) lines[i] += syllables[i];
            lines[i] += '!';
        }
    }
    engine::DialogueTimeline timeline;
    engine::ReserveDialogueTimeline(timeline);
    size_t rendered = 0;
    for (size_t v = 0; v < library.voices.size(); ++v) {
        const auto& voice = library.voices[v];
        Pcm combined;
        std::ofstream cues(output / (voice.id + "_speech_cues.csv"));
        std::ofstream reveals(output / (voice.id + "_reveal_cues.csv"));
        if (!cues) throw std::runtime_error("Cannot write cue sheet");
        if (!reveals) throw std::runtime_error("Cannot write reveal sheet");
        reveals << "mood,line,start_seconds,character\n" << std::fixed << std::setprecision(9);
        cues << "mood,line,clip,start_seconds,end_seconds,pitch,volume,text_group,last_pitch,max_pitch_step\n" << std::fixed << std::setprecision(9);
        for (size_t m = 0; m < engine::DialogueMoodCount; ++m) {
            const auto mood = static_cast<engine::DialogueMood>(m);
            std::vector<double> mix;
            Pcm moodPcm;
            for (size_t line = 0; line < lines.size(); ++line) {
                if (!engine::BuildDialogueTimeline(lines[line], mood,
                        library.settings, &voice, timeline)) throw std::runtime_error("Planning failed");
                const size_t lineStart = mix.size();
                mix.resize(lineStart + static_cast<size_t>(std::ceil(timeline.seconds * Rate)), 0);
                size_t previousByte = 0;
                for (const auto& reveal : timeline.reveals) {
                    reveals << engine::DialogueMoodName(mood) << ',' << line+1 << ','
                            << lineStart/double(Rate)+reveal.seconds << ','
                            << Csv(lines[line].substr(previousByte, reveal.byteCount-previousByte)) << '\n';
                    previousByte = reveal.byteCount;
                }
                for (size_t cueIndex = 0; cueIndex < timeline.cues.size(); ++cueIndex) {
                    const auto& cue = timeline.cues[cueIndex];
                    const auto source = std::find_if(library.pending.begin(), library.pending.end(), [&](const auto& request) {
                        return request.voice == v && request.mood == cue.bankMood && request.source == cue.source;
                    });
                    if (source == library.pending.end()) throw std::runtime_error("Cue source missing");
                    const auto& pcm = ReadPcm(cache, source->path);
                    const size_t start = lineStart + static_cast<size_t>(std::llround(cue.start * Rate));
                    double sample = 0;
                    size_t n = 0;
                    float lastPitch = engine::DialogueCuePitch(cue, cue.start), maxPitchStep = 0;
                    while (sample < pcm.size()) {
                        const size_t lo = static_cast<size_t>(sample), hi = std::min(lo+1, pcm.size()-1);
                        const double value = (pcm[lo] + (pcm[hi]-double(pcm[lo])) * (sample-lo)) * cue.volume;
                        if (start+n >= mix.size()) mix.resize(start+n+1, 0);
                        mix[start+n] += value;
                        const double elapsed = cue.start+n/double(Rate);
                        const float pitch = engine::DialogueTimelinePitch(timeline, cue, elapsed);
                        maxPitchStep = std::max(maxPitchStep, std::abs(pitch-lastPitch));
                        lastPitch = pitch;
                        sample += pitch;
                        ++n;
                    }
                    const size_t length = n;
                    cues << engine::DialogueMoodName(mood) << ',' << line+1 << ',' << Csv(source->path.stem().string()) << ','
                         << (combined.size()+start)/double(Rate) << ',' << (combined.size()+start+length)/double(Rate) << ','
                         << cue.pitch << ',' << cue.volume << ',' << Csv(lines[line].substr(cue.beginByte, cue.endByte-cue.beginByte))
                         << ',' << lastPitch << ',' << maxPitchStep << '\n';
                    ++rendered;
                }
                mix.resize(mix.size() + Rate/2, 0);
            }
            moodPcm.reserve(mix.size());
            for (double value : mix) {
                if (std::abs(value) > 32767) throw std::runtime_error("Overlapping preview clips");
                moodPcm.push_back(static_cast<int16_t>(std::lround(value)));
            }
            WriteWave(output / (voice.id + "_" + engine::DialogueMoodName(mood) + "_speech.wav"), moodPcm);
            combined.insert(combined.end(), moodPcm.begin(), moodPcm.end());
            combined.resize(combined.size()+Rate, 0);
        }
        WriteWave(output / (voice.id + "_speech.wav"), combined);
        if (!cues) throw std::runtime_error("Cue sheet write failed");
        if (!reveals) throw std::runtime_error("Reveal sheet write failed");
        std::cout << voice.id << ": " << combined.size()/double(Rate) << " seconds\n";
    }
    engine::UnloadDialogueVoices(assets, library);
    assets.Shutdown();
    std::cout << "Rendered " << rendered << " speech events. No listening validation performed.\n";
    if (!fixedRate && !stress) return PreviewDialogue(assetsRoot, output / "fixed_rate", continuousPitch, false, true);
    return 0;
}
} // namespace game

int main(int argc, char** argv)
{
    std::filesystem::path assets = "assets", output = "audio_generation/previews/vowel_speech";
    bool continuousPitch = false;
    bool stress = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            std::cout << "dialogue_speech_preview [--assets-root assets] [--output-dir directory] [--continuous-pitch] [--stress]\n";
            return 0;
        }
        if (argument == "--continuous-pitch") { continuousPitch = true; continue; }
        if (argument == "--stress") { stress = true; continue; }
        if ((argument != "--assets-root" && argument != "--output-dir") || i+1 == argc) {
            std::cerr << "Unknown or incomplete option: " << argument << '\n'; return 1;
        }
        (argument == "--assets-root" ? assets : output) = argv[++i];
    }
    SetTraceLogLevel(LOG_WARNING);
    try { return game::PreviewDialogue(assets, output, continuousPitch, stress); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
