# Offline dialogue voice generator

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

# Reprocess missing finals from raw cache and rebuild previews, without API calls.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --max-requests 0 --build-previews

# Replace exactly one reported clip; keep all previous raw takes.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --regenerate female_afraid_03 --max-requests 1 --build-previews

# Replace several clips in one bounded run.
python3 tools/dialogue_voice_generator/generate_dialogue_voices.py --regenerate male_angry_02 --regenerate female_pained_06 --max-requests 2 --build-previews
```

`--profile` and `--mood` intersect. When `--regenerate` is present, only those IDs
are selected; conflicting filters and unknown IDs fail before requests. A normal
run skips valid existing finals. Configuration or hash mismatches on valid
finals are reported as stale and require explicit regeneration. Missing/invalid
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

FFmpeg converts sources to mono 48 kHz PCM. Boundary silence detection uses
−60 dBFS and 10 ms minimum silence, preserving internal pauses. Trimming retains
15 ms of safety around detected audio, adding missing padding if speech touches
a file boundary. Three-millisecond edge fades prevent clicks. Measured linear
gain targets −3 dBFS, followed by signed 16-bit PCM WAV encoding. There is no
compression, denoising, reverb, distortion, time stretching, or pitch shifting.

FFprobe and decoded sample inspection validate WAV structure, codec, channels,
sample rate, sample format, complete nonzero content, peak normalization, and
absence of clipped final samples. Source peaks below −75 dBFS are rejected.
Durations outside 80–1200 ms are retained with warnings; gain above 20 dB and
suspected source clipping also receive warnings. Thresholds are configurable.

These checks establish technical properties only. They cannot establish that a
clip sounds human, maintains identity, expresses its intended mood, contains no
recognizable words, or lacks generated room sound. Every clip starts with
`listening_review: "pending"`. No duration correction is applied merely to pass
validation, and no extra generations are purchased to replace warnings.

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
