#!/usr/bin/env python3
"""Offline male-voice diagnostics; never replaces the game's speech bank.

Compare equal-RMS current speech, vowels only, and vowels without nonlinear
peak shaping (with the original consonants). Also provide a pure -9 dB test.
"""
import argparse
import csv
import json
from pathlib import Path
import shutil
import subprocess

import audition_vowel_texture as audio
import generate_vowel_voices as synth

PROFILES = ('male', 'male_young', 'male_elderly')
VARIANTS = ('current', 'vowels_only', 'no_peak_shaping')


def breath_test(assets, output, executable):
    """Keep the same voiced stem; change only its accompanying breath.

    All variants omit consonants and peak shaping, already tested separately.
    Apply one bank gain to preserve the voiced stem across the three variants;
    only the listening exports receive whole-phrase RMS matching and makeup gain.
    """
    variants = ('usual_breath', 'no_breath', 'darker_breath')
    before = audio.fingerprint(assets)
    config = json.loads(synth.CONFIG.read_text())
    synth.validate(config)
    if config['synthesis'].get('source', 'texture') == 'stable_pulse':
        raise ValueError('The stable source has no breath to compare; use the archived texture audition instead')
    settings = json.loads((assets/'config/dialogue_speech.json').read_text())
    settings['voices'] = {'male': settings['voices']['male']}
    report = dict(production_sha256=before, comparisons={}, assets=[],
                  dark_breath_lowpass_hz=1800, dark_breath_filter_passes=2,
                  requested_export_gain_db=9, export_sample_peak_ceiling_dbfs=-2)
    shutil.copy2(synth.CONFIG, output/synth.CONFIG.name)
    for source in (Path(__file__), Path(synth.__file__), Path(audio.__file__)):
        shutil.copy2(source, output/source.name)
    for variant in variants:
        root = output/'assets'/variant
        (root/'config').mkdir(parents=True)
        (root/'config/dialogue_speech.json').write_text(json.dumps(settings, indent=2)+'\n')
        folder = root/'audio/dialogue_vowels/male'
        shutil.copytree(assets/'audio/dialogue_vowels/male', folder)
        manifest_path = folder/'voice_manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest.pop('consonants', None)
        manifest_path.write_text(json.dumps(manifest, indent=2)+'\n')
    for mood in synth.MOODS:
        bank = {}
        for vowel in synth.VOWELS:
            tone, breath, _ = synth.synthesize_stems(config, 'male', mood, vowel)
            darker = breath
            for _ in range(2):
                darker = [x-y for x, y in zip(darker, synth.high_pass(darker, 1800))]
            # Filtering an enveloped stem leaves a tiny tail. Fade that tail
            # to zero before measuring/restoring its original noise energy.
            fade = round(.005*synth.RATE)
            for n in range(fade):
                darker[-fade+n] *= (fade-1-n)/(fade-1)
            darker = synth.remove_dc(darker)
            scale = synth.rms(breath)/synth.rms(darker)
            darker = [x*scale for x in darker]
            for variant, air in zip(variants, (breath, [0.0]*len(tone), darker)):
                bank[variant, vowel] = [v+b for v, b in zip(tone, air)]
        # Same scalar for every variant/vowel: no nonlinear shaping and no
        # independent per-vowel level changes that could confound the test.
        gain = 32767*10**(config['synthesis']['peak_db']/20)/max(
            max(map(abs, samples)) for samples in bank.values())
        for (variant, vowel), samples in bank.items():
            pcm = [round(x*gain) for x in samples]
            pcm[0] = pcm[-1] = 0
            audio.write_wave(output/'assets'/variant/'audio/dialogue_vowels/male'/mood/f'{vowel}.wav', pcm)
        report['assets'].append(dict(mood=mood, common_bank_gain=gain))
    cues = {}
    for variant in variants:
        subprocess.run([str(executable), '--assets-root', str(output/'assets'/variant),
                        '--output-dir', str(output/'renders'/variant)], check=True)
        with (output/'renders'/variant/'male_speech_cues.csv').open() as file:
            cues[variant] = list(csv.DictReader(file))
    if any(cues[v] != cues[variants[0]] for v in variants):
        raise ValueError('Breath test changed scheduling, pitch or playback gain')
    rows = [r for r in cues[variants[0]] if r['mood'] == 'neutral' and r['line'] == '2']
    start = max(0, round((min(float(r['start_seconds']) for r in rows)-.1)*audio.RATE))
    end = round((max(float(r['end_seconds']) for r in rows)+.1)*audio.RATE)
    for name, mood in (('male_quick', 'neutral'), ('male_neutral', 'neutral'), ('male_panicked', 'panicked')):
        versions = [audio.read_wave(output/'renders'/v/f'male_{mood}_speech.wav') for v in variants]
        if name == 'male_quick':
            versions = [v[start:end] for v in versions]
        target = min(map(synth.rms, versions))
        matched = []
        for samples in versions:
            match_gain = target/synth.rms(samples)
            matched.append([x*match_gain for x in samples])
        peak = max(max(map(abs, v)) for v in matched)
        gain = min(10**(9/20), 32767*10**(-2/20)/peak)
        # One linear boost shared by all three, without limiting/compression.
        boosted = [[x*gain for x in v] for v in matched]
        report['comparisons'][name] = dict(export_gain_db=synth.db(gain),
            versions=audio.write_matched(output, name, boosted, variants))
    if audio.fingerprint(assets) != before:
        raise ValueError('Production assets changed during breath test')
    report['production_unchanged'] = True
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'README.txt').write_text(
        'ORDER: usual breath, no breath, darker breath. One-second gaps.\n'
        'Same base male profile, vowel tone, pitch, timing and mood in each comparison.\n'
        'All three omit consonants and nonlinear peak shaping. Usual breath is therefore\n'
        'an isolated vowel baseline, not an exact copy of the production mix.\n'
        'Darker breath: two 1800 Hz low-pass passes, restored to the original breath RMS.\n'
        'Exports match whole-phrase RMS and add up to 9 dB, limited by -2 dBFS sample peaks.\n'
        'This is linear gain, not a compressor or perceptual loudness normalization.\n'
        'Start with male_quick_comparison.wav. CSVs label boundaries and measured levels.\n'
        'Production assets and game volume are unchanged. No listening verification performed.\n')
    print(f'Breath diagnostics ready: {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-root', type=Path, default=synth.ROOT/'assets')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--breath-test', action='store_true', help='Isolate vowel breath with louder listening exports')
    parser.add_argument('--preview-executable', type=Path, default=synth.ROOT/'cmake-build-debug/dialogue_speech_preview')
    args = parser.parse_args()
    default_name = 'male_breath_diagnostics' if args.breath_test else 'male_rattle_diagnostics'
    assets = args.assets_root.resolve()
    output = (args.output_dir or synth.ROOT/'audio_generation/previews'/default_name).resolve()
    if output == assets or assets in output.parents or output in assets.parents:
        parser.error('Diagnostic output must be separate from game assets')
    output.mkdir(parents=True, exist_ok=False)
    if args.breath_test:
        breath_test(assets, output, args.preview_executable.resolve())
        return
    original = audio.fingerprint(assets)
    config = json.loads(synth.CONFIG.read_text())
    synth.validate(config)
    report = dict(production_sha256=original, unshaped_assets=[], comparisons={}, levels={})
    shutil.copy2(synth.CONFIG, output/synth.CONFIG.name)
    for source in (Path(__file__), Path(synth.__file__), Path(audio.__file__)):
        shutil.copy2(source, output/source.name)
    settings = json.loads((assets/'config/dialogue_speech.json').read_text())
    settings['voices'] = {key: value for key, value in settings['voices'].items() if key in PROFILES}
    if len(settings['voices']) != len(PROFILES):
        raise ValueError('Expected the three authored male profiles')
    for variant in VARIANTS:
        root = output/'assets'/variant
        (root/'config').mkdir(parents=True)
        (root/'config/dialogue_speech.json').write_text(json.dumps(settings, indent=2)+'\n')
        for identity in PROFILES:
            folder = root/'audio/dialogue_vowels'/identity
            shutil.copytree(assets/'audio/dialogue_vowels'/identity, folder)
            if variant == 'vowels_only':
                path = folder/'voice_manifest.json'
                manifest = json.loads(path.read_text())
                manifest.pop('consonants', None)
                path.write_text(json.dumps(manifest, indent=2)+'\n')
            if variant == 'no_peak_shaping':
                for mood in synth.MOODS:
                    raw = {}
                    for vowel in synth.VOWELS:
                        tone, breath, _ = synth.synthesize_stems(config, identity, mood, vowel)
                        mixed = [t+b for t, b in zip(tone, breath)]
                        # Linear gain only: no tanh, compression, clipping or
                        # per-sample multiplication of breath by the tone.
                        gain = 10**(config['synthesis']['rms_db']/20)/synth.rms(mixed)
                        raw[vowel] = [x*gain*32767 for x in mixed]
                    # One common headroom gain for the whole mood (including
                    # consonants) preserves the vowel/consonant level balance.
                    peak = max(max(map(abs, samples)) for samples in raw.values())
                    headroom = min(1.0, 32767*10**(config['synthesis']['peak_db']/20)/peak)
                    for vowel, samples in raw.items():
                        pcm = [round(x*headroom) for x in samples]
                        pcm[0] = pcm[-1] = 0
                        audio.write_wave(folder/mood/f'{vowel}.wav', pcm)
                        report['unshaped_assets'].append(dict(profile=identity, mood=mood, vowel=vowel,
                            headroom_gain_db=synth.db(headroom),
                            rms_dbfs=synth.db(synth.rms(pcm)/32768), peak_dbfs=synth.db(max(map(abs, pcm))/32768)))
                    for path in (folder/mood/'consonants').glob('*.wav'):
                        audio.write_wave(path, [round(x*headroom) for x in audio.read_wave(path)])
        subprocess.run([str(args.preview_executable.resolve()), '--assets-root', str(root),
                        '--output-dir', str(output/'renders'/variant)], check=True)
    for identity in PROFILES:
        cue_sheets = {}
        for variant in VARIANTS:
            with (output/'renders'/variant/f'{identity}_speech_cues.csv').open() as file:
                cue_sheets[variant] = list(csv.DictReader(file))
        def vowel_signature(rows):
            return [(r['mood'], r['line'], r['clip'], r['start_seconds'], r['end_seconds'], r['pitch'])
                    for r in rows if r['clip'] in synth.VOWELS]
        if any(vowel_signature(cue_sheets[v]) != vowel_signature(cue_sheets['current']) for v in VARIANTS):
            raise ValueError('Diagnostic changed vowel scheduling or pitch')
        for mood in ('neutral', 'panicked'):
            name = f'{identity}_{mood}'
            versions = [audio.read_wave(output/'renders'/v/f'{name}_speech.wav') for v in VARIANTS]
            report['comparisons'][name] = audio.write_matched(output, name, versions, VARIANTS)
        # The neutral mood is first, so CSV times are local to its WAV.
        rows = [r for r in cue_sheets['current'] if r['mood'] == 'neutral' and r['line'] == '2']
        start = max(0, round((min(float(r['start_seconds']) for r in rows)-.1)*audio.RATE))
        end = round((max(float(r['end_seconds']) for r in rows)+.1)*audio.RATE)
        versions = [audio.read_wave(output/'renders'/v/f'{identity}_neutral_speech.wav')[start:end] for v in VARIANTS]
        report['comparisons'][f'{identity}_quick'] = audio.write_matched(output, f'{identity}_quick', versions, VARIANTS)
        current = versions[0]
        quiet = [round(x*10**(-9/20)) for x in current]
        audio.write_wave(output/f'{identity}_level_check.wav', current+[0]*audio.RATE+quiet)
        report['levels'][identity] = dict(order=['current', 'same signal -9 dB'],
            first_end_seconds=len(current)/audio.RATE,
            second_start_seconds=len(current)/audio.RATE+1)
    if audio.fingerprint(assets) != original:
        raise ValueError('Game assets changed during diagnostic generation')
    report['production_unchanged'] = True
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'README.txt').write_text(
        'QUICK / FULL COMPARISON ORDER: current, vowels only, no vowel peak shaping.\n'
        'One second separates versions; CSVs label exact boundaries. All three are RMS matched downward.\n'
        'No-peak-shaping keeps the original consonants; a common gain lowers the whole mood\n'
        'for headroom, preserving vowel/consonant RMS balance. Final comparisons match overall RMS.\n'
        'LEVEL CHECK ORDER: current, identical signal -9 dB. Do not turn up playback volume between them.\n'
        'These tests narrow possible causes; they do not prove the cause of an in-game rattle.\n'
        'Game assets, timing, pitch and female voices have not been changed.\n'
        'No listening or interactive GUI verification was performed.\n')
    print(f'Diagnostics ready: {output}')


if __name__ == '__main__':
    main()
