#!/usr/bin/env python3
"""Verify/replay only this frozen private experiment; build outside the archive."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parent
CPP = ['PitchDetector.cpp', 'TimeDomainPursuit.cpp', 'YinEstimator.cpp',
       'ArticulationTracker.cpp', 'PolyphonicTracker.cpp', 'NoiseConditioner.cpp',
       'InputDynamics.cpp']
RUNS = ('cold330-baseline', 'cold330-fast', 'payload-cold330-baseline',
        'payload-cold330-fast', 'profile-thread-open30-baseline',
        'profile-thread-open30-fast', 'score-audit-open30')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_map(source):
    return {p.relative_to(source).as_posix(): sha(p) for p in source.rglob('*')
            if p.suffix in ('.cpp', '.h')}


def file_map(root):
    return {p.relative_to(root).as_posix(): {'sha256': sha(p), 'bytes': p.stat().st_size}
            for p in root.rglob('*') if p.is_file()}


def verify_archive():
    manifest = json.loads((ROOT / 'archive-manifest.json').read_text())
    actual = {p.relative_to(ROOT).as_posix(): {'sha256': sha(p), 'bytes': p.stat().st_size}
              for p in ROOT.rglob('*') if p.is_file() and p != ROOT / 'archive-manifest.json'}
    assert actual == manifest['files'], 'Archive inventory/hash mismatch'
    origin = json.loads((ROOT / 'frozen-origin.json').read_text())
    baseline = ROOT / 'frozen-source/baseline/Source'
    fast = ROOT / 'frozen-source/fast/Source'
    assert file_map(baseline) == origin['files'], 'Frozen baseline origin map mismatch'
    old_files, new_files = file_map(baseline), file_map(fast)
    assert old_files.keys() == new_files.keys()
    assert [p for p in old_files if old_files[p] != new_files[p]] == ['Pitch/TimeDomainPursuit.cpp']
    assert sha(ROOT / 'profile-fast.patch') == origin['private_patch_sha256']
    assert sha(baseline / 'Pitch/TimeDomainPursuit.cpp') == origin['baseline_time_domain_pursuit_sha256']
    assert sha(fast / 'Pitch/TimeDomainPursuit.cpp') == origin['fast_time_domain_pursuit_sha256']
    for name in RUNS:
        run = ROOT / 'original-runs' / name
        meta = json.loads((run / 'run.json').read_text())
        if name.startswith('profile-thread'):
            label = name.rsplit('-', 1)[1] + '-timed'
            driver = 'profile-thread-probe.cpp'
        elif name == 'score-audit-open30':
            label, driver = 'fast', 'score-audit-probe.cpp'
        else:
            label = name.rsplit('-', 1)[1]
            driver = 'payload-cold.cpp' if name.startswith('payload') else 'probe-cold.cpp'
        assert source_map(ROOT / 'frozen-source' / label / 'Source') == meta['source_sha256']
        assert sha(ROOT / driver) == meta['driver_sha256']
        assert sha(run / 'events.csv') == meta['events_sha256'], 'Original event binding mismatch'
        assert sha(run / 'metrics.csv') == meta['metrics_sha256']
        assert meta['exit_code'] == 0
        build = json.loads((run / 'build.json').read_text())
        assert all(meta[key] == value for key, value in build.items())
    for prefix in ('cold330', 'payload-cold330', 'profile-thread-open30'):
        a, b = [ROOT / 'original-runs' / (prefix + '-' + label) / 'events.csv'
                for label in ('baseline', 'fast')]
        assert a.read_bytes() == b.read_bytes(), prefix + ' payload mismatch'
    return {'verified_files': len(actual), 'verified_bytes': sum(x['bytes'] for x in actual.values()),
            'original_run_bindings': len(RUNS)}


def compile_and_run(out, name, label, driver, args=(), extra=(), micro=False):
    run = out / name
    run.mkdir()
    source = ROOT / 'frozen-source' / label / 'Source'
    binary = run / 'capture'
    command = ['g++', '-std=c++17', '-O2', '-DNDEBUG', *extra, '-I', str(source),
               str(ROOT / driver)]
    if not micro:
        command += [str(source / 'Pitch' / cpp) for cpp in CPP]
    command += ['-o', str(binary)]
    before = source_map(source)
    with (run / 'compile.txt').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert source_map(source) == before
    start = time.perf_counter()
    stdout = run / ('comparison.json' if micro else 'events.csv')
    stderr = run / ('scores.json' if micro else 'metrics.csv')
    with stdout.open('w') as events, stderr.open('w') as metrics:
        subprocess.run([str(binary), *args], stdout=events, stderr=metrics, check=True)
    metadata = {'command': command, 'run_command': [str(binary), *args],
                'compiler': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                'source_sha256': before, 'driver_sha256': sha(ROOT / driver),
                'executable_sha256': sha(binary), 'stdout_sha256': sha(stdout),
                'stderr_sha256': sha(stderr), 'elapsed_seconds': time.perf_counter() - start,
                'exit_code': 0}
    (run / 'run.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(name + ' complete', flush=True)
    return run


def check_micro(run, audit):
    comparison = json.loads((run / 'comparison.json').read_text())
    expected = json.loads((ROOT / 'micro-audit-comparison.json').read_text())
    # Thread times vary. All remaining numeric evidence, including checksum, is deterministic.
    for key, value in expected.items():
        if key not in ('original_thread_ns', 'fast_thread_ns', 'timing_loops', 'checksum'):
            assert comparison[key] == value, (key, comparison[key], value)
    assert comparison['kernel_bit_differences'] == comparison['cached_scale_bit_differences'] == 0
    if audit:
        assert json.loads((run / 'scores.json').read_text()) == json.loads((ROOT / 'micro-audit-scores.json').read_text())
    else:
        original = json.loads((ROOT / 'micro.json').read_text())
        assert comparison['timing_loops'] == original['timing_loops']
        assert comparison['checksum'] == original['checksum']
    return comparison


def check_events(run, original):
    expected = ROOT / 'original-runs' / original / 'events.csv'
    assert (run / 'events.csv').read_bytes() == expected.read_bytes(), 'Replay payload changed: ' + original


def check_patch(out):
    proof = out / 'patch-proof'
    shutil.copytree(ROOT / 'frozen-source' / 'baseline' / 'Source', proof / 'Source')
    command = ['patch', '--batch', '--forward', '-p1', '-i', str(ROOT / 'profile-fast.patch')]
    result = subprocess.run(command, cwd=proof, text=True, capture_output=True, check=True)
    (proof / 'patch.txt').write_text(result.stdout + result.stderr)
    assert file_map(proof / 'Source') == file_map(ROOT / 'frozen-source' / 'fast' / 'Source')
    return {'command': command, 'applied_source_matches_fast': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=('verify', 'micro', 'payload', 'timing', 'audit', 'all'), default='verify')
    parser.add_argument('--output', type=Path, help='New external directory, required for compilation/replay')
    args = parser.parse_args()
    report = {'archive': verify_archive(), 'suite': args.suite}
    if args.suite == 'verify':
        print(json.dumps(report, indent=2))
        return
    if args.output is None:
        parser.error('--output is required for replay')
    out = args.output.resolve()
    if out == ROOT or ROOT in out.parents:
        parser.error('--output must be outside this archive')
    out.mkdir(parents=True, exist_ok=False)
    report['patch'] = check_patch(out)
    if args.suite in ('micro', 'all'):
        for audit in (False, True):
            name = 'micro-audit' if audit else 'micro'
            run = compile_and_run(out, name, 'fast', 'micro.cpp',
                                  extra=('-DFLYSNAPPER_PROFILE_SCORE_AUDIT',) if audit else (), micro=True)
            report[name] = check_micro(run, audit)
    if args.suite in ('payload', 'all'):
        for label in ('baseline', 'fast'):
            name = 'payload-cold330-' + label
            run = compile_and_run(out, name, label, 'payload-cold.cpp')
            check_events(run, name)
        assert (out / 'payload-cold330-baseline/events.csv').read_bytes() == (out / 'payload-cold330-fast/events.csv').read_bytes()
        report['payload330_byte_identical_to_original_and_between_versions'] = True
    if args.suite in ('timing', 'all'):
        for label in ('baseline', 'fast'):
            name = 'profile-thread-open30-' + label
            run = compile_and_run(out, name, label + '-timed', 'profile-thread-probe.cpp', args=('10',))
            check_events(run, name)
        report['timing30_events_byte_identical_to_original'] = True
    if args.suite in ('audit', 'all'):
        audit_path = out / 'score-audit-open30' / 'score-audit.json'
        run = compile_and_run(out, 'score-audit-open30', 'fast', 'score-audit-probe.cpp', args=('10',),
                              extra=('-DFLYSNAPPER_PROFILE_SCORE_AUDIT', '-DFLYSNAPPER_AUDIT_PATH=' + json.dumps(str(audit_path))))
        check_events(run, 'score-audit-open30')
        report['audit'] = json.loads(audit_path.read_text())
        assert report['audit'] == json.loads((ROOT / 'original-runs/score-audit-open30/score-audit.json').read_text())
    (out / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
