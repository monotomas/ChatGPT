#!/usr/bin/env python3
"""DEV-only causal real-pluck benchmark. Reserved data cannot be selected here.

SPDX-License-Identifier: AGPL-3.0-or-later
Reference nominal notes/positive cores were authored before any detector run.
Timing relative to acoustic activity is descriptive; no physical contact/damping
or continuous human performance truth is asserted. No tolerances are fitted.
"""
from pathlib import Path
from collections import defaultdict
import argparse,csv,datetime,hashlib,json,math,shutil,subprocess,sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
FIXTURES=ROOT/'Tests/AudioFixtures/guitar_reliability_2026_11_06'
PRE_CAPTURE_SHA256='d312dabcffb03ea49b14eb74a8f7a9934317b7535988d278f8ff2dbd2c3fbe92'
FLOAT_MANIFEST_SHA256='4246566546d334ad8b21b1a733ff1697fd2ce3dedb58fa09b21ce18183f7961b'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,d):p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n')
def load(p):return json.loads(p.read_text())
def readcsv(p):
 with p.open(newline='') as f:return list(csv.DictReader(f))
def nearest(x):return int(math.floor(x+.5))
def summarize(out,data,inventory):
 annotation={x['path']:x for x in load(data/'independent-acoustic-annotations.json')['notes']}
 events=defaultdict(list)
 for e in readcsv(out/'events.csv'):events[(e['record'],e['profile'],e['idle_samples'])].append(e)
 rows=[]
 for m in readcsv(out/'capture.csv'):
  source=inventory[m['record']];a=annotation[source['source_path']];rate=int(m['rate']);idle=int(m['idle_samples']);raw=events[(m['record'],m['profile'],m['idle_samples'])];nominal=source['midi_nominal']
  on=[e for e in raw if e['type']=='N'];wanted=[e for e in on if int(e['note'])==nominal];wrong=[e for e in on if int(e['note'])!=nominal]
  # All lifetimes are reconstructed from actual raw decisions, never shifted.
  active={};intervals=[];orphanOffs=0;overlaps=0
  for e in raw:
   at=int(e['sample']);voice=int(e['voice_id']);note=int(e['note'])
   if e['type']=='N':
    if voice in active:
     old=active.pop(voice);old['off']=at;intervals.append(old);overlaps+=1
    active[voice]={'note':note,'on':at,'off':None}
   elif e['type']=='F':
    if voice in active and active[voice]['note']==note:
     item=active.pop(voice);item['off']=at;intervals.append(item)
    else:orphanOffs+=1
  intervals.extend(active.values());end=int(m['total_frames'])
  core=tuple(idle+nearest(t*rate) for t in a['conservative_positive_core_seconds']);segments=[]
  for v in intervals:
   if v['note']!=nominal:continue
   lo=max(core[0],v['on']);hi=min(core[1],v['off'] if v['off'] is not None else end)
   if hi>lo:segments.append((lo,hi))
  segments.sort();merged=[]
  for lo,hi in segments:
   if merged and lo<=merged[-1][1]:merged[-1]=(merged[-1][0],max(merged[-1][1],hi))
   else:merged.append((lo,hi))
  covered=sum(hi-lo for lo,hi in merged);gap=max(0,core[1]-core[0]-covered)
  missingCore=covered==0;coreWrong=sum(core[0]<=int(e['sample'])<core[1] for e in wrong)
  first=min((int(e['sample']) for e in wanted),default=None)
  cue=a['activity_cue_seconds'];timing=None if first is None else {'since_file_start_ms':1000*(first-idle)/rate,'since_activity_low_ms':1000*((first-idle)/rate-cue['low_boundary']),'since_activity_high_ms':1000*((first-idle)/rate-cue['high_boundary']),'physical_contact_verified':False}
  row=dict(record=m['record'],source_path=source['source_path'],profile=m['profile'],idle_samples=idle,rate=rate,block=int(m['block']),nominal_note=nominal,note_ons=len(on),wanted_note_ons=len(wanted),wrong_note_ons=len(wrong),repeated_nominal_note_ons=max(0,len(wanted)-1),false_octave_note_ons=sum(abs(int(e['note'])-nominal) in (12,24,36) for e in wrong),lower_octave_note_ons=sum(nominal-int(e['note']) in (12,24,36) for e in wrong),wrong_ons_in_positive_core=coreWrong,positive_core_samples=list(core),positive_core_covered_samples=covered,positive_core_uncovered_ms=1000*gap/rate,positive_core_missing=missingCore,orphan_offs=orphanOffs,duplicate_active_voice=overlaps,active_at_end=int(m['active_at_end']),overflow=int(m['overflow']),deadline_misses=int(m['deadline_misses']),max_callback_us=float(m['max_callback_us']),first_correct_descriptive_timing=timing,events=[{'sample':int(e['sample']),'type':e['type'],'note':int(e['note'])} for e in raw if e['type']!='P'])
  row['bounded_note_and_core_checks_passed']=not(any(row[k] for k in ('wrong_note_ons','repeated_nominal_note_ons','positive_core_missing','positive_core_uncovered_ms','orphan_offs','duplicate_active_voice','active_at_end','overflow')))
  rows.append(row)
 counts=('note_ons','wanted_note_ons','wrong_note_ons','repeated_nominal_note_ons','false_octave_note_ons','lower_octave_note_ons','wrong_ons_in_positive_core','positive_core_missing','orphan_offs','duplicate_active_voice','active_at_end','overflow','deadline_misses')
 def totals(selected):return {'runs':len(selected),'bounded_note_and_core_checks_passed':sum(r['bounded_note_and_core_checks_passed'] for r in selected),'counts':{k:sum(r[k] for r in selected) for k in counts},'positive_core_uncovered_ms':sum(r['positive_core_uncovered_ms'] for r in selected)}
 summary={'scope':'Seven predeclared DEV isolated real electric-guitar samples; raw44100Hz/64sample causal mono, no gain/resampling, four profiles and two explicit startup contexts; no continuous human performance/physical contact/damping/universal readiness claim','reserved_holdout_samples_run':0,'physical_latency_measured':False,'physical_damping_scored':False,'tolerances_fitted_to_capture':False,'total':totals(rows),'profiles':{profile:totals([r for r in rows if r['profile']==profile]) for profile in sorted({r['profile'] for r in rows})},'idle_contexts':{str(idle):totals([r for r in rows if r['idle_samples']==idle]) for idle in sorted({r['idle_samples'] for r in rows})}}
 write(out/'cases.json',rows);write(out/'summary.json',summary);return summary
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-root',type=Path,default=ROOT);p.add_argument('--fixtures',type=Path,default=FIXTURES);p.add_argument('--decoded',type=Path,required=True,help='External output from the fixture edition decode_exact.py; no generated audio is written to the immutable edition');p.add_argument('--output',type=Path,required=True);p.add_argument('--compiler',default='g++');a=p.parse_args()
 root=a.source_root.resolve();data=a.fixtures.resolve();decoded_root=a.decoded.resolve();out=a.output.resolve()
 if out==data or data in out.parents:p.error('The immutable fixture edition cannot be a measurement output')
 if decoded_root==data or data in decoded_root.parents:p.error('Decoded audio must be outside the immutable fixture edition')
 if sha(data/'PRE_CAPTURE_SEAL.json')!=PRE_CAPTURE_SHA256:raise RuntimeError('Unexpected pre-capture seal edition')
 if sha(data/'float32-exact-manifest.json')!=FLOAT_MANIFEST_SHA256:raise RuntimeError('Unexpected numeric decoding edition')
 if sha(decoded_root/'float32-exact-manifest.json')!=FLOAT_MANIFEST_SHA256:raise RuntimeError('External decoded inventory differs from the immutable edition')
 subprocess.run([sys.executable,'-B',str(data/'verify.py')],check=True)
 if out.exists() and any(out.iterdir()):p.error('Refuse nonempty measurement output')
 out.mkdir(parents=True,exist_ok=True)
 seal=load(data/'PRE_CAPTURE_SEAL.json')
 for name,digest in seal.items():
  if sha(data/name)!=digest:raise RuntimeError('Pre-capture metadata seal changed: '+name)
 split=load(data/'split-before-any-detector.json');wanted={r['path']:r['midi_nominal'] for r in split['development']};reserved={r['path'] for r in split['reserved_holdout']}
 if len(wanted)!=7 or set(wanted)&reserved:raise RuntimeError('Invalid predeclared split')
 decoded=load(data/'float32-exact-manifest.json');inputs={Path(r['source_path']).stem:r for r in decoded['files'] if r['source_path'] in wanted}
 if len(inputs)!=7 or {r['source_path'] for r in inputs.values()}!=set(wanted):raise RuntimeError('Incomplete DEV inventory')
 metadata_hashes={name:sha(data/name) for name in (*seal,'PRE_CAPTURE_SEAL.json','float32-exact-manifest.json','INVENTORY_SHA256.json','verify.py','decode_exact.py')}
 input_hashes={}
 for name,r in inputs.items():
  if sha(decoded_root/r['path'])!=r['sha256'] or sha(data/'original-source'/r['source_path'])!=r['source_sha256']:raise RuntimeError('Selected source input changed')
  if r['midi_nominal']!=wanted[r['source_path']] or r['sample_rate']!=44100:raise RuntimeError('Unexpected input identity/rate')
  input_hashes[r['source_path']]={'wav_sha256':r['source_sha256'],'float32_sha256':r['sha256']}
 sourcefiles=sorted(x for x in (root/'Source/Pitch').iterdir() if x.suffix in ('.cpp','.h'))
 before={p.relative_to(root).as_posix():sha(p) for p in sourcefiles};snapshot=out/'source/Source/Pitch';snapshot.mkdir(parents=True)
 for file in sourcefiles:shutil.copy2(file,snapshot/file.name)
 if any(sha(out/'source'/name)!=digest for name,digest in before.items()):raise RuntimeError('Snapshot source changed while copying')
 if before!={p.relative_to(root).as_posix():sha(p) for p in sourcefiles}:raise RuntimeError('Live source changed during snapshot')
 shutil.copy2(HERE/'guitar_reliability_capture.cpp',out/'guitar_reliability_capture.cpp');shutil.copy2(Path(__file__),out/'guitar_reliability.py')
 # Only DEV entries are exposed to the executable; there is no reserved option.
 inventory=out/'development-inputs.tsv'
 with inventory.open('w') as f:
  f.write('record\tfloat_path\trate\tnominal_note\tframes\tsplit\n')
  for name,r in inputs.items():f.write(f"{name}\t{decoded_root/r['path']}\t{r['sample_rate']}\t{r['midi_nominal']}\t{r['frames']}\tdevelopment\n")
 compiler=shutil.which(a.compiler)
 if compiler is None:raise RuntimeError('Compiler unavailable')
 executable=out/('GuitarReliabilityCapture.exe' if sys.platform=='win32' else 'GuitarReliabilityCapture');cpp=[p for p in sorted(snapshot.glob('*.cpp')) if p.name!='MpeOutputRouter.cpp']
 command=[compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(out/'source/Source'),str(out/'guitar_reliability_capture.cpp'),*map(str,cpp),'-o',str(executable)]
 manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_root':str(root),'source_sha256':before,'compiled_sources':[p.relative_to(out/'source').as_posix() for p in cpp],'source_scope':'Pitch C++ and headers only; MpeOutputRouter excluded from standalone executable','compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],'compile_command':command,'driver_sha256':sha(out/'guitar_reliability_capture.cpp'),'orchestration_sha256':sha(out/'guitar_reliability.py'),'metadata_sha256':metadata_hashes,'decoded_inventory_sha256':sha(decoded_root/'float32-exact-manifest.json'),'input_sha256':input_hashes,'reserved_inputs_exposed_to_executable':False,'expected_runs':56,'startup_contexts':{'cold':0,'digital_idle_seconds':.350},'appended_silence_seconds':.300,'input_gain':1,'resampling':False,'frontend_calibration':'off; dynamics profiles explicitlyoff/default, no learned capture','physical_contact_latency_claim':False,'physical_damping_claim':False}
 try:manifest['base_git_head']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
 except subprocess.CalledProcessError:manifest['base_git_head']=None
 write(out/'before-compile-manifest.json',manifest)
 with (out/'compile.txt').open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
 manifest['compiler_exit_code']=r.returncode
 if r.returncode:write(out/'measurement-manifest.json',manifest);return r.returncode
 manifest['executable_sha256']=sha(executable)
 with (out/'capture-console.txt').open('w') as f:r=subprocess.run([str(executable),str(inventory),str(out)],stdout=f,stderr=subprocess.STDOUT)
 manifest['capture_exit_code']=r.returncode
 if not r.returncode:
  summary=summarize(out,data,inputs)
  if summary['total']['runs']!=56:raise RuntimeError('Missing capture configurations')
 manifest['frozen_source_unchanged']=all(sha(out/'source'/name)==digest for name,digest in before.items())
 manifest['metadata_unchanged']=all(sha(data/name)==digest for name,digest in metadata_hashes.items())
 manifest['audio_inputs_unchanged']=all(sha(decoded_root/r['path'])==r['sha256'] and sha(data/'original-source'/r['source_path'])==r['source_sha256'] for r in inputs.values())
 manifest['files_sha256']={p.relative_to(out).as_posix():sha(p) for p in sorted(out.rglob('*')) if p.is_file() and p.name!='measurement-manifest.json'}
 write(out/'measurement-manifest.json',manifest)
 if not manifest['frozen_source_unchanged'] or not manifest['metadata_unchanged'] or not manifest['audio_inputs_unchanged']:raise RuntimeError('Measured inputs changed')
 print(json.dumps(summary['total'],indent=2) if not r.returncode else 'capture failed')
 return r.returncode
if __name__=='__main__':raise SystemExit(main())
