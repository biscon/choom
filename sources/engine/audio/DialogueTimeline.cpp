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
bool StartsWith(const std::array<Character, DialogueMaximumCodepoints>& characters,
        size_t word, size_t end, std::string_view prefix)
{
    if (end-word < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (Fold(characters[word+i].value) != static_cast<unsigned char>(prefix[i])) return false;
    return true;
}
DialogueConsonant MapConsonant(const std::array<Character, DialogueMaximumCodepoints>& characters,
        size_t word, size_t end, size_t n, size_t& consumed)
{
    using C = DialogueConsonant;
    const auto c = Fold(characters[n].value);
    const auto next = n+1 < end ? Fold(characters[n+1].value) : 0;
    consumed = 1;
    if (c == 'w' && next == 'h') { consumed = 2; return C::Count; }
    if ((c == 's' || c == 't' || c == 'p' || c == 'c') && next == 'h') {
        consumed = 2;
        if (c == 's') return C::Sh;
        if (c == 't') return C::Th;
        if (c == 'p') return C::F;
        for (auto prefix : {"chor", "chem", "charact", "christ", "school", "echo"})
            if (StartsWith(characters, word, end, prefix)) return C::K;
        for (auto prefix : {"chef", "chic", "machin"})
            if (StartsWith(characters, word, end, prefix)) return C::Sh;
        return C::Ch;
    }
    if (c == 'c' && next == 'k') { consumed = 2; return C::K; }
    if (next == c) consumed = 2;
    switch (c) {
    case 't': return C::T; case 'd': return C::D;
    case 'k': case 'q': return C::K; case 'p': return C::P;
    case 'b': return C::B; case 'g': return C::G;
    case 's': return C::S; case 'z': return C::Z;
    case 'v': return C::V; case 'f': return C::F; case 'h': return C::H;
    case 'c': return next == 'e' || next == 'i' || next == 'y' ? C::S : C::K;
    case 'x': return n == word ? C::Z : C::Ks;
    default: consumed = 1; return C::Count;
    }
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

const char* DialogueVowelName(DialogueVowel vowel)
{
    constexpr std::array<const char*, DialogueVowelCount> names{"ah", "eh", "ee", "oh", "oo", "uh"};
    const size_t i = static_cast<size_t>(vowel);
    return i < names.size() ? names[i] : "uh";
}
const char* DialogueConsonantName(DialogueConsonant consonant)
{
    constexpr std::array<const char*, DialogueConsonantCount> names{
        "t", "d", "k", "p", "b", "g", "s", "z", "v", "f", "h", "sh", "th", "ch", "ks"};
    const auto i = static_cast<size_t>(consonant);
    return i < names.size() ? names[i] : "h";
}

bool IsDialogueConsonant(const DialogueCue& cue)
{
    return cue.source >= DialogueVowelCount && cue.source < DialogueVowelCount+DialogueConsonantCount;
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
        size_t accentedWord = count;
        size_t finalWord = phrase;
        uint32_t previousVowel = UINT32_MAX;
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
            // One restrained accent per phrase, on its first longer word.
            // This is a spelling heuristic, not linguistic stress analysis.
            if (prosody.wordAccentSemitones > 0 && accentedWord == count && letters >= 5)
                accentedWord = word;
            size_t group = 0;
            for (size_t n = word; n < wordEnd;) {
                if (!Vowel(characters[n].value, n == word)) {
                    size_t consumed = 1;
                    const auto consonant = MapConsonant(characters, word, wordEnd, n, consumed);
                    if (voice && consonant != DialogueConsonant::Count) {
                        const auto c = static_cast<size_t>(consonant);
                        const DialogueClip* clip = &voice->consonants[m][c];
                        auto bankMood = mood;
                        if (IsNull(clip->sound)) { clip = &voice->consonants[0][c]; bankMood = DialogueMood::Neutral; }
                        // Missing optional consonants are silent, including old manifests.
                        if (!IsNull(clip->sound)) {
                            DialogueCue cue;
                            cue.start = timeline.reveals[n].seconds;
                            cue.end = cue.start + clip->seconds;
                            cue.source = static_cast<uint32_t>(DialogueVowelCount+c);
                            cue.sound = clip->sound;
                            cue.bankMood = bankMood;
                            cue.beginByte = n ? characters[n-1].end : 0;
                            cue.endByte = characters[n+consumed-1].end;
                            // Relative consonant levels are baked into the clips.
                            // Match AudioSystem's unit-volume ceiling in offline previews too.
                            cue.volume = std::min(1.0f, settings.volume * prosody.gain * (capitals ? 1.6f : 1.0f));
                            timeline.cues.push_back(cue);
                        }
                    }
                    n += consumed;
                    continue;
                }
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
                if (word == accentedWord)
                    semitones += prosody.wordAccentSemitones * std::sin(3.14159265358979323846f * wordProgress);
                if (question && word == finalWord) semitones += 3.0f * wordProgress;
                if (exclamation) semitones += 1.5f * (1-progress);
                if (capitals) semitones += 1.5f;
                cue.pitch = std::pow(2.0f, std::clamp(semitones, -5.0f, 5.0f) / 12);
                cue.volume = settings.volume * prosody.gain * (capitals ? 1.25f : 1.0f);
                if (ellipsis && word == finalWord) cue.volume *= 1.0f - 0.8f * wordProgress;
                cue.wobbleHz = prosody.wobbleHz;
                cue.wobbleSemitones = prosody.wobbleSemitones;
                cue.end = cue.start + cue.end / cue.pitch;
                if (voice && voice->continuousPitch && previousVowel != UINT32_MAX)
                    timeline.cues[previousVowel].nextPitchCue = static_cast<uint32_t>(timeline.cues.size());
                previousVowel = static_cast<uint32_t>(timeline.cues.size());
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
        DialogueSequence& sequence, float rawDt, bool paused, const DialogueCue** consonant)
{
    if (consonant) *consonant = nullptr;
    if (paused) return nullptr;
    const double dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    const double elapsed = sequence.position + dt;
    sequence.position = elapsed;
    const DialogueCue* newest = nullptr;
    const DialogueCue* newestConsonant = nullptr;
    while (sequence.nextCue < timeline.cues.size() && timeline.cues[sequence.nextCue].start <= sequence.position) {
        const auto* cue = &timeline.cues[sequence.nextCue++];
        (IsDialogueConsonant(*cue) ? newestConsonant : newest) = cue;
    }
    if (consonant && newestConsonant && elapsed-newestConsonant->start <= 0.05) *consonant = newestConsonant;
    sequence.finished = sequence.position >= timeline.seconds;
    return newest && elapsed - newest->start <= 0.05 ? newest : nullptr;
}

float DialogueCuePitch(const DialogueCue& cue, double elapsed)
{
    const double wobble = cue.wobbleSemitones * std::sin(elapsed * cue.wobbleHz * 6.283185307179586);
    return cue.pitch * std::pow(2.0, wobble / 12.0);
}

float DialogueTimelinePitch(const DialogueTimeline& timeline, const DialogueCue& cue, double elapsed)
{
    const DialogueCue* current = &cue;
    float pitch = cue.pitch;
    uint32_t previousIndex = 0;
    while (current->nextPitchCue < timeline.cues.size()) {
        const uint32_t index = current->nextPitchCue;
        const auto& next = timeline.cues[index];
        if (next.start <= current->start || index < previousIndex) break;
        if (elapsed < next.start) {
            const double t = std::clamp((elapsed-current->start)/(next.start-current->start), 0.0, 1.0);
            pitch = current->pitch * std::pow(next.pitch/current->pitch, t);
            break;
        }
        current = &next;
        pitch = next.pitch;
        previousIndex = index+1;
    }
    return pitch * DialogueCuePitch(cue, elapsed) / cue.pitch;
}
} // namespace engine
