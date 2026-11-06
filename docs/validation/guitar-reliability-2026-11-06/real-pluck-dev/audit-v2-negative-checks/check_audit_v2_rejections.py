#!/usr/bin/env python3
"""Mutate copies of an actual DEV capture to check offline audit rejection.

No audio, decoder, compiler or detector is executed. These are metadata/input
validation counterexamples, not new DSP measurements or fake release tests.
"""
from pathlib import Path
import argparse,csv,hashlib,json,shutil,subprocess,sys,tempfile
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def csv_rows(p):
 with p.open(newline='') as f:r=csv.DictReader(f);return r.fieldnames,list(r)
def csv_write(p,fields,rows):
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('capture','tool','fixtures','output'):parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args();capture=args.capture.resolve();tool=args.tool.resolve();fixtures=args.fixtures.resolve();out=args.output.resolve()
 if out.exists() and any(out.iterdir()):parser.error('Refuse nonempty proof output')
 out.mkdir(parents=True,exist_ok=True)
 before={p.relative_to(capture).as_posix():sha(p) for p in capture.rglob('*') if p.is_file()}
 fixture_before={p.relative_to(fixtures).as_posix():sha(p) for p in fixtures.rglob('*') if p.is_file() and p.suffix!='.wav'}
 results=[]
 mutations=[('raw_event_hash_changed','original proof hash differs: events.csv'),('independent_core_changed_with_rebound_case_hash','case core differs from independent pre-capture annotation'),('reserved_record_with_rebound_event_metric_case_hashes','unknown/reserved/partial/duplicate metric inventory'),('partial55_cases_with_rebound_output_hashes','unknown/reserved/partial/duplicate metric inventory'),('metadata_hash_changed_in_both_manifests','capture metadata differs from immutable edition')]
 with tempfile.TemporaryDirectory(prefix='flysnapper-audit-v2-negative-') as temp:
  for name,marker in mutations:
   copied=Path(temp)/name
   shutil.copytree(capture,copied,ignore=lambda directory,names:[n for n in names if n in ('RealPluckCapture','GuitarReliabilityCapture','GuitarReliabilityCapture.exe')])
   manifest=load(copied/'measurement-manifest.json');changed=[]
   if name=='raw_event_hash_changed':
    with (copied/'events.csv').open('ab') as f:f.write(b'\n')
    changed=['events.csv'];description='Append one byte to actual raw event output without changing its recorded hash.'
   elif name=='independent_core_changed_with_rebound_case_hash':
    cases=load(copied/'cases.json');cases[0]['positive_core_samples'][0]+=1;write(copied/'cases.json',cases)
    changed=['cases.json'];manifest['files_sha256']['cases.json']=sha(copied/'cases.json');description='Change one actual positive-core boundary and rebind the case-file hash; fixed pre-capture annotation must still reject it.'
   elif name=='reserved_record_with_rebound_event_metric_case_hashes':
    for filename in ('events.csv','capture.csv'):
     fields,rows=csv_rows(copied/filename)
     for row in rows:
      if row['record']=='staccato_e4_rr3':row['record']='staccato_a4_rr1'
     csv_write(copied/filename,fields,rows)
    cases=load(copied/'cases.json')
    for case in cases:
     if case['record']=='staccato_e4_rr3':case['record']='staccato_a4_rr1'
    write(copied/'cases.json',cases);changed=['events.csv','capture.csv','cases.json']
    for filename in changed:manifest['files_sha256'][filename]=sha(copied/filename)
    description='Replace actual DEV record identities consistently in all raw/metric/case outputs with a known reserved name, rebinding all output hashes while retaining the claimed DEV-only metadata.'
   elif name=='partial55_cases_with_rebound_output_hashes':
    removed=('twang_e3_f_rr1','mono_plain_dynamics_off','0')
    for filename in ('events.csv','capture.csv'):
     fields,rows=csv_rows(copied/filename);rows=[r for r in rows if (r['record'],r['profile'],r['idle_samples'])!=removed];csv_write(copied/filename,fields,rows)
    cases=load(copied/'cases.json');write(copied/'cases.json',[c for c in cases if (c['record'],c['profile'],str(c['idle_samples']))!=removed]);changed=['events.csv','capture.csv','cases.json']
    for filename in changed:manifest['files_sha256'][filename]=sha(copied/filename)
    description='Remove one actual configuration from events, metrics and cases; rebind all three hashes without changing the claimed expected56 configurations.'
   else:
    manifest['metadata_sha256']['split-before-any-detector.json']='0'*64
    prior=load(copied/'before-compile-manifest.json');prior['metadata_sha256']['split-before-any-detector.json']='0'*64;write(copied/'before-compile-manifest.json',prior);manifest['files_sha256']['before-compile-manifest.json']=sha(copied/'before-compile-manifest.json');changed=['before-compile-manifest.json'];description='Change the split metadata digest in both before-compile and measured manifests, rebinding their proof hash; immutable edition must still reject the self-consistent false binding.'
   write(copied/'measurement-manifest.json',manifest);changed.append('measurement-manifest.json')
   audit_out=Path(temp)/(name+'-audit')
   command=[sys.executable,'-B',str(tool),'--capture',str(copied),'--fixtures',str(fixtures),'--output',str(audit_out)]
   result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   (out/(name+'.stdout.txt')).write_text(result.stdout);(out/(name+'.stderr.txt')).write_text(result.stderr)
   assert result.returncode!=0 and marker in result.stderr,(name,result.returncode,result.stdout,result.stderr)
   assert not audit_out.exists(),'Rejected capture created an audit/claim output'
   results.append({'name':name,'description':description,'result':'rejected-as-required','exit_code':result.returncode,'expected_rejection_marker':marker,'changed_input_files_sha256':{f:sha(copied/f) for f in changed},'stdout_file':name+'.stdout.txt','stderr_file':name+'.stderr.txt','audit_output_created':False,'audio_decoder_compiler_detector_executed':False})
 assert before=={p.relative_to(capture).as_posix():sha(p) for p in capture.rglob('*') if p.is_file()}
 assert fixture_before=={p.relative_to(fixtures).as_posix():sha(p) for p in fixtures.rglob('*') if p.is_file() and p.suffix!='.wav'}
 shutil.copy2(Path(__file__),out/Path(__file__).name)
 report={'status':'passed','scope':'Actual DEV capture copies mutated only for offline input-validation checks; no new DSP measurements','actual_capture':str(capture),'actual_capture_manifest_sha256':sha(capture/'measurement-manifest.json'),'audit_tool_sha256':sha(tool),'original_capture_files_unchanged':len(before),'fixture_metadata_unchanged':True,'reserved_audio_loaded':False,'reserved_detector_runs':0,'checks':results}
 write(out/'rejection-checks.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
