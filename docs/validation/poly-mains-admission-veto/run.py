#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
from pathlib import Path
import argparse,hashlib,json,platform,subprocess,time
ROOT=Path(__file__).resolve().parent
REPO=Path('/workspace/ChatGPT')
SETS=[('controlled72','readiness',True),('legacy-public','',False),('legacy-holdout','holdout',False),('legacy-confirmation','confirmation',False),('recorded-noise128','readiness/recorded_noise',True)]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('variant',choices=['baseline','candidate']);a=p.parse_args()
 source=ROOT/a.variant;build=ROOT/'build'/a.variant;buildmeta=json.loads((build/'build-manifest.json').read_text())
 for name,digest in buildmeta['source_sha256'].items():assert sha(source/name)==digest
 output=ROOT/'reports'/a.variant;output.mkdir(parents=True,exist_ok=False)
 for name,path,readiness in SETS:
  fixture=REPO/'Tests/AudioFixtures'/path;out=output/name;out.mkdir()
  exe=build/('ReadinessCapture' if readiness else 'AudioValidation')
  start=time.perf_counter()
  with (out/'capture-console.txt').open('w') as log:r=subprocess.run([str(exe),str(fixture),str(out)],stdout=log,stderr=subprocess.STDOUT)
  m={'scope':'Bounded research comparison; every supplied source followed by existing driver behavior','variant':a.variant,'dataset':name,'capture_exit_code':r.returncode,'capture_seconds':time.perf_counter()-start,'binary_sha256':sha(exe),'source_sha256':buildmeta['source_sha256'],'source_changed_during_measurement':any(sha(source/n)!=d for n,d in buildmeta['source_sha256'].items()),'fixture_manifest_sha256':sha(fixture/'fixtures.json')}
  if readiness:
   m['fixture_seal']=(fixture/'SEALED_SHA256.txt').read_text().strip()
   if r.returncode==0:
    with (out/'scoring-console.txt').open('w') as log:s=subprocess.run(['python',str(REPO/'Tests/AudioFixtures/readiness/score.py'),'--fixtures',str(fixture),'--capture',str(out),'--output',str(out/'scoring')],stdout=log,stderr=subprocess.STDOUT)
    m['score_exit_code']=s.returncode
  (out/'measurement-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
  print(a.variant,name,'capture',r.returncode,'score',m.get('score_exit_code'),'seconds',round(m['capture_seconds'],2),flush=True)
  if r.returncode:raise RuntimeError('Capture failed')
if __name__=='__main__':main()
