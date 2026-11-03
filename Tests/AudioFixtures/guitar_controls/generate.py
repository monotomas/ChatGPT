#!/usr/bin/env python3
"""Independent guitar behaviour controls; no detector output is read.

SPDX-License-Identifier: AGPL-3.0-or-later
Normal tests use retained PCM. Regeneration requires NumPy and FFmpeg.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave

import numpy as np

HERE = Path(__file__).resolve().parent
FIXTURES = HERE.parent
RATE = 48000
START = 2400


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make(output):
    output.mkdir(parents=True, exist_ok=True)
    (output / 'pcm').mkdir(exist_ok=True)
    records, cases = [], []

    def write(name, audio, truth, family, description, modes=('mono',), **extra):
        if not np.isfinite(audio).all() or np.max(np.abs(audio), initial=0) > .98:
            raise ValueError('Nonfinite or clipped fixture: ' + name)
        file = 'pcm/' + name + '.wav'
        pcm = np.round(audio * 32767).astype('<i2')
        with wave.open(str(output / file), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(pcm.tobytes())
        record = dict(name=name, file=file, sha256=digest(output / file), frames=len(audio),
                      family=family, description=description, truth=truth, **extra)
        records.append(record)
        for mode in modes:
            cases.append(dict(name=name + '_' + mode, family=family, mode=mode,
                              file=file, truth=truth))

    def wrap(body, tail=.30):
        result = np.zeros(START + len(body) + round(tail * RATE))
        result[START:START + len(body)] = body
        return result

    def truth(note, count):
        return [dict(note=note, on_sample=START, off_sample=START + count)]

    def modal(note, duration, seed, gain=.24, decay_db=0, floor=.006, stiffness=0., morph=False, bend=None):
        rng = np.random.default_rng(seed)
        t = np.arange(round(duration * RATE)) / RATE
        pitch = np.full(len(t), float(note)) if bend is None else note + bend(t)
        f0 = 440 * 2 ** ((pitch - 69) / 12)
        phase = 2 * np.pi * np.cumsum(f0) / RATE
        result = np.zeros(len(t))
        for h in range(1, min(21, int(RATE * .43 / np.max(f0))) + 1):
            partial = h * np.sqrt((1 + stiffness * h * h) / (1 + stiffness))
            amplitude = (abs(np.sin(.187 * np.pi * h)) + .09) / h ** 1.08
            if morph:
                # The fundamental stays physically present while partial strengths swap.
                transition = np.clip((t - .20) / .30, 0, 1)
                amplitude = amplitude * ((1 - transition) + transition * (.10 if h % 2 else 2.1))
            result += amplitude * np.sin(phase * partial + rng.uniform(-np.pi, np.pi))
        result /= np.max(np.abs(result))
        if decay_db:
            envelope = np.maximum(floor / gain, 10 ** (-decay_db * t / duration / 20))
        else:
            envelope = np.ones(len(t))
        result *= gain * envelope * (1 - np.exp(-t / .0012))
        result[-192:] *= np.linspace(1, 0, 192)
        return result, pitch

    # Low but still periodic tails remain present until a known 4ms damping.
    for i, note in enumerate((40, 52, 57, 64, 76)):
        for gain, level in ((.24, 'normal'), (.035, 'quiet')):
            body, _ = modal(note, 1.45, 301901 + i, gain=gain, decay_db=48,
                            floor=.00055 if level == 'quiet' else .0012)
            write(f'decay_{note}_{level}', wrap(body), truth(note, len(body)), 'guitar-dynamic-decay',
                  'Single physically continuous modal tone:48dB decay then fixed low tail;4ms damping; no reattack.',
                  modes=('mono', 'poly') if note in (40, 64) else ('mono',),
                  peak_gain=gain, floor_peak=.00055 if level == 'quiet' else .0012)
    for i, note in enumerate((40, 52, 64, 76)):
        body, _ = modal(note, 1.05, 460105 + i, morph=True)
        write(f'timbre_{note}', wrap(body), truth(note, len(body)), 'guitar-timbre-morph',
              'Continuous same fundamental; odd/even partial balance changes without a new pick.', modes=('mono', 'poly'))
    for i, note in enumerate((40, 52, 64)):
        body, _ = modal(note, 1.05, 510120 + i, stiffness=.00022, morph=True)
        write(f'stiff_{note}', wrap(body), truth(note, len(body)), 'guitar-inharmonic-tail',
              'Stiff-string partials and changing tone colour; fundamental does not change.', modes=('mono', 'poly'))

    for note in (40, 52, 64):
        body, _ = modal(note, .75, 620100 + note)
        for gain in (.03, .09, .27):
            scaled = body * (gain / .24)
            write(f'level_{note}_{round(gain * 100):02d}', wrap(scaled), truth(note, len(body)),
                  'guitar-level-invariance', 'Identical modal waveform at three absolute gains; all attacks above default gate.', peak_gain=gain)

    # A genuine octave change is a changed oscillator, not an octave hallucination.
    for direction in (-12, 12):
        first, _ = modal(52, .48, 732110)
        second, _ = modal(52 + direction, .48, 732120 + direction)
        joined = np.concatenate((first, second))
        note_truth = [dict(note=52, on_sample=START, off_sample=START + len(first)),
                      dict(note=52 + direction, on_sample=START + len(first), off_sample=START + len(joined))]
        write('true_octave_' + ('down' if direction < 0 else 'up'), wrap(joined), note_truth,
              'guitar-true-octave-transition', 'Two separately damped/rearticulated physical notes exactly one octave apart.')

    # Late noise cannot be mistaken for an indefinitely sustained tonal decay.
    body, _ = modal(52, .65, 832100, gain=.12, decay_db=28, floor=.003)
    rng = np.random.default_rng(832101)
    noise = rng.normal(0, .0009, round(.40 * RATE))
    write('damp_then_noise', wrap(np.concatenate((body, noise))), truth(52, len(body)),
          'guitar-damp-noise', 'Known source damp then broadband noise around-61dB RMS; no tonal source remains.', modes=('mono', 'poly'))
    write('noise_only', wrap(rng.normal(0, .0014, RATE)), [], 'guitar-negative-noise',
          'Broadband noise only; no stable pitched source.', modes=('mono', 'poly'))

    # Bend truth is continuous, and is scored separately from fixed-note transcription.
    bend_curves = {
        'whole_step': lambda t: 2 * np.clip((t - .20) / .30, 0, 1) * np.clip((1.20 - t) / .30, 0, 1),
        'semitone': lambda t: np.clip((t - .20) / .24, 0, 1) * np.clip((1.20 - t) / .24, 0, 1),
        'vibrato': lambda t: .30 * np.sin(2 * np.pi * 5.5 * t) * np.clip((t - .15) / .12, 0, 1),
        'fast_bend': lambda t: 2 * np.clip((t - .23) / .08, 0, 1),
    }
    bend_records = []
    for i, (name, curve) in enumerate(bend_curves.items()):
        body, pitch = modal(57, 1.35, 910001 + i, gain=.20, bend=curve)
        record_name = 'bend_' + name
        write(record_name, wrap(body), truth(57, len(body)), 'guitar-continuous-bend',
              'Single physical note with phase-continuous frequency curve; fixed-note evaluator is diagnostic only.',
              pitch_curve=name, pitch_reference='pitch-curves.tsv')
        for sample in range(0, len(body), 64):
            bend_records.append(f'{record_name}\t{START + sample}\t{pitch[sample]:.9f}')
    (output / 'pitch-curves.tsv').write_text('case\tsample\tpitch\n' + '\n'.join(bend_records) + '\n')

    # Existing recordings are explicitly disclosed sources; the extended tails
    # exceed the650/700ms previous crops but are not called unseen recordings.
    for name, note, relative in (
        ('acoustic_E2', 40, 'tonejs/E2.ogg'),
        ('acoustic_G3', 55, 'tonejs/G3.ogg'),
        ('electric_Fs3', 54, 'confirmation/tonejs/electric_Fs3.ogg'),
        ('nylon_Cs3', 49, 'confirmation/tonejs/nylon_Cs3.ogg'),
    ):
        source = FIXTURES / relative
        decoded = np.frombuffer(subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(source),
                              '-f', 'f32le', '-ac', '1', '-ar', str(RATE), 'pipe:1']), dtype='<f4').astype(float)
        body = decoded[:round(1.55 * RATE)].copy()
        body *= .24 / max(.001, np.max(np.abs(body)))
        body[-192:] *= np.linspace(1, 0, 192)
        onset = int(np.flatnonzero(np.abs(body) >= .0005)[0])
        for attenuation in (0, 12):
            current = body * 10 ** (-attenuation / 20)
            write(f'recorded_{name}_minus{attenuation:02d}', wrap(current),
                  [dict(note=note, on_sample=START + onset, off_sample=START + len(body))],
                  'guitar-recorded-extended', 'Known recorded note with a longer natural tail; deterministic gain,4ms controlled cutoff. Not an unseen source.',
                  recorded_source=relative, recorded_source_sha256=digest(source), attenuation_db=attenuation)

    manifest = dict(schema=1, scope='guitar-user-defect-controls', sample_rate=RATE,
                    source19_baseline_commit='bcd0b202e9b74c58b7c76e8f77abf2c14541df97',
                    detector_used_during_generation=False, recordings_are_disclosed=True,
                    records=records, cases=cases)
    (output / 'fixtures.json').write_text(json.dumps(manifest, indent=2) + '\n')
    rows = ['name\tfamily\tmode\twav\ttruth_note_on_off_samples']
    for c in cases:
        rows.append('\t'.join((c['name'], c['family'], c['mode'], c['file'], ','.join(
            f"{t['note']}:{t['on_sample']}:{t['off_sample']}" for t in c['truth']))))
    (output / 'fixtures.tsv').write_text('\n'.join(rows) + '\n')
    seal = hashlib.sha256((output / 'fixtures.json').read_bytes() + (output / 'fixtures.tsv').read_bytes()
                          + (output / 'pitch-curves.tsv').read_bytes()).hexdigest()
    (output / 'SEALED_SHA256.txt').write_text(seal + '\n')
    print(f'Generated{len(records)} PCM waveforms/{len(cases)} mode cases; pre-evaluation seal:{seal}')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=HERE)
    args = p.parse_args()
    make(args.output)


if __name__ == '__main__':
    main()
