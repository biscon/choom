import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL))
import generate_vowel_voices as vowels
sys.path.pop(0)


class StableVowelTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((TOOL/'vowel_profiles.json').read_text())

    def test_sustained_pulse_is_periodic(self):
        # At 120 Hz a period is exactly 400 samples. Compare sustained cycles,
        # away from attack/release and the filter startup transient.
        self.config['profiles']['male']['fundamental_hz'] = 120
        tone, _ = vowels.synthesize_tone(self.config, 'male', 'neutral', 'ah')
        first, next_cycle = tone[2400:4000], tone[2800:4400]
        error = vowels.rms([a-b for a,b in zip(first, next_cycle)])/vowels.rms(first)
        self.assertLess(error, .001)

    def test_linear_level_control_and_deterministic_edges(self):
        for profile in self.config['profiles']:
            for vowel in vowels.VOWELS:
                pcm, report = vowels.synthesize_with_report(self.config, profile, 'neutral', vowel)
                self.assertEqual(pcm, vowels.synthesize(self.config, profile, 'neutral', vowel))
                data = struct.unpack('<'+'h'*(len(pcm)//2), pcm)
                self.assertEqual(len(data), 6720)
                self.assertEqual((data[0], data[-1]), (0, 0))
                self.assertLessEqual(report['peak_dbfs'], self.config['synthesis']['peak_db']+.001)
                self.assertLess(abs(report['dc_offset']), 1e-5)
                tone, _ = vowels.synthesize_tone(self.config, profile, 'neutral', vowel)
                # Output must be a scalar multiple, never a nonlinear limiter.
                scalar = sum(a*b for a,b in zip(tone, data))/sum(x*x for x in tone)
                self.assertLess(max(abs(a*scalar-b) for a,b in zip(tone, data)), 1)

    def test_generation_writes_vowel_only_bank(self):
        self.config['profiles'] = {'male': self.config['profiles']['male']}
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)/'male'
            folder.mkdir()
            vowels.generate(self.config, Path(tmp))
            manifest = json.loads((folder/'voice_manifest.json').read_text())
            self.assertEqual(set(manifest), {'schema_version', 'voice_identity_id', 'display_name', 'continuous_pitch', 'moods', 'prosody'})
            self.assertTrue(manifest['continuous_pitch'])
            self.assertEqual(manifest['prosody']['neutral']['pitch_semitones'], .5)
            self.assertEqual(manifest['prosody']['neutral']['contour_semitones'], .5)
            self.assertEqual(manifest['prosody']['neutral']['word_accent_semitones'], .75)
            self.assertNotIn('word_accent_semitones', manifest['prosody']['happy'])
            self.assertEqual(len(list(folder.rglob('*.wav'))), 42)
            report = json.loads((Path(tmp)/'generation_manifest.json').read_text())
            self.assertEqual(report['source'], 'stable_pulse')
            self.assertEqual(len(report['assets']), 42)

    def test_invalid_source_and_level_offsets(self):
        self.config['synthesis']['source'] = 'unknown'
        with self.assertRaises(ValueError):
            vowels.validate(self.config)
        self.config['synthesis']['source'] = 'stable_pulse'
        self.config['vowels']['ee']['rms_offset_db'] = float('nan')
        with self.assertRaises(ValueError):
            vowels.validate(self.config)


if __name__ == '__main__':
    unittest.main()
