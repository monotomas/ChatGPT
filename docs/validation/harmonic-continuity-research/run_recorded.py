#!/usr/bin/env python3
"""Oracle known-source observer probe; no MIDI/acquisition/readiness claim.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import argparse,csv,datetime,hashlib,json,platform,shutil,subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
VARIANTS=('v1','v2-persistent-rejected','v3-association-rejected','v4-fit-only-rejected','v5-frozen','v6-nnls','v7-carrier')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=VARIANTS,required=True)
    parser.add_argument('--fixtures',type=Path,default=ROOT/'Tests/AudioFixtures/readiness/recorded_noise')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler',default='g++')
    args=parser.parse_args();out=args.output.resolve();source=HERE/args.variant;fixtures=args.fixtures.resolve()
    if out.exists() and any(out.iterdir()):parser.error('Refuse nonempty output directory')
    out.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which(args.compiler)
    if not compiler:parser.error('C++17 compiler unavailable')
    files=[source/'recorded_probe.cpp',source/'Source/Pitch/HarmonicContinuity.cpp',source/'Source/Pitch/HarmonicContinuity.h']
    def hashes():return {p.relative_to(HERE).as_posix():digest(p) for p in files}
    before=hashes();executable=out/'observer_probe'
    command=[compiler,'-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-I',str(source/'Source'),str(files[0]),str(files[1]),'-o',str(executable)]
    with (out/'compile.txt').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    if hashes()!=before:raise RuntimeError('Observer changed during compilation')
    document=json.loads((fixtures/'fixtures.json').read_text());records={r['name']:r for r in document['records']};results=[]
    inputs={}
    for case in document['cases']:
        if case['mode']!='mono' or len(case['reference_notes'])!=1:continue
        reference=case['reference_notes'][0];record=records[case['record']];wave=fixtures/record['file'];target=out/(case['name']+'.csv')
        actual=digest(wave)
        if actual!=record['sha256']:raise RuntimeError('Input hash differs from sealed record:'+record['file'])
        inputs[record['file']]=actual
        subprocess.run([str(executable),str(wave),str(reference['anchor']),str(reference['on_sample']),str(reference['off_sample']),str(target)],check=True)
        rows=[r for r in csv.DictReader(target.open()) if int(r['within_reference'])]
        def energy(r):return int(r['energy_support'])>=3 and float(r['foreground_novelty'])>.25 and float(r['novelty_fraction'])>.65
        def phase(r):return int(r['departure'])>=3 and abs(float(r['reset']))>.2 and float(r['reset_coherence'])>.85
        results.append(dict(case=case['name'],family=case['family'],held_frames=len(rows),continuing_ge2=sum(int(r['continuing'])>=2 for r in rows),continuing_ge3=sum(int(r['continuing'])>=3 for r in rows),energy_frames=sum(map(energy,rows)),phase_frames=sum(map(phase,rows))))
    if hashes()!=before:raise RuntimeError('Observer changed during measurement')
    families={}
    for family in sorted({r['family'] for r in results}):
        subset=[r for r in results if r['family']==family]
        families[family]={'cases':len(subset),**{k:sum(r[k] for r in subset) for k in ('held_frames','continuing_ge2','continuing_ge3','energy_frames','phase_frames')}}
    manifest=dict(scope='Nominal-reference-pitch oracle observer, not detector or MIDI acceptance',variant=args.variant,created_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),platform=platform.platform(),compiler=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],source_sha256=before,executable_sha256=digest(executable),fixtures_manifest_sha256=digest(fixtures/'fixtures.json'),fixture_seal=(fixtures/'SEALED_SHA256.txt').read_text().strip(),inputs_sha256=inputs,source_changed=False,musical_readiness_eligible=False,families=families,cases=results)
    (out/'summary.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(families,indent=2))
if __name__=='__main__':main()
