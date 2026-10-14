import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL))
import audition_vowel_texture as audition
sys.path.pop(0)


class TextureAuditionTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((TOOL/'vowel_profiles.json').read_text())

    def test_source_is_repeatable_and_preserves_level_and_edges(self):
        for identity, vowel in [('male', 'ah'), ('female', 'oo')]:
            pcm, report = audition.synthesize(self.config, identity, 'neutral', vowel)
            self.assertEqual((pcm, report), audition.synthesize(self.config, identity, 'neutral', vowel))
            self.assertEqual(len(pcm), 6720)
            self.assertEqual((pcm[0], pcm[-1]), (0, 0))
            self.assertLess(abs(pcm[1]), 30)
            self.assertLess(abs(pcm[-2]), 30)
            self.assertLessEqual(report['peak_dbfs'], -12)
            self.assertAlmostEqual(report['rms_dbfs'], -20, places=2)
            self.assertAlmostEqual(report['breath_to_voice_db'], -15)
            self.assertLess(abs(report['dc_offset']), 1e-5)

    def test_breath_has_more_energy_above_vowel_resonances(self):
        _, old_air, _ = audition.base.legacy_synthesize_stems(self.config, 'male', 'neutral', 'oo')
        _, new_air, _ = audition.source_stems(self.config, 'male', 'neutral', 'oo')
        def upper_fraction(samples):
            high = audition.base.high_pass(audition.base.high_pass(samples, 3000), 3000)
            return audition.base.rms(high)/audition.base.rms(samples)
        # The softened audition intentionally reduces the broad-noise branch,
        # while retaining more high-frequency breath than the production bank.
        self.assertGreater(upper_fraction(new_air), upper_fraction(old_air))

    def test_tilt_is_not_cancelled_by_band_normalization(self):
        _, _, original = audition.source_stems(self.config, 'male', 'neutral', 'ah')
        self.config['moods']['neutral']['tilt_offset'] += .5
        _, _, softer = audition.source_stems(self.config, 'male', 'neutral', 'ah')
        self.assertLess(softer['relative_formant_band_rms_db'][2],
                        original['relative_formant_band_rms_db'][2]-2)

    def test_matching_only_attenuates_and_handles_different_tails(self):
        versions = [[0, 2000, -2000, 0], [0, 1000, -1000, 0],
                    [0, 3000, -3000, 0, 0], [0, 1500, -1500, 0]]
        with tempfile.TemporaryDirectory() as tmp:
            report = audition.write_matched(Path(tmp), 'test', versions)
            self.assertTrue(all(r['gain_db'] <= 0 for r in report))
            self.assertLess(max(r['rms_dbfs'] for r in report)-min(r['rms_dbfs'] for r in report), .01)
            self.assertEqual(len(audition.read_wave(Path(tmp)/'test_four_way.wav')), 4*(5+48000))


if __name__ == '__main__':
    unittest.main()
