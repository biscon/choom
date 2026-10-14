#!/usr/bin/env python3
"""Render before/boosted/after vowel speech and loudness-matched listening pairs.

Uses the existing headless engine preview so spelling, timing, pitch and overlap
come from the production planner. Never opens a window or audio device.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import struct
import subprocess
import wave

RATE = 48000
CASES = (("male", "neutral"), ("female", "neutral"),
         ("male", "panicked"), ("female", "panicked"),
         ("male_young", "neutral"), ("female_young", "neutral"),
         ("male_elderly", "neutral"), ("female_elderly", "neutral"))


def read_wave(path):
    with wave.open(str(path)) as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, RATE):
            raise ValueError(f'Expected mono PCM16 at 48 kHz: {path}')
        data = wav.readframes(wav.getnframes())
    return list(struct.unpack('<'+'h'*(len(data)//2), data))


def rms(samples):
    return math.sqrt(sum(float(x)*x for x in samples)/len(samples))


def scale(samples, gain):
    result = [round(x*gain) for x in samples]
    if max(map(abs, result)) > 32767:
        raise ValueError('Comparison would clip; refusing to silently limit it')
    return result


def write_comparison(path, sections):
    """One-second silent separators; exact section/gain labels in a CSV."""
    cursor = 0
    with wave.open(str(path), 'wb') as wav, path.with_suffix('.csv').open('w', newline='') as csv_file:
        wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        writer = csv.writer(csv_file)
        writer.writerow(('section', 'start_seconds', 'end_seconds', 'gain_db', 'rms_dbfs', 'peak_dbfs'))
        for label, samples, gain in sections:
            data = scale(samples, gain)
            level = rms(data)/32768
            peak = max(map(abs, data))/32768
            writer.writerow((label, cursor/RATE, (cursor+len(data))/RATE,
                             20*math.log10(gain), 20*math.log10(level), 20*math.log10(peak)))
            wav.writeframes(struct.pack('<'+'h'*len(data), *data))
            wav.writeframes(bytes(RATE*2))
            cursor += len(data)+RATE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-assets', type=Path, required=True)
    parser.add_argument('--assets-root', type=Path, default=Path('assets'))
    parser.add_argument('--preview-executable', type=Path, default=Path('cmake-build-debug/dialogue_speech_preview'))
    parser.add_argument('--output-dir', type=Path, default=Path('audio_generation/previews/vowel_revision_2/comparisons'))
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name, assets in [('before', args.baseline_assets), ('after', args.assets_root)]:
        subprocess.run([str(args.preview_executable.resolve()), '--assets-root', str(assets.resolve()),
                        '--output-dir', str(output/name)], check=True)
    boost = 10**(6/20)
    metrics = []
    for profile, mood in CASES:
        stem = f'{profile}_{mood}'
        before = read_wave(output/'before'/f'{stem}_speech.wav')
        after = read_wave(output/'after'/f'{stem}_speech.wav')
        write_comparison(output/f'{stem}_three_way.wav', [
            ('original', before, 1), ('original_plus_6dB', before, boost), ('revised', after, 1)])
        # Whole-clip RMS includes the same phrases and audition separators. Match
        # downward to the quieter version to retain headroom in both sections.
        target = min(rms(before)*boost, rms(after))
        write_comparison(output/f'{stem}_matched.wav', [
            ('original_boosted_matched', before, target/rms(before)),
            ('revised_matched', after, target/rms(after))])
        metrics.append({'profile': profile, 'mood': mood,
                        'speech_rms_increase_db': 20*math.log10(rms(after)/rms(before))})
    (output/'comparison_report.json').write_text(json.dumps(metrics, indent=2)+'\n')
    (output/'README.txt').write_text(
        'THREE_WAY: original, original +6 dB, revised.\n'
        'MATCHED: original boosted and revised, matched in whole-clip RMS.\n'
        'One second of silence separates versions. CSV files label every section.\n'
        'before/ and after/ contain all six profiles and seven moods, including fixed-rate versions.\n'
        'RMS matching is an objective level comparison, not perceptual loudness certification.\n')
    print(f'Wrote {len(CASES)*2} listening comparisons to {output}')


if __name__ == '__main__':
    main()
