#!/usr/bin/env python3
"""Rebuild the rejected candidate from a restored study; compare deterministic outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('restored_study', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.restored_study.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit('Output must not exist')
    output.mkdir(parents=True)
    units = ['ArticulationTracker', 'BurgPredictor', 'EarlyEstimator', 'InputDynamics',
             'NoiseConditioner', 'PitchDetector', 'PolyphonicTracker',
             'TimeDomainPursuit', 'YinEstimator']
    command = ['g++', '-std=c++17', '-O2', '-DNDEBUG', '-I', str(source / 'Source'),
               str(source / 'quiet_repick_counter.cpp')]
    command += [str(source / 'Source/Pitch' / (unit + '.cpp')) for unit in units]
    command += ['-o', str(output / 'Capture')]
    with (output / 'compile.txt').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    result = subprocess.run([str(output / 'Capture'), str(output)], check=True,
                            text=True, capture_output=True)
    (output / 'console.txt').write_text(result.stdout + result.stderr)
    parity = {}
    for name in ['counter-results.csv', 'counter-events.csv']:
        expected, actual = source / 'quiet-counter' / name, output / name
        parity[name] = {'expected_sha256': sha(expected), 'actual_sha256': sha(actual),
                        'exact': expected.read_bytes() == actual.read_bytes()}
        assert parity[name]['exact'], name
    binding = {'actual_compile_command': command,
               'actual_run_command': [str(output / 'Capture'), str(output)],
               'executable_sha256': sha(output / 'Capture'), 'parity': parity}
    (output / 'rebuild-proof.json').write_text(json.dumps(binding, indent=2) + '\n')
    print(json.dumps(binding, indent=2))


if __name__ == '__main__':
    main()
