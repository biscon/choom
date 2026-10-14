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
CONSONANTS = ('t', 'd', 'k', 'p', 'b', 'g', 's', 'z', 'v', 'f', 'h', 'sh', 'th', 'ch', 'ks')
MOODS = ('neutral', 'happy', 'angry', 'afraid', 'panicked', 'pained', 'relieved')


def finite(value, low, high):
    return isinstance(value, (float, int)) and not isinstance(value, bool) and math.isfinite(value) and low <= value <= high


def validate(config):
    if config['schema_version'] != 2:
        raise ValueError('Unsupported authoring schema')
    settings = config['synthesis']
    if settings.get('source', 'texture') not in ('texture', 'stable_pulse'):
        raise ValueError('Unsupported synthesis source')
    if not isinstance(settings.get('consonants_enabled', True), bool):
        raise ValueError('Invalid consonants_enabled')
    if not finite(settings.get('output_gain_db', 0), -12, 0):
        raise ValueError('Invalid synthesis.output_gain_db')
    for name, profile in config['profiles'].items():
        if not name or len(name) > 63 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in name):
            raise ValueError('Invalid profile ID')
        for key, lo, hi in [('fundamental_hz', 70, 350), ('formant_scale', .7, 1.5), ('tilt', .5, 3), ('breath_to_voice_db', -40, -12), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1)]:
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
        for key, lo, hi in [('pitch_semitones', -3, 3), ('contour_semitones', 0, 6), ('gain', 0, 1.5), ('wobble_hz', 0, 8), ('wobble_semitones', 0, 1), ('tilt_offset', -.5, 1), ('breath_offset_db', -10, 15), ('attack_ms', 5, 35)]:
            if not finite(data[key], lo, hi):
                raise ValueError(f'Invalid {mood}.{key}')
    for key, lo, hi in [('duration_ms', 100, 200), ('release_ms', 20, 60), ('peak_db', -30, -3), ('rms_db', -40, -15), ('jitter_percent', 0, 1), ('shimmer_percent', 0, 4), ('formant_drift_percent', 0, 2)]:
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
def glottal_table(harmonics, tilt, opening=.60, closing=.16, rounded_closure=False):
    """Band-limited differentiated glottal flow, opening slowly and closing fast.

    Project the asymmetric pulse onto a finite Fourier series, then tabulate it.
    No discontinuous waveform is sampled directly during synthesis. Harmonics
    stop at 8 kHz, well below Nyquist even with authored pitch irregularity.
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


def legacy_synthesize_stems(config, identity, mood, vowel):
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


TEXTURE_PARAMETERS = dict(cycle_jitter_percent=.6, cycle_amplitude_percent=2,
                  extra_tilt=.4, opening_range=[.57, .59], closing_range=[.20, .22],
                  voiced_bandwidth_scale=1.25, air_bandwidth_scale=3,
                  broadband_air_energy_fraction=.125, broadband_highpass_hz=600,
                  broadband_lowpass_hz=6500, breath_boost_db=3, breath_ceiling_db=-12)

STABLE_PARAMETERS = dict(opening=.58, closing=.24, extra_tilt=.4,
                         voiced_bandwidth_scale=1.25)


def table_sample(table, phase):
    position = phase * len(table)
    lo = int(position)
    return table[lo] + (table[(lo+1) % len(table)]-table[lo])*(position-lo)


def stable_synthesize_stems(config, identity, mood, vowel):
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
    tone, drift, band_levels = [0.0]*frames, [0.0]*total, []
    for f, bw, gain_db in zip(formants['formants'], formants['bandwidths'], formants['gains_db']):
        f *= profile['formant_scale']
        calibration = resonator(reference, f, bw, drift)[warmup:]
        band = resonator(source, f, bw*STABLE_PARAMETERS['voiced_bandwidth_scale'], drift)[warmup:]
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
    return remove_dc(tone), [0.0]*frames, dict(target_breath_to_voice_db=None,
        relative_formant_band_rms_db=[db(x/band_levels[0]) for x in band_levels])


def synthesize_stems(config, identity, mood, vowel):
    if config['synthesis'].get('source', 'texture') == 'stable_pulse':
        return stable_synthesize_stems(config, identity, mood, vowel)
    profile, emotion = config['profiles'][identity], config['moods'][mood]
    formants, settings = config['vowels'][vowel], config['synthesis']
    frames = round(settings['duration_ms']*RATE/1000)
    warmup = round(.03*RATE)
    total = frames+warmup
    fundamental = profile['fundamental_hz']
    harmonics = int(8000/fundamental)
    tilt = profile['tilt']+emotion['tilt_offset']+TEXTURE_PARAMETERS['extra_tilt']
    tables = [glottal_table(harmonics, tilt, opening, closing)
              for opening, closing in zip(TEXTURE_PARAMETERS['opening_range'], TEXTURE_PARAMETERS['closing_range'])]
    # A fixed neutral reference calibrates each vowel's resonances. Do not
    # renormalize the experimental bands: that would undo softer upper harmonics.
    reference_table = glottal_table(harmonics, profile['tilt'])
    rng = seeded_rng(identity, mood, vowel, 'texture-cycle-v1')
    noise_rng = seeded_rng(identity, mood, vowel, 'texture-air-v1')
    slow_pitch = irregular_curve(seeded_rng(identity, mood, vowel, 'pitch'),
                                     total, settings['jitter_percent']/100, 12, 35)
    source, reference, noise = [], [], []
    phase = reference_phase = 0.0
    previous_shape = shape = rng.random()
    previous_amplitude = amplitude = 1.0
    period_scale = 1.0
    for n in range(total):
        phase += fundamental/RATE*(1+slow_pitch[n])*period_scale
        if phase >= 1:
            phase -= 1
            previous_shape, shape = shape, rng.random()
            previous_amplitude = amplitude
            amplitude = 1+rng.uniform(-1, 1)*TEXTURE_PARAMETERS['cycle_amplitude_percent']/100
            period_scale = 1+rng.uniform(-1, 1)*TEXTURE_PARAMETERS['cycle_jitter_percent']/100
        blend = phase*phase*(3-2*phase)
        morph = previous_shape+(shape-previous_shape)*blend
        strength = previous_amplitude+(amplitude-previous_amplitude)*blend
        low, high = (table_sample(table, phase) for table in tables)
        source.append((low+(high-low)*morph)*strength)
        reference_phase = (reference_phase+fundamental/RATE) % 1
        reference.append(table_sample(reference_table, reference_phase))
        noise.append(noise_rng.uniform(-1, 1)*(.85+.15*math.sin(math.pi*min(phase/.8, 1))**2))
    noise = high_pass(noise, 150)
    tone, air = [0.0]*frames, [0.0]*frames
    band_levels = []
    for band, (f, bw, gain) in enumerate(zip(formants['formants'], formants['bandwidths'], formants['gains_db'])):
        f *= profile['formant_scale']
        drift = irregular_curve(seeded_rng(identity, mood, vowel, f'formant{band}'),
                                    total, settings['formant_drift_percent']/100, 20, 55)
        calibration = resonator(reference, f, bw, [0.0]*total)[warmup:]
        voiced_band = resonator(source, f, bw*TEXTURE_PARAMETERS['voiced_bandwidth_scale'], drift)[warmup:]
        breath_band = resonator(noise, f, bw*TEXTURE_PARAMETERS['air_bandwidth_scale'], drift)[warmup:]
        gain = 10**(gain/20)/max(rms(calibration), 1e-12)
        band_levels.append(rms(voiced_band)*gain)
        for n in range(frames):
            tone[n] += voiced_band[n]*gain
            air[n] += breath_band[n]*gain
    broad = high_pass(noise, TEXTURE_PARAMETERS['broadband_highpass_hz'])
    high = high_pass(broad, TEXTURE_PARAMETERS['broadband_lowpass_hz'])
    broad = [x-y for x, y in zip(broad, high)][warmup:]
    narrow_scale, broad_scale = 1/rms(air), 1/rms(broad)
    fraction = TEXTURE_PARAMETERS['broadband_air_energy_fraction']
    # Nominal energy weights before mixing; branches are correlated, so measure
    # and normalize the final breath stem below instead of assuming independence.
    air = [a*narrow_scale*math.sqrt(1-fraction)+b*broad_scale*math.sqrt(fraction)
           for a, b in zip(air, broad)]
    attack = round(emotion['attack_ms']*RATE/1000)
    release = round(settings['release_ms']*RATE/1000)
    for n in range(frames):
        envelope = 1.0
        if n < attack:
            envelope = .5-.5*math.cos(math.pi*n/attack)
        if n >= frames-release:
            envelope *= .5-.5*math.cos(math.pi*(frames-1-n)/release)
        tone[n] *= envelope
        air[n] *= envelope*(1+.7*(1-envelope))
    tone, air = remove_dc(tone), remove_dc(air)
    target = min(TEXTURE_PARAMETERS['breath_ceiling_db'], profile['breath_to_voice_db']
                 +emotion['breath_offset_db']+TEXTURE_PARAMETERS['breath_boost_db'])
    scale = rms(tone)*10**(target/20)/rms(air)
    air = [x*scale for x in air]
    return tone, air, dict(target_breath_to_voice_db=target,
                          relative_formant_band_rms_db=[db(x/band_levels[0]) for x in band_levels])


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
    stable = settings.get('source', 'texture') == 'stable_pulse'
    target_rms = settings['rms_db'] + (config['vowels'][vowel].get('rms_offset_db', 0) if stable else 0)
    drive = 0.0
    if not stable:
        target_crest = 10**((settings['peak_db']-target_rms-.05)/20)
        voiced, breath, drive = control_crest(voiced, breath, target_crest)
    report['peak_control_drive'] = drive
    report['breath_to_voice_db'] = None if stable else db(rms(breath)/rms(voiced))
    report['source'] = settings.get('source', 'texture')
    samples = [v+b for v, b in zip(voiced, breath)]
    peak_gain = 10**(settings['peak_db']/20)/max(abs(x) for x in samples)
    rms_gain = 10**(target_rms/20)/rms(samples)
    gain = min(peak_gain, rms_gain)
    output_db = (settings.get('output_gain_db', 0)+config['moods'][mood].get('output_gain_db', 0)) if stable else 0
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


CONSONANT_PARAMETERS = dict(hard_relative_rms=.18, noise_relative_rms=.25,
                  emphasis_gain=1.6, hard_duration_ms=24, noise_duration_ms=50,
                  hard_attack_ms=2.5, noise_attack_ms=8, noise_release_ms=15)


def filtered_noise(noise, low, high):
    signal = high_pass(noise, low)
    upper = high_pass(signal, high)
    return [x-y for x, y in zip(signal, upper)]


def synthesize_consonant(sound, identity, mood, word, position, vowel_rms, emphasized):
    if sound in ('ch', 'ks'):
        hard, soft = ('t', 'sh') if sound == 'ch' else ('k', 's')
        first = synthesize_consonant(hard, identity, mood, word, position, vowel_rms, emphasized)
        second = synthesize_consonant(soft, identity, mood, word, position, vowel_rms, emphasized)
        offset = round(.008*RATE)
        result = [0.0]*max(len(first), offset+len(second))
        for n, x in enumerate(first):
            result[n] += x
        for n, x in enumerate(second):
            result[offset+n] += x
        return result
    hard = sound in 'tdkpbg'
    frames = round(CONSONANT_PARAMETERS['hard_duration_ms' if hard else 'noise_duration_ms']*RATE/1000)
    warmup = round(.02*RATE)
    rng = seeded_rng(identity, mood, word.lower(), f'consonant-v1/{position}/{sound}')
    noise = [rng.uniform(-1, 1) for _ in range(frames+warmup)]
    bands = {'p': (180, 1200), 'b': (150, 1000), 'k': (700, 3500), 'g': (500, 2800),
             't': (1800, 6500), 'd': (1000, 4500), 's': (2500, 8500), 'z': (1800, 6500),
             'sh': (1200, 5000), 'f': (700, 6500), 'v': (400, 4200),
             'th': (1000, 6500), 'h': (300, 4000)}
    samples = filtered_noise(noise, *bands[sound])[warmup:]
    attack = round(CONSONANT_PARAMETERS['hard_attack_ms' if hard else 'noise_attack_ms']*RATE/1000)
    release = round((CONSONANT_PARAMETERS['hard_duration_ms']-CONSONANT_PARAMETERS['hard_attack_ms']
                     if hard else CONSONANT_PARAMETERS['noise_release_ms'])*RATE/1000)
    for n in range(frames):
        envelope = 1.0
        if n < attack:
            envelope = .5-.5*math.cos(math.pi*n/attack)
        if n >= frames-release:
            envelope *= .5-.5*math.cos(math.pi*(frames-1-n)/release)
        samples[n] *= envelope
    samples = remove_dc(samples)
    samples[0] = samples[-1] = 0.0
    relative = CONSONANT_PARAMETERS['hard_relative_rms' if hard else 'noise_relative_rms']
    gain = vowel_rms*relative*(CONSONANT_PARAMETERS['emphasis_gain'] if emphasized else 1)/rms(samples)
    return [x*gain for x in samples]


def generate(config, output):
    validate(config)
    consonants = config['synthesis'].get('consonants_enabled', True)
    stable = config['synthesis'].get('source', 'texture') == 'stable_pulse'
    report = {'generator_version': 4, 'config_sha256': hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest(),
              'texture': STABLE_PARAMETERS if stable else TEXTURE_PARAMETERS,
              'breath_enabled': not stable, 'consonants_enabled': consonants,
              'consonants': CONSONANT_PARAMETERS if consonants else None, 'assets': []}
    for identity, profile in config['profiles'].items():
        folder = output / identity
        manifest = {'schema_version': 2, 'voice_identity_id': identity, 'display_name': profile['label'],
                    'continuous_pitch': True, 'moods': {}, 'prosody': {}}
        if consonants:
            manifest['consonants'] = {}
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
            levels = []
            for vowel in VOWELS:
                relative = f'{mood}/{vowel}.wav'
                path = folder / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                pcm, measurements = synthesize_with_report(config, identity, mood, vowel)
                levels.append(10**(measurements['rms_dbfs']/20)*32768)
                with wave.open(str(path), 'wb') as wav:
                    wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                    wav.writeframes(pcm)
                manifest['moods'][mood][vowel] = relative
                report['assets'].append({'path': f'{identity}/{relative}', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), **measurements})
            if not consonants:
                continue
            manifest['consonants'][mood] = {}
            for sound in CONSONANTS:
                # One immutable noise texture per class/voice/mood. Word timing
                # and spelling remain deterministic without runtime synthesis.
                samples = synthesize_consonant(sound, identity, mood, 'bank', 0, sum(levels)/len(levels), False)
                pcm = [round(x) for x in samples]
                if max(map(abs, pcm)) > 32767:
                    raise ValueError(f'Clipping consonant: {identity}/{mood}/{sound}')
                relative = f'{mood}/consonants/{sound}.wav'
                path = folder / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                with wave.open(str(path), 'wb') as wav:
                    wav.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
                    wav.writeframes(struct.pack('<'+'h'*len(pcm), *pcm))
                manifest['consonants'][mood][sound] = relative
                report['assets'].append({'path': f'{identity}/{relative}', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                        'rms_dbfs': db(rms(pcm)/32768), 'peak_dbfs': db(max(map(abs, pcm))/32768),
                                        'kind': 'consonant', 'seconds': len(pcm)/RATE})
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
        count = len(VOWELS)+(len(CONSONANTS) if config['synthesis'].get('consonants_enabled', True) else 0)
        print(f'Validated {len(config["profiles"])} profiles; {len(config["profiles"])*len(MOODS)*count} WAVs; no writes')
    else:
        generate(config, args.output_dir)


if __name__ == '__main__':
    main()
