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

    def test_shipped_source_has_no_noise_or_random_instability(self):
        self.assertEqual(self.config['synthesis']['source'], 'stable_pulse')
        self.assertFalse(self.config['synthesis']['consonants_enabled'])
        tone, air, _ = vowels.synthesize_stems(self.config, 'male', 'neutral', 'ah')
        self.assertEqual(air, [0.0]*len(tone))
        # Old breath/jitter controls must not reintroduce rejected texture.
        self.config['synthesis'].update(jitter_percent=1, shimmer_percent=4, formant_drift_percent=2)
        self.config['profiles']['male']['breath_to_voice_db'] = -12
        self.assertEqual(tone, vowels.synthesize_stems(self.config, 'male', 'neutral', 'ah')[0])

    def test_sustained_pulse_is_periodic(self):
        # At 120 Hz a period is exactly 400 samples. Compare sustained cycles,
        # away from attack/release and the filter startup transient.
        self.config['profiles']['male']['fundamental_hz'] = 120
        tone, _, _ = vowels.synthesize_stems(self.config, 'male', 'neutral', 'ah')
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
                self.assertEqual(report['peak_control_drive'], 0)
                self.assertIsNone(report['breath_to_voice_db'])
                self.assertLessEqual(report['peak_dbfs'], self.config['synthesis']['peak_db']+.001)
                self.assertLess(abs(report['dc_offset']), 1e-5)
                tone, _, _ = vowels.synthesize_stems(self.config, profile, 'neutral', vowel)
                # Output must be a scalar multiple, never a nonlinear limiter.
                scalar = sum(a*b for a,b in zip(tone, data))/sum(x*x for x in tone)
                self.assertLess(max(abs(a*scalar-b) for a,b in zip(tone, data)), 1)

    def test_generation_disables_existing_consonant_bank(self):
        self.config['profiles'] = {'male': self.config['profiles']['male']}
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)/'male'
            folder.mkdir()
            # Regeneration over an old bank must replace its manifest, so
            # leftover consonant WAVs cannot be loaded accidentally.
            (folder/'voice_manifest.json').write_text('{"consonants":{"neutral":{"t":"old.wav"}}}')
            vowels.generate(self.config, Path(tmp))
            manifest = json.loads((folder/'voice_manifest.json').read_text())
            self.assertNotIn('consonants', manifest)
            self.assertTrue(manifest['continuous_pitch'])
            self.assertEqual(len(list(folder.rglob('*.wav'))), 42)
            report = json.loads((Path(tmp)/'generation_manifest.json').read_text())
            self.assertFalse(report['breath_enabled'])
            self.assertFalse(report['consonants_enabled'])
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
