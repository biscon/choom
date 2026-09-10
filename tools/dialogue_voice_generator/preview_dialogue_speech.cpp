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

int PreviewDialogue(const std::filesystem::path& assetsRoot, const std::filesystem::path& output)
{
    engine::AssetManager assets;
    if (!assets.Initialize()) throw std::runtime_error("AssetManager initialization failed");
    engine::DialogueVoiceLibrary library;
    engine::BeginLoadDialogueVoices(assets, library, assetsRoot);
    engine::PrepareDialogueVoices(assets, library, library.pending.size());
    // Never initialize an audio device or upload the queued sounds. The same
    // loaded durations/manifest settings and pure planner drive offline mixing.
    if (library.voices.size() != 2) throw std::runtime_error("Both voice manifests are required");
    std::filesystem::create_directories(output);
    std::map<std::filesystem::path, Pcm> cache;
    const std::array<std::string, 2> lines{
            "Wait!", "Yes finally another person!!. Come closer. Are you hurt?"};
    engine::DialogueTimeline timeline;
    engine::ReserveDialogueTimeline(timeline);
    size_t rendered = 0;
    for (size_t v = 0; v < library.voices.size(); ++v) {
        const auto& voice = library.voices[v];
        Pcm combined;
        engine::DialogueSelectionHistory history;
        std::ofstream cues(output / (voice.id + "_speech_cues.csv"));
        if (!cues) throw std::runtime_error("Cannot write cue sheet");
        cues << "mood,line,clip,start_seconds,end_seconds,pitch,volume,text_group\n" << std::fixed << std::setprecision(6);
        for (size_t m = 0; m < engine::DialogueMoodCount; ++m) {
            const auto mood = static_cast<engine::DialogueMood>(m);
            if (voice.banks[m].empty()) throw std::runtime_error("Missing mood bank");
            Pcm moodPcm;
            for (size_t line = 0; line < lines.size(); ++line) {
                if (!engine::BuildDialogueTimeline(lines[line], mood, 456u + line,
                        library.settings, &voice, timeline, &history)) throw std::runtime_error("Planning failed");
                const size_t lineStart = moodPcm.size();
                moodPcm.resize(lineStart + static_cast<size_t>(std::ceil(timeline.seconds * Rate)), 0);
                for (const auto& cue : timeline.cues) {
                    const auto source = std::find_if(library.pending.begin(), library.pending.end(), [&](const auto& request) {
                        return request.voice == v && request.mood == mood && request.source == cue.source;
                    });
                    if (source == library.pending.end()) throw std::runtime_error("Cue source missing");
                    const auto& pcm = ReadPcm(cache, source->path);
                    const size_t start = lineStart + static_cast<size_t>(std::llround(cue.start * Rate));
                    const size_t length = static_cast<size_t>(std::ceil(pcm.size() / double(cue.pitch)));
                    moodPcm.resize(std::max(moodPcm.size(), start + length), 0);
                    for (size_t n = 0; n < length; ++n) {
                        const double sample = std::min(double(pcm.size()-1), n * double(cue.pitch));
                        const size_t lo = static_cast<size_t>(sample), hi = std::min(lo+1, pcm.size()-1);
                        const double value = (pcm[lo] + (pcm[hi]-double(pcm[lo])) * (sample-lo)) * cue.volume;
                        if (std::abs(value) >= 32767) throw std::runtime_error("Preview clipping");
                        moodPcm[start+n] = static_cast<int16_t>(std::lround(value));
                    }
                    cues << engine::DialogueMoodName(mood) << ',' << line+1 << ',' << Csv(source->path.stem().string()) << ','
                         << (combined.size()+start)/double(Rate) << ',' << (combined.size()+start+length)/double(Rate) << ','
                         << cue.pitch << ',' << cue.volume << ',' << Csv(lines[line].substr(cue.beginByte, cue.endByte-cue.beginByte)) << '\n';
                    engine::CommitDialogueSelection(history, cue);
                    ++rendered;
                }
                moodPcm.resize(moodPcm.size() + Rate/2, 0);
            }
            WriteWave(output / (voice.id + "_" + engine::DialogueMoodName(mood) + "_speech.wav"), moodPcm);
            combined.insert(combined.end(), moodPcm.begin(), moodPcm.end());
            combined.resize(combined.size()+Rate, 0);
        }
        WriteWave(output / (voice.id + "_speech.wav"), combined);
        if (!cues) throw std::runtime_error("Cue sheet write failed");
        std::cout << voice.id << ": " << combined.size()/double(Rate) << " seconds\n";
    }
    engine::UnloadDialogueVoices(assets, library);
    assets.Shutdown();
    std::cout << "Rendered " << rendered << " complete fragments. No listening validation performed.\n";
    return 0;
}
} // namespace game

int main(int argc, char** argv)
{
    std::filesystem::path assets = "assets", output = "audio_generation/previews/engine_speech";
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            std::cout << "dialogue_speech_preview [--assets-root assets] [--output-dir directory]\n";
            return 0;
        }
        if ((argument != "--assets-root" && argument != "--output-dir") || i+1 == argc) {
            std::cerr << "Unknown or incomplete option: " << argument << '\n'; return 1;
        }
        (argument == "--assets-root" ? assets : output) = argv[++i];
    }
    SetTraceLogLevel(LOG_WARNING);
    try { return game::PreviewDialogue(assets, output); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
