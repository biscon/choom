import importlib.util
import json
import math
from pathlib import Path
import struct
import tempfile
import unittest
import wave

TOOL = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vowels', TOOL / 'generate_vowel_voices.py')
vowels = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vowels)


class VowelTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((TOOL / 'vowel_profiles.json').read_text())
        # Retained revision-3 texture coverage; production uses stable vowels.
        self.config['synthesis'].update(source='texture', consonants_enabled=True,
            peak_db=-12, rms_db=-20, jitter_percent=.6, shimmer_percent=2, formant_drift_percent=1)

    def test_deterministic_smooth_headroom_and_moods(self):
        vowels.validate(self.config)
        neutral = vowels.synthesize(self.config, 'male', 'neutral', 'ah')
        self.assertEqual(neutral, vowels.synthesize(self.config, 'male', 'neutral', 'ah'))
        for mood in vowels.MOODS:
            pcm = vowels.synthesize(self.config, 'male', mood, 'ah')
            data = struct.unpack('<' + 'h' * (len(pcm)//2), pcm)
            self.assertEqual(len(data), 6720)
            self.assertEqual((data[0], data[-1]), (0, 0))
            self.assertLessEqual(max(map(abs, data)), math.ceil(32767*10**(-12/20)))
            self.assertLess(max(map(abs, data[:48])), 800)
            self.assertLess(max(map(abs, data[-48:])), 200)
            if mood != 'neutral':
                self.assertNotEqual(neutral, pcm)

    def test_independent_identity_controls(self):
        original = vowels.synthesize(self.config, 'male', 'neutral', 'ee')
        self.config['profiles']['male']['fundamental_hz'] += 20
        pitched = vowels.synthesize(self.config, 'male', 'neutral', 'ee')
        self.config['profiles']['male']['fundamental_hz'] -= 20
        self.config['profiles']['male']['formant_scale'] += .1
        tract = vowels.synthesize(self.config, 'male', 'neutral', 'ee')
        self.assertNotEqual(original, pitched)
        self.assertNotEqual(original, tract)
        self.assertNotEqual(pitched, tract)

    def test_generated_manifest_and_format(self):
        self.config['profiles'] = {'female_elderly': self.config['profiles']['female_elderly']}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            vowels.generate(self.config, root)
            manifest = json.loads((root/'female_elderly/voice_manifest.json').read_text())
            self.assertEqual(manifest['schema_version'], 2)
            self.assertEqual(manifest['prosody']['neutral']['wobble_hz'], 2)
            for bank in manifest['moods'].values():
                self.assertEqual(set(bank), set(vowels.VOWELS))
                for relative in bank.values():
                    with wave.open(str(root/'female_elderly'/relative)) as wav:
                        self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()), (1, 2, 48000, 6720))

    def test_measured_breath_ratio_and_level_report(self):
        for profile, mood, expected in [('male', 'neutral', -15),
                                        ('female_young', 'neutral', -17),
                                        ('male_elderly', 'afraid', -12)]:
            tone, air, report = vowels.synthesize_stems(self.config, profile, mood, 'ah')
            self.assertAlmostEqual(20*math.log10(vowels.rms(air)/vowels.rms(tone)), expected, places=6)
            pcm, report = vowels.synthesize_with_report(self.config, profile, mood, 'ah')
            self.assertAlmostEqual(report['breath_to_voice_db'], expected)
            self.assertLessEqual(report['peak_dbfs'], -12+.001)
            self.assertLess(abs(report['dc_offset']), 1e-5)
            self.assertLess(abs(report['first_step']), 30)
            self.assertLess(abs(report['last_step']), 30)
            self.assertGreater(report['rms_dbfs'], -24)
            if not report['peak_limited']:
                self.assertAlmostEqual(report['rms_dbfs'], -20, places=2)

    def test_breathy_resonances_reach_target_without_excessive_drive(self):
        # Strong aspiration made crest non-monotonic at high drive. The old
        # endpoint-only search missed a gentle solution and delivered low RMS.
        _, report = vowels.synthesize_with_report(self.config, 'male_elderly', 'pained', 'eh')
        self.assertFalse(report['peak_limited'])
        self.assertAlmostEqual(report['rms_dbfs'], -20, places=2)
        self.assertLess(report['peak_control_drive'], 1)
        self.assertAlmostEqual(report['breath_to_voice_db'], -12)

    def test_breath_control_is_relative_and_independent(self):
        tone_a, air_a, _ = vowels.synthesize_stems(self.config, 'male', 'neutral', 'ee')
        self.config['profiles']['male']['breath_to_voice_db'] = -12
        tone_b, air_b, _ = vowels.synthesize_stems(self.config, 'male', 'neutral', 'ee')
        self.assertEqual(tone_a, tone_b)
        self.assertAlmostEqual(vowels.rms(air_b)/vowels.rms(air_a), 10**(3/20), places=6)

    def test_upper_resonances_are_not_suppressed(self):
        # Former female ee/oo were almost entirely single low-frequency tones.
        # The accepted softer source replaces upper harmonics with breath.
        # Check the complete sustained signal still carries upper-band energy.
        for vowel in ('ee', 'oo'):
            tone, air, _ = vowels.synthesize_stems(self.config, 'female', 'neutral', vowel)
            sustained = [t+a for t, a in zip(tone, air)][1440:4320]
            upper = vowels.high_pass(vowels.high_pass(sustained, 1000), 1000)[480:]
            self.assertGreater(vowels.rms(upper)/vowels.rms(sustained[480:]), .12)

    def test_irregularity_is_bounded_and_seeded(self):
        args = ('male', 'neutral', 'ah', 'test')
        first = vowels.irregular_curve(vowels.seeded_rng(*args), 5000, .006, 12, 35)
        second = vowels.irregular_curve(vowels.seeded_rng(*args), 5000, .006, 12, 35)
        self.assertEqual(first, second)
        self.assertLessEqual(max(map(abs, first)), .006)
        self.assertLess(max(abs(b-a) for a,b in zip(first,first[1:])), .0001)
        self.assertNotEqual(first[:1000], first[1000:2000])

    def test_four_voice_shout_headroom(self):
        # Four identical aligned peaks are a stricter bound than staggered
        # overlapping vowels. Include maximum shipped mood and capitals gain.
        loudest_mood = max(m['gain'] for m in self.config['moods'].values())
        bound = 4*10**(self.config['synthesis']['peak_db']/20)*.65*loudest_mood*1.25
        self.assertLess(bound, 1)
        for vowel in vowels.VOWELS:
            pcm = vowels.synthesize(self.config, 'male', 'angry', vowel)
            data = struct.unpack('<'+'h'*(len(pcm)//2), pcm)
            self.assertLess(max(map(abs, data))*4*.65*loudest_mood*1.25, 32767)

    def test_invalid_values(self):
        for value in [float('nan'), 0, 900, True]:
            self.config['profiles']['male']['fundamental_hz'] = value
            with self.assertRaises(ValueError):
                vowels.validate(self.config)


if __name__ == '__main__':
    unittest.main()
