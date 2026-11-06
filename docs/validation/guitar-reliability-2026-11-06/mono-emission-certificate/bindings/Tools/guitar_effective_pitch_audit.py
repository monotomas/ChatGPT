#!/usr/bin/env python3
"""Offline raw absolute-pitch audit, separate from nominal MIDI-anchor counts.

SPDX-License-Identifier: AGPL-3.0-or-later
This audits detector N/F/P output before MIDI correction/rendering. It does
not supply physical pick/damping truth or infer a continuous human technique.
The author's nominal MIDI note defines its exact rounding bin [n-.5,n+.5).
"""
from pathlib import Path,PurePosixPath
from collections import defaultdict
import argparse,csv,hashlib,json,math,shutil,datetime
FIXTURES=Path(__file__).resolve().parents[1]/'Tests/AudioFixtures/guitar_reliability_2026_11_06'
PRE_CAPTURE_SHA256='d312dabcffb03ea49b14eb74a8f7a9934317b7535988d278f8ff2dbd2c3fbe92'
FLOAT_MANIFEST_SHA256='4246566546d334ad8b21b1a733ff1697fd2ce3dedb58fa09b21ce18183f7961b'
PROFILES={'mono_plain_dynamics_off','mono_plain_dynamics_default','mono_bend_dynamics_off','mono_bend_dynamics_default'}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def write(p,d):p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n')
def readcsv(p):
 with p.open(newline='') as f:return list(csv.DictReader(f))
def union_length(intervals):
 merged=[]
 for lo,hi in sorted(intervals):
  if hi<=lo:continue
  if merged and lo<=merged[-1][1]:merged[-1]=(merged[-1][0],max(hi,merged[-1][1]))
  else:merged.append((lo,hi))
 return sum(hi-lo for lo,hi in merged)
def require(condition,message):
 if not condition:raise ValueError('Unverified DEV capture: '+message)
def digest_string(value):return isinstance(value,str) and len(value)==64 and all(c in '0123456789abcdef' for c in value)
def relative_name(name):
 require(isinstance(name,str) and name and '\\' not in name,'invalid relative proof path')
 p=PurePosixPath(name);require(not p.is_absolute() and '..' not in p.parts and ':' not in name,'unsafe relative proof path')
 return p
def regular(path):
 require(path.is_file() and not path.is_symlink(),'missing or linked input: '+str(path))
 return path.read_bytes()
def unique_csv(path,expected):
 with path.open(newline='') as f:
  reader=csv.DictReader(f);require(reader.fieldnames==expected,'unexpected CSV schema: '+path.name)
  rows=list(reader)
 require(all(set(row)==set(expected) and all(isinstance(v,str) for v in row.values()) for row in rows),'truncated or extra CSV fields: '+path.name)
 return rows
def nearest(value):return int(math.floor(value+.5))
def validate_dev_capture(capture,fixtures):
 """Validate text/metadata bindings before making any DEV/reserved claim.

 No audio or executable bytes are loaded or executed. This checks a recorded
 capture's declared inputs and retained proof; it is not a fresh detector run
 or an attestation that arbitrary native code was executed.
 """
 fixture_hashes={}
 def metadata(name,expected=None):
  relative_name(name);value=regular(fixtures/name);digest=hashlib.sha256(value).hexdigest()
  if expected is not None:require(digest==expected,'immutable metadata hash differs: '+name)
  fixture_hashes[name]=digest;return json.loads(value)
 seal=metadata('PRE_CAPTURE_SEAL.json',PRE_CAPTURE_SHA256)
 require(len(seal)==5,'pre-capture seal inventory differs')
 documents={name:metadata(name,digest) for name,digest in seal.items() if name.endswith('.json')}
 for name,digest in seal.items():
  if name.endswith('.json'):continue
  value=regular(fixtures/name);require(hashlib.sha256(value).hexdigest()==digest,'sealed metadata differs: '+name);fixture_hashes[name]=digest
 numeric=metadata('float32-exact-manifest.json',FLOAT_MANIFEST_SHA256)
 source=documents['source-manifest.json'];split=documents['split-before-any-detector.json'];annotation=documents['independent-acoustic-annotations.json']
 dev={r['path']:r['midi_nominal'] for r in split['development']};reserved={r['path'] for r in split['reserved_holdout']}
 require(len(dev)==len(reserved)==7 and not set(dev)&reserved,'fixed development/reserved split differs')
 originals={r['path']:r for r in source['files']};conversions={r['source_path']:r for r in numeric['files']};notes={r['path']:r for r in annotation['notes']}
 expected={}
 for path,note in dev.items():
  require(path in originals and path in conversions and path in notes,'missing development metadata')
  r=conversions[path];a=notes[path];record=PurePosixPath(path).stem
  require(record not in expected and r['midi_nominal']==note==originals[path]['midi_nominal']==a['midi_nominal_from_author_sfz'],'author nominal note differs')
  require(r['source_sha256']==originals[path]['sha256']==a['source_sha256'] and a['split']=='development','development source identity differs')
  require(r['sample_rate']==44100 and r['frames']==originals[path]['wave']['frames'],'development source shape differs')
  expected[record]={'source_path':path,'note':note,'conversion':r,'annotation':a}
 reserved_records={PurePosixPath(path).stem for path in reserved}
 capture_hashes={}
 manifest_bytes=regular(capture/'measurement-manifest.json');manifest=json.loads(manifest_bytes);capture_hashes['measurement-manifest.json']=hashlib.sha256(manifest_bytes).hexdigest()
 require(manifest.get('compiler_exit_code')==0 and manifest.get('capture_exit_code')==0,'capture did not complete successfully')
 require(manifest.get('frozen_source_unchanged') is True and manifest.get('metadata_unchanged') is True,'capture inputs were not frozen')
 require(manifest.get('reserved_inputs_exposed_to_executable') is False,'capture does not declare DEV-only executable input')
 require(manifest.get('expected_runs')==56,'unexpected declared run count')
 require(manifest.get('startup_contexts')=={'cold':0,'digital_idle_seconds':.350} and manifest.get('appended_silence_seconds')==.300,'startup/padding scope differs')
 require(manifest.get('input_gain')==1 and manifest.get('resampling') is False,'input processing scope differs')
 require(manifest.get('frontend_calibration')=='off; dynamics profiles explicitlyoff/default, no learned capture','calibration/profile scope differs')
 require(manifest.get('physical_contact_latency_claim') is False and manifest.get('physical_damping_claim') is False,'capture makes unsupported physical claims')
 if 'audio_inputs_unchanged' in manifest:require(manifest['audio_inputs_unchanged'] is True,'capture audio changed')
 if 'decoded_inventory_sha256' in manifest:require(manifest['decoded_inventory_sha256']==FLOAT_MANIFEST_SHA256,'decoded inventory binding differs')
 inventory=manifest.get('files_sha256');require(isinstance(inventory,dict) and inventory,'missing original proof inventory')
 def bound(name):
  relative_name(name);require(name in inventory and digest_string(inventory[name]),'missing proof hash: '+name)
  value=regular(capture/name);digest=hashlib.sha256(value).hexdigest();require(digest==inventory[name],'original proof hash differs: '+name);capture_hashes[name]=digest;return value
 source_hashes=manifest.get('source_sha256');require(isinstance(source_hashes,dict) and source_hashes,'missing frozen source hashes')
 for name,digest in source_hashes.items():
  p=relative_name(name);require(p.parent.as_posix()=='Source/Pitch' and p.suffix in ('.cpp','.h') and digest_string(digest),'unexpected source inventory entry')
  bound('source/'+name);require(capture_hashes['source/'+name]==digest,'frozen source binding differs: '+name)
 compiled=sorted(name for name in source_hashes if name.endswith('.cpp') and PurePosixPath(name).name!='MpeOutputRouter.cpp')
 require(sorted(manifest.get('compiled_sources',[]))==compiled,'compiled source inventory differs')
 driver=[name for name in ('capture.cpp','guitar_reliability_capture.cpp') if name in inventory]
 runner=[name for name in ('run.py','guitar_reliability.py') if name in inventory]
 require(len(driver)==len(runner)==1,'unknown or ambiguous capture driver/orchestration')
 bound(driver[0]);bound(runner[0]);require(capture_hashes[driver[0]]==manifest.get('driver_sha256') and capture_hashes[runner[0]]==manifest.get('orchestration_sha256'),'driver/orchestration binding differs')
 executable=[name for name in ('RealPluckCapture','GuitarReliabilityCapture','GuitarReliabilityCapture.exe') if name in inventory]
 require(len(executable)==1 and inventory[executable[0]]==manifest.get('executable_sha256'),'declared executable hash differs')
 allowed={'before-compile-manifest.json','capture-console.txt','capture.csv','cases.json','compile.txt','development-inputs.tsv','events.csv','summary.json',driver[0],runner[0],executable[0],*('source/'+name for name in source_hashes)}
 require(set(inventory)==allowed,'unknown or missing original capture proof file')
 for name in sorted(allowed-{executable[0]}):bound(name)
 # Archived copies may omit only the runtime executable; never read it here.
 actual={p.relative_to(capture).as_posix() for p in capture.rglob('*') if p.is_file()}
 require(not actual-(allowed|{'measurement-manifest.json'}),'unlisted files in original capture')
 before_compile=json.loads(bound('before-compile-manifest.json'))
 for name in ('source_sha256','driver_sha256','orchestration_sha256','metadata_sha256','input_sha256','reserved_inputs_exposed_to_executable','expected_runs'):
  require(name in before_compile,'missing before-compile binding: '+name)
 require(all(manifest.get(name)==value for name,value in before_compile.items()),'before-compile and measured manifest differ')
 metadata_hashes=manifest.get('metadata_sha256');mandatory={*seal,'PRE_CAPTURE_SEAL.json','float32-exact-manifest.json'};optional={'INVENTORY_SHA256.json','verify.py','decode_exact.py'}
 require(isinstance(metadata_hashes,dict) and mandatory<=set(metadata_hashes)<=mandatory|optional,'metadata binding inventory differs')
 for name,digest in metadata_hashes.items():
  value=regular(fixtures/name);require(digest_string(digest) and hashlib.sha256(value).hexdigest()==digest,'capture metadata differs from immutable edition: '+name);fixture_hashes[name]=digest
 require(manifest.get('input_sha256')=={path:{'wav_sha256':originals[path]['sha256'],'float32_sha256':conversions[path]['sha256']} for path in dev},'capture audio identity is not the exact fixed DEV set')
 with (capture/'development-inputs.tsv').open(newline='') as f:
  reader=csv.DictReader(f,delimiter='\t');require(reader.fieldnames==['record','float_path','rate','nominal_note','frames','split'],'development TSV schema differs');input_rows=list(reader)
 require(len(input_rows)==7 and {r['record'] for r in input_rows}==set(expected),'development TSV includes unknown/reserved/duplicate records')
 for row in input_rows:
  r=expected[row['record']]['conversion'];parts=PurePosixPath(row['float_path'].replace('\\','/')).parts;wanted=PurePosixPath(r['path']).parts
  require('..' not in parts and tuple(parts[-len(wanted):])==wanted,'development TSV points to an unexpected decoded input')
  require(row['split']=='development' and int(row['rate'])==44100 and int(row['nominal_note'])==r['midi_nominal'] and int(row['frames'])==r['frames'],'development TSV shape/note/split differs')
 metrics=unique_csv(capture/'capture.csv',['record','profile','idle_samples','rate','block','source_frames','total_frames','events','overflow','deadline_misses','max_callback_us','active_at_end'])
 raw=unique_csv(capture/'events.csv',['record','profile','idle_samples','rate','block','sample','type','note','velocity','pitch','voice_id'])
 cases=json.loads(bound('cases.json'));require(isinstance(cases,list),'case results must be a list')
 keys={(record,profile,idle) for record in expected for profile in PROFILES for idle in (0,15435)}
 def key(row):return row['record'],row['profile'],int(row['idle_samples'])
 require(len(metrics)==56 and {key(m) for m in metrics}==keys,'unknown/reserved/partial/duplicate metric inventory')
 require(len(cases)==56 and {key(c) for c in cases}==keys,'unknown/reserved/partial/duplicate case inventory')
 by_metric={key(m):m for m in metrics};by_case={key(c):c for c in cases};grouped=defaultdict(list)
 for e in raw:
  k=key(e);require(k in keys,'unknown or reserved event record/profile/context');m=by_metric[k];at=int(e['sample'])
  require(int(e['rate'])==44100 and int(e['block'])==64 and 0<=at<int(m['total_frames']),'invalid event timing/configuration')
  require(e['type'] in ('N','F','P') and 0<=int(e['note'])<=127 and 0<=int(e['velocity'])<=127 and int(e['voice_id'])==0,'invalid mono event')
  if e['type']=='P':require(e['profile'].startswith('mono_bend_') and math.isfinite(float(e['pitch'])),'unexpected or nonfinite continuous expression')
  else:require(e['pitch']=='','note event contains an unexpected absolute-pitch value')
  require(not grouped[k] or at>=int(grouped[k][-1]['sample']),'nonmonotonic event decisions');grouped[k].append(e)
 for k,m in by_metric.items():
  record,profile,idle=k;r=expected[record];conversion=r['conversion'];c=by_case[k]
  require(int(m['rate'])==44100 and int(m['block'])==64 and int(m['source_frames'])==conversion['frames'] and int(m['total_frames'])==conversion['frames']+idle+13230,'metric shape differs from exact DEV input')
  require(int(m['events'])==len(grouped[k]),'raw event count and metric differ')
  for field in ('overflow','deadline_misses','active_at_end'):require(int(m[field])>=0 and c[field]==int(m[field]),'case and metric disagree: '+field)
  require(math.isfinite(float(m['max_callback_us'])) and float(m['max_callback_us'])>=0 and c['max_callback_us']==float(m['max_callback_us']),'case and metric CPU observation differ')
  require(c['source_path']==r['source_path'] and c['nominal_note']==r['note'] and c['rate']==44100 and c['block']==64,'case reference identity differs')
  expected_core=[idle+nearest(t*44100) for t in r['annotation']['conservative_positive_core_seconds']]
  require(c['positive_core_samples']==expected_core,'case core differs from independent pre-capture annotation')
  require(c['events']==[{'sample':int(e['sample']),'type':e['type'],'note':int(e['note'])} for e in grouped[k] if e['type']!='P'],'case timeline and raw decisions differ')
  ons=[e for e in grouped[k] if e['type']=='N'];wanted=sum(int(e['note'])==r['note'] for e in ons)
  require(c['note_ons']==len(ons) and c['wanted_note_ons']==wanted and c['wrong_note_ons']==len(ons)-wanted and c['repeated_nominal_note_ons']==max(0,wanted-1),'case onset counts and raw decisions differ')
 reserved_in_metrics={m['record'] for m in metrics}&reserved_records
 exposed_reserved={r['record'] for r in input_rows}&reserved_records
 report={'status':'verified-fixed-development-capture','development_records':sorted(expected),'profiles':sorted(PROFILES),'idle_samples':[0,15435],'verified_configurations':len(keys),'reserved_records_in_capture':sorted(reserved_in_metrics),'reserved_record_runs':sum(m['record'] in reserved_records for m in metrics),'reserved_inputs_exposed':bool(exposed_reserved),'metadata_files_sha256':fixture_hashes,'capture_files_sha256':capture_hashes,'source_inputs_verified':len(source_hashes),'audio_bytes_loaded_or_decoded':False,'native_executable_bytes_loaded_or_reexecuted':False,'scope':'Verified retained capture metadata, fixed DEV input inventory and hashes; no new audio/detector execution, physical measurement or native-execution attestation'}
 return cases,grouped,{k:int(m['total_frames']) for k,m in by_metric.items()},report
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);p.add_argument('--fixtures',type=Path,default=FIXTURES);p.add_argument('--output',type=Path,required=True);a=p.parse_args();capture=a.capture.resolve();out=a.output.resolve()
 if out==capture or capture in out.parents:p.error('The original capture cannot be an audit output directory')
 fixtures=a.fixtures.resolve()
 if out==fixtures or fixtures in out.parents:p.error('The immutable fixture edition cannot be an audit output directory')
 if out.exists() and any(out.iterdir()):p.error('Refuse nonempty audit output')
 original,grouped,totalFrames,validation=validate_dev_capture(capture,fixtures)
 before=validation['capture_files_sha256'];out.mkdir(parents=True,exist_ok=True)
 rows=[]
 for c in original:
  key=(c['record'],c['profile'],c['idle_samples']);raw=grouped[key];nominal=c['nominal_note'];rate=c['rate'];core=tuple(c['positive_core_samples']);held={};pending={};segments=[];ons=[]
  def finish(voice,at):
   state=held[voice]
   if at>state['since']:segments.append((state['since'],at,state['pitch'],state['anchor']))
   state['since']=at
  for e in raw:
   at=int(e['sample']);voice=int(e['voice_id']);note=int(e['note']);kind=e['type']
   if kind=='P':
    value=float(e['pitch'])
    if not math.isfinite(value):raise ValueError('Nonfinite raw pitch')
    if voice in held:finish(voice,at);held[voice]['pitch']=value
    pending[voice]=(at,value)
   elif kind=='N':
    if voice in held:finish(voice,at)
    previous=pending.get(voice)
    pitch=previous[1] if previous and previous[0]<=at else float(note)
    held[voice]={'anchor':note,'pitch':pitch,'since':at}
    ons.append({'sample':at,'file_relative_ms':1000*(at-c['idle_samples'])/rate,'anchor':note,'absolute_pitch':pitch,'nominal_anchor_matches':note==nominal,'absolute_pitch_in_author_rounding_bin':nominal-.5<=pitch<nominal+.5,'physical_instantaneous_source_pitch_verified':False})
   elif kind=='F':
    if voice in held and held[voice]['anchor']==note:finish(voice,at);held.pop(voice);pending.pop(voice,None)
  for voice in list(held):finish(voice,totalFrames[key])
  active=[];good=[];pitches=[];anchorGood=[]
  for lo,hi,pitch,anchor in segments:
   lo=max(lo,core[0]);hi=min(hi,core[1])
   if hi<=lo:continue
   active.append((lo,hi));pitches.append(pitch)
   if nominal-.5<=pitch<nominal+.5:good.append((lo,hi))
   if anchor==nominal:anchorGood.append((lo,hi))
  length=core[1]-core[0];activeLength=union_length(active);goodLength=union_length(good);anchorLength=union_length(anchorGood)
  rows.append({'record':c['record'],'profile':c['profile'],'idle_samples':c['idle_samples'],'nominal_note':nominal,'positive_core_samples':list(core),'core_ms':1000*length/rate,'core_silent_ms':1000*(length-activeLength)/rate,'core_active_outside_author_pitch_bin_ms':1000*(activeLength-goodLength)/rate,'core_effective_pitch_uncovered_ms':1000*(length-goodLength)/rate,'core_nominal_anchor_uncovered_ms':1000*(length-anchorLength)/rate,'core_effective_pitch_completely_missing':goodLength==0,'core_nominal_anchor_completely_missing':anchorLength==0,'active_core_pitch_range':None if not pitches else [min(pitches),max(pitches)],'raw_onsets':ons,'scope':'Absolute detector pitch before correction/rendering; nominal author bin, no per-instant physical source pitch or damping truth'})
 def totals(selected):return {'runs':len(selected),'core_effective_pitch_completely_missing':sum(r['core_effective_pitch_completely_missing'] for r in selected),'core_nominal_anchor_completely_missing':sum(r['core_nominal_anchor_completely_missing'] for r in selected),'core_silent_ms':sum(r['core_silent_ms'] for r in selected),'core_active_outside_author_pitch_bin_ms':sum(r['core_active_outside_author_pitch_bin_ms'] for r in selected),'core_effective_pitch_uncovered_ms':sum(r['core_effective_pitch_uncovered_ms'] for r in selected),'core_nominal_anchor_uncovered_ms':sum(r['core_nominal_anchor_uncovered_ms'] for r in selected)}
 summary={'scope':'Separate offline audit of raw N/F/P effective absolute pitch; original nominal-anchor cases/summary remain unchanged. This stops before MIDI scale correction/synth rendering. Positive cores and author nominal anchors were set independently before detector capture.','rounding_bin':'[author nominal note -0.5, author nominal note +0.5)','physical_instantaneous_source_pitch_verified':False,'physical_contact_or_damping_truth':False,'reserved_holdout_samples_run':validation['reserved_record_runs'],'total':totals(rows),'profiles':{profile:totals([r for r in rows if r['profile']==profile]) for profile in sorted({r['profile'] for r in rows})}}
 write(out/'cases.json',rows);write(out/'summary.json',summary);shutil.copy2(Path(__file__),out/'guitar_effective_pitch_audit.py')
 write(out/'development-bindings.json',validation)
 if before!={name:sha(capture/name) for name in before}:raise RuntimeError('Original capture changed during audit')
 if validation['metadata_files_sha256']!={name:sha(fixtures/name) for name in validation['metadata_files_sha256']}:raise RuntimeError('Immutable metadata changed during audit')
 manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'original_capture':str(capture),'original_files_sha256':before,'original_capture_unchanged':True,'audit_driver_sha256':sha(out/'guitar_effective_pitch_audit.py'),'audit_files_sha256':{p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()},'reserved_inputs_exposed':validation['reserved_inputs_exposed'],'development_bindings_verified':validation['status']=='verified-fixed-development-capture'}
 write(out/'audit-manifest.json',manifest);print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
