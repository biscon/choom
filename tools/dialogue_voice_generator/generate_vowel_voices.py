#!/usr/bin/env python3
"""Deterministic offline formant vowels. Python standard library; no network/device."""
import argparse
import hashlib
from functools import lru_cache
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
    if config['schema_version'] != 2:
        raise ValueError('Unsupported authoring schema')
    for name, profile in config['profiles'].items():
        if not name or len(name) > 63 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in name):
            raise ValueError('Invalid profile ID')
        for key, lo, hi in [('fundamental_hz', 70, 350), ('formant_scale', .7, 1.5), ('tilt', .5, 3), ('breath_to_voice_db', -40, -12), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1)]:
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
    for key, lo, hi in [('duration_ms', 100, 200), ('release_ms', 20, 60), ('peak_db', -30, -12), ('rms_db', -40, -15), ('jitter_percent', 0, 1), ('shimmer_percent', 0, 4), ('formant_drift_percent', 0, 2)]:
        if not finite(config['synthesis'][key], lo, hi):
            raise ValueError(f'Invalid synthesis.{key}')


def rms(samples):
    return math.sqrt(sum(x*x for x in samples) / len(samples))


def db(value):
    return 20 * math.log10(max(value, 1e-15))


def seeded_rng(identity, mood, vowel, channel):
    seed = hashlib.sha256(f'v2/{identity}/{mood}/{vowel}/{channel}'.encode()).digest()
    return random.Random(int.from_bytes(seed[:8], 'little'))


def irregular_curve(rng, frames, depth, min_ms, max_ms):
    """Smooth bounded random targets with irregular spacing, never a vibrato LFO."""
    result = []
    first = rng.uniform(-depth, depth)
    while len(result) < frames:
        last = rng.uniform(-depth, depth)
        count = round(rng.uniform(min_ms, max_ms) * RATE / 1000)
        for n in range(min(count, frames-len(result))):
            t = n / count
            blend = t*t*(3-2*t)
            result.append(first + (last-first)*blend)
        first = last
    return result


@lru_cache(maxsize=64)
def glottal_table(harmonics, tilt):
    """Band-limited differentiated glottal flow, opening slowly and closing fast.

    Project the asymmetric pulse onto a finite Fourier series, then tabulate it.
    No discontinuous waveform is sampled directly during synthesis. Harmonics
    stop at 8 kHz, well below Nyquist even with authored pitch irregularity.
    """
    size = 1024
    opening, closing = .60, .16
    pulse = []
    for n in range(size):
        phase = n / size
        if phase < opening:
            value = .5 * math.pi / opening * math.sin(math.pi * phase / opening)
        elif phase < opening + closing:
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


def high_pass(samples, cutoff):
    pole = math.exp(-math.tau*cutoff/RATE)
    previous = output = 0.0
    result = []
    for value in samples:
        output = pole * (output + value - previous)
        previous = value
        result.append(output)
    return result


def resonator(samples, frequency, bandwidth, drift):
    """Constant-peak biquad bandpass with smoothly moving center frequency."""
    x1 = x2 = y1 = y2 = 0.0
    result = []
    for value, offset in zip(samples, drift):
        f = frequency * (1+offset)
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


def synthesize_stems(config, identity, mood, vowel):
    """Return envelope-shaped voiced/breath stems before final level normalization."""
    profile, emotion, formants = config['profiles'][identity], config['moods'][mood], config['vowels'][vowel]
    settings = config['synthesis']
    frames = round(settings['duration_ms'] * RATE / 1000)
    attack = round(emotion['attack_ms'] * RATE / 1000)
    release = round(settings['release_ms'] * RATE / 1000)
    warmup = round(.03*RATE)
    total = warmup+frames
    pitch = irregular_curve(seeded_rng(identity, mood, vowel, 'pitch'), total,
                            settings['jitter_percent']/100, 12, 35)
    amplitude = irregular_curve(seeded_rng(identity, mood, vowel, 'amplitude'), total,
                                settings['shimmer_percent']/100, 8, 24)
    table = glottal_table(int(8000/profile['fundamental_hz']), profile['tilt']+emotion['tilt_offset'])
    source, noise = [], []
    noise_rng = seeded_rng(identity, mood, vowel, 'aspiration')
    phase = 0.0
    for n in range(total):
        phase = (phase + profile['fundamental_hz']/RATE*(1+pitch[n])) % 1
        position = phase * len(table)
        lo = int(position)
        value = table[lo] + (table[(lo+1) % len(table)]-table[lo])*(position-lo)
        source.append(value*(1+amplitude[n]))
        # Airflow is stronger while the glottis is open, without becoming a
        # completely switched noise gate or an unrelated constant hiss.
        airflow = .75 + .25*math.sin(math.pi*min(phase/.76, 1))**2
        noise.append(noise_rng.uniform(-1, 1)*airflow)
    noise = high_pass(noise, 150)
    voiced, breath = [0.0]*frames, [0.0]*frames
    band_rms = []
    for band, (f, bandwidth, gain_db) in enumerate(zip(formants['formants'], formants['bandwidths'], formants['gains_db'])):
        drift = irregular_curve(seeded_rng(identity, mood, vowel, f'formant{band}'), total,
                                settings['formant_drift_percent']/100, 20, 55)
        f *= profile['formant_scale']
        tone_band = resonator(source, f, bandwidth, drift)[warmup:]
        air_band = resonator(noise, f, bandwidth, drift)[warmup:]
        # Author band gains against measured output, rather than applying a
        # second spectral tilt on top of the already tilted glottal source.
        scale = 10**(gain_db/20) / max(rms(tone_band), 1e-12)
        band_rms.append(rms(tone_band)*scale)
        for n in range(frames):
            voiced[n] += tone_band[n]*scale
            breath[n] += air_band[n]*scale
    for n in range(frames):
        envelope = 1.0
        if n < attack:
            envelope = .5-.5*math.cos(math.pi*n/attack)
        if n >= frames-release:
            envelope *= .5-.5*math.cos(math.pi*(frames-1-n)/release)
        voiced[n] *= envelope
        breath[n] *= envelope*(1+.7*(1-envelope))
    voiced, breath = remove_dc(voiced), remove_dc(breath)
    target = min(-12, profile['breath_to_voice_db']+emotion['breath_offset_db'])
    scale = rms(voiced)*10**(target/20)/max(rms(breath), 1e-12)
    breath = [x*scale for x in breath]
    return voiced, breath, {'target_breath_to_voice_db': target,
                           'breath_to_voice_db': db(rms(breath)/rms(voiced)),
                           'relative_formant_band_rms_db': [db(x/band_rms[0]) for x in band_rms]}


def control_crest(voiced, breath, target_ratio):
    """Gentle offline soft-knee peak control; never a runtime mix limiter.

    Apply the same smooth gain to both stems, then restore the authored breath
    ratio. Search for the least drive that meets the requested peak/RMS ratio.
    This avoids silently delivering quiet, peak-limited assets. A bounded drive
    retains a reported peak-limited fallback for unusually extreme authoring.
    """
    combined = [v+b for v, b in zip(voiced, breath)]
    level = rms(combined)
    ratio = rms(breath)/rms(voiced)
    window = [math.sin(math.pi*n/(len(voiced)-1))**2 for n in range(len(voiced))]
    window_sum = sum(window)

    def candidate(drive):
        tone, air = [], []
        for v, b, value in zip(voiced, breath, combined):
            x = value / level * drive
            gain = math.tanh(x)/x if abs(x) > 1e-12 else 1
            tone.append(v*gain)
            air.append(b*gain)
        for stem in (tone, air):
            correction = sum(stem)/window_sum
            for n, w in enumerate(window):
                stem[n] -= correction*w
        scale = rms(tone)*ratio/rms(air)
        air = [x*scale for x in air]
        mixed = [v+b for v, b in zip(tone, air)]
        crest = max(map(abs, mixed))/rms(mixed)
        return tone, air, crest

    if max(map(abs, combined))/level <= target_ratio:
        return voiced, breath, 0.0
    # Restoring the air ratio can make crest non-monotonic at high drive.
    # Find the first useful bracket; do not assume maximum drive is best.
    low = 0.0
    best, best_drive = (voiced, breath, max(map(abs, combined))/level), 0.0
    for high in (.125, .25, .5, .75, 1.0, 1.5, 2.0, 3.0, 4.0):
        result = candidate(high)
        if result[2] < best[2]:
            best, best_drive = result, high
        if result[2] <= target_ratio:
            best = result
            break
        low = high
    else:
        return best[0], best[1], best_drive
    for _ in range(14):
        mid = (low+high)/2
        result = candidate(mid)
        if result[2] > target_ratio:
            low = mid
        else:
            high, best = mid, result
    return best[0], best[1], high


def synthesize_with_report(config, identity, mood, vowel):
    voiced, breath, report = synthesize_stems(config, identity, mood, vowel)
    settings = config['synthesis']
    target_crest = 10**((settings['peak_db']-settings['rms_db']-.05)/20)
    voiced, breath, drive = control_crest(voiced, breath, target_crest)
    report['peak_control_drive'] = drive
    report['breath_to_voice_db'] = db(rms(breath)/rms(voiced))
    samples = [v+b for v, b in zip(voiced, breath)]
    peak_gain = 10**(settings['peak_db']/20)/max(abs(x) for x in samples)
    rms_gain = 10**(settings['rms_db']/20)/rms(samples)
    gain = min(peak_gain, rms_gain)
    pcm = [round(x*gain*32767) for x in samples]
    pcm[0] = pcm[-1] = 0
    quantized = [x/32768 for x in pcm]
    report.update(rms_dbfs=db(rms(quantized)), peak_dbfs=db(max(map(abs, quantized))),
                  peak_limited=peak_gain < rms_gain, target_rms_dbfs=settings['rms_db'],
                  dc_offset=sum(quantized)/len(quantized),
                  first_step=pcm[1]-pcm[0], last_step=pcm[-1]-pcm[-2])
    return struct.pack('<'+'h'*len(pcm), *pcm), report


def synthesize(config, identity, mood, vowel):
    return synthesize_with_report(config, identity, mood, vowel)[0]


def generate(config, output):
    validate(config)
    report = {'generator_version': 2, 'config_sha256': hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest(), 'assets': []}
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
                pcm, measurements = synthesize_with_report(config, identity, mood, vowel)
                with wave.open(str(path), 'wb') as wav:
                    wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                    wav.writeframes(pcm)
                manifest['moods'][mood][vowel] = relative
                report['assets'].append({'path': f'{identity}/{relative}', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), **measurements})
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
