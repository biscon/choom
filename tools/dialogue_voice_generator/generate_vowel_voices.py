#!/usr/bin/env python3
"""Deterministic offline formant vowels. Python standard library; no network/device."""
import argparse
import hashlib
import json
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
ROOT = Path(__file__).resolve().parents[2]
CONFIG = Path(__file__).with_name('vowel_profiles.json')
VOWELS = ('ah', 'eh', 'ee', 'oh', 'oo', 'uh')
MOODS = ('neutral', 'happy', 'angry', 'afraid', 'panicked', 'pained', 'relieved')


def finite(value, low, high):
    return isinstance(value, (float, int)) and not isinstance(value, bool) and math.isfinite(value) and low <= value <= high


def validate(config):
    if config['schema_version'] != 1:
        raise ValueError('Unsupported authoring schema')
    for name, profile in config['profiles'].items():
        if not name or len(name) > 63 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in name):
            raise ValueError('Invalid profile ID')
        for key, lo, hi in [('fundamental_hz', 70, 350), ('formant_scale', .7, 1.5), ('tilt', .5, 3), ('breath_db', -60, -15), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1)]:
            if not finite(profile[key], lo, hi):
                raise ValueError(f'Invalid {name}.{key}')
    for vowel in VOWELS:
        data = config['vowels'][vowel]
        for key, lo, hi in [('formants', 150, 5000), ('bandwidths', 20, 500), ('gains_db', -50, 0)]:
            if len(data[key]) != 3 or not all(finite(x, lo, hi) for x in data[key]):
                raise ValueError(f'Invalid {vowel}.{key}')
    for mood in MOODS:
        data = config['moods'][mood]
        for key, lo, hi in [('pitch_semitones', -3, 3), ('contour_semitones', 0, 6), ('gain', 0, 1.5), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1), ('tilt_offset', -.5, 1), ('breath_offset_db', -10, 15), ('attack_ms', 5, 35)]:
            if not finite(data[key], lo, hi):
                raise ValueError(f'Invalid {mood}.{key}')
    for key, lo, hi in [('duration_ms', 100, 200), ('release_ms', 20, 60), ('peak_db', -30, -18), ('jitter_percent', 0, 1)]:
        if not finite(config['synthesis'][key], lo, hi):
            raise ValueError(f'Invalid synthesis.{key}')


def synthesize(config, identity, mood, vowel):
    profile, emotion, formants = config['profiles'][identity], config['moods'][mood], config['vowels'][vowel]
    settings = config['synthesis']
    seed = int.from_bytes(hashlib.sha256(f'{identity}/{mood}/{vowel}'.encode()).digest()[:8], 'little')
    rng = random.Random(seed)
    frames = round(settings['duration_ms'] * RATE / 1000)
    attack = round(emotion['attack_ms'] * RATE / 1000)
    release = round(settings['release_ms'] * RATE / 1000)
    # Additive, band-limited vocal source, with independently shaped formants.
    # Harmonic amplitudes approximate three resonances; summing in frequency
    # space avoids filter transients at the beginning of these very short vowels.
    fundamental = profile['fundamental_hz']
    tilt = profile['tilt'] + emotion['tilt_offset']
    harmonics = []
    for h in range(1, int(min(8000, RATE/2) / fundamental) + 1):
        freq = h * fundamental
        gain = 0.0
        for f, bw, db in zip(formants['formants'], formants['bandwidths'], formants['gains_db']):
            f *= profile['formant_scale']
            gain += 10 ** (db / 20) / math.sqrt(1 + ((freq-f)/(bw/2)) ** 2)
        harmonics.append((h, gain / h ** tilt))
    scale = sum(g for _, g in harmonics)
    breath = 10 ** ((profile['breath_db'] + emotion['breath_offset_db']) / 20)
    jitter = settings['jitter_percent'] / 100
    phases = (rng.random() * math.tau, rng.random() * math.tau)
    phase, noise = 0.0, 0.0
    samples = []
    for n in range(frames):
        t = n / RATE
        phase += math.tau * fundamental / RATE * (1 + jitter * (.65*math.sin(t*math.tau*7+phases[0]) + .35*math.sin(t*math.tau*13+phases[1])))
        buzz = sum(g * math.sin(h*phase) for h, g in harmonics) / scale
        noise += .35 * (rng.uniform(-1, 1) - noise)
        envelope = 1.0
        if n < attack:
            envelope = .5 - .5 * math.cos(math.pi*n/attack)
        if n >= frames-release:
            envelope *= .5 - .5 * math.cos(math.pi*(frames-1-n)/release)
        samples.append((buzz + breath*noise) * envelope)
    # Equal RMS where possible, with a hard peak ceiling for overlapping vowels.
    # A smooth correction removes DC without disturbing the zero endpoints.
    window = [math.sin(math.pi*n/(frames-1))**2 for n in range(frames)]
    dc = sum(samples) / sum(window)
    samples = [x - dc*w for x, w in zip(samples, window)]
    peak = max(abs(x) for x in samples)
    rms = math.sqrt(sum(x*x for x in samples)/frames)
    gain = min(10**(settings['peak_db']/20)/peak, 10**(-26/20)/rms)
    pcm = [round(x*gain*32767) for x in samples]
    pcm[0] = pcm[-1] = 0
    return struct.pack('<' + 'h'*len(pcm), *pcm)


def generate(config, output):
    validate(config)
    report = {'generator_version': 1, 'config_sha256': hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest(), 'assets': []}
    for identity, profile in config['profiles'].items():
        folder = output / identity
        manifest = {'schema_version': 2, 'voice_identity_id': identity, 'display_name': profile['label'], 'moods': {}, 'prosody': {}}
        for mood in MOODS:
            emotion = config['moods'][mood]
            manifest['moods'][mood] = {}
            prosody = {k: emotion[k] for k in ('pitch_semitones', 'contour_semitones', 'gain', 'wobble_hz', 'wobble_semitones')}
            # Elderly identity wobble remains regular, even in emotional banks.
            if profile['wobble_hz']:
                prosody['wobble_hz'] = profile['wobble_hz']
                prosody['wobble_semitones'] = max(profile['wobble_semitones'], prosody['wobble_semitones'])
            manifest['prosody'][mood] = prosody
            for vowel in VOWELS:
                relative = f'{mood}/{vowel}.wav'
                path = folder / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                pcm = synthesize(config, identity, mood, vowel)
                with wave.open(str(path), 'wb') as wav:
                    wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                    wav.writeframes(pcm)
                manifest['moods'][mood][vowel] = relative
                report['assets'].append({'path': f'{identity}/{relative}', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        (folder / 'voice_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (output / 'generation_manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'Generated {len(report["assets"])} deterministic vowels in {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=CONFIG)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'assets/audio/dialogue_vowels')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    validate(config)
    if args.dry_run:
        print(f'Validated {len(config["profiles"])} profiles; {len(config["profiles"])*42} WAVs; no writes')
    else:
        generate(config, args.output_dir)


if __name__ == '__main__':
    main()
