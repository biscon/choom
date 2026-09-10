#!/usr/bin/env python3
"""Generate and prepare offline dialogue vocalizations; no engine integration."""
from __future__ import annotations

import argparse
import array
import csv
import datetime as dt
import fcntl
import hashlib
import http.client
import io
import json
import math
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import wave


ROOT = Path(__file__).resolve().parents[2]
TOOL = Path(__file__).resolve().parent
MOODS = ("neutral", "happy", "angry", "afraid", "panicked", "pained", "relieved")
PROFILES = ("male", "female")
VOICES = {"alloy", "ash", "ballad", "coral", "echo", "fable", "onyx", "nova",
          "sage", "shimmer", "verse", "marin", "cedar"}
ENDPOINT = "https://api.openai.com/v1/audio/speech"
PROCESS_VERSION = 4
INITIAL_LIMIT = 112


class GenerationError(Exception):
    pass


class RejectedAudio(GenerationError):
    pass


class FatalAPIError(GenerationError):
    pass


def redacted(value):
    text = str(value)
    key = os.environ.get("OPENAI_API_KEY", "")
    if key:
        text = text.replace(key, "[REDACTED]")
    text = re.sub(r"(?i)Bearer\s+\S+", "Bearer [REDACTED]", text)
    return re.sub(r"\bsk-[A-Za-z0-9_-]+", "[REDACTED]", text)


def announce(message):
    print(redacted(message), flush=True)


def utc_now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def file_hash(path):
    return digest(path.read_bytes())


def atomic_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix="." + path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def atomic_json(path, value):
    atomic_bytes(path, (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode())


def numeric(value, low, high):
    return type(value) in (int, float) and math.isfinite(value) and low <= value <= high


def load_config(path):
    try:
        config = json.loads(path.read_text())
        if config["schema_version"] != 1 or config["model"] != "gpt-4o-mini-tts":
            raise ValueError()
        if set(config["profiles"]) != set(PROFILES) or set(config["moods"]) != set(MOODS):
            raise ValueError()
        texts = []
        for profile in config["profiles"].values():
            if profile["voice"] not in VOICES or not isinstance(profile["instructions"], str):
                raise ValueError()
            if not profile["instructions"].strip():
                raise ValueError()
        for mood in config["moods"].values():
            if not isinstance(mood["instructions"], str) or not mood["instructions"].strip():
                raise ValueError()
            if not numeric(mood.get("tempo", 1.0), 1.0, 3.0):
                raise ValueError()
            fragments = mood["fragments"]
            if not isinstance(fragments, list) or len(fragments) != 8:
                raise ValueError()
            if any(not isinstance(s, str) or not re.fullmatch(r"[a-z]{3,10}", s) for s in fragments):
                raise ValueError()
            texts.extend(fragments)
        if len(set(texts)) != 56:
            raise ValueError()
        if not isinstance(config["common_instructions"], str) or not config["common_instructions"].strip():
            raise ValueError()
        for profile in config["profiles"].values():
            for mood in config["moods"].values():
                if len(" ".join((config["common_instructions"], profile["instructions"], mood["instructions"]))) > 4096:
                    raise ValueError()
        p = config["processing"]
        bounds = {"analysis_window_ms": (5, 20), "sustained_ms": (20, 60),
                  "core_relative_db": (-45, -20), "boundary_relative_db": (-65, -30),
                  "boundary_extension_ms": (50, 300), "dc_rejection_gain_db": (20, 45),
                  "dc_power_fraction_max": (0.4, 0.95),
                  "padding_ms": (10, 20), "fade_ms": (1, 5), "peak_db": (-6, -1),
                  "duration_min_ms": (40, 200), "duration_max_ms": (500, 2000),
                  "audible_peak_min_db": (-90, -50), "gain_warning_db": (6, 40)}
        if set(p) != set(bounds) or any(not numeric(p[k], *b) for k, b in bounds.items()):
            raise ValueError()
        if p["boundary_relative_db"] >= p["core_relative_db"]:
            raise ValueError()
        defaults = config["runtime_defaults"]
        for name, low, high in (("pitch_range", 0.5, 2), ("volume_range", 0, 2),
                                ("characters_per_sound_range", 1, 20)):
            v = defaults[name]
            if not isinstance(v, list) or len(v) != 2 or any(not numeric(x, low, high) for x in v) or v[0] > v[1]:
                raise ValueError()
        if not numeric(defaults["minimum_interval_ms"], 70, 100) or defaults["max_active_clips_per_speaker"] != 1:
            raise ValueError()
        return config
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        raise GenerationError("Invalid configuration: check schema, profiles, moods, 56 unique lowercase fragments, instructions, and numeric settings.") from None


def asset_specs(config):
    result = []
    for profile in PROFILES:
        for mood in MOODS:
            for index, fragment in enumerate(config["moods"][mood]["fragments"], 1):
                identity = config["profiles"][profile]
                payload = {"model": config["model"], "voice": identity["voice"], "input": fragment,
                           "instructions": " ".join((config["common_instructions"], identity["instructions"], config["moods"][mood]["instructions"])),
                           "response_format": "wav", "speed": 1.0}
                result.append({"asset_id": f"{profile}_{mood}_{index:02d}", "profile": profile,
                               "mood": mood, "source_text": fragment, "request": payload,
                               "request_fingerprint": digest(canonical(payload))})
    return result


def paths(root, spec):
    asset_id = spec["asset_id"]
    if not re.fullmatch(r"(?:male|female)_(?:" + "|".join(MOODS) + r")_0[1-8]", asset_id):
        raise GenerationError("Invalid asset ID.")
    if asset_id.rsplit("_", 1)[0] != f"{spec['profile']}_{spec['mood']}":
        raise GenerationError("Asset ID does not match profile/mood.")
    return (root / "assets/audio/dialogue_voices" / spec["profile"] / spec["mood"] / (asset_id + ".wav"),
            root / "audio_generation/raw" / asset_id)


def safe_relative(root, value):
    path = (root / value).resolve()
    if not path.is_relative_to(root.resolve()):
        raise GenerationError("Manifest path escapes repository.")
    return path


def run_audio(arguments):
    env = {k: v for k, v in os.environ.items() if k != "OPENAI_API_KEY"}
    result = subprocess.run(arguments, capture_output=True, env=env, timeout=120)
    if result.returncode:
        raise GenerationError("Audio command failed: " + redacted(result.stderr.decode(errors="replace")[-1200:]))
    return result


def probe(path):
    result = run_audio(["ffprobe", "-v", "error", "-select_streams", "a", "-show_entries",
                        "stream=codec_name,sample_fmt,sample_rate,channels:format=format_name,duration",
                        "-of", "json", str(path)])
    try:
        info = json.loads(result.stdout)
        if len(info["streams"]) != 1 or "wav" not in info["format"]["format_name"]:
            raise ValueError()
        return info
    except (KeyError, ValueError):
        raise GenerationError("Expected a decodable WAV containing one audio stream.") from None


def pcm_samples(path):
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getframerate(), wav.getsampwidth(), wav.getcomptype()) != (1, 48000, 2, "NONE"):
            raise GenerationError("Incorrect final WAV format.")
        count = wav.getnframes()
        data = wav.readframes(count)
    if len(data) != count * 2 or not count:
        raise GenerationError("Truncated or empty WAV.")
    samples = array.array("h", data)
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def measurements(path):
    info = probe(path)
    stream = info["streams"][0]
    if (stream["codec_name"], stream["sample_fmt"], stream["sample_rate"], stream["channels"]) != ("pcm_s16le", "s16", "48000", 1):
        raise GenerationError("Expected mono 48 kHz signed 16-bit PCM WAV.")
    samples = pcm_samples(path)
    peak = max(abs(x) for x in samples)
    if not peak:
        raise GenerationError("No audible content: all samples are zero.")
    return {"duration_ms": len(samples) / 48, "peak_dbfs": 20 * math.log10(peak / 32768),
            "clipped_samples": sum(x <= -32768 or x >= 32767 for x in samples), "frames": len(samples)}


def validate_final(path, settings):
    result = measurements(path)
    if result["clipped_samples"]:
        raise GenerationError("Final audio contains clipped samples.")
    if abs(result["peak_dbfs"] - settings["peak_db"]) > 0.15:
        raise GenerationError("Final peak is outside normalization tolerance.")
    result["warnings"] = []
    if not settings["duration_min_ms"] <= result["duration_ms"] <= settings["duration_max_ms"]:
        result["warnings"].append("duration_outside_80_1200_ms")
    return result


def content_bounds(samples, settings):
    """Locate sustained AC energy; isolated tail noise must not extend a word."""
    window = round(settings["analysis_window_ms"] * 48)
    energy = []
    total, squares = sum(samples), sum(x*x for x in samples)
    dc_fraction = total*total / (len(samples)*squares) if squares else 1.0
    peak = max(abs(x) for x in samples)
    gain = settings["peak_db"] - 20*math.log10(max(1, peak)/32768)
    if gain > settings["dc_rejection_gain_db"] and dc_fraction > settings["dc_power_fraction_max"]:
        raise RejectedAudio("Low-level DC-dominated source; refusing excessive normalization.")
    for start in range(0, len(samples), window):
        chunk = samples[start:start+window]
        mean = sum(chunk)/len(chunk)
        energy.append(math.sqrt(sum((x-mean)**2 for x in chunk)/len(chunk)))
    peak_rms = max(energy)
    if peak_rms < 1:
        raise RejectedAudio("No usable varying speech signal.")
    core = peak_rms * 10**(settings["core_relative_db"]/20)
    boundary = peak_rms * 10**(settings["boundary_relative_db"]/20)
    required = math.ceil(settings["sustained_ms"]/settings["analysis_window_ms"])
    anchors, run = [], 0
    for i, value in enumerate(energy):
        run = run+1 if value >= core else 0
        if run >= required:
            anchors.extend(range(i-required+1, i+1))
    if not anchors:
        raise RejectedAudio("No sustained speech region found.")
    first, last = min(anchors), max(anchors)
    extension = math.ceil(settings["boundary_extension_ms"]/settings["analysis_window_ms"])
    limited = False
    for direction in (-1, 1):
        edge = first if direction < 0 else last
        retained, quiet = edge, 0
        for step in range(1, extension+1):
            i = edge+step*direction
            if not 0 <= i < len(energy): break
            quiet = quiet+1 if energy[i] < boundary else 0
            if energy[i] >= boundary: retained = i
            if quiet >= 2: break
            if step == extension: limited = True
        if direction < 0: first = retained
        else: last = retained
    # Refine within the edge windows, leaving faded waveform boundaries followed
    # by separate safety padding. Only outer silence is removed.
    start, end = first*window, min(len(samples), (last+1)*window)
    threshold = max(1.0, boundary)
    while start < end and abs(samples[start]) < threshold: start += 1
    while end > start and abs(samples[end-1]) < threshold: end -= 1
    return start, end, {"dc_power_fraction": dc_fraction,
                        "core_rms": core, "boundary_rms": boundary,
                        "boundary_extension_limited": limited}


def process_audio(raw, destination, settings, tempo=1.0):
    if not numeric(tempo, 1.0, 3.0):
        raise GenerationError("Tempo must be a finite number between 1.0 and 3.0.")
    probe(raw)
    commands = []
    with tempfile.TemporaryDirectory(prefix="dialogue-process-") as folder:
        work = Path(folder)
        decoded = work / "decoded.wav"
        def ffmpeg(args):
            cmd = ["ffmpeg", "-hide_banner", "-nostdin", "-y", *args]
            commands.append(cmd)
            return run_audio(cmd)
        ffmpeg(["-v", "error", "-i", str(raw), "-map", "0:a:0", "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(decoded)])
        source = measurements(decoded)
        if source["peak_dbfs"] < settings["audible_peak_min_db"]:
            raise GenerationError("Source is silent or below the audible-content threshold.")
        trim_start, trim_end, detection = content_bounds(pcm_samples(decoded), settings)
        if trim_start >= trim_end:
            raise RejectedAudio("No retained speech content.")
        leading_pad = trailing_pad = round(settings["padding_ms"] * 48)
        retained_frames = trim_end-trim_start
        # Time-scale the complete retained utterance before computing fades.
        # Always start from the raw take; reprocessing must not compound tempo.
        retimed = work / "retimed.wav"
        filters = f"atrim=start_sample={trim_start}:end_sample={trim_end},asetpts=PTS-STARTPTS"
        if tempo != 1.0:
            # Above 2x a single atempo stage skips samples. Keep each stage
            # within its overlap/blend range to preserve full articulation.
            if tempo > 2.0:
                stage = math.sqrt(tempo)
                filters += f",atempo={stage:.9g},atempo={stage:.9g}"
            else:
                filters += f",atempo={tempo:.9g}"
        ffmpeg(["-v", "error", "-i", str(decoded), "-af", filters, "-c:a", "pcm_s16le", str(retimed)])
        frames = measurements(retimed)["frames"]
        fade = min(round(settings["fade_ms"] * 48), frames//2)
        filters = (f"afade=t=in:ss=0:ns={fade},afade=t=out:ss={max(0, frames-fade)}:ns={max(1, fade-1)},"
                   f"adelay={leading_pad}S:all=1,apad=pad_len={trailing_pad}")
        trimmed = work / "trimmed.wav"
        ffmpeg(["-v", "error", "-i", str(retimed), "-af", filters, "-c:a", "pcm_s16le", str(trimmed)])
        gain = settings["peak_db"] - measurements(trimmed)["peak_dbfs"]
        final = work / "final.wav"
        ffmpeg(["-v", "error", "-i", str(trimmed), "-af", f"volume={gain:.9f}dB", "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", "-map_metadata", "-1", str(final)])
        result = validate_final(final, settings)
        if gain > settings["gain_warning_db"]:
            result["warnings"].append("large_normalization_gain")
        if source["clipped_samples"]:
            result["warnings"].append("source_clipping_suspected")
        if detection["boundary_extension_limited"]:
            result["warnings"].append("boundary_extension_limit_review")
        if (source["frames"]-retained_frames)/48 > 500:
            result["warnings"].append("large_outer_trim_review")
        output = pcm_samples(final)
        result.update({"source_duration_ms": source["duration_ms"],
                       "removed_outer_ms": (source["frames"]-retained_frames)/48,
                       "tempo": tempo, "retained_duration_ms": retained_frames/48,
                       "retimed_duration_ms": frames/48,
                       "boundary_detection": detection,
                       "leading_boundary_jump": abs(output[leading_pad]-output[leading_pad-1]),
                       "trailing_boundary_jump": abs(output[-trailing_pad-1]-output[-trailing_pad]),
                       "gain_db": gain, "source_peak_dbfs": source["peak_dbfs"],
                       "trim_start_sample": trim_start, "trim_end_sample": trim_end,
                       "leading_padding_added_samples": leading_pad, "trailing_padding_added_samples": trailing_pad,
                       "processing_commands": commands, "processing_settings": settings,
                       "processing_version": PROCESS_VERSION, "processed_at": utc_now()})
        atomic_bytes(destination, final.read_bytes())
        return result


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise FatalAPIError("Unexpected API redirect; request stopped.")


def speech_request(payload, on_success, opener=None, sleep=time.sleep):
    key = os.environ.get("OPENAI_API_KEY")
    if not key:
        raise FatalAPIError("OPENAI_API_KEY is missing or empty.")
    opener = opener or urllib.request.build_opener(NoRedirect()).open
    request = urllib.request.Request(ENDPOINT, data=canonical(payload), method="POST",
                                     headers={"Authorization": "Bearer " + key, "Content-Type": "application/json"})
    for attempt in range(3):
        retry_after = 0
        received_success = False
        try:
            with opener(request, timeout=90) as response:
                received_success = True
                on_success()  # Count HTTP success even if download/decode later fails.
                data = response.read(20 * 1024 * 1024 + 1)
                if len(data) > 20 * 1024 * 1024:
                    raise GenerationError("Speech response exceeded the 20 MiB safety limit.")
                return data, attempt + 1
        except urllib.error.HTTPError as error:
            try:
                body = json.loads(error.read(65536))
                details = body.get("error", {})
                code = str(details.get("code", "")) + " " + str(details.get("type", ""))
            except (ValueError, AttributeError, OSError):
                code = ""
            finally:
                error.close()
            if error.code in (401, 403):
                raise FatalAPIError(f"API authentication/access failure (HTTP {error.code}); stopped.") from None
            if any(x in code.lower() for x in ("quota", "billing", "payment", "credit", "budget")):
                raise FatalAPIError(f"API quota/billing failure (HTTP {error.code}); stopped.") from None
            if error.code not in (408, 429) and not 500 <= error.code <= 599:
                raise FatalAPIError(f"Permanent API failure (HTTP {error.code}); stopped.") from None
            try:
                retry_after = min(30, max(0, float(error.headers.get("Retry-After", "0"))))
            except (ValueError, AttributeError):
                pass
        except (urllib.error.URLError, TimeoutError, OSError, http.client.HTTPException):
            if received_success:
                raise FatalAPIError("Download interrupted after HTTP success; counted against the limit and not automatically retried.") from None
        if attempt == 2:
            raise GenerationError("Transient request failure after 3 attempts.") from None
        delay = max(retry_after, min(30, 2 ** (attempt + 1) + random.uniform(0, 0.5)))
        announce(f"Transient API/transport failure; retry {attempt + 1}/2 in {delay:.1f}s.")
        sleep(delay)


def processing_fingerprint(config, mood):
    return digest(canonical({"version": PROCESS_VERSION, "settings": config["processing"],
                             "tempo": float(config["moods"][mood].get("tempo", 1.0))}))


def matching_raw(root, spec, record):
    for take in reversed(record.get("takes", [])):
        if take["request_fingerprint"] != spec["request_fingerprint"]:
            continue
        raw = safe_relative(root, take["raw_path"])
        if raw.is_file() and file_hash(raw) == take["raw_sha256"]:
            return take
    return None


def plan_assets(root, config, specs, records, regenerate=(), max_requests=112, reprocess=False):
    plan = []
    used = 0
    for spec in specs:
        final, _ = paths(root, spec)
        record = records.get(spec["asset_id"], {})
        action = "request"
        valid = False
        if final.is_file():
            try:
                validate_final(final, config["processing"])
                valid = True
            except (GenerationError, OSError, wave.Error, EOFError):
                pass
        current = record.get("final", {})
        if valid:
            if (current.get("request_fingerprint") == spec["request_fingerprint"] and
                    current.get("processing_fingerprint") == processing_fingerprint(config, spec["mood"]) and
                    current.get("sha256") == file_hash(final)):
                action = "cached"
            else:
                action = "stale"
        elif matching_raw(root, spec, record):
            action = "process"
        elif any(t.get("request_fingerprint") == spec["request_fingerprint"] for t in record.get("takes", [])) or record.get("interrupted_success"):
            action = "blocked"  # Never silently spend again on a completed generation.
        if current.get("status") == "rejected":
            action = "blocked"
        if reprocess:
            action = "process" if matching_raw(root, spec, record) else "blocked"
        if spec["asset_id"] in regenerate:
            action = "request"
        if action == "request":
            if used >= max_requests:
                action = "deferred"
            else:
                used += 1
        plan.append({"spec": spec, "action": action})
    return plan


def runtime_manifest(profile, config, specs, records, root):
    moods = {m: [] for m in MOODS}
    base = root / "assets/audio/dialogue_voices" / profile
    for spec in specs:
        if spec["profile"] != profile:
            continue
        final = records.get(spec["asset_id"], {}).get("final")
        if final and final.get("status") in ("accepted", "warned"):
            path, _ = paths(root, spec)
            if path.is_file() and file_hash(path) == final["sha256"]:
                moods[spec["mood"]].append(path.relative_to(base).as_posix())
    return {"schema_version": 1, "voice_identity_id": profile, "moods": moods,
            **config["runtime_defaults"]}


def csv_bytes(rows, fields):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue().encode()


def build_previews(root, specs, records):
    directory = root / "audio_generation/previews"
    directory.mkdir(parents=True, exist_ok=True)
    groups = [("master", specs)]
    for profile in PROFILES:
        groups.append((profile, [s for s in specs if s["profile"] == profile]))
        for mood in MOODS:
            groups.append((f"{profile}_{mood}", [s for s in specs if s["profile"] == profile and s["mood"] == mood]))
    preview_reports = []
    for name, selection in groups:
        cues, missing = [], []
        stream = io.BytesIO()
        position = 0
        previous = None
        with wave.open(stream, "wb") as output:
            output.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            for spec in selection:
                final, _ = paths(root, spec)
                entry = records.get(spec["asset_id"], {}).get("final", {})
                if entry.get("status") not in ("accepted", "warned") or not final.is_file() or file_hash(final) != entry.get("sha256"):
                    missing.append(spec["asset_id"])
                    continue
                if previous:
                    gap = 72000 if previous["profile"] != spec["profile"] else 43200 if previous["mood"] != spec["mood"] else 12000
                    output.writeframesraw(bytes(gap * 2))
                    position += gap
                with wave.open(str(final), "rb") as wav:
                    data = wav.readframes(wav.getnframes())
                frames = len(data) // 2
                cues.append({"order": len(cues) + 1, "asset_id": spec["asset_id"], "profile": spec["profile"],
                             "mood": spec["mood"], "start_sample": position, "end_sample": position + frames,
                             "start_seconds": f"{position/48000:.6f}", "end_seconds": f"{(position+frames)/48000:.6f}",
                             "warnings": ";".join(entry.get("warnings", []))})
                output.writeframesraw(data)
                position += frames
                previous = spec
        if cues:
            atomic_bytes(directory / f"{name}.wav", stream.getvalue())
            atomic_bytes(directory / f"{name}.csv", csv_bytes(cues, ["order", "asset_id", "profile", "mood", "start_sample", "end_sample", "start_seconds", "end_seconds", "warnings"]))
        else:
            # Do not present empty WAVs (or stale previews) as listening artifacts.
            (directory / f"{name}.wav").unlink(missing_ok=True)
            (directory / f"{name}.csv").unlink(missing_ok=True)
        preview_reports.append({"preview": f"audio_generation/previews/{name}.wav", "clips": len(cues),
                                "status": "complete" if not missing else "partial" if cues else "unavailable",
                                "missing": missing})
    return preview_reports


def read_json(path, default):
    if not path.exists():
        return default
    try:
        return json.loads(path.read_text())
    except (ValueError, OSError):
        raise GenerationError("Cannot read existing authoring state; refusing to reset request accounting.") from None


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--profile", choices=PROFILES)
    parser.add_argument("--mood", choices=MOODS)
    parser.add_argument("--regenerate", action="append", default=[], metavar="ASSET-ID")
    parser.add_argument("--build-previews", action="store_true")
    parser.add_argument("--reprocess", action="store_true", help="Rebuild selected finals from cached raw audio; never call the API")
    parser.add_argument("--max-requests", type=int, default=112)
    args = parser.parse_args(argv)
    if not 0 <= args.max_requests <= 112:
        parser.error("--max-requests must be between 0 and 112")
    if args.reprocess and args.regenerate:
        parser.error("--reprocess and --regenerate are mutually exclusive")
    return args


def execute(args, root=ROOT, config_path=TOOL / "voice_profiles.json"):
    config = load_config(config_path)
    specs = asset_specs(config)
    known = {s["asset_id"] for s in specs}
    if set(args.regenerate) - known:
        raise GenerationError("Unknown regeneration asset ID.")
    selected = [s for s in specs if (not args.profile or s["profile"] == args.profile) and (not args.mood or s["mood"] == args.mood)]
    if args.regenerate:
        selected = [s for s in selected if s["asset_id"] in args.regenerate]
        if len(selected) != len(set(args.regenerate)):
            raise GenerationError("Regeneration IDs conflict with profile/mood filters.")
    manifest_path = root / "tools/dialogue_voice_generator/authoring_manifest.json"
    report_dir = root / "audio_generation/reports"
    ledger_path = report_dir / "request_ledger.json"
    state = read_json(manifest_path, {"schema_version": 1, "initial_successes": 0, "assets": {}})
    ledger = read_json(ledger_path, {"initial_successes": state["initial_successes"], "total_successes": state["initial_successes"]})
    ledger["initial_successes"] = max(ledger["initial_successes"], state["initial_successes"])
    if not args.dry_run:
        missing = [name for name in ("ffmpeg", "ffprobe") if not shutil.which(name)]
        if missing:
            raise GenerationError("Missing required audio tools: " + ", ".join(missing))
    allowance = args.max_requests if args.regenerate else min(args.max_requests, max(0, INITIAL_LIMIT-ledger["initial_successes"]))
    plan = plan_assets(root, config, selected, state["assets"], args.regenerate, allowance, args.reprocess)
    counts = {action: sum(p["action"] == action for p in plan) for action in ("cached", "stale", "process", "request", "deferred", "blocked")}
    announce(f"Plan: {len(selected)} expected final files; {counts['cached']} valid cached finals; {counts['process']} reusable raw files; {counts['request']} new API requests; {counts['stale']} stale; {counts['deferred']} deferred; {counts['blocked']} blocked. Initial successes: {ledger['initial_successes']}/112.")
    if args.dry_run:
        for entry in plan:
            announce(f"{entry['spec']['asset_id']}: {entry['action']}")
        announce("Dry run: completed API requests=0; no persistent writes.")
        return 0
    if counts["request"] and not os.environ.get("OPENAI_API_KEY"):
        raise GenerationError("OPENAI_API_KEY is missing or empty.")
    records = state["assets"]
    version = run_audio(["ffmpeg", "-version"]).stdout.decode().splitlines()[0]
    run = {"started_at": utc_now(), "planned_requests": counts["request"], "completed_requests": 0,
           "ffmpeg_version": version, "assets": [], "listening_review": "pending"}
    aborted = False
    def checkpoint():
        state["initial_successes"] = ledger["initial_successes"]
        atomic_json(ledger_path, ledger)
        atomic_json(manifest_path, state)
        atomic_json(report_dir / "latest_run.json", run)
    try:
        for planned in plan:
            spec, action = planned["spec"], planned["action"]
            asset_id = spec["asset_id"]
            record = records.setdefault(asset_id, {"asset_id": asset_id, "profile": spec["profile"], "mood": spec["mood"], "takes": []})
            row = {"asset_id": asset_id, "action": action, "status": action, "warnings": []}
            run["assets"].append(row)
            if action in ("stale", "deferred", "blocked"):
                row["warnings"] = ["explicit_regeneration_required" if action != "deferred" else "request_limit_reached"]
                continue
            if action == "cached":
                row.update({k: record["final"][k] for k in ("status", "duration_ms", "peak_dbfs", "warnings")})
                continue
            final, raw_dir = paths(root, spec)
            take = None
            try:
                take = matching_raw(root, spec, record)
                if action == "request":
                    def success():
                        ledger["total_successes"] += 1
                        if not args.regenerate:
                            ledger["initial_successes"] += 1
                        run["completed_requests"] += 1
                        record["interrupted_success"] = True
                        checkpoint()
                    data, attempts = speech_request(spec["request"], success)
                    raw_path = raw_dir / f"take_{len(record['takes'])+1:03d}_{digest(data)[:12]}.wav"
                    atomic_bytes(raw_path, data)
                    take = {**spec, "generated_at": utc_now(), "http_attempts": attempts,
                            "raw_path": raw_path.relative_to(root).as_posix(), "raw_sha256": digest(data), "status": "downloaded"}
                    record["takes"].append(take)
                    record.pop("interrupted_success", None)
                    checkpoint()
                previous_duration = record.get("final", {}).get("duration_ms")
                result = process_audio(safe_relative(root, take["raw_path"]), final, config["processing"],
                                       config["moods"][spec["mood"]].get("tempo", 1.0))
                take["status"] = "processed"
                record["final"] = {**result, "previous_duration_ms": previous_duration, "status": "warned" if result["warnings"] else "accepted",
                                   "path": final.relative_to(root).as_posix(), "sha256": file_hash(final),
                                   "request_fingerprint": spec["request_fingerprint"], "raw_sha256": take["raw_sha256"],
                                   "processing_fingerprint": processing_fingerprint(config, spec["mood"]), "ffmpeg_version": version,
                                   "listening_review": "pending"}
                row.update({k: record["final"][k] for k in ("status", "duration_ms", "peak_dbfs", "warnings")})
                announce(f"{asset_id}: {row['status']}, {row['duration_ms']:.0f} ms, {row['peak_dbfs']:.2f} dBFS; completed requests {run['completed_requests']}/{run['planned_requests']}.")
            except (GenerationError, OSError, wave.Error, EOFError, subprocess.TimeoutExpired) as error:
                row.update({"status": "failed", "error": redacted(error)})
                record["last_failure"] = {"at": utc_now(), "error": redacted(error)}
                if isinstance(error, RejectedAudio) and record.get("final"):
                    record["final"]["status"] = "rejected"
                    record["final"]["warnings"] = ["source_quality_rejected"]
                if take and (take.get("status") == "downloaded" or isinstance(error, RejectedAudio)):
                    take["status"] = "rejected"
                announce(f"{asset_id}: failed: {redacted(error)}")
                if isinstance(error, FatalAPIError):
                    aborted = True
                    break
            finally:
                checkpoint()
    except KeyboardInterrupt:
        aborted = True
        announce("Interrupted; saved completed takes and request accounting.")
    finally:
        visited = {row["asset_id"] for row in run["assets"]}
        for entry in plan:
            if entry["spec"]["asset_id"] not in visited:
                run["assets"].append({"asset_id": entry["spec"]["asset_id"], "status": "deferred", "warnings": ["run_stopped"]})
        run["finished_at"] = utc_now()
        checkpoint()
    for profile in PROFILES:
        atomic_json(root / "assets/audio/dialogue_voices" / profile / "voice_manifest.json", runtime_manifest(profile, config, specs, records, root))
    if args.build_previews:
        run["previews"] = build_previews(root, specs, records)
    run["counts"] = {status: sum(row["status"] == status for row in run["assets"]) for status in ("accepted", "warned", "failed", "stale", "deferred", "blocked")}
    checkpoint()
    atomic_json(report_dir / (run["started_at"].replace(":", "-") + ".json"), run)
    rows = []
    for spec in specs:
        record = records.get(spec["asset_id"], {})
        entry = record.get("final", {})
        rows.append({"asset_id": spec["asset_id"], "profile": spec["profile"], "mood": spec["mood"],
                     "status": entry.get("status", "missing"), "duration_ms": entry.get("duration_ms", ""),
                     "peak_dbfs": entry.get("peak_dbfs", ""),
                     "previous_duration_ms": entry.get("previous_duration_ms", ""),
                     "tempo": entry.get("tempo", 1.0),
                     "retained_duration_ms": entry.get("retained_duration_ms", ""),
                     "retimed_duration_ms": entry.get("retimed_duration_ms", ""),
                     "removed_outer_ms": entry.get("removed_outer_ms", ""),
                     "leading_boundary_jump": entry.get("leading_boundary_jump", ""),
                     "trailing_boundary_jump": entry.get("trailing_boundary_jump", ""), "warnings": ";".join(entry.get("warnings", [])),
                     "last_failure": record.get("last_failure", {}).get("error", ""), "listening_review": "pending"})
    atomic_bytes(report_dir / "validation.csv", csv_bytes(rows, list(rows[0])))
    announce(f"Completed API requests: {run['completed_requests']}/{run['planned_requests']}; results: {run['counts']}. Listening review pending.")
    return 1 if aborted or run["counts"]["failed"] else 2 if any(run["counts"][k] for k in ("stale", "deferred", "blocked")) else 0


def main():
    args = parse_args()
    try:
        if args.dry_run:
            return execute(args)
        lock_path = ROOT / "audio_generation/reports/generator.lock"
        lock_path.parent.mkdir(parents=True, exist_ok=True)
        with lock_path.open("a") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise GenerationError("Another dialogue generator is running.") from None
            return execute(args)
    except (GenerationError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        announce("Generation stopped: " + redacted(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
