#!/usr/bin/env python3
"""Offline raw absolute-pitch audit, separate from nominal MIDI-anchor counts.

SPDX-License-Identifier: AGPL-3.0-or-later
This audits detector N/F/P output before MIDI correction/rendering. It does
not supply physical pick/damping truth or infer a continuous human technique.
The author's nominal MIDI note defines its exact rounding bin [n-.5,n+.5).
"""
from pathlib import Path
from collections import defaultdict
import argparse,csv,hashlib,json,math,shutil,datetime
FIXTURES=Path(__file__).resolve().parents[1]/'Tests/AudioFixtures/guitar_reliability_2026_11_06'
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
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();capture=a.capture.resolve();out=a.output.resolve()
 if out==capture or capture in out.parents:p.error('The original capture cannot be an audit output directory')
 fixtures=FIXTURES.resolve()
 if out==fixtures or fixtures in out.parents:p.error('The immutable fixture edition cannot be an audit output directory')
 if out.exists() and any(out.iterdir()):p.error('Refuse nonempty audit output')
 out.mkdir(parents=True,exist_ok=True)
 before={name:sha(capture/name) for name in ('events.csv','capture.csv','cases.json','measurement-manifest.json')}
 original=load(capture/'cases.json');grouped=defaultdict(list)
 for e in readcsv(capture/'events.csv'):grouped[(e['record'],e['profile'],int(e['idle_samples']))].append(e)
 totalFrames={(m['record'],m['profile'],int(m['idle_samples'])):int(m['total_frames']) for m in readcsv(capture/'capture.csv')}
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
 summary={'scope':'Separate offline audit of raw N/F/P effective absolute pitch; original nominal-anchor cases/summary remain unchanged. This stops before MIDI scale correction/synth rendering. Positive cores and author nominal anchors were set independently before detector capture.','rounding_bin':'[author nominal note -0.5, author nominal note +0.5)','physical_instantaneous_source_pitch_verified':False,'physical_contact_or_damping_truth':False,'reserved_holdout_samples_run':0,'total':totals(rows),'profiles':{profile:totals([r for r in rows if r['profile']==profile]) for profile in sorted({r['profile'] for r in rows})}}
 write(out/'cases.json',rows);write(out/'summary.json',summary);shutil.copy2(Path(__file__),out/'guitar_effective_pitch_audit.py')
 if before!={name:sha(capture/name) for name in before}:raise RuntimeError('Original capture changed during audit')
 manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'original_capture':str(capture),'original_files_sha256':before,'original_capture_unchanged':True,'audit_driver_sha256':sha(out/'guitar_effective_pitch_audit.py'),'audit_files_sha256':{p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()},'reserved_inputs_exposed':False}
 write(out/'audit-manifest.json',manifest);print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
