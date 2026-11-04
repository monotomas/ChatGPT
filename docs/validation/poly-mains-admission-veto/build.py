#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
from pathlib import Path
import argparse,hashlib,json,platform,subprocess,time
ROOT=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('variant',choices=['baseline','candidate']);a=p.parse_args()
 source=ROOT/a.variant;build=ROOT/'build'/a.variant;build.mkdir(parents=True,exist_ok=True)
 sources=[p for p in sorted((source/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp']
 paths=[*sources,*sorted((source/'Source/Pitch').glob('*.h')),*sorted((source/'Tools').glob('*.cpp'))]
 before={p.relative_to(source).as_posix():sha(p) for p in paths};objects=[];commands=[];start=time.time()
 with (build/'compile.txt').open('w') as log:
  for cpp in sources:
   obj=build/(cpp.stem+'.o');cmd=['g++','-std=c++17','-O2','-DNDEBUG','-Wall','-Wextra','-Wpedantic','-I',str(source/'Source'),'-c',str(cpp),'-o',str(obj)]
   commands.append(cmd);subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True);objects.append(obj)
  for name,driver in [('AudioValidation','audio_validation.cpp'),('ReadinessCapture','readiness_capture.cpp')]:
   cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source/'Source'),str(source/'Tools'/driver),*[str(p) for p in objects],'-o',str(build/name)]
   commands.append(cmd);subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
 after={p.relative_to(source).as_posix():sha(p) for p in paths}
 if before!=after:raise RuntimeError('Snapshot changed while compiling')
 m={'scope':'Isolated research capture executables; not release acceptance','platform':platform.platform(),'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'variant':a.variant,'source_sha256':before,'source_changed':False,'commands':commands,'build_seconds':time.time()-start,'binaries_sha256':{name:sha(build/name) for name in ('AudioValidation','ReadinessCapture')}}
 (build/'build-manifest.json').write_text(json.dumps(m,indent=2)+'\n');print('Built',a.variant,len(sources),'DSP translation units')
if __name__=='__main__':main()
