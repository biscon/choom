#include "engine/audio/DialogueSpeech.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace engine {
namespace {
constexpr std::array<const char*, DialogueMoodCount> MoodNames{
        "neutral", "happy", "angry", "afraid", "panicked", "pained", "relieved"};
uint32_t Random(uint32_t& seed)
{
    if (!seed) seed = 0x6d2b79f5u;
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
float Unit(uint32_t& seed) { return (Random(seed) & 0xffffu) / 65535.0f; }
struct Character { uint32_t value; size_t end; };
bool Decode(std::string_view text, std::array<Character, DialogueMaximumCodepoints>& out, size_t& count)
{
    count = 0;
    for (size_t i = 0; i < text.size();) {
        if (count == out.size()) return false;
        uint32_t c = static_cast<unsigned char>(text[i++]);
        int remaining = 0;
        uint32_t minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { c &= 31; remaining = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { c &= 15; remaining = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { c &= 7; remaining = 3; minimum = 0x10000; }
        else if (c >= 0x80) return false;
        while (remaining--) {
            if (i == text.size()) return false;
            const unsigned char next = text[i++];
            if ((next & 0xc0) != 0x80) return false;
            c = (c << 6) | (next & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
        out[count++] = {c, i};
    }
    return count != 0;
}
bool Space(uint32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == 0xa0 || c == 0x3000; }
float Pause(uint32_t c, const DialogueSettings& s)
{
    if (c == ',') return s.commaPause;
    if (c == ':' || c == ';' || c == 0x2014 || c == 0x2013) return s.clausePause;
    if (c == '.' || c == '!' || c == '?' || c == '\n' || c == 0x3002 || c == 0xff01 || c == 0xff1f) return s.sentencePause;
    if (c == 0x2026) return s.ellipsisPause;
    return 0;
}
bool Punctuation(uint32_t c)
{
    return c == ',' || c == ':' || c == ';' || c == 0x2014 || c == 0x2013
            || c == '.' || c == '!' || c == '?' || c == '\n'
            || c == 0x3002 || c == 0xff01 || c == 0xff1f || c == 0x2026;
}
bool Letter(uint32_t c)
{
    if (Space(c) || Punctuation(c)) return false;
    if (c < 128) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    return c != 0x2018 && c != 0x2019 && c != 0x201c && c != 0x201d;
}
const DialogueClip* SelectClip(const std::vector<DialogueClip>* bank,
        DialogueSelectionState& state, uint32_t& seed)
{
    uint64_t available = 0;
    if (bank) for (const auto& clip : *bank)
        if (clip.source < 64 && !IsNull(clip.sound) && std::isfinite(clip.seconds) && clip.seconds > 0)
            available |= uint64_t(1) << clip.source;
    if (!available) return nullptr;
    if (state.available != available) { state = {}; state.available = available; }
    if (!state.remaining) state.remaining = available;
    uint64_t candidates = state.remaining;
    // Prefer excluding all three recent sources. With a small bank, relax the
    // oldest restriction first; even one-clip fallback banks must keep working.
    for (size_t retained = state.recent.size();; --retained) {
        candidates = state.remaining;
        for (size_t n = 0; n < retained; ++n)
            if (state.recent[n] < 64) candidates &= ~(uint64_t(1) << state.recent[n]);
        if (candidates || retained == 0) break;
    }
    unsigned count = 0;
    for (uint64_t bits = candidates; bits; bits &= bits-1) ++count;
    unsigned selected = Random(seed) % count;
    uint32_t source = 0;
    for (; source < 64; ++source) if (candidates & (uint64_t(1) << source)) {
        if (selected-- == 0) break;
    }
    state.remaining &= ~(uint64_t(1) << source);
    state.recent = {source, state.recent[0], state.recent[1]};
    for (const auto& clip : *bank) if (clip.source == source) return &clip;
    return nullptr;
}
} // namespace

const char* DialogueMoodName(DialogueMood mood)
{
    const size_t index = static_cast<size_t>(mood);
    return index < MoodNames.size() ? MoodNames[index] : "neutral";
}
bool ParseDialogueMood(std::string_view text, DialogueMood& mood)
{
    for (size_t i = 0; i < MoodNames.size(); ++i) {
        if (text == MoodNames[i]) { mood = static_cast<DialogueMood>(i); return true; }
    }
    return false;
}
void ReserveDialogueTimeline(DialogueTimeline& timeline)
{
    timeline.reveals.reserve(DialogueMaximumCodepoints);
    timeline.cues.reserve(DialogueMaximumCodepoints);
}

bool BuildDialogueTimeline(std::string_view text, DialogueMood mood, uint32_t seed,
        const DialogueSettings& settings, const DialogueVoice* voice, DialogueTimeline& timeline,
        const DialogueSelectionHistory* history)
{
    std::array<Character, DialogueMaximumCodepoints> characters{};
    size_t count = 0;
    if (text.size() > 8192 || !Decode(text, characters, count)
            || static_cast<size_t>(mood) >= DialogueMoodCount) return false;
    const float rate = settings.charactersPerSecond[static_cast<size_t>(mood)];
    if (!std::isfinite(rate) || rate <= 0) return false;
    if (voice && (!std::isfinite(voice->pitchRange[0]) || !std::isfinite(voice->pitchRange[1])
            || voice->pitchRange[0] <= 0 || voice->pitchRange[0] > voice->pitchRange[1])) return false;
    if (timeline.reveals.capacity() < count || timeline.cues.capacity() < count) {
        std::fprintf(stderr, "[Dialogue WARNING] Timeline was not pre-reserved; growing storage.\n");
        ReserveDialogueTimeline(timeline);
    }
    timeline.reveals.clear(); timeline.cues.clear(); timeline.revealCursor = 0;
    timeline.seconds = 0;
    const auto* bank = voice ? &voice->banks[static_cast<size_t>(mood)] : nullptr;
    DialogueMood bankMood = mood;
    if (bank && bank->empty()) { bank = &voice->banks[0]; bankMood = DialogueMood::Neutral; }
    DialogueSelectionHistory planned = history ? *history : DialogueSelectionHistory{};
    auto& selection = planned.banks[static_cast<size_t>(bankMood)];
    for (size_t i = 0; i < count;) {
        const float punctuation = Pause(characters[i].value, settings);
        if (Punctuation(characters[i].value)) {
            float delay = punctuation;
            int dots = 0;
            while (i < count && Punctuation(characters[i].value)) {
                const uint32_t c = characters[i].value;
                delay = std::max(delay, Pause(c, settings));
                dots += c == '.';
                timeline.reveals.push_back({timeline.seconds, characters[i++].end});
            }
            if (dots >= 3) delay = std::max(delay, settings.ellipsisPause);
            timeline.seconds += delay * (24.0 / rate);
            continue;
        }
        if (!Letter(characters[i].value)) {
            timeline.reveals.push_back({timeline.seconds, characters[i++].end});
            continue;
        }
        const size_t begin = i;
        size_t wordLetters = 0;
        while (i < count && !Space(characters[i].value) && !Punctuation(characters[i].value)) {
            wordLetters += Letter(characters[i].value);
            ++i;
        }
        // Pair only adjacent short words; never bridge punctuation or let a
        // long recording swallow an arbitrary number of displayed words.
        size_t nextWord = i;
        while (nextWord < count && Space(characters[nextWord].value)) ++nextWord;
        size_t nextEnd = nextWord, nextLetters = 0;
        while (nextEnd < count && !Space(characters[nextEnd].value) && !Punctuation(characters[nextEnd].value)) {
            nextLetters += Letter(characters[nextEnd].value);
            ++nextEnd;
        }
        if (wordLetters <= 3 && nextLetters > 0 && nextLetters <= 3) i = nextEnd;
        const float pitch = voice ? std::clamp(0.98f + Unit(seed) * 0.04f,
                voice->pitchRange[0], voice->pitchRange[1]) : 1.0f;
        const DialogueClip* chosen = SelectClip(bank, selection, seed);
        size_t letters = 0;
        for (size_t n = begin; n < i; ++n) letters += Letter(characters[n].value);
        const double duration = chosen ? chosen->seconds / pitch : std::max(0.08, letters / double(rate));
        const double start = timeline.seconds;
        const float volume = voice ? settings.volume * (voice->volumeRange[0]
                + Unit(seed) * (voice->volumeRange[1]-voice->volumeRange[0])) : 0;
        timeline.cues.push_back({start, start+duration,
                chosen ? chosen->sound : SoundHandle{},
                chosen ? chosen->source : UINT32_MAX, pitch, volume,
                begin ? characters[begin-1].end : 0, characters[i-1].end});
        timeline.cues.back().bankMood = bankMood;
        timeline.cues.back().selectionAfter = selection;
        for (size_t n = begin; n < i; ++n)
            timeline.reveals.push_back({start + duration * double(n-begin+1) / double(i-begin), characters[n].end});
        timeline.seconds += duration;
        // Punctuation owns the pause at phrase boundaries; gaps only join words.
        size_t next = i;
        while (next < count && Space(characters[next].value)) ++next;
        if (next < count && !Punctuation(characters[next].value))
            timeline.seconds += (settings.gapSeconds[0] + Unit(seed) * (settings.gapSeconds[1]-settings.gapSeconds[0])) * 24.0 / rate;
    }
    return true;
}

void CommitDialogueSelection(DialogueSelectionHistory& history, const DialogueCue& cue)
{
    if (IsNull(cue.sound) || cue.source >= 64 || static_cast<size_t>(cue.bankMood) >= DialogueMoodCount) return;
    auto& state = history.banks[static_cast<size_t>(cue.bankMood)];
    const uint64_t available = cue.selectionAfter.available;
    if (state.available != available) { state = {}; state.available = available; }
    if (!state.remaining) state.remaining = available;
    // Earlier planned cues may have failed to play. Consume only this actual
    // start, rather than copying the provisional history of the whole prefix.
    state.remaining &= ~(uint64_t(1) << cue.source);
    state.recent = {cue.source, state.recent[0], state.recent[1]};
}

size_t AdvanceDialogueReveal(DialogueTimeline& timeline, double elapsed)
{
    while (timeline.revealCursor < timeline.reveals.size()
            && timeline.reveals[timeline.revealCursor].seconds <= elapsed) ++timeline.revealCursor;
    return timeline.revealCursor ? timeline.reveals[timeline.revealCursor-1].byteCount : 0;
}

const DialogueCue* AdvanceDialogueSequence(const DialogueTimeline& timeline,
        DialogueSequence& sequence, float rawDt, bool devicePlaying, bool paused)
{
    if (paused || sequence.finished) return nullptr;
    const double dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    if (sequence.fragmentActive) {
        const auto& cue = timeline.cues[sequence.nextCue-1];
        sequence.fragmentElapsed += dt;
        sequence.position = std::min(cue.end, cue.start + sequence.fragmentElapsed);
        const bool finished = sequence.usesDevice ? !devicePlaying
                : sequence.fragmentElapsed >= cue.end-cue.start;
        if (!finished) return nullptr;
        sequence.position = cue.end;
        sequence.fragmentActive = false;
        sequence.usesDevice = false;
        // Do not apply time from the old sound to a new sound or its gap.
    } else if (sequence.nextCue < timeline.cues.size()) {
        const auto& cue = timeline.cues[sequence.nextCue];
        sequence.position = std::min(cue.start, sequence.position + dt);
        if (sequence.position < cue.start) return nullptr;
        sequence.fragmentActive = true;
        sequence.fragmentElapsed = 0;
        ++sequence.nextCue;
        return &cue;
    } else {
        sequence.position = std::min(timeline.seconds, sequence.position + dt);
    }
    sequence.finished = !sequence.fragmentActive && sequence.nextCue == timeline.cues.size()
            && sequence.position >= timeline.seconds;
    return nullptr;
}
} // namespace engine
