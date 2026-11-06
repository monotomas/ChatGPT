#!/usr/bin/env python3
"""Rebuild the rejected spectral-envelope diagnostic from its exact Git base.
SPDX-License-Identifier: AGPL-3.0-or-later
This is standalone detector research, not product validation or release approval.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys,time
HERE=Path(__file__).resolve().parent
COMMIT='e235c8a098dadce6e0a4b8bd6ca9de789a5d174d'
CPP=['PitchDetector.cpp','TimeDomainPursuit.cpp','YinEstimator.cpp','ArticulationTracker.cpp','PolyphonicTracker.cpp','NoiseConditioner.cpp','InputDynamics.cpp']
def digest(data):return hashlib.sha256(data).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--project-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--compiler',default='g++');a=p.parse_args()
    out=a.output.resolve()
    if out.exists() and any(out.iterdir()):p.error('Output must be absent or empty')
    out.mkdir(parents=True,exist_ok=True)
    expected=json.loads((HERE/'source-inputs.json').read_text());sources={}
    for label in ('baseline','spectral'):
        source=out/label/'Source';sources[label]=source
        for rel,sha in expected['baseline'].items():
            data=subprocess.check_output(['git','-C',str(a.project_root),'show',COMMIT+':'+rel]);assert digest(data)==sha,rel
            path=out/label/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        if label=='spectral':
            for path in (HERE/'candidate-source').rglob('*'):
                if path.is_file():
                    rel=path.relative_to(HERE/'candidate-source');data=path.read_bytes();assert digest(data)==expected['spectral'][rel.as_posix()]
                    (out/label/rel).write_bytes(data)
    records={}
    for label,source in sources.items():
        for kind,driver in [('falsifier','falsifier.cpp'),('open','open-probe.cpp')]:
            folder=out/(('falsifier-'+label) if kind=='falsifier' else ('open-'+label));folder.mkdir()
            binary=folder/'capture';command=[a.compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(source),str(HERE/driver),*[str(source/'Pitch'/s) for s in CPP],'-o',str(binary)]
            with (folder/'compile.txt').open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
            if r.returncode:return r.returncode
            event_path=folder/'events.csv' if kind=='falsifier' else out/('open-baseline-fresh.csv' if label=='baseline' else 'open-strings.csv')
            start=time.perf_counter()
            with event_path.open('w') as events,(folder/'callbacks.csv').open('w') as errors:r=subprocess.run([str(binary),*(['10'] if kind=='open' else [])],stdout=events,stderr=errors)
            records[label+'-'+kind]={'command':command,'process_exit_code':r.returncode,'elapsed_seconds':time.perf_counter()-start,'executable_sha256':digest(binary.read_bytes()),'events_sha256':digest(event_path.read_bytes())}
            if r.returncode:return r.returncode
    (out/'reproduction-execution.json').write_text(json.dumps(records,indent=2)+'\n')
    subprocess.run([sys.executable,str(HERE/'analyse_results.py'),str(out)],check=True)
    subprocess.run([sys.executable,str(HERE/'timing_release_audit.py'),str(out)],check=True)
    print('Completed rejected diagnostic; no musical-ready or release approval is implied.')
    return 0
if __name__=='__main__':raise SystemExit(main())
