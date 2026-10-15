#!/usr/bin/env python3
"""Deterministic offline formant vowels. Python standard library; no network/device."""
import argparse
import hashlib
from functools import lru_cache
import json
import math
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
    if config['schema_version'] != 2:
        raise ValueError('Unsupported authoring schema')
    settings = config['synthesis']
    if settings.get('source', 'stable_pulse') != 'stable_pulse':
        raise ValueError('Unsupported synthesis source')
    if not finite(settings.get('output_gain_db', 0), -12, 0):
        raise ValueError('Invalid synthesis.output_gain_db')
    for name, profile in config['profiles'].items():
        if not name or len(name) > 63 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in name):
            raise ValueError('Invalid profile ID')
        for key, lo, hi in [('fundamental_hz', 70, 350), ('formant_scale', .7, 1.5), ('tilt', .5, 3), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1)]:
            if not finite(profile[key], lo, hi):
                raise ValueError(f'Invalid {name}.{key}')
    for vowel in VOWELS:
        data = config['vowels'][vowel]
        if not finite(data.get('rms_offset_db', 0), -6, 6):
            raise ValueError(f'Invalid {vowel}.rms_offset_db')
        for key, lo, hi in [('formants', 150, 5000), ('bandwidths', 20, 500), ('gains_db', -50, 0)]:
            if len(data[key]) != 3 or not all(finite(x, lo, hi) for x in data[key]):
                raise ValueError(f'Invalid {vowel}.{key}')
    for mood in MOODS:
        data = config['moods'][mood]
        if not finite(data.get('output_gain_db', 0), -12, 0):
            raise ValueError(f'Invalid {mood}.output_gain_db')
        if not finite(data.get('word_accent_semitones', 0), 0, 1.5):
            raise ValueError(f'Invalid {mood}.word_accent_semitones')
        for key, lo, hi in [('pitch_semitones', -3, 3), ('contour_semitones', 0, 6), ('gain', 0, 1.5), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1), ('tilt_offset', -.5, 1), ('attack_ms', 5, 35)]:
            if not finite(data[key], lo, hi):
                raise ValueError(f'Invalid {mood}.{key}')
    for key, lo, hi in [('duration_ms', 100, 200), ('release_ms', 20, 60), ('peak_db', -30, -3), ('rms_db', -40, -15)]:
        if not finite(config['synthesis'][key], lo, hi):
            raise ValueError(f'Invalid synthesis.{key}')


def rms(samples):
    return math.sqrt(sum(x*x for x in samples) / len(samples))


def db(value):
    return 20 * math.log10(max(value, 1e-15))


@lru_cache(maxsize=64)
def glottal_table(harmonics, tilt, opening=.60, closing=.16, rounded_closure=False):
    """Band-limited differentiated glottal flow, opening slowly and closing fast.

    Project the asymmetric pulse onto a finite Fourier series, then tabulate it.
    No discontinuous waveform is sampled directly during synthesis. Harmonics
    stop at 8 kHz, well below Nyquist. The original closure is used only
    as the fixed band-gain calibration reference; audible pulses use the
    rounded closure.
    """
    size = 1024
    pulse = []
    for n in range(size):
        phase = n / size
        if phase < opening:
            value = .5 * math.pi / opening * math.sin(math.pi * phase / opening)
        elif phase < opening + closing:
            if rounded_closure:
                # Return to zero over a full half-sine instead of cutting the
                # closing pulse off at its largest negative value.
                value = -.5*math.pi/closing*math.sin(math.pi*(phase-opening)/closing)
            else:
                value = -math.pi / (2*closing) * math.sin(math.pi * (phase-opening) / (2*closing))
        else:
            value = 0
        pulse.append(value)
    coefficients = []
    for h in range(1, harmonics+1):
        scale = 2 / size / h**(tilt-1)
        real = scale * sum(x*math.cos(math.tau*h*n/size) for n, x in enumerate(pulse))
        imag = scale * sum(x*math.sin(math.tau*h*n/size) for n, x in enumerate(pulse))
        coefficients.append((h, real, imag))
    table = [sum(a*math.cos(math.tau*h*n/size) + b*math.sin(math.tau*h*n/size)
                 for h, a, b in coefficients) for n in range(size)]
    scale = 1 / rms(table)
    return tuple(x*scale for x in table)


def resonator(samples, frequency, bandwidth):
    """Constant-peak biquad bandpass with a fixed center frequency."""
    x1 = x2 = y1 = y2 = 0.0
    result = []
    for value in samples:
        f = frequency
        omega = math.tau*f/RATE
        alpha = math.sin(omega) * bandwidth / (2*f)
        norm = 1 / (1+alpha)
        b0 = alpha * norm
        a1, a2 = -2*math.cos(omega)*norm, (1-alpha)*norm
        output = b0*(value-x2) - a1*y1 - a2*y2
        x2, x1, y2, y1 = x1, value, y1, output
        result.append(output)
    return result


def remove_dc(samples):
    # Smooth correction preserves zero endpoints; unlike subtracting a constant,
    # it cannot introduce a step at the edges of the short vowel.
    window = [math.sin(math.pi*n/(len(samples)-1))**2 for n in range(len(samples))]
    correction = sum(samples) / sum(window)
    return [x-correction*w for x, w in zip(samples, window)]


STABLE_PARAMETERS = dict(opening=.58, closing=.24, extra_tilt=.4,
                         voiced_bandwidth_scale=1.25)


def table_sample(table, phase):
    position = phase * len(table)
    lo = int(position)
    return table[lo] + (table[(lo+1) % len(table)]-table[lo])*(position-lo)


def synthesize_tone(config, identity, mood, vowel):
    """Accepted soft pulse: no noise, pulse variation or moving resonances.

    Preserve the old neutral filter calibration; recalibrating with the soft
    pulse would boost its upper harmonics and undo the accepted spectral shape.
    Runtime still supplies phrase intonation and the authored emotional wobble.
    """
    profile, emotion = config['profiles'][identity], config['moods'][mood]
    formants, settings = config['vowels'][vowel], config['synthesis']
    frames = round(settings['duration_ms']*RATE/1000)
    warmup = round(.03*RATE)
    total = frames+warmup
    fundamental = profile['fundamental_hz']
    harmonics = int(8000/fundamental)
    table = glottal_table(harmonics, profile['tilt']+emotion['tilt_offset']+STABLE_PARAMETERS['extra_tilt'],
                          STABLE_PARAMETERS['opening'], STABLE_PARAMETERS['closing'], rounded_closure=True)
    reference_table = glottal_table(harmonics, profile['tilt'])
    source, reference = [], []
    phase = reference_phase = 0.0
    for _ in range(total):
        phase += fundamental/RATE
        if phase >= 1:
            phase -= 1
        source.append(table_sample(table, phase))
        reference_phase = (reference_phase+fundamental/RATE) % 1
        reference.append(table_sample(reference_table, reference_phase))
    tone, band_levels = [0.0]*frames, []
    for f, bw, gain_db in zip(formants['formants'], formants['bandwidths'], formants['gains_db']):
        f *= profile['formant_scale']
        calibration = resonator(reference, f, bw)[warmup:]
        band = resonator(source, f, bw*STABLE_PARAMETERS['voiced_bandwidth_scale'])[warmup:]
        gain = 10**(gain_db/20)/max(rms(calibration), 1e-12)
        band_levels.append(rms(band)*gain)
        for n in range(frames):
            tone[n] += band[n]*gain
    attack = round(emotion['attack_ms']*RATE/1000)
    release = round(settings['release_ms']*RATE/1000)
    for n in range(frames):
        envelope = 1.0
        if n < attack:
            envelope = .5-.5*math.cos(math.pi*n/attack)
        if n >= frames-release:
            envelope *= .5-.5*math.cos(math.pi*(frames-1-n)/release)
        tone[n] *= envelope
    return remove_dc(tone), dict(relative_formant_band_rms_db=[db(x/band_levels[0]) for x in band_levels])


def synthesize_with_report(config, identity, mood, vowel):
    samples, report = synthesize_tone(config, identity, mood, vowel)
    settings = config['synthesis']
    target_rms = settings['rms_db'] + config['vowels'][vowel].get('rms_offset_db', 0)
    peak_gain = 10**(settings['peak_db']/20)/max(abs(x) for x in samples)
    rms_gain = 10**(target_rms/20)/rms(samples)
    gain = min(peak_gain, rms_gain)
    output_db = settings.get('output_gain_db', 0)+config['moods'][mood].get('output_gain_db', 0)
    gain *= 10**(output_db/20)
    pcm = [round(x*gain*32767) for x in samples]
    pcm[0] = pcm[-1] = 0
    quantized = [x/32768 for x in pcm]
    report.update(rms_dbfs=db(rms(quantized)), peak_dbfs=db(max(map(abs, quantized))),
                  peak_limited=peak_gain < rms_gain, target_rms_dbfs=target_rms+output_db,
                  output_gain_db=output_db,
                  dc_offset=sum(quantized)/len(quantized),
                  first_step=pcm[1]-pcm[0], last_step=pcm[-1]-pcm[-2])
    return struct.pack('<'+'h'*len(pcm), *pcm), report


def synthesize(config, identity, mood, vowel):
    return synthesize_with_report(config, identity, mood, vowel)[0]


def generate(config, output):
    validate(config)
    report = {'generator_version': 5, 'config_sha256': hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest(),
              'source': 'stable_pulse', 'pulse': STABLE_PARAMETERS, 'assets': []}
    for identity, profile in config['profiles'].items():
        folder = output / identity
        manifest = {'schema_version': 2, 'voice_identity_id': identity, 'display_name': profile['label'],
                    'continuous_pitch': True, 'moods': {}, 'prosody': {}}
        for mood in MOODS:
            emotion = config['moods'][mood]
            manifest['moods'][mood] = {}
            prosody = {k: emotion[k] for k in ('pitch_semitones', 'contour_semitones', 'gain', 'wobble_hz', 'wobble_semitones')}
            if emotion.get('word_accent_semitones', 0):
                prosody['word_accent_semitones'] = emotion['word_accent_semitones']
            # Elderly identity wobble remains regular, even in emotional banks.
            if profile['wobble_hz']:
                prosody['wobble_hz'] = profile['wobble_hz']
                prosody['wobble_semitones'] = max(profile['wobble_semitones'], prosody['wobble_semitones'])
            manifest['prosody'][mood] = prosody
            for vowel in VOWELS:
                relative = f'{mood}/{vowel}.wav'
                path = folder / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                pcm, measurements = synthesize_with_report(config, identity, mood, vowel)
                with wave.open(str(path), 'wb') as wav:
                    wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                    wav.writeframes(pcm)
                manifest['moods'][mood][vowel] = relative
                report['assets'].append({'path': f'{identity}/{relative}', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), **measurements})
        (folder / 'voice_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (output / 'generation_manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'Generated {len(report["assets"])} deterministic speech assets in {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=CONFIG)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'assets/audio/dialogue_vowels')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    validate(config)
    if args.dry_run:
        count = len(VOWELS)
        print(f'Validated {len(config["profiles"])} profiles; {len(config["profiles"])*len(MOODS)*count} WAVs; no writes')
    else:
        generate(config, args.output_dir)


if __name__ == '__main__':
    main()
