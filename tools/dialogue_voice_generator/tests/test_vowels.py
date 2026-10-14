import importlib.util
import json
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

    def test_deterministic_smooth_headroom_and_moods(self):
        vowels.validate(self.config)
        neutral = vowels.synthesize(self.config, 'male', 'neutral', 'ah')
        self.assertEqual(neutral, vowels.synthesize(self.config, 'male', 'neutral', 'ah'))
        for mood in vowels.MOODS:
            pcm = vowels.synthesize(self.config, 'male', mood, 'ah')
            data = struct.unpack('<' + 'h' * (len(pcm)//2), pcm)
            self.assertEqual(len(data), 6720)
            self.assertEqual((data[0], data[-1]), (0, 0))
            self.assertLessEqual(max(map(abs, data)), 4126)
            self.assertLess(max(map(abs, data[:48])), 400)
            self.assertLess(max(map(abs, data[-48:])), 100)
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

    def test_invalid_values(self):
        for value in [float('nan'), 0, 900, True]:
            self.config['profiles']['male']['fundamental_hz'] = value
            with self.assertRaises(ValueError):
                vowels.validate(self.config)


if __name__ == '__main__':
    unittest.main()
