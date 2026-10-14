from pathlib import Path
import sys
import unittest

TOOL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL))
import audition_consonants as consonants
sys.path.pop(0)


class ConsonantAuditionTests(unittest.TestCase):
    def test_spelling_context_digraphs_and_doubled_letters(self):
        events = consonants.word_events
        self.assertEqual(list(events('cat'))[0], (0, 1, 'k'))
        self.assertEqual(list(events('city'))[0], (0, 1, 's'))
        self.assertEqual(list(events('chair'))[0], (0, 2, 'ch'))
        self.assertEqual(list(events('chef'))[0], (0, 2, 'sh'))
        self.assertEqual(list(events('chorus'))[0], (0, 2, 'k'))
        self.assertEqual(list(events('box'))[-1], (2, 1, 'ks'))
        self.assertEqual(list(events('xylophone'))[0], (0, 1, 'z'))
        self.assertEqual(list(events('shh'))[0], (0, 2, 'sh'))
        self.assertEqual(list(events('thin'))[0], (0, 2, 'th'))
        self.assertEqual(list(events('letter')), [(2, 2, 't')])
        self.assertEqual(list(events('why')), [])

    def test_noise_and_bursts_are_quiet_smooth_and_repeatable(self):
        for sound in ('t', 'd', 'k', 'p', 'b', 'g', 's', 'z', 'v', 'f', 'h', 'sh', 'th', 'ch', 'ks'):
            args = (sound, 'male', 'neutral', 'word', 0, 2000)
            samples = consonants.consonant(*args, False)
            self.assertEqual(samples, consonants.consonant(*args, False))
            self.assertEqual((samples[0], samples[-1]), (0, 0))
            self.assertLess(abs(samples[1]), 2)
            self.assertLess(abs(samples[-2]), 2)
            self.assertLess(consonants.base.rms(samples), 650)
            emphasized = consonants.consonant(*args, True)
            self.assertAlmostEqual(consonants.base.rms(emphasized)/consonants.base.rms(samples), 1.6)
            self.assertLess(consonants.base.rms(emphasized), 2000)

    def test_events_follow_reveal_times_without_duplicate_digraphs(self):
        text = 'SH cat'
        rows = [dict(line='1', character=c, start_seconds=str((n+1)/30)) for n, c in enumerate(text)]
        original = [0]*24000
        mixed, events = consonants.mix_consonants(original, rows, 'female', 'neutral', 2000)
        self.assertEqual([e['sound'] for e in events], ['sh', 'k', 't'])
        self.assertEqual([e['start_seconds'] for e in events], [1/30, 4/30, 6/30])
        self.assertEqual([e['emphasized'] for e in events], [True, False, False])
        self.assertEqual(mixed[:1600], original[:1600])
        self.assertNotEqual(mixed, original)
        self.assertEqual(original, [0]*24000)


if __name__ == '__main__':
    unittest.main()
