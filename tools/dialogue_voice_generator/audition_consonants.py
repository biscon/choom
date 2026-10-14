#!/usr/bin/env python3
"""Add quiet consonants to an existing texture audition, never to game assets."""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import shutil
import subprocess

import audition_vowel_texture as texture

base = texture.base
RATE = texture.RATE
PARAMETERS = base.CONSONANT_PARAMETERS


def word_events(word):
    """Approximate English spelling, not pronunciation or a phonetic dictionary.

    Emit (character index, consumed letters, sound). Nasals/liquids and w/y/j
    remain silent; existing vowels keep y's current vowel-group behavior.
    """
    lower = word.lower()
    i = 0
    while i < len(lower):
        pair = lower[i:i+2]
        count, sound = 1, None
        if pair == 'wh':
            count = 2
        elif pair in ('sh', 'th', 'ph', 'ck', 'ch'):
            count = 2
            sound = {'sh': 'sh', 'th': 'th', 'ph': 'f', 'ck': 'k', 'ch': 'ch'}[pair]
            if pair == 'ch':
                if lower.startswith(('chor', 'chem', 'charact', 'christ', 'school', 'echo')):
                    sound = 'k'
                elif lower.startswith(('chef', 'chic', 'machin')):
                    sound = 'sh'
        elif lower[i] in 'tdkpbgszvfhq':
            sound = 'k' if lower[i] == 'q' else lower[i]
        elif lower[i] == 'c':
            sound = 's' if i+1 < len(lower) and lower[i+1] in 'eiy' else 'k'
        elif lower[i] == 'x':
            sound = 'z' if i == 0 else 'ks'
        if sound:
            # A doubled spelling is one articulation, not a machine-gun burst.
            if count == 1 and i+1 < len(lower) and lower[i+1] == lower[i]:
                count = 2
            yield i, count, sound
        i += count


consonant = base.synthesize_consonant


def mix_consonants(pcm, rows, identity, mood, vowel_rms):
    mixed, events = list(map(float, pcm)), []
    for line in sorted({r['line'] for r in rows}, key=int):
        characters = [r for r in rows if r['line'] == line]
        text = ''.join(r['character'] for r in characters)
        # The fixed audition corpus is ASCII; reject a changed corpus rather
        # than silently indexing byte/character mappings incorrectly.
        if any(len(r['character']) != 1 for r in characters):
            raise ValueError('Expected one character per reveal')
        for match in re.finditer(r'[A-Za-z]+', text):
            word = match.group()
            emphasized = len(word) > 1 and word.isupper()
            for offset, count, sound in word_events(word):
                start_seconds = float(characters[match.start()+offset]['start_seconds'])
                samples = consonant(sound, identity, mood, word, offset, vowel_rms, emphasized)
                start = round(start_seconds*RATE)
                if start+len(samples) > len(mixed):
                    mixed.extend([0.0]*(start+len(samples)-len(mixed)))
                for n, x in enumerate(samples):
                    mixed[start+n] += x
                events.append(dict(line=line, word=word, spelling=word[offset:offset+count],
                                   sound=sound, start_seconds=start_seconds,
                                   duration_seconds=len(samples)/RATE, emphasized=emphasized,
                                   rms_relative_db=base.db(base.rms(samples)/vowel_rms)))
    if max(map(abs, mixed)) > 32767:
        raise ValueError('Consonant mix clips; reduce authored consonant gain')
    return [round(x) for x in mixed], events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audition-dir', type=Path, default=base.ROOT/'audio_generation/previews/vowel_texture_audition')
    parser.add_argument('--preview-executable', type=Path, default=base.ROOT/'cmake-build-debug/dialogue_speech_preview')
    args = parser.parse_args()
    audition = args.audition_dir.resolve()
    output = audition/'consonants'
    output.mkdir(exist_ok=False)
    for source in (Path(__file__), Path(texture.__file__), Path(base.__file__),
                   Path(__file__).with_name('preview_dialogue_speech.cpp')):
        shutil.copy2(source, output/source.name)
    reference = output/'reference'
    subprocess.run([str(args.preview_executable.resolve()), '--assets-root', str(audition/'source_assets'),
                    '--output-dir', str(reference), '--continuous-pitch'], check=True)
    settings = json.loads((audition/'source_assets/config/dialogue_speech.json').read_text())
    config = json.loads((audition/'authoring_config.json').read_text())
    report = dict(parameters=PARAMETERS, comparisons={}, events={})
    labels = ('combined_vowels', 'combined_with_quiet_consonants')
    for identity, mood in texture.CASES:
        name = f'{identity}_{mood}'
        pcm = texture.read_wave(reference/f'{name}_speech.wav')
        if pcm != texture.read_wave(audition/'renders/combined'/f'{name}_speech.wav'):
            raise ValueError('Reference changed; create a fresh texture audition first')
        with (reference/f'{identity}_reveal_cues.csv').open() as file:
            rows = [r for r in csv.DictReader(file) if r['mood'] == mood]
        # Reference the measured vowel level, including the actual mood/runtime
        # gain. Matching occurs after summation; it cannot turn consonants up.
        bank = audition/'source_assets/audio/dialogue_vowels'/identity/mood
        vowel_rms = sum(base.rms(texture.read_wave(bank/f'{v}.wav')) for v in base.VOWELS)/len(base.VOWELS)
        vowel_rms *= settings['volume']*config['moods'][mood]['gain']
        mixed, events = mix_consonants(pcm, rows, identity, mood, vowel_rms)
        report['events'][name] = events
        report['comparisons'][name] = texture.write_matched(output, name, [pcm, mixed], labels)
        if mood == 'neutral':
            line = [r for r in rows if r['line'] == '2']
            start = max(0, round((float(line[0]['start_seconds'])-.1)*RATE))
            end = round((float(line[-1]['start_seconds'])+.25)*RATE)
            report['comparisons'][f'{identity}_quick'] = texture.write_matched(
                output, f'{identity}_quick', [pcm[start:end], mixed[start:end]], labels)
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    (output/'README.txt').write_text(
        'ORDER: combined vowel experiment, then the same with quiet consonants.\n'
        'Start with male_quick_comparison.wav and female_quick_comparison.wav.\n'
        'Hard consonants use softened noise bursts at about -15 dB relative to vowels.\n'
        'Soft consonants use shaped noise at -12 dB. Capitals add about 4 dB.\n'
        'ch and x can combine a burst and noise; c/ch/x use simple spelling rules.\n'
        'w/y/j and nasals/liquids add no consonants; existing vowel-y behavior remains.\n'
        'Timing uses actual engine character reveals; digraphs/double letters fire once.\n'
        'These rules are approximate English spelling, not a pronunciation dictionary.\n'
        'Pairs are RMS-matched downward. CSVs label section times; report.json lists events.\n'
        'No game assets or runtime behavior changed; no listening verification performed.\n')
    print(f'Consonant audition ready: {output}')


if __name__ == '__main__':
    main()
