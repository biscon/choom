#!/usr/bin/env python3
"""Offline four-way texture audition; never installs experimental game assets.

Uses the engine's headless preview for timing, overlap, punctuation and moods.
The source experiment is intentionally separate from the production generator.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import wave

import generate_vowel_voices as base

RATE = base.RATE
CASES = [('male', 'neutral'), ('female', 'neutral'),
         ('male', 'panicked'), ('female', 'panicked')]
VARIANTS = ('baseline', 'source', 'pitch', 'combined')
# Revision 2 retains the airy spectrum while reducing rasp: half the pulse
# irregularity/shape excursion, half the broad-noise weight, and 3 dB less air.
PARAMETERS = base.TEXTURE_PARAMETERS
source_stems = base.synthesize_stems


def synthesize(config, identity, mood, vowel):
    pcm, report = base.synthesize_with_report(config, identity, mood, vowel)
    return list(struct.unpack('<'+'h'*(len(pcm)//2), pcm)), report


def read_wave(path):
    with wave.open(str(path)) as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, RATE):
            raise ValueError(f'Expected mono PCM16 at 48 kHz: {path}')
        return list(struct.unpack('<'+'h'*wav.getnframes(), wav.readframes(wav.getnframes())))


def write_wave(path, pcm):
    if not pcm or max(map(abs, pcm)) > 32767:
        raise ValueError(f'Empty or clipping audition: {path}')
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), 'wb') as wav:
        wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        wav.writeframes(struct.pack('<'+'h'*len(pcm), *pcm))


def write_matched(output, name, versions, labels=VARIANTS):
    if len(versions) != len(labels):
        raise ValueError('Each comparison version needs a label')
    # Identical length keeps RMS comparisons independent of changed tail lengths.
    length = max(map(len, versions))
    versions = [v+[0]*(length-len(v)) for v in versions]
    target = min(map(base.rms, versions))
    combined, report = [], []
    for label, samples in zip(labels, versions):
        gain = target/base.rms(samples)
        pcm = [round(x*gain) for x in samples]
        report.append(dict(variant=label, start_seconds=len(combined)/RATE,
                           end_seconds=(len(combined)+len(pcm))/RATE,
                           gain_db=base.db(gain), rms_dbfs=base.db(base.rms(pcm)/32768),
                           peak_dbfs=base.db(max(map(abs, pcm))/32768)))
        write_wave(output/'individual'/f'{name}_{label}.wav', pcm)
        combined.extend(pcm)
        combined.extend([0]*RATE)
    suffix = 'four_way' if len(labels) == 4 else 'comparison'
    write_wave(output/f'{name}_{suffix}.wav', combined)
    with (output/f'{name}_{suffix}.csv').open('w', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=report[0].keys())
        writer.writeheader()
        writer.writerows(report)
    if max(r['rms_dbfs'] for r in report)-min(r['rms_dbfs'] for r in report) > .01:
        raise ValueError('Loudness matching exceeded 0.01 dB tolerance')
    return report


def fingerprint(root):
    paths = [root/'config/dialogue_speech.json', *sorted((root/'audio/dialogue_vowels').rglob('*'))]
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-root', type=Path, default=base.ROOT/'assets')
    parser.add_argument('--output-dir', type=Path, default=base.ROOT/'audio_generation/previews/vowel_texture_audition')
    parser.add_argument('--preview-executable', type=Path, default=base.ROOT/'cmake-build-debug/dialogue_speech_preview')
    args = parser.parse_args()
    output, assets = args.output_dir.resolve(), args.assets_root.resolve()
    if output == assets or assets in output.parents or output in assets.parents:
        parser.error('Audition output must be separate from production assets')
    # Fresh directory prevents accidentally overwriting an earlier listening baseline.
    output.mkdir(parents=True, exist_ok=False)
    config = json.loads(base.CONFIG.read_text())
    base.validate(config)
    before = fingerprint(assets)
    report = dict(experiment_version=2, parameters=PARAMETERS, production_sha256=before,
                  assets=[], comparisons={}, pitch_audit={})
    (output/'authoring_config.json').write_text(json.dumps(config, indent=2)+'\n')
    # Preserve the exact code used to generate this audition, not just its knobs.
    for source in (Path(__file__), Path(base.__file__), Path(__file__).with_name('preview_dialogue_speech.cpp')):
        shutil.copy2(source, output/source.name)
    for name in ('baseline_assets', 'source_assets'):
        root = output/name
        (root/'config').mkdir(parents=True)
        runtime = json.loads((assets/'config/dialogue_speech.json').read_text())
        runtime['voices'] = {k: v for k, v in runtime['voices'].items() if k in ('male', 'female')}
        (root/'config/dialogue_speech.json').write_text(json.dumps(runtime, indent=2)+'\n')
        for identity in ('male', 'female'):
            shutil.copytree(assets/'audio/dialogue_vowels'/identity, root/'audio/dialogue_vowels'/identity)
    for identity, mood in CASES:
        for vowel in base.VOWELS:
            pcm, metrics = synthesize(config, identity, mood, vowel)
            path = output/'source_assets/audio/dialogue_vowels'/identity/mood/f'{vowel}.wav'
            write_wave(path, pcm)
            report['assets'].append(dict(profile=identity, mood=mood, vowel=vowel,
                                         sha256=hashlib.sha256(path.read_bytes()).hexdigest(), **metrics))
        print(f'Generated source experiment: {identity} {mood}', flush=True)
    for variant in VARIANTS:
        root = 'source_assets' if variant in ('source', 'combined') else 'baseline_assets'
        command = [str(args.preview_executable.resolve()), '--assets-root', str(output/root),
                   '--output-dir', str(output/'renders'/variant)]
        if variant in ('pitch', 'combined'):
            command.append('--continuous-pitch')
        subprocess.run(command, check=True)
    for identity, mood in CASES:
        rows = {}
        for variant in VARIANTS:
            with (output/'renders'/variant/f'{identity}_speech_cues.csv').open() as file:
                rows[variant] = [r for r in csv.DictReader(file) if r['mood'] == mood]
        signature = lambda data: [(r['line'], r['clip'], r['start_seconds'], r['pitch'], r['volume']) for r in data]
        if any(signature(rows[v]) != signature(rows['baseline']) for v in VARIANTS):
            raise ValueError('Experiment changed cue scheduling, vowel choice or starting pitch')
        maximum_step = max(float(r['max_pitch_step']) for r in rows['combined'])
        moving = sum(abs(float(r['last_pitch'])-float(r['pitch'])) > .001 for r in rows['combined'])
        if maximum_step > .001 or not moving:
            raise ValueError('Continuous pitch audit failed')
        report['pitch_audit'][f'{identity}_{mood}'] = dict(maximum_sample_pitch_step=maximum_step, moving_cues=moving)
        for fixed in (False, True):
            suffix = '/fixed_rate' if fixed else ''
            name = f'{identity}_{mood}'+('_fixed_rate' if fixed else '')
            versions = [read_wave(output/'renders'/f'{variant}{suffix}'/f'{identity}_{mood}_speech.wav') for variant in VARIANTS]
            report['comparisons'][name] = write_matched(output, name, versions)
        if mood == 'neutral':
            # Neutral is first in the combined CSV, so its timestamps are local
            # to the neutral WAV. Same crop for all four versions, including tails.
            line = [r for variant in VARIANTS for r in rows[variant] if r['line'] == '2']
            start = max(0, round((min(float(r['start_seconds']) for r in line)-.1)*RATE))
            end = round((max(float(r['end_seconds']) for r in line)+.1)*RATE)
            versions = [read_wave(output/'renders'/v/f'{identity}_neutral_speech.wav')[start:end] for v in VARIANTS]
            report['comparisons'][f'{identity}_quick'] = write_matched(output, f'{identity}_quick', versions)
    if fingerprint(assets) != before:
        raise ValueError('Production assets changed during audition generation')
    report['production_unchanged'] = True
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'README.txt').write_text(
        'FOUR-WAY ORDER: baseline, source/breath only, continuous pitch only, combined.\n'
        'One second of silence separates versions. CSVs give exact section times.\n'
        'Start with male_quick_four_way.wav and female_quick_four_way.wav.\n'
        'Quick text: Yes finally another person! Come closer. Are you hurt?\n'
        'Other files contain four lines; fixed_rate files keep all moods at 30 cps.\n'
        'individual/ contains matched standalone versions for direct A/B playback.\n'
        'Matching uses whole-clip RMS, not perceptual loudness certification.\n'
        'Production assets and runtime are unchanged. Pitch interpolation is audition-only.\n'
        'No listening or interactive GUI verification has been performed.\n')
    print(f'Audition ready: {output}', flush=True)


if __name__ == '__main__':
    main()
