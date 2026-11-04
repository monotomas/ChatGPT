#!/usr/bin/env python3
"""Capture/score the independent readiness controls against an exact DSP snapshot.
SPDX-License-Identifier: AGPL-3.0-or-later
No capture/scoring success alone grants a musical-ready or physical-latency claim.
"""
from pathlib import Path
import argparse,datetime,hashlib,importlib.util,json,os,platform,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--source-snapshot',type=Path,default=ROOT);parser.add_argument('--fixtures',type=Path,default=ROOT/'Tests/AudioFixtures/readiness');parser.add_argument('--full',action='store_true');parser.add_argument('--compiler',default='g++');args=parser.parse_args()
    source=args.source_snapshot.resolve();fixtures=args.fixtures.resolve();out=args.output.resolve()
    if out.exists() and any(out.iterdir()):parser.error('Refuse existing nonempty measurement output')
    out.mkdir(parents=True,exist_ok=True)
    spec=importlib.util.spec_from_file_location('readiness_integrity',ROOT/'Tests/AudioFixtures/readiness/verify.py');verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify);verify.verify(fixtures)
    # Raw detector measurement stops before MIDI-engine/MPE routing. The
    # separately tested output adapter lives beside the detector but requires
    # Core and is not part of this executable's signal path.
    sources=[p for p in sorted((source/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp'];headers=[p for p in sorted((source/'Source/Pitch').glob('*.h')) if p.name!='MpeOutputRouter.h'];files=[*sources,*headers,ROOT/'Tools/readiness_capture.cpp']
    def hashes():return {('driver/'+p.name if p==ROOT/'Tools/readiness_capture.cpp' else p.relative_to(source).as_posix()):sha(p) for p in files}
    before=hashes();compiler=shutil.which(args.compiler)
    if not compiler:parser.error('Compiler unavailable:'+args.compiler)
    build_key=hashlib.sha256(str(out).encode('utf-8')).hexdigest()[:12]
    build=ROOT/'build/readiness'/(out.name+'-'+build_key);build.mkdir(parents=True,exist_ok=True);executable=build/('ReadinessCapture.exe' if os.name=='nt' else 'ReadinessCapture')
    command=[compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(source/'Source'),str(ROOT/'Tools/readiness_capture.cpp'),*[str(p) for p in sources],'-o',str(executable)]
    manifest=dict(recorded_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),platform=platform.platform(),compiler=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],source_snapshot=str(source),source_commit=None,base_git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),source_sha256=before,compiled_sources=[p.relative_to(source).as_posix() for p in sources],fixture_seal=(fixtures/'SEALED_SHA256.txt').read_text().strip(),configuration='44100/48000/96000 x32/64/128' if args.full else '48000/64',scope='Research detector capture before MIDI/MPE routing, not musical readiness or physical latency proof')
    with (out/'compile.txt').open('w') as log:compiled=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    manifest['compiler_exit_code']=compiled.returncode
    if compiled.returncode:
        (out/'measurement-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return compiled.returncode
    if hashes()!=before:raise RuntimeError('Source changed during compile; mixed-source measurement refused')
    manifest['executable_sha256']=sha(executable)
    capture=[str(executable),str(fixtures),str(out)]+(['full'] if args.full else [])
    with (out/'capture-console.txt').open('w') as log:result=subprocess.run(capture,stdout=log,stderr=subprocess.STDOUT)
    manifest['capture_exit_code']=result.returncode;manifest['source_changed_during_measurement']=hashes()!=before
    if result.returncode==0:
        with (out/'scoring-console.txt').open('w') as log:score=subprocess.run([sys.executable,str(ROOT/'Tests/AudioFixtures/readiness/score.py'),'--fixtures',str(fixtures),'--capture',str(out),'--output',str(out/'scoring')],stdout=log,stderr=subprocess.STDOUT)
        manifest['score_exit_code']=score.returncode
    manifest['files_sha256']={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file() and p.name!='measurement-manifest.json'}
    (out/'measurement-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if manifest['source_changed_during_measurement']:raise RuntimeError('Source changed during measurement; this is not final proof')
    print(json.dumps({k:manifest.get(k) for k in ('capture_exit_code','score_exit_code','source_changed_during_measurement','fixture_seal')},indent=2))
    return result.returncode or manifest.get('score_exit_code',2)
if __name__=='__main__':raise SystemExit(main())
