#!/usr/bin/env python3
"""Rebuild one rejected snapshot in a NEW scratch directory; no release approval.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parent
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--variant',choices=('default-gate','double-conditioner','discard-inner','skip-inner','gate-pulse','trained'),required=True)
 p.add_argument('--suite',choices=('controlled','positive','trained'),required=True)
 p.add_argument('--fixtures',type=Path,help='Existing sealed repository readiness directory for controlled suite')
 p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 out=a.output.resolve()
 if out.exists():p.error('Output must not exist')
 if (a.variant=='trained')!=(a.suite=='trained'):p.error('Trained protocol must use trained variant and suite together')
 out.mkdir(parents=True)
 variant={'default-gate':'base','double-conditioner':'base'}.get(a.variant,a.variant)
 source=ROOT/'variants'/variant;shutil.copytree(source/'Source',out/'source/Source')
 shutil.copy2(source/'guitar_evaluation_capture.cpp',out/'source/guitar_evaluation_capture.cpp')
 if a.suite=='controlled':
  if not a.fixtures:p.error('--fixtures required for controlled suite')
  fixtures=a.fixtures.resolve();expected=json.loads((ROOT/'referenced-readiness-files.json').read_text())
  for key,value in expected.items():
   if key=='fixture_seal':continue
   name=Path(key).name
   if digest(fixtures/name)!=value['sha256']:raise RuntimeError('Changed fixture metadata:'+name)
 elif a.suite=='positive':
  fixtures=out/'fixtures';subprocess.run([sys.executable,'-B',str(ROOT/'fixtures/positive/generate.py'),str(fixtures)],check=True)
 else:
  helper=out/'helper-repository/Tests/AudioFixtures/readiness';helper.mkdir(parents=True)
  shutil.copy2(ROOT/'fixtures/trained/helper/generate.py',helper/'generate.py')
  fixtures=out/'fixtures';subprocess.run([sys.executable,'-B',str(ROOT/'fixtures/trained/generate.py'),str(out/'helper-repository'),str(fixtures)],check=True)
 # Omitted generated PCM must reproduce its original bytes before inference.
 if a.suite!='controlled':
  omitted=json.loads((ROOT/'omitted-artifacts.json').read_text())['items'];suffix='/positive-controls/pcm/' if a.suite=='positive' else '/fixtures/pcm/'
  wanted={Path(path).name:value for path,value in omitted.items() if suffix in path}
  for name,value in wanted.items():
   if digest(fixtures/'pcm'/name)!=value['sha256']:raise RuntimeError('Generated PCM differs:'+name)
 sources=[str(f) for f in sorted((out/'source/Source/Pitch').glob('*.cpp')) if f.name!='MpeOutputRouter.cpp']
 executable=out/'Capture'
 command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(out/'source/Source'),str(out/'source/guitar_evaluation_capture.cpp'),*sources,'-o',str(executable)]
 (out/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
 with (out/'compile.txt').open('w') as log:r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
 if r.returncode:return r.returncode
 mode={'default-gate':'default','double-conditioner':'preclean'}.get(a.variant,'preclean-noinner')
 capture=out/'capture';capture.mkdir()
 with (out/'capture-console.txt').open('w') as log:r=subprocess.run([str(executable),str(fixtures),str(capture),'quick',mode],stdout=log,stderr=subprocess.STDOUT)
 if r.returncode:return r.returncode
 if a.suite=='controlled':
  return subprocess.run([sys.executable,'-B',str(ROOT/'scoring/Tools/guitar_evaluation_score.py'),'--fixtures',str(fixtures),'--capture',str(capture),'--output',str(out/'scoring')]).returncode
 print('Capture completed. This does not approve the rejected snapshot.');return 0
if __name__=='__main__':raise SystemExit(main())
