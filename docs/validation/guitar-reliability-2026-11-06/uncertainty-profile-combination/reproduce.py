#!/usr/bin/env python3
"""Verify the private factor archive; optionally replay into a new external path."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
LABELS = ('baseline','fast','baseline-timed','fast-timed')
RUNS = ('payload-cold330-baseline','payload-cold330-fast',
        'profile-thread-open30-baseline','profile-thread-open30-fast')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_map(root, compiled=False):
    return {p.relative_to(root).as_posix(): sha(p) for p in root.rglob('*')
            if p.is_file() and (not compiled or p.suffix in ('.cpp','.h'))}


def verify():
    manifest = json.loads((ROOT / 'archive-manifest.json').read_text())
    actual = {p.relative_to(ROOT).as_posix(): {'sha256':sha(p),'bytes':p.stat().st_size}
              for p in ROOT.rglob('*') if p.is_file() and p != ROOT / 'archive-manifest.json'}
    assert actual == manifest['files'], 'Archive seal mismatch'
    binding = json.loads((ROOT / 'input-binding.json').read_text())
    baseline = ROOT / 'frozen-source/baseline/Source'
    fast = ROOT / 'frozen-source/fast/Source'
    old, new = file_map(baseline), file_map(fast)
    assert old == binding['frequency_source_full_map'] == binding['baseline_Source']
    assert new == binding['fast_Source']
    assert old.keys() == new.keys()
    assert [p for p in old if old[p] != new[p]] == ['Pitch/TimeDomainPursuit.cpp']
    assert sha(ROOT / 'input-source-inventory.json') == binding['frequency_source_inventory_sha256']
    inventory = json.loads((ROOT / 'input-source-inventory.json').read_text())
    assert {'Source/'+p:h for p,h in old.items()} == inventory['files_sha256']
    for name in RUNS:
        run = ROOT / 'runs' / name
        meta = json.loads((run / 'run.json').read_text())
        label = name.rsplit('-',1)[1]
        timed = name.startswith('profile-thread')
        source = ROOT / 'frozen-source' / (label + '-timed' if timed else label) / 'Source'
        driver = ROOT / ('profile-thread-probe.cpp' if timed else 'payload-cold.cpp')
        assert file_map(source, compiled=True) == meta['source_sha256']
        assert sha(driver) == meta['driver_sha256']
        assert sha(run / 'events.csv') == meta['events_sha256']
        assert sha(run / 'metrics.csv') == meta['metrics_sha256']
        assert meta['exit_code'] == 0
        build = json.loads((run / 'build.json').read_text())
        assert all(meta[k] == v for k,v in build.items())
    for prefix in ('payload-cold330','profile-thread-open30'):
        assert (ROOT/'runs'/(prefix+'-baseline')/'events.csv').read_bytes() == (ROOT/'runs'/(prefix+'-fast')/'events.csv').read_bytes()
    reference = json.loads((ROOT/'original-frequency-cold-parity.json').read_text())
    original = ROOT/'original-frequency-cold330-events.csv'
    assert sha(original) == reference['original_frequency_cold330_sha256']
    rows = list(csv.DictReader((ROOT/'runs/payload-cold330-baseline/events.csv').open()))
    columns = ('case','cents','timbre','dynamics','sample','type','note')
    projection = ','.join(columns)+'\n'+''.join(','.join(r[c] for c in columns)+'\n' for r in rows if r['type'] != 'P')
    assert projection.encode() == original.read_bytes()
    print(json.dumps({'verified_files':len(actual),'verified_bytes':sum(x['bytes'] for x in actual.values()),
                      'run_bindings':len(RUNS)},indent=2),flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,help='New external directory; omit for verification only')
    args = parser.parse_args()
    verify()
    if args.output is None:
        return
    out = args.output.resolve()
    if out == ROOT or ROOT in out.parents:
        parser.error('--output must be outside archive')
    out.mkdir(parents=True,exist_ok=False)
    for label in LABELS:
        shutil.copytree(ROOT/'frozen-source'/label/'Source',out/(label+'-Source'))
    for name in ('run_factor.py','build_and_measure.py','payload-cold.cpp','profile-thread-probe.cpp'):
        shutil.copy2(ROOT/name,out/name)
    proof = out / 'patch-proof'
    shutil.copytree(out/'baseline-Source',proof/'Source')
    patch_command = ['patch','--batch','--forward','-p1','-i',str(ROOT/'profile-fast.patch')]
    patch = subprocess.run(patch_command,cwd=proof,text=True,capture_output=True,check=True)
    (proof/'patch.txt').write_text(patch.stdout+patch.stderr)
    assert file_map(proof/'Source') == file_map(out/'fast-Source')
    subprocess.run([sys.executable,'-B',str(out/'run_factor.py')],check=True)
    for name in RUNS:
        assert (out/name/'events.csv').read_bytes() == (ROOT/'runs'/name/'events.csv').read_bytes(), name+' differs from archived capture'
    (out/'archive-replay-verification.json').write_text(json.dumps({
       'patch_command':patch_command,'patch_matches_all_fast_source_files':True,
       'all_four_event_streams_byte_identical_to_archive':True,
       'new_run_metadata':'Each run.json binds full compile/runtime commands, input maps and output/binary hashes.',
    },indent=2)+'\n')


if __name__ == '__main__':
    main()
