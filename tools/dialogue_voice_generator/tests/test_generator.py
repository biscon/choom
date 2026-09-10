import array
import contextlib
import copy
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
import urllib.error
import wave


TOOL = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("dialogue_generator", TOOL / "generate_dialogue_voices.py")
g = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(g)


def wav_bytes(seconds=0.3, silence=0.1, amplitude=8000, channels=1, rate=48000):
    samples = array.array("h")
    for n in range(round((seconds + silence * 2) * rate)):
        active = silence * rate <= n < (silence + seconds) * rate
        sample = round(amplitude * math.sin(2 * math.pi * 300 * n / rate)) if active else 0
        samples.extend([sample] * channels)
    if sys.byteorder != "little":
        samples.byteswap()
    data = io.BytesIO()
    with wave.open(data, "wb") as wav:
        wav.setparams((channels, 2, rate, 0, "NONE", "not compressed"))
        wav.writeframes(samples.tobytes())
    return data.getvalue()


class FixtureCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = g.load_config(TOOL / "voice_profiles.json")
        self.specs = g.asset_specs(self.config)
        self.network = patch.object(g.urllib.request, "build_opener", side_effect=AssertionError("Tests must not use live network"))
        self.network.start()
        self.addCleanup(self.network.stop)


class GeneratorTests(FixtureCase):
    def test_config_and_asset_ids(self):
        self.assertEqual(len(self.specs), 112)
        self.assertEqual(len({s["asset_id"] for s in self.specs}), 112)
        self.assertEqual(self.specs[0]["asset_id"], "male_neutral_01")
        self.assertEqual(self.specs[-1]["asset_id"], "female_relieved_08")
        self.assertEqual(self.specs[0]["source_text"], self.specs[56]["source_text"])
        self.assertEqual(self.specs[0]["request"]["response_format"], "wav")
        self.assertEqual(self.specs[0]["request"]["voice"], "onyx")

    def test_invalid_configurations(self):
        changes = [lambda c: c.update(model="tts-1"),
                   lambda c: c["profiles"]["male"].update(voice="unknown"),
                   lambda c: c["moods"]["neutral"].update(fragments=["hello world"] * 8),
                   lambda c: c["moods"]["neutral"].update(fragments=["nehvuh"] * 8),
                   lambda c: c["moods"].pop("angry"),
                   lambda c: c["processing"].update(peak_db=float("nan")),
                   lambda c: c["processing"].update(padding_ms=True),
                   lambda c: c.update(common_instructions=""),
                   lambda c: c["runtime_defaults"].update(pitch_range=[1.1, 0.9])]
        for change in changes:
            config = copy.deepcopy(self.config)
            change(config)
            path = self.root / "config.json"
            path.write_text(json.dumps(config))
            with self.subTest(config=config), self.assertRaises(g.GenerationError):
                g.load_config(path)

    def test_tempo_configuration_defaults_and_validation(self):
        path = self.root / "config.json"
        config = copy.deepcopy(self.config)
        config["moods"]["neutral"].pop("tempo")
        path.write_text(json.dumps(config))
        default = g.load_config(path)
        config["moods"]["neutral"]["tempo"] = 1
        self.assertEqual(g.processing_fingerprint(default, "neutral"),
                         g.processing_fingerprint(config, "neutral"))
        for value in (0.99, 3.01, True, None, "1.4", float("nan"), float("inf")):
            config["moods"]["neutral"]["tempo"] = value
            path.write_text(json.dumps(config))
            with self.subTest(tempo=value), self.assertRaises(g.GenerationError):
                g.load_config(path)

    def test_tempo_changes_only_affected_processing_cache(self):
        changed = copy.deepcopy(self.config)
        changed["moods"]["neutral"]["tempo"] = 1.6
        self.assertEqual(g.asset_specs(changed), self.specs)  # API cache unchanged
        specs = [self.specs[0], self.specs[8], self.specs[56]]
        records = {}
        for spec in specs:
            final, _ = g.paths(self.root, spec)
            g.atomic_bytes(final, wav_bytes())
            records[spec["asset_id"]] = {"final": {
                "sha256": g.file_hash(final), "request_fingerprint": spec["request_fingerprint"],
                "processing_fingerprint": g.processing_fingerprint(self.config, spec["mood"])}}
        with patch.object(g, "validate_final", return_value={}):
            plan = g.plan_assets(self.root, changed, specs, records, max_requests=0)
        self.assertEqual([p["action"] for p in plan], ["stale", "cached", "stale"])

    def test_paths_and_traversal(self):
        final, raw = g.paths(self.root, self.specs[0])
        self.assertEqual(final.relative_to(self.root).as_posix(), "assets/audio/dialogue_voices/male/neutral/male_neutral_01.wav")
        self.assertEqual(raw.name, "male_neutral_01")
        with self.assertRaises(g.GenerationError):
            g.paths(self.root, {**self.specs[0], "asset_id": "../escape"})
        with self.assertRaises(g.GenerationError):
            g.safe_relative(self.root, "../escape.wav")

    def test_request_plan_and_limits(self):
        plan = g.plan_assets(self.root, self.config, self.specs, {}, max_requests=3)
        self.assertEqual(sum(p["action"] == "request" for p in plan), 3)
        self.assertEqual(sum(p["action"] == "deferred" for p in plan), 109)
        plan = g.plan_assets(self.root, self.config, self.specs, {}, max_requests=0)
        self.assertTrue(all(p["action"] == "deferred" for p in plan))

    def test_dry_run_no_writes_or_network(self):
        args = g.parse_args(["--dry-run", "--profile", "female", "--mood", "afraid", "--max-requests", "4"])
        before = list(self.root.rglob("*"))
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(g.execute(args, self.root), 0)
        self.assertEqual(before, list(self.root.rglob("*")))
        self.assertIn("8 expected final files", output.getvalue())
        self.assertIn("4 new API requests", output.getvalue())

    def test_invalid_regeneration_and_filters(self):
        for arguments in (["--dry-run", "--regenerate", "nope"],
                          ["--dry-run", "--profile", "male", "--regenerate", "female_neutral_01"]):
            with self.assertRaises(g.GenerationError):
                g.execute(g.parse_args(arguments), self.root)

    def test_raw_cache_and_missing_raw_guard(self):
        spec = self.specs[0]
        raw = self.root / "audio_generation/raw/take.wav"
        g.atomic_bytes(raw, wav_bytes())
        take = {"request_fingerprint": spec["request_fingerprint"], "raw_path": raw.relative_to(self.root).as_posix(), "raw_sha256": g.file_hash(raw)}
        records = {spec["asset_id"]: {"takes": [take]}}
        self.assertEqual(g.plan_assets(self.root, self.config, [spec], records)[0]["action"], "process")
        raw.write_bytes(b"corrupted")
        self.assertEqual(g.plan_assets(self.root, self.config, [spec], records)[0]["action"], "blocked")
        self.assertEqual(g.plan_assets(self.root, self.config, [spec], records, [spec["asset_id"]])[0]["action"], "request")

    def test_final_cache_and_stale_configuration(self):
        spec = self.specs[0]
        final, _ = g.paths(self.root, spec)
        g.atomic_bytes(final, wav_bytes())
        records = {spec["asset_id"]: {"final": {"sha256": g.file_hash(final), "request_fingerprint": spec["request_fingerprint"], "processing_fingerprint": g.processing_fingerprint(self.config, spec["mood"])}}}
        with patch.object(g, "validate_final", return_value={}):
            self.assertEqual(g.plan_assets(self.root, self.config, [spec], records)[0]["action"], "cached")
            changed = {**spec, "request_fingerprint": "changed"}
            self.assertEqual(g.plan_assets(self.root, self.config, [changed], records)[0]["action"], "stale")

    def test_runtime_manifest_excludes_authoring_details(self):
        spec = self.specs[0]
        final, _ = g.paths(self.root, spec)
        g.atomic_bytes(final, wav_bytes())
        records = {spec["asset_id"]: {"final": {"status": "accepted", "sha256": g.file_hash(final)}}}
        manifest = g.runtime_manifest("male", self.config, self.specs, records, self.root)
        self.assertEqual(manifest["moods"]["neutral"], ["neutral/male_neutral_01.wav"])
        self.assertEqual(manifest["pitch_range"], [0.96, 1.04])
        self.assertEqual(set(manifest["moods"]), set(g.MOODS))
        for forbidden in ("request", "instructions", "source_text", "voice", "model", "tempo"):
            self.assertNotIn(forbidden, manifest)

    def test_atomic_replace_failure_preserves_original(self):
        target = self.root / "asset.wav"
        target.write_bytes(b"original")
        with patch.object(g.os, "replace", side_effect=OSError("simulated")), self.assertRaises(OSError):
            g.atomic_bytes(target, b"new")
        self.assertEqual(target.read_bytes(), b"original")
        self.assertEqual(list(self.root.iterdir()), [target])

    def test_secret_redaction(self):
        with patch.dict(os.environ, {"OPENAI_API_KEY": "test-secret-value"}):
            result = g.redacted("test-secret-value Authorization: Bearer another-secret sk-project-fake")
            for secret in ("test-secret-value", "another-secret", "sk-project-fake"):
                self.assertNotIn(secret, result)

    def test_mock_request_schema_and_success(self):
        callback = Mock()
        response = io.BytesIO(wav_bytes())
        opener = Mock(return_value=response)
        with patch.dict(os.environ, {"OPENAI_API_KEY": "test-secret-value"}):
            data, attempts = g.speech_request(self.specs[0]["request"], callback, opener=opener)
        request = opener.call_args.args[0]
        self.assertEqual(json.loads(request.data)["response_format"], "wav")
        self.assertNotIn(b"test-secret-value", request.data)
        self.assertTrue(data.startswith(b"RIFF"))
        self.assertEqual(attempts, 1)
        callback.assert_called_once()

    def http_error(self, status, code="rate_limit_exceeded"):
        return urllib.error.HTTPError(g.ENDPOINT, status, "secret must not be logged", {"Retry-After": "0"}, io.BytesIO(json.dumps({"error": {"code": code, "message": "test-secret-value"}}).encode()))

    def test_bounded_retries(self):
        opener = Mock(side_effect=[self.http_error(429), self.http_error(503), io.BytesIO(wav_bytes())])
        sleep = Mock()
        with patch.dict(os.environ, {"OPENAI_API_KEY": "test-secret-value"}):
            _, attempts = g.speech_request(self.specs[0]["request"], Mock(), opener, sleep)
        self.assertEqual(attempts, 3)
        self.assertEqual(sleep.call_count, 2)

    def test_permanent_errors_stop_without_leaking_body(self):
        for status, code in ((401, "invalid_api_key"), (429, "insufficient_quota"), (429, "billing_hard_limit_reached"), (400, "bad_request")):
            opener = Mock(side_effect=self.http_error(status, code))
            with patch.dict(os.environ, {"OPENAI_API_KEY": "test-secret-value"}), self.assertRaises(g.FatalAPIError) as caught:
                g.speech_request(self.specs[0]["request"], Mock(), opener, Mock())
            self.assertNotIn("test-secret-value", str(caught.exception))
            self.assertEqual(opener.call_count, 1)

    def test_transport_retry_exhaustion(self):
        opener = Mock(side_effect=urllib.error.URLError("private transport detail"))
        with patch.dict(os.environ, {"OPENAI_API_KEY": "fake"}), self.assertRaises(g.GenerationError):
            g.speech_request(self.specs[0]["request"], Mock(), opener, Mock())
        self.assertEqual(opener.call_count, 3)

    def test_interrupted_success_is_not_retried(self):
        response = Mock()
        response.__enter__ = Mock(return_value=response)
        response.__exit__ = Mock(return_value=False)
        response.read.side_effect = TimeoutError()
        opener, callback = Mock(return_value=response), Mock()
        with patch.dict(os.environ, {"OPENAI_API_KEY": "fake"}), self.assertRaises(g.FatalAPIError):
            g.speech_request(self.specs[0]["request"], callback, opener, Mock())
        callback.assert_called_once()
        opener.assert_called_once()

    def test_initial_limit_survives_missing_intermediate_ledger(self):
        state = {"schema_version": 1, "initial_successes": 112, "assets": {}}
        g.atomic_json(self.root / "tools/dialogue_voice_generator/authoring_manifest.json", state)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            g.execute(g.parse_args(["--dry-run"]), self.root)
        self.assertIn("0 new API requests", output.getvalue())


@unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "Audio tools required")
class AudioTests(FixtureCase):
    def write_samples(self, path, samples):
        with wave.open(str(path), "wb") as wav:
            wav.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            data = array.array("h", samples)
            if sys.byteorder != "little": data.byteswap()
            wav.writeframes(data.tobytes())

    def test_tempo_duration_pitch_padding_and_complete_ending(self):
        source, final = self.root / "source.wav", self.root / "final.wav"
        # A final higher-pitched syllable catches truncation of the utterance.
        samples = [round(8000 * math.sin(2*math.pi*(300 if n < 33600 else 600)*n/48000))
                   for n in range(48000)]
        self.write_samples(source, samples)
        original = source.read_bytes()
        for tempo in (1.4, 1.65, 2.0, 2.5, 3.0):
            result = g.process_audio(source, final, self.config["processing"], tempo)
            self.assertEqual(source.read_bytes(), original)
            self.assertAlmostEqual(result["duration_ms"], 1000/tempo + 30, delta=35)
            self.assertAlmostEqual(result["peak_dbfs"], -3, delta=0.02)
            self.assertEqual(result["clipped_samples"], 0)
            self.assertEqual(result["leading_boundary_jump"], 0)
            self.assertEqual(result["trailing_boundary_jump"], 0)
            output = g.pcm_samples(final)
            self.assertFalse(any(output[:720]))
            self.assertFalse(any(output[-720:]))
            for segment, frequency in ((output[2400:7200], 300), (output[-4080:-1680], 600)):
                crossings = sum(a <= 0 < b for a, b in zip(segment, segment[1:]))
                self.assertAlmostEqual(crossings*48000/len(segment), frequency, delta=21)
            self.assertEqual(result["tempo"], tempo)
            self.assertAlmostEqual(result["removed_outer_ms"],
                                   result["source_duration_ms"]-result["retained_duration_ms"])

    def test_reprocess_tempo_uses_raw_without_network_or_compounding(self):
        state = {"schema_version": 1, "initial_successes": 112, "assets": {}}
        for spec in self.specs[:8]:
            final, raw_dir = g.paths(self.root, spec)
            raw = raw_dir / "take.wav"
            g.atomic_bytes(raw, wav_bytes(seconds=0.6))
            g.atomic_bytes(final, wav_bytes(seconds=0.1))  # wrong prior speed
            state["assets"][spec["asset_id"]] = {"takes": [{
                "raw_path": raw.relative_to(self.root).as_posix(), "raw_sha256": g.file_hash(raw),
                "request_fingerprint": spec["request_fingerprint"], "status": "processed"}]}
        manifest = self.root / "tools/dialogue_voice_generator/authoring_manifest.json"
        g.atomic_json(manifest, state)
        args = g.parse_args(["--reprocess", "--profile", "male", "--mood", "neutral", "--max-requests", "0"])
        with patch.dict(os.environ, {"OPENAI_API_KEY": ""}), patch.object(g, "speech_request", side_effect=AssertionError("No API calls")), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(g.execute(args, self.root), 0)
            first = {s["asset_id"]: g.paths(self.root, s)[0].read_bytes() for s in self.specs[:8]}
            self.assertEqual(g.execute(args, self.root), 0)
        after = json.loads(manifest.read_text())
        plan = g.plan_assets(self.root, self.config, self.specs[:8], after["assets"], max_requests=0)
        self.assertTrue(all(p["action"] == "cached" for p in plan))
        for spec in self.specs[:8]:
            self.assertEqual(g.paths(self.root, spec)[0].read_bytes(), first[spec["asset_id"]])
            entry = after["assets"][spec["asset_id"]]["final"]
            tempo = self.config["moods"]["neutral"]["tempo"]
            self.assertAlmostEqual(entry["duration_ms"], 600/tempo + 30, delta=35)
            self.assertEqual(entry["tempo"], tempo)
        self.assertEqual(after["initial_successes"], 112)
        run = json.loads((self.root / "audio_generation/reports/latest_run.json").read_text())
        self.assertEqual(run["planned_requests"], 0)
        self.assertEqual(run["completed_requests"], 0)

    def test_noise_tail_breaths_and_isolated_impulse(self):
        source = self.root / "noise_tail.wav"
        samples = []
        for n in range(48000):
            amplitude = 80 if n < 3840 or 18240 <= n < 22080 else 8000 if n < 18240 else 0
            samples.append(round(amplitude * math.sin(2*math.pi*300*n/48000)) if amplitude else 30 + n % 3)
        samples[40000] = 1500  # an isolated late spike must not extend the utterance
        self.write_samples(source, samples)
        before = source.read_bytes()
        result = g.process_audio(source, self.root / "final.wav", self.config["processing"])
        self.assertEqual(source.read_bytes(), before)
        self.assertAlmostEqual(result["duration_ms"], 490, delta=3)
        self.assertEqual(result["leading_boundary_jump"], 0)
        self.assertEqual(result["trailing_boundary_jump"], 0)
        self.assertIn("large_outer_trim_review", result["warnings"])

    def test_dc_dominated_source_rejected_and_real_boundary_fades(self):
        source = self.root / "dc.wav"
        self.write_samples(source, [30 + n % 3 for n in range(14400)])
        with self.assertRaises(g.RejectedAudio):
            g.process_audio(source, self.root / "bad.wav", self.config["processing"])
        self.write_samples(source, [round(8000*math.cos(2*math.pi*300*n/48000)) for n in range(14400)])
        result = g.process_audio(source, self.root / "good.wav", self.config["processing"])
        self.assertEqual(result["leading_boundary_jump"], 0)
        self.assertEqual(result["trailing_boundary_jump"], 0)

    def test_reprocess_is_offline_and_rejects_bad_existing_final(self):
        spec = self.specs[0]
        raw = self.root / "audio_generation/raw/take.wav"
        raw.parent.mkdir(parents=True)
        self.write_samples(raw, [30 + n % 3 for n in range(14400)])
        final, _ = g.paths(self.root, spec)
        g.atomic_bytes(final, wav_bytes())
        before = final.read_bytes()
        record = {"takes": [{"raw_path": raw.relative_to(self.root).as_posix(), "raw_sha256": g.file_hash(raw),
                             "request_fingerprint": spec["request_fingerprint"], "status": "processed"}],
                  "final": {"status": "accepted", "sha256": g.file_hash(final), "duration_ms": 500}}
        state = {"schema_version": 1, "initial_successes": 112, "assets": {spec["asset_id"]: record}}
        g.atomic_json(self.root / "tools/dialogue_voice_generator/authoring_manifest.json", state)
        with patch.object(g, "speech_request", side_effect=AssertionError("Reprocessing must never call API")), contextlib.redirect_stdout(io.StringIO()):
            args = g.parse_args(["--reprocess", "--profile", "male", "--mood", "neutral", "--max-requests", "112"])
            self.assertEqual(g.execute(args, self.root), 1)
        state = json.loads((self.root / "tools/dialogue_voice_generator/authoring_manifest.json").read_text())
        self.assertEqual(state["initial_successes"], 112)
        self.assertEqual(state["assets"][spec["asset_id"]]["final"]["status"], "rejected")
        self.assertEqual(g.plan_assets(self.root, self.config, [spec], state["assets"])[0]["action"], "blocked")
        self.assertEqual(final.read_bytes(), before)  # rejected prior bytes retained for review only
        self.assertEqual(g.runtime_manifest("male", self.config, self.specs, state["assets"], self.root)["moods"]["neutral"], [])

    def test_processing_format_trim_peak_and_padding(self):
        source, final = self.root / "source.wav", self.root / "final.wav"
        source.write_bytes(wav_bytes(channels=2, rate=44100))
        before = source.read_bytes()
        result = g.process_audio(source, final, self.config["processing"])
        self.assertEqual(source.read_bytes(), before)
        self.assertAlmostEqual(result["duration_ms"], 330, delta=3)
        self.assertAlmostEqual(result["peak_dbfs"], -3, delta=0.02)
        self.assertEqual(result["clipped_samples"], 0)
        self.assertEqual(result["warnings"], [])

    def test_quiet_content_and_internal_pause_survive(self):
        source = self.root / "quiet.wav"
        source.write_bytes(wav_bytes(seconds=0.4, amplitude=80))
        result = g.process_audio(source, self.root / "quiet_final.wav", self.config["processing"])
        self.assertAlmostEqual(result["duration_ms"], 430, delta=3)
        self.assertIn("large_normalization_gain", result["warnings"])
        source.write_bytes(wav_bytes(seconds=0.2, silence=0.1))
        with wave.open(str(source), "rb") as wav:
            audio = wav.readframes(wav.getnframes())
        with wave.open(str(source), "wb") as wav:
            wav.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            wav.writeframes(audio + bytes(4800 * 2) + audio)
        result = g.process_audio(source, self.root / "pause_final.wav", self.config["processing"])
        self.assertAlmostEqual(result["duration_ms"], 730, delta=3)

    def test_silent_and_malformed_audio_rejected(self):
        source = self.root / "bad.wav"
        for data in (b"not wav", wav_bytes(amplitude=0)):
            source.write_bytes(data)
            with self.assertRaises(g.GenerationError):
                g.process_audio(source, self.root / "bad_final.wav", self.config["processing"])
        self.assertFalse((self.root / "bad_final.wav").exists())

    def test_long_audio_warned_and_retained(self):
        source, final = self.root / "long.wav", self.root / "long_final.wav"
        source.write_bytes(wav_bytes(seconds=1.4))
        result = g.process_audio(source, final, self.config["processing"])
        self.assertIn("duration_outside_80_1200_ms", result["warnings"])
        self.assertTrue(final.exists())

    def test_preview_cues_and_gap_lengths(self):
        chosen = [self.specs[0], self.specs[1], self.specs[8], self.specs[56]]
        records = {}
        for spec in chosen:
            final, _ = g.paths(self.root, spec)
            g.atomic_bytes(final, wav_bytes(seconds=0.1, silence=0))
            records[spec["asset_id"]] = {"final": {"sha256": g.file_hash(final), "status": "accepted"}}
        report = g.build_previews(self.root, self.specs, records)
        self.assertEqual(len(report), 17)
        rows = list(g.csv.DictReader(io.StringIO((self.root / "audio_generation/previews/master.csv").read_text())))
        self.assertEqual([r["asset_id"] for r in rows], [s["asset_id"] for s in chosen])
        self.assertEqual([int(rows[i]["start_sample"]) - int(rows[i-1]["end_sample"]) for i in range(1, 4)], [12000, 43200, 72000])
        self.assertEqual(len(g.pcm_samples(self.root / "audio_generation/previews/master.wav")), int(rows[-1]["end_sample"]))

    def test_empty_previews_are_unavailable(self):
        report = g.build_previews(self.root, self.specs, {})
        self.assertEqual(len(report), 17)
        self.assertTrue(all(r["status"] == "unavailable" for r in report))
        self.assertEqual(list((self.root / "audio_generation/previews").glob("*.wav")), [])

    def test_quota_failure_stops_batch_and_preserves_report(self):
        args = g.parse_args(["--max-requests", "112", "--build-previews"])
        with patch.dict(os.environ, {"OPENAI_API_KEY": "test-secret-value"}), patch.object(g, "speech_request", side_effect=g.FatalAPIError("API quota/billing failure (HTTP 429); stopped.")) as request, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(g.execute(args, self.root), 1)
        request.assert_called_once()
        report = json.loads((self.root / "audio_generation/reports/latest_run.json").read_text())
        self.assertEqual(report["counts"]["failed"], 1)
        self.assertEqual(report["counts"]["deferred"], 111)
        self.assertEqual(report["completed_requests"], 0)
        self.assertTrue(all(p["status"] == "unavailable" for p in report["previews"]))
        for path in self.root.rglob("*.json"):
            self.assertNotIn("test-secret-value", path.read_text())

    def test_successful_but_invalid_audio_is_not_regenerated(self):
        def fake_request(payload, success):
            success()
            return b"invalid WAV response", 1
        args = g.parse_args(["--profile", "male", "--mood", "neutral", "--max-requests", "1"])
        with patch.dict(os.environ, {"OPENAI_API_KEY": "fake"}), patch.object(g, "speech_request", side_effect=fake_request) as request, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(g.execute(args, self.root), 1)
            self.assertEqual(g.execute(g.parse_args(["--max-requests", "0"]), self.root), 1)
            request.assert_called_once()
        state = json.loads((self.root / "tools/dialogue_voice_generator/authoring_manifest.json").read_text())
        self.assertEqual(state["initial_successes"], 1)
        self.assertEqual(state["assets"]["male_neutral_01"]["takes"][0]["status"], "rejected")
        raw = list((self.root / "audio_generation/raw").rglob("*.wav"))
        self.assertEqual(len(raw), 1)
        self.assertEqual(raw[0].read_bytes(), b"invalid WAV response")

    def test_execute_generation_resume_and_regeneration(self):
        calls = []
        def fake_request(payload, success):
            calls.append(payload)
            success()
            return wav_bytes(), 1
        args = g.parse_args(["--profile", "male", "--mood", "neutral", "--max-requests", "1"])
        with patch.dict(os.environ, {"OPENAI_API_KEY": "fake"}), patch.object(g, "speech_request", side_effect=fake_request), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(g.execute(args, self.root), 2)
            self.assertEqual(len(calls), 1)
            final, _ = g.paths(self.root, self.specs[0])
            original = final.read_bytes()
            final.unlink()
            self.assertEqual(g.execute(g.parse_args(["--max-requests", "0"]), self.root), 2)
            self.assertEqual(len(calls), 1)
            self.assertEqual(final.read_bytes(), original)
            regen = g.parse_args(["--regenerate", "male_neutral_01", "--max-requests", "1"])
            self.assertEqual(g.execute(regen, self.root), 0)
        state = json.loads((self.root / "tools/dialogue_voice_generator/authoring_manifest.json").read_text())
        self.assertEqual(len(state["assets"]["male_neutral_01"]["takes"]), 2)
        self.assertEqual(state["initial_successes"], 1)


if __name__ == "__main__":
    unittest.main()
