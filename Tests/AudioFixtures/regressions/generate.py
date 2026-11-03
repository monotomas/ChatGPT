#!/usr/bin/env python3
"""Reproduce supplemental PCM in a chosen output directory.
SPDX-License-Identifier: AGPL-3.0-or-later
Requires NumPy only for regeneration; normal tests use the retained WAV files.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location('modal_fixtures', ROOT / 'Tools/audio_validation_fixtures.py')
modal = importlib.util.module_from_spec(spec)
spec.loader.exec_module(modal)
RATE = 48000


def stiff_tone(note, seed, stiffness, brightness, duration, gain):
    rng = np.random.default_rng(seed)
    t = np.arange(round(duration * RATE)) / RATE
    frequency = 440 * 2 ** ((note - 69) / 12)
    audio = np.zeros(len(t))
    for harmonic in range(1, min(17, int(RATE * .44 / frequency)) + 1):
        hz = frequency * harmonic * np.sqrt((1 + stiffness * harmonic * harmonic) / (1 + stiffness))
        amplitude = (abs(np.sin(np.pi * .23 * harmonic)) + .06) / harmonic ** brightness
        audio += amplitude * np.sin(2 * np.pi * hz * t + rng.uniform(-np.pi, np.pi)) * np.exp(-t * (.7 + .13 * harmonic))
    audio /= max(.1, np.max(abs(audio)))
    audio *= gain * (1 - np.exp(-t / .001))
    audio[-192:] *= np.linspace(1, 0, 192)
    return audio


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    output = parser.parse_args().output
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((HERE / 'manifest.json').read_text())
    expected = {record['file']: record['sha256'] for record in [manifest, *manifest['additional_regressions']]}

    def write(name, audio):
        digest = modal.write_wav(output / name, audio)
        if digest != expected[name]:
            raise ValueError('Regeneration differs from preserved PCM: ' + name)

    audio = np.zeros(RATE)
    for note, duration, brightness, seed in ((40, .6, .55, 1), (52, .25, 1.25, 2)):
        voice = modal.modal(note, duration=duration, gain=.18, seed=seed, brightness=brightness)
        audio[2400:2400 + len(voice)] += voice
    write('bright_lower_upper_damped.wav', audio)

    voice = stiff_tone(40, 101, .001, .7, .65, .24)
    audio = np.zeros(round(.95 * RATE))
    audio[2400:2400 + len(voice)] = voice
    write('held_stiff_E2.wav', audio)

    audio = np.zeros(RATE)
    for note, seed, brightness, duration, gain in ((40, 904, .7, .7, .20), (59, 911, 1.4, .25, .12)):
        voice = stiff_tone(note, seed, 0., brightness, duration, gain)
        audio[2400:2400 + len(voice)] += voice
    write('twelfth_withdrawal_E2.wav', audio)

    width = 9600
    parts = [modal.modal(40, duration=.35, gain=.22, seed=2011 + i, brightness=1.9) for i in range(3)]
    whole = np.zeros(3 * width)
    for index, part in enumerate(parts):
        chunk = part[:width].copy()
        if index:
            chunk[:48] = parts[index - 1][width:width + 48] * np.linspace(1, 0, 48) + chunk[:48] * np.linspace(0, 1, 48)
        whole[index * width:(index + 1) * width] = chunk
    whole[-192:] *= np.linspace(1, 0, 192)
    audio = np.zeros(RATE)
    audio[2400:2400 + len(whole)] = whole
    write('continuous_repick_E2.wav', audio)
    print('Reproduced all four supplemental WAVs with their preserved SHA256 values.')


if __name__ == '__main__':
    main()
