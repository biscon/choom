# Offline dialogue voice tools

The current engine uses [locally synthesized vowels](#current-vowel-speech-system).
The TTS workflow below is retained for reference; it no longer supplies dialogue
playback. See [engine speech previews](#engine-speech-previews) for listening files.

## Retained TTS generator

Authoring only: generates short invented conversational fragments for later use
as typewriter dialogue chatter. It does not implement playback or change the
engine, dialogue scripts, or runtime asset loading.

## Requirements and first run

Use Python 3.10+ on Linux/macOS, FFmpeg, and FFprobe. No Python packages are
required. `OPENAI_API_KEY` must already be available in the environment for
generation. Never put it in this directory, command-line arguments, or a tracked
file. The tool does not load `.env` files, log credentials, or pass the key to
FFmpeg/FFprobe. Tests block live network access.

Run from the repository root (the script also works from another directory):

```sh
python3 -m unittest discover -s tools/dialogue_voice_generator/tests
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --dry-run --max-requests 112
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --max-requests 112 --build-previews
```

The dry run validates the configuration and inspects cached audio without network
access or persistent writes. It prints expected final files, valid final caches,
reusable raw files, new API requests, stale files, and deferred/blocked work.

Generation uses the official
[Speech API](https://developers.openai.com/api/reference/resources/audio/subresources/speech/methods/create)
at `https://api.openai.com/v1/audio/speech`, with JSON `model`, `voice`, `input`,
`instructions`, `response_format: "wav"`, and `speed: 1.0`. WAV output is requested
directly, then independently decoded and converted to the runtime format.

`voice_profiles.json` holds all voice and performance decisions. The initial
model is `gpt-4o-mini-tts`, with `onyx` for male and `nova` for female. There are
56 unique, deliberately invented fragments, eight per mood, shared between the
two identities. Text is explicit and stable; API waveforms are not deterministic.
Short invented syllables can accidentally resemble real words or names in some
languages; listening review is necessary to catch unwanted associations.

## Selection, request limits, and recovery

```sh
# Plan one bank (8 clips).
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --dry-run --profile female --mood afraid

# Reprocess existing finals from cached raw takes using current processing settings.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --reprocess --max-requests 0 --build-previews

# Replace exactly one reported clip; keep all previous raw takes.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --regenerate female_afraid_03 --max-requests 1 --build-previews

# Replace several clips in one bounded run.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --regenerate male_angry_02 --regenerate female_pained_06 --max-requests 2 --build-previews
```

`--profile` and `--mood` intersect. When `--regenerate` is present, only those IDs
are selected; conflicting filters and unknown IDs fail before requests. A normal
run skips valid existing finals. Configuration or hash mismatches on valid
finals are reported as stale. Use `--reprocess` for processing-only changes and
`--regenerate` when a new API take is intended. Missing/invalid
finals are rebuilt from matching raw takes. Successful raw responses that fail
audio validation are retained and reported, not automatically regenerated.

`--max-requests` defaults to 112 and accepts 0–112. It limits logical generation
jobs per invocation, including failed jobs, excluding up to two bounded transient
HTTP/transport retries per job. Jobs exceeding the limit are deferred. The initial
batch additionally has a persistent ceiling of 112 successful HTTP generations;
this includes responses later rejected or interrupted during download. Explicit
selective regeneration is a later batch and does not reset the initial counter.

The successful-request ledger is checkpointed before reading each response body,
and mirrored into the tracked authoring manifest. Preserve both accounting files;
do not delete them to circumvent the initial cap. A transport timeout before a
response is received may still have been processed by the server; the client
cannot guarantee provider billing counts for that ambiguous case. No automatic
retry occurs after HTTP success.

At most three attempts use exponential backoff (approximately 2 and 4 seconds),
with rate-limit `Retry-After` delays capped at 30 seconds. Authentication, quota,
billing, permanent API errors, and unexpected redirects stop the run. Downloads
have a 90-second timeout and 20 MiB response bound. A process lock prevents two
generators from spending against the same ledger concurrently. Ctrl-C preserves
completed work. Resume with the same command after resolving the reported issue.

Exit status: `0` selected work complete, `1` failure/interruption, `2` deferred,
stale, or blocked work. Duration warnings alone do not cause failure. Final
manifests list only existing assets with verified stored hashes.

## Processing and validation

FFmpeg converts sources to mono 48 kHz PCM. The CPU boundary detector examines
10 ms windows of AC energy (the window mean is removed for analysis only).
Speech anchors must exceed peak-window RMS minus 34 dB for at least 30 ms;
boundaries expand to peak-window RMS minus 46 dB, stopping after two quiet windows
or 150 ms of extension. This preserves nearby breaths and consonants while
ignoring isolated distant noise. It only trims outer material; internal pauses
remain intact. Within the outer windows it refines the cut to the threshold.

Each mood has an optional `tempo` multiplier in `voice_profiles.json`, defaulting
to 1.0 and accepting finite numbers from 1.0 through 3.0. Current settings are
neutral 2.0, happy 2.1, angry 2.2, afraid 1.75, panicked 2.5, pained 1.7, and
relieved 1.9. FFmpeg's `atempo` filter accelerates the complete trimmed utterance
without raising its pitch. Multipliers above 2.0 use two equal stages whose
product is the requested tempo, keeping each stage below the sample-skipping
range. A 2.0 multiplier makes speech approximately half as long;
safety padding and engine punctuation pauses remain separate from that ratio.
Tempo belongs to offline processing, so it does not change the API request or
invalidate raw takes. Its effective value is included in each asset's processing
fingerprint; changing one mood leaves other moods' cached finals valid.

These settings put ordinary conversation at approximately the previous panicked
pace. The engine config also uses neutral's former panicked pause rate (36),
with the other mood rates increased proportionally. Actual line duration depends
on the selected recordings, word grouping, and punctuation; a tempo multiplier
alone cannot guarantee an identical words-per-minute rate across banks.

After measuring the retimed waveform, FFmpeg applies 5 ms fades at its edges,
then adds 15 ms of silent safety padding on each side. Fades smooth the transition into
padding, including sources that start or stop at nonzero amplitude. Linear gain
targets −3 dBFS. There is no dynamic-range compression, denoising, reverb, or
pitch shifting in the final source assets.

Low-level sources requiring more than 30 dB gain with more than 60% of their
power in DC are rejected, preventing amplified offset/noise from becoming a
runtime vocalization. Clean quiet content remains usable with a gain warning.
Source peaks below −75 dBFS are rejected. FFprobe and sample inspection validate
format, nonzero content, normalization and clipped samples. Durations outside
80–1200 ms, gain above 20 dB, trimming over 500 ms, and reaching the boundary
extension limit are flagged for review. These thresholds are configurable.

`--reprocess` rebuilds selected assets from hash-verified raw caches, including
valid existing finals, and never requests generation even if `--max-requests`
is nonzero. Missing raw caches are reported as blocked. It cannot be combined
with `--regenerate`. Successful writes are atomic; a rejected source keeps its
old final bytes for diagnosis but is excluded from runtime manifests and previews.
Unmodified raw takes and request accounting are retained. Authoring metadata and
`validation.csv` include prior/new duration, tempo, retained duration before and
after tempo adjustment, removed outer time, and sample jumps at the padding
boundaries. Detection thresholds remain in the authoring manifest. Reprocessing
always starts from raw audio, so tempo adjustments never compound.

These checks establish technical properties only. They cannot establish that a
clip sounds human, maintains identity, expresses its intended mood, contains no
recognizable words, or lacks generated room sound. Every clip starts with
`listening_review: "pending"`. No duration correction is applied merely to pass
validation, and no extra generations are purchased automatically to replace warnings.

## Files and provenance

- `assets/audio/dialogue_voices/{male,female}/{mood}/{asset_id}.wav`: final assets.
- Each identity's `voice_manifest.json`: compact metadata recommendations only.
  Clip paths are relative to the identity directory; volume and pitch ranges are
  multipliers, timing is milliseconds, and character counts exclude whitespace.
  Defaults are pitch 0.96–1.04, volume 0.92–1.08, minimum interval 85 ms,
  3–5 characters per sound, and one active clip per speaker. Punctuation mappings
  are omitted in this initial set.
- `authoring_manifest.json` beside this README: tracked source/provenance snapshot
  with immutable raw-take references, source text, full instructions, model/voice,
  UTC dates, SHA-256 hashes, processing version/settings/commands, FFmpeg version,
  validation measurements, warnings, and listening status. Temporary processing
  paths in recorded commands describe the original execution; rebuild with this
  script to allocate new temporary paths.
- `audio_generation/raw/{asset_id}/take_*.wav`: unmodified responses, never
  overwritten by selective regeneration. These are local intermediates: back
  them up separately if exact source waveforms must survive workspace removal.
- `audio_generation/reports/`: persistent request ledger, per-run JSON reports,
  `latest_run.json`, and `validation.csv` covering all 112 assets. Per-run status
  distinguishes newly failed replacements from any preserved valid older final.
- `audio_generation/previews/`: 17 preview WAVs, each with its own CSV cue sheet.

Only raw files, previews, reports, and this tool's Python cache folders are
ignored. Final WAVs, runtime manifests, attribution, configuration, tests, and
the authoring snapshot belong in source control.

## Listening pass

Start with `audio_generation/previews/master.wav`, then `male.wav` and
`female.wav` in the same directory. Isolate banks with names such as
`male_afraid.wav` and `female_panicked.wav`.

Order is male then female; within each identity: neutral, happy, angry, afraid,
panicked, pained, relieved; then clip numbers 01–08. Gaps are 250 ms between
clips, 900 ms between moods, and 1500 ms between identities. Cue sheets contain
the exact order, asset IDs, sample offsets, timestamps, and warnings. Incomplete
previews identify missing assets in the run report.

When a group has no usable clips, no empty preview WAV or cue sheet is emitted;
the run report marks that preview unavailable. Preview files become available
after successful generation. An API quota/billing failure needs to be resolved
in the account before resuming the same generation command; changing the API key
in configuration is never a recovery step.

Check consistent identity across requests, grounded delivery, unwanted English
words or extra speech, tonal exaggeration, preserved consonants/breaths, room
sound, and edge artifacts. Give special attention to duration/gain warnings and
the afraid, panicked, and pained banks. Report undesirable asset IDs for selective
regeneration; all source text and mood directions remain editable in JSON.

## Current vowel speech system

The engine now uses deterministic synthetic vowels and quiet consonants, not the TTS fragment
banks described above. The old generator, raw takes, accounting, and final clips
are retained as references. No network or API key is used by the vowel generator.

```sh
python3 tools/dialogue_voice_generator/generate_vowel_voices.py --dry-run
python3 tools/dialogue_voice_generator/generate_vowel_voices.py
python3 -m unittest discover -s tools/dialogue_voice_generator/tests
cmake --build cmake-build-debug --target dialogue_speech_preview -j2
cmake-build-debug/dialogue_speech_preview
```

`vowel_profiles.json` (authoring schema 2) controls six profiles, seven moods and
six vowel identities and fifteen consonant classes. Generator revision 3 ships
the accepted softened texture. It uses a band-limited differentiated glottal
pulse with a slow opening and faster closure, then filters it through three
resonators. Fundamental frequency and vocal-tract scale remain independent.
The original formant centers for ah/eh/ee/oh/oo derive from the first three bass
bands in the [Csound tables](https://csound.com/manual/misc/formants/); uh is an
authored central vowel. Resonances use fixed neutral calibration, allowing softer
upper harmonics instead of renormalizing them away. Breath uses broadened vowel
resonances plus a restrained 600–6500 Hz noise branch. The softer pulse shape
and modest seeded cycle variation are preserved from the accepted audition.

Breath is measured relative to the voiced stem after envelope shaping. The
source adds 3 dB to each profile's `breath_to_voice_db`: neutral adult -15 dB,
young -17 dB, elderly -13 dB before mood offsets, capped at -12 dB. Its envelope
is slightly stronger near onset/release. Seeded streams supply smooth pitch
movement (+/-0.6%), cycle timing variation (+/-0.6%), cycle amplitude variation
(+/-2%), and formant movement (+/-1%). Elderly runtime wobble remains 2 Hz with
a 0.15-semitone base depth; emotional wobble is at most 0.2 semitones.
Profile/mood settings live in JSON; `TEXTURE_PARAMETERS` and
`CONSONANT_PARAMETERS` in the generator hold the shared synthesis controls.

Files still last 140 ms, with mood-specific attacks, 40 ms releases and zero
endpoints. The configurable RMS target is now -20 dBFS and the peak ceiling
-12 dBFS, both 6 dB above revision 1. Because the richer glottal signal has larger
peaks, normalization alone would not reach the intended level. Minimal offline
soft-knee peak control reduces the peak/RMS ratio before normalization, using a
shared smooth gain for both stems and restoring their authored breath ratio.
Drive is bounded and reported; unusual authoring can still produce a reported
peak-limited result. There is no runtime compressor, ducking, or change to other
sounds. Runtime volume remains 0.65 and distance attenuation is unchanged.

The generation manifest records achieved RMS/peak, target and measured breath
ratio, peak-control drive, peak-limited status, DC offset and endpoint steps.
The vowel-only four-overlap bound stays below full scale. The headless preview
also checks summed vowels plus consonants; `--stress` exercises dense capitalized
speech at 60 characters/second across all profiles and moods. These are headroom
checks, not proof that other sounds cannot mask speech in a particular scene.

Generated files go to `assets/audio/dialogue_vowels/{profile}/{mood}/{vowel}.wav`.
Version-2 runtime manifests map each mood's vowel names to relative WAV paths and
include pitch contour, gain and wobble controls. Optional `continuous_pitch: true`
enables a shared logarithmic pitch contour between vowel cues within each phrase.
Optional `consonants` maps each mood's consonant names to WAV paths under
`{mood}/consonants/`. Legacy manifests remain vowel-only with stepped pitch.
The generation manifest records
configuration and WAV SHA-256 hashes. Regeneration is deterministic for identical
configuration/tool versions on the same Python/platform. `--output-dir` can send
experiments to a separate directory. The six presets are artistic starting points,
not physiological models of age or gender.

The generator writes 252 vowels and 630 consonants across six profiles and seven
moods. Consonants use the audition's levels/envelopes: 24 ms softened hard bursts
at 18% of vowel RMS and 50 ms shaped noise at 25%, with ch/ks combining both.
Runtime applies the same mood gain and up to a 1.6x capitals boost, respecting
the audio system's unit-volume ceiling. Each class has one
fixed seeded texture per voice/mood, rather than the audition's per-word noise
generation; no synthesis or asset loading occurs during playback. Four vowel
slots and two separate consonant slots share pause/release/skip logic. Late frames
consume both streams without replaying a backlog. Releasing sounds freeze their
last pitch before a replacement timeline can be used.

The engine config's `voices` catalog maps stable profile IDs to manifests. Adding
a profile means adding authoring JSON, generating its WAVs, and adding its manifest
to this catalog; no C++ enum changes are required. Existing `male`/`female` IDs now
select the middle-aged profiles. Reopen the NPC editor to refresh its metadata;
reload the level to load changed WAVs. A missing profile stays visible as
`Missing: <id>` in the picker and is preserved on save.

## Engine speech previews

The headless C++ utility reads the production manifests and calls the same timeline
planner and pitch-modulation helper as runtime. No window/audio device is opened.
It sums overlapping samples in floating point, includes full tails and rejects
clipping before quantizing. Output defaults to the ignored directory
`audio_generation/previews/vowel_speech/`; old `engine_speech/` previews are kept.
Use `--assets-root` or `--output-dir` to override paths.

Each profile has a combined WAV, seven mood WAVs, and a CSV with vowel IDs,
timestamps, pitch, gain and text groups. The `fixed_rate/` subdirectory renders
all moods at 30 characters/second, allowing texture/intonation differences to be
judged independently of speed. The corpus includes repeated words/names, questions,
capitals and ellipses. Half-second line gaps and one-second mood gaps are audition
separators, not runtime delays.

Start with male/female neutral and panicked, then compare young/adult/elderly using
the same sentence. Check vowel quality, recognizable identity, mood distinction,
word-like associations, overlap harshness and clicking. Automated checks cannot
establish that these presets sound good. In-game listening must still check
interruption, mute/unmute, pause, positional audio and left-click reveal/dismiss.

## Revision 2 listening comparisons

Before regeneration, preserve the old `assets/audio/dialogue_vowels` tree and
`assets/config/dialogue_speech.json` in a separate assets root. The initial
revision-1 snapshot for this change is in the ignored directory
`audio_generation/previews/vowel_revision_2/baseline_assets/`, alongside copies of
its original generator and authoring config. Do not overwrite that baseline when
regenerating the new assets.

```sh
python3 tools/dialogue_voice_generator/compare_vowel_voices.py \
  --baseline-assets audio_generation/previews/vowel_revision_2/baseline_assets
```

The tool invokes the headless C++ preview for both assets roots, then produces
16 comparisons under `audio_generation/previews/vowel_revision_2/comparisons/`:

- `*_three_way.wav`: original, original +6 dB, revised; one second separates versions.
- `*_matched.wav`: original boosted and revised, matched in whole-clip RMS.
- Matching CSVs label sections, exact times, applied gain, RMS and peak.
- `comparison_report.json` reports the achieved speech RMS increase.
- `before/` and `after/` contain every profile/mood, including fixed-rate examples.

The comparisons include neutral examples for all six profiles plus adult male
and female panic. Matching downward preserves headroom and helps separate timbre
from volume; RMS matching does not guarantee equal perceived loudness. Listen for
audible breath without continuous hiss, reduced electronic tone, recognizable
vowels and identities, and sufficient in-game speech level. No automated listening
or interactive GUI verification is claimed.

## Texture and continuous-pitch audition

The following describes the pre-integration experiments; their saved WAVs remain
under `audio_generation/previews/`. The softened source, continuous pitch and
quiet consonants are now implemented by the production generator and runtime.
Audition tools share those synthesis helpers to prevent drift.

Running this comparison tool leaves the game's bank unchanged. It renders
four versions in a fixed order: **baseline, source/breath only, continuous pitch
only, combined**. Male/female neutral and panicked speech use the same production
planner, text, event times, starting pitches and gains. The renderer's optional
`--continuous-pitch` flag interpolates pitch logarithmically between vowel starts
within each phrase, sharing the trajectory across overlapping samples. It holds
the last pitch at phrase ends, retains existing mood wobble, and resets at
punctuation. This flag forces continuous pitch for older manifests in the headless
tool; production manifests already enable it.

```sh
cmake --build cmake-build-debug --target dialogue_speech_preview -j2
python3 tools/dialogue_voice_generator/audition_vowel_texture.py
```

Output is under `audio_generation/previews/vowel_texture_audition/`. The script
requires a fresh output directory; use `--output-dir` for another run to preserve
previous listening baselines. It snapshots the input assets, generator code and
authoring settings, then verifies production asset hashes remain unchanged.

Start with `male_quick_four_way.wav` and `female_quick_four_way.wav`. Each repeats
“Yes finally another person! Come closer. Are you hurt?” in the four versions,
with one-second separators. Full comparisons include repeated names, questions,
capitals and ellipses. `*_fixed_rate_four_way.wav` holds every mood at 30 cps;
`individual/` contains standalone matched versions. CSVs label section times.

The source experiment uses a fixed neutral calibration for the resonances,
instead of normalizing away each new source's spectral changes. Softer glottal
closure, extra spectral tilt and seeded cycle-to-cycle timing/amplitude/shape
changes reduce source regularity. Breath uses broader formant filters plus a
600–6500 Hz broad noise branch, with a measured breath/voice ratio 3 dB above
the baseline (capped at -12 dB). Parameters now live in the production generator and are recorded in `report.json`.

Texture revision 2 softens the first audition's raspy character: cycle timing
variation is reduced from 1.2% to 0.6%, amplitude variation from 4% to 2%, and
pulse-shape excursions are halved around the same average shape. The nominal
broad-noise energy weight falls from 25% to 12.5%; measured breath/voice level
falls by 3 dB (neutral -12 to -15 dB, panicked -9 to -12 dB). Overall vowel
loudness, pitch trajectories, consonant settings and reveal timing are retained.
The original audition remains available for comparison. Generate a fresh set with:

```sh
python3 tools/dialogue_voice_generator/audition_vowel_texture.py \
  --output-dir audio_generation/previews/vowel_texture_softened
python3 tools/dialogue_voice_generator/audition_consonants.py \
  --audition-dir audio_generation/previews/vowel_texture_softened
```

Individual assets retain the current -20 dBFS RMS / -12 dBFS peak targets.
Comparisons match whole-clip RMS downward to the quietest variant, pad differing
tails, and reject clipping. Matching is not a guarantee of equal perceived
loudness. The report audits unchanged event scheduling, continuous pitch steps,
measured levels and source asset hashes. The next choice depends on listening:
less musical/buzzy, audible breath without constant hiss, and retained identity.

### Optional quiet consonants

After generating the texture audition, run:

```sh
python3 tools/dialogue_voice_generator/audition_consonants.py
```

This adds a separate `consonants/` audition directory. Its two-way comparisons
play the combined vowel experiment first, then the same speech with consonants.
Start with `male_quick_comparison.wav` and `female_quick_comparison.wav`.
The existing four-way comparisons remain intact. As with the texture tool,
use a fresh audition directory for a new run; prior results are not overwritten.

Hard t/d/k/p (also b/g) are 24 ms filtered-noise bursts with 2.5 ms attacks and
smooth decays, at 18% of vowel RMS (about -15 dB). Soft s/z/v/f/h/sh/th are 50 ms
shaped-noise sounds at 25% of vowel RMS (-12 dB). Capitals boost consonants by
1.6x (about 4 dB), keeping them below the vowel reference. The noise is seeded by
voice, mood, word and character position. No impulses or added text delays are used.

Spelling rules select soft c before e/i/y and hard c otherwise; ch defaults to
burst-plus-noise, with a few explicit k/sh exceptions; initial x uses z noise,
other x uses k+s. Digraphs and doubled letters trigger once. w/y/j and
nasals/liquids add no consonant events; the existing vowel treatment of y remains.
This is intentionally approximate English spelling. General pronunciation,
silent-letter recognition, and voiced/unvoiced consonant distinctions are not
implemented in this audition.

The preview exports actual character reveal times in `*_reveal_cues.csv`, which
the consonant tool uses for scheduling. It verifies its vowel-only reference
matches the saved combined audition before mixing. Both comparison sections are
RMS-matched downward after summation and checked for clipping. The report lists
every consonant's spelling, sound, onset, duration, emphasis and relative RMS.
The saved audition remains separate; the runtime now uses pre-generated
consonants with the same envelopes, levels and spelling rules.
