#!/usr/bin/env python3
"""Freeze, capture and score an isolated continuous-poly model, without edits.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import argparse,datetime,hashlib,importlib.util,json,shutil,subprocess,sys
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=Path,required=True,help='Parent of frozen Pitch/ directory');parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('poly_fixture_integrity',HERE/'verify.py');verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify);verify.verify(HERE)
    source=args.source.resolve();output=args.output.resolve();assert not output.exists(),'Refuse existing capture output'
    paths=sorted((source/'Pitch').glob('*'));paths=[p for p in paths if p.is_file() and p.suffix in ('.h','.cpp')]
    hashes={p.name:sha(p) for p in paths};assert all(name in hashes for name in ('PolyphonicTracker.cpp','PolyphonicTracker.h','TimeDomainPursuit.cpp','YinEstimator.cpp'))
    snapshot=output/'source/Pitch';snapshot.mkdir(parents=True)
    for p in paths:shutil.copyfile(p,snapshot/p.name)
    assert hashes=={p.name:sha(p) for p in paths}=={p.name:sha(p) for p in snapshot.iterdir()}
    support=('capture.cpp','score.py','verify.py','run.py')
    for name in support:shutil.copyfile(HERE/name,output/name)
    build=Path('/tmp')/('poly-oracle-'+hashlib.sha256(str(output).encode()).hexdigest()[:16]);build.mkdir();binary=build/'capture'
    command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(snapshot.parent),str(output/'capture.cpp'),str(snapshot/'TimeDomainPursuit.cpp'),str(snapshot/'YinEstimator.cpp'),'-o',str(binary)]
    manifest=dict(recorded_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_original=str(source),source_sha256=hashes,fixture_seal=(HERE/'SEALED_SHA256.txt').read_text().strip(),support_sha256={n:sha(output/n) for n in support},scope='All true identities/pitches seeded once: optimistic tracking only, no acquisition or musical readiness')
    with (output/'compile.txt').open('w') as log:compiled=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    manifest['compile_exit_code']=compiled.returncode
    if compiled.returncode:(output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return compiled.returncode
    manifest['executable_sha256']=sha(binary)
    with (output/'capture-console.txt').open('w') as log:capture=subprocess.run([str(binary),str(HERE),str(output/'capture')],stdout=log,stderr=subprocess.STDOUT)
    manifest['capture_exit_code']=capture.returncode
    if not capture.returncode:
        with (output/'scoring-console.txt').open('w') as log:scoring=subprocess.run([sys.executable,str(output/'score.py'),'--fixtures',str(HERE),'--capture',str(output/'capture'),'--output',str(output/'scoring')],stdout=log,stderr=subprocess.STDOUT)
        manifest['scoring_exit_code']=scoring.returncode
    manifest['source_snapshot_changed']=hashes!={p.name:sha(p) for p in snapshot.iterdir()};manifest['owner_source_changed']=hashes!={p.name:sha(p) for p in paths}
    manifest['files_sha256']={p.relative_to(output).as_posix():sha(p) for p in output.rglob('*') if p.is_file() and p.name!='manifest.json'}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');assert not manifest['source_snapshot_changed'],'Frozen model changed during capture'
    verify.verify(HERE);print(json.dumps({k:manifest.get(k) for k in ('capture_exit_code','scoring_exit_code','source_snapshot_changed','owner_source_changed','fixture_seal')},indent=2));return capture.returncode or manifest.get('scoring_exit_code',2)
if __name__=='__main__':raise SystemExit(main())
