#include "engine/audio/DialogueSpeech.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace engine {
namespace {
constexpr std::array<const char*, DialogueMoodCount> MoodNames{
        "neutral", "happy", "angry", "afraid", "panicked", "pained", "relieved"};
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
uint32_t Fold(uint32_t c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xc0 && c <= 0xde && c != 0xd7) c += 32;
    if (c >= 0xe0 && c <= 0xe5) return 'a';
    if (c >= 0xe8 && c <= 0xeb) return 'e';
    if (c >= 0xec && c <= 0xef) return 'i';
    if ((c >= 0xf2 && c <= 0xf6) || c == 0xf8) return 'o';
    if (c >= 0xf9 && c <= 0xfc) return 'u';
    if (c == 0xfd || c == 0xff) return 'y';
    return c;
}
bool Vowel(uint32_t c, bool initial)
{
    c = Fold(c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || (c == 'y' && !initial);
}
DialogueVowel MapVowel(uint32_t first, uint32_t second)
{
    first = Fold(first); second = Fold(second);
    if (first == 'o' && (second == 'o' || second == 'u')) return DialogueVowel::Oo;
    if (first == 'e' && (second == 'e' || second == 'a')) return DialogueVowel::Ee;
    switch (first) {
    case 'a': return DialogueVowel::Ah;
    case 'e': return DialogueVowel::Eh;
    case 'i': case 'y': return DialogueVowel::Ee;
    case 'o': return DialogueVowel::Oh;
    default: return DialogueVowel::Uh;
    }
}
uint32_t Hash(uint32_t hash, uint32_t value) { return (hash ^ value) * 16777619u; }
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

const char* DialogueVowelName(DialogueVowel vowel)
{
    constexpr std::array<const char*, DialogueVowelCount> names{"ah", "eh", "ee", "oh", "oo", "uh"};
    const size_t i = static_cast<size_t>(vowel);
    return i < names.size() ? names[i] : "uh";
}

bool BuildDialogueTimeline(std::string_view text, DialogueMood mood,
        const DialogueSettings& settings, const DialogueVoice* voice, DialogueTimeline& timeline)
{
    std::array<Character, DialogueMaximumCodepoints> characters{};
    size_t count = 0;
    const size_t m = static_cast<size_t>(mood);
    if (text.size() > 8192 || !Decode(text, characters, count) || m >= DialogueMoodCount) return false;
    const float rate = settings.charactersPerSecond[m];
    if (!std::isfinite(rate) || rate <= 0) return false;
    if (timeline.reveals.capacity() < count || timeline.cues.capacity() < count) {
        std::fprintf(stderr, "[Dialogue WARNING] Timeline was not pre-reserved; growing storage.\n");
        ReserveDialogueTimeline(timeline);
    }
    timeline.reveals.clear(); timeline.cues.clear(); timeline.revealCursor = 0;
    timeline.seconds = 0;
    // Reveal each codepoint, but bound a run of punctuation to one pause.
    double clock = 0;
    for (size_t i = 0; i < count;) {
        clock += 1.0 / rate;
        if (Punctuation(characters[i].value)) {
            float pause = 0;
            size_t dots = 0;
            while (i < count && Punctuation(characters[i].value)) {
                pause = std::max(pause, Pause(characters[i].value, settings));
                dots += characters[i].value == '.';
                timeline.reveals.push_back({clock, characters[i++].end});
            }
            if (dots >= 3) pause = std::max(pause, settings.ellipsisPause);
            if (i < count) clock += std::max(0.0, pause * 30.0 / rate - 1.0 / rate);
        } else timeline.reveals.push_back({clock, characters[i++].end});
    }
    timeline.seconds = timeline.reveals.back().seconds;
    const auto prosody = voice ? voice->prosody[m] : DialogueProsody{};
    uint32_t identity = 2166136261u;
    if (voice) for (unsigned char c : voice->id) identity = Hash(identity, c);
    for (size_t phrase = 0; phrase < count;) {
        if (Punctuation(characters[phrase].value)) { ++phrase; continue; }
        size_t end = phrase;
        while (end < count && !Punctuation(characters[end].value)) ++end;
        bool question = false, exclamation = false, ellipsis = false;
        size_t dots = 0;
        for (size_t n = end; n < count && Punctuation(characters[n].value); ++n) {
            const auto c = characters[n].value;
            question |= c == '?' || c == 0xff1f;
            exclamation |= c == '!' || c == 0xff01;
            ellipsis |= c == 0x2026;
            dots += c == '.';
        }
        ellipsis |= dots >= 3;
        size_t finalWord = phrase;
        for (size_t n = phrase; n < end; ++n)
            if (Letter(characters[n].value) && (n == phrase || !Letter(characters[n-1].value))) finalWord = n;
        for (size_t word = phrase; word < end;) {
            if (!Letter(characters[word].value)) { ++word; continue; }
            size_t wordEnd = word;
            uint32_t wordHash = identity;
            bool capitals = true;
            size_t letters = 0;
            while (wordEnd < end && Letter(characters[wordEnd].value)) {
                const auto c = characters[wordEnd++].value;
                capitals &= c >= 'A' && c <= 'Z';
                ++letters;
                wordHash = Hash(wordHash, Fold(c));
            }
            capitals &= letters >= 2;
            size_t group = 0;
            for (size_t n = word; n < wordEnd;) {
                if (!Vowel(characters[n].value, n == word)) { ++n; continue; }
                const size_t begin = n++;
                while (n < wordEnd && Vowel(characters[n].value, false)) ++n;
                const auto vowel = MapVowel(characters[begin].value, begin+1 < n ? characters[begin+1].value : 0);
                const size_t v = static_cast<size_t>(vowel);
                DialogueCue cue;
                cue.start = timeline.reveals[begin].seconds;
                cue.source = static_cast<uint32_t>(v);
                cue.beginByte = begin ? characters[begin-1].end : 0;
                cue.endByte = characters[n-1].end;
                cue.bankMood = mood;
                if (voice) {
                    const DialogueClip* clip = &voice->banks[m][v];
                    if (IsNull(clip->sound)) { clip = &voice->banks[0][v]; cue.bankMood = DialogueMood::Neutral; }
                    cue.sound = clip->sound;
                    cue.end = clip->seconds;
                }
                const float progress = float(begin-phrase) / std::max(size_t(1), end-phrase-1);
                const float wordProgress = float(begin-word+1) / (wordEnd-word);
                const float jitter = (Hash(wordHash, static_cast<uint32_t>(group++)) & 65535u) / 65535.0f * 0.3f - 0.15f;
                float semitones = prosody.pitchSemitones + (0.5f-progress)*prosody.contourSemitones + jitter;
                if (question && word == finalWord) semitones += 3.0f * wordProgress;
                if (exclamation) semitones += 1.5f * (1-progress);
                if (capitals) semitones += 1.5f;
                cue.pitch = std::pow(2.0f, std::clamp(semitones, -5.0f, 5.0f) / 12);
                cue.volume = settings.volume * prosody.gain * (capitals ? 1.25f : 1.0f);
                if (ellipsis && word == finalWord) cue.volume *= 1.0f - 0.8f * wordProgress;
                cue.wobbleHz = prosody.wobbleHz;
                cue.wobbleSemitones = prosody.wobbleSemitones;
                cue.end = cue.start + cue.end / cue.pitch;
                timeline.cues.push_back(cue);
            }
            word = wordEnd;
        }
        phrase = end;
    }
    return true;
}

size_t AdvanceDialogueReveal(DialogueTimeline& timeline, double elapsed)
{
    while (timeline.revealCursor < timeline.reveals.size()
            && timeline.reveals[timeline.revealCursor].seconds <= elapsed) ++timeline.revealCursor;
    return timeline.revealCursor ? timeline.reveals[timeline.revealCursor-1].byteCount : 0;
}

const DialogueCue* AdvanceDialogueSequence(const DialogueTimeline& timeline,
        DialogueSequence& sequence, float rawDt, bool paused)
{
    if (paused) return nullptr;
    const double dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    const double elapsed = sequence.position + dt;
    sequence.position = elapsed;
    const DialogueCue* newest = nullptr;
    while (sequence.nextCue < timeline.cues.size() && timeline.cues[sequence.nextCue].start <= sequence.position)
        newest = &timeline.cues[sequence.nextCue++];
    sequence.finished = sequence.position >= timeline.seconds;
    return newest && elapsed - newest->start <= 0.05 ? newest : nullptr;
}

float DialogueCuePitch(const DialogueCue& cue, double elapsed)
{
    const double wobble = cue.wobbleSemitones * std::sin(elapsed * cue.wobbleHz * 6.283185307179586);
    return cue.pitch * std::pow(2.0, wobble / 12.0);
}
} // namespace engine
