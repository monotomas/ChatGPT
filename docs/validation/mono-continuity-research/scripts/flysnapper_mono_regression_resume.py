#!/usr/bin/env python3
from pathlib import Path
import subprocess,hashlib,json,concurrent.futures,argparse,datetime
p=argparse.ArgumentParser();p.add_argument('snapshot',type=Path);p.add_argument('output',type=Path);p.add_argument('--units',action='store_true');args=p.parse_args()
root=Path('/workspace/ChatGPT');src=args.snapshot.resolve();out=args.output.resolve();out.mkdir(exist_ok=False)
cpp=[f for f in sorted((src/'Source/Pitch').glob('*.cpp')) if f.name!='MpeOutputRouter.cpp'];files=cpp+[f for f in sorted((src/'Source/Pitch').glob('*.h')) if f.name!='MpeOutputRouter.h']
def hashes():return {f.relative_to(src).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
before=hashes();jobs=[('validation',root/'Tools/audio_validation.cpp')]
if args.units:jobs.extend([('guitar',root/'Tests/GuitarQualityTests.cpp'),('pitch',root/'Tests/PitchTests.cpp')])
def compile(job):
    name,driver=job;exe=out/name
    with (out/(name+'-compile.txt')).open('w') as log:subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(src/'Source'),str(driver),*[str(f) for f in cpp],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
    return name,exe
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:executables=dict(pool.map(compile,jobs))
assert hashes()==before,'Source changed during compile'
commands=[]
for name,fixtures in [('public',root/'Tests/AudioFixtures'),('holdout',root/'Tests/AudioFixtures/holdout'),('confirmation',root/'Tests/AudioFixtures/confirmation')]:
    folder=out/name;folder.mkdir();commands.append((name,[str(executables['validation']),str(fixtures),str(folder)]))
if args.units:
    commands.extend([('guitar',[str(executables['guitar']),str(root/'Tests/AudioFixtures/guitar_controls')]),('pitch',[str(executables['pitch']),str(root/'Tests/AudioFixtures')])])
def run(pair):
    name,command=pair
    with (out/(name+'-console.txt')).open('w') as log:result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    return name,result.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:results=dict(pool.map(run,commands))
manifest=dict(scope='Seen functional regressions, NOT musical readiness or physical latency',source=str(src),recorded_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_sha256=before,source_changed=hashes()!=before,results=results)
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name,status in results.items():
    print(name,'exit',status)
    summary=out/name/'audio-summary.txt'
    if summary.exists():print(summary.read_text())
    else:print('\n'.join((out/(name+'-console.txt')).read_text().splitlines()[-8:]))
if manifest['source_changed']:raise SystemExit('Source changed')
