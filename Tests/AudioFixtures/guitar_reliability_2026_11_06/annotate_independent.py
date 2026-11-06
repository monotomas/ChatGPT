"""Raw author WAV analysis only. Never imports/executes Flysnapper or reads MIDI."""
from pathlib import Path
import hashlib,json,math,time,wave
import numpy as np
root=Path(__file__).resolve().parent
manifest=json.loads((root/'source-manifest.json').read_text())
split=json.loads((root/'split-before-any-detector.json').read_text())
dev={r['path'] for r in split['development']}
def decode(path):
 with wave.open(str(path),'rb') as w:
  assert w.getsampwidth()==3 and w.getnchannels()==1
  b=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3)
  n=(b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16));n=(n^0x800000)-0x800000
  return n.astype(float)/8388608,w.getframerate()
def db(v):return float(20*np.log10(max(1e-12,float(v))))
def estimates(x,rate,note,center):
 half=int(rate*.08);idx=int(center*rate);a=x[max(0,idx-half):min(len(x),idx+half)]
 if len(a)<half:return []
 size=1<<int(np.ceil(np.log2(len(a)*8)))
 spec=np.abs(np.fft.rfft((a-a.mean())*np.hanning(len(a)),n=size));f=np.fft.rfftfreq(size,1/rate)
 want=440*2**((note-69)/12)
 out=[]
 for h in range(1,6):
  lo,hi=want*h*2**(-.4/12),want*h*2**(.4/12)
  good=np.flatnonzero((f>=lo)&(f<=hi));k=int(good[np.argmax(spec[good])])
  if k==good[0] or k==good[-1]:continue
  s=np.log(np.maximum(spec[k-1:k+2],1e-15));delta=.5*(s[0]-s[2])/(s[0]-2*s[1]+s[2])
  hz=(k+delta)*rate/size/h
  out.append({'partial':h,'inferred_midi':float(69+12*np.log2(hz/440)),'peak_db_unscaled':db(spec[k])})
 return out
ann={'created_at_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'created_before_any_detector_capture':True,'source_manifest_sha256':hashlib.sha256((root/'source-manifest.json').read_bytes()).hexdigest(),'split_sha256':hashlib.sha256((root/'split-before-any-detector.json').read_bytes()).hexdigest(),'method':'All annotations derive directly from unmodified authored 24bit WAV + authorSFZ. No detector imports, candidate outputs or note-driven threshold fitting. Activity bands are acoustic authored-sample cues, NOT physical pick/contact latency truth. Whole-file EOF is NOT a physical damping reference.','notes':[]}
for rec in manifest['files']:
 x,r=decode(root/'original-source'/rec['path']);peak=float(max(abs(x)));hop=int(.005*r)
 rms=np.array([np.sqrt(np.mean(x[k:k+hop]**2)) for k in range(0,len(x),hop)])
 # Fixed activity cue spans a low/full amplitude boundary, rather than pretending either is pick contact.
 pre=float(np.sqrt(np.mean(x[:round(.015*r)]**2)))
 low=max(pre*6,peak*.02);high=max(pre*12,peak*.12)
 li=np.flatnonzero(abs(x)>=low);hi=np.flatnonzero(abs(x)>=high)
 lo_t=float(li[0]/r) if len(li) else None;hi_t=float(hi[0]/r) if len(hi) else None
 is_stac='/stac/' in rec['path']
 core=(.08,.14) if is_stac else (.12,1.2)
 note={'path':rec['path'],'source_sha256':rec['sha256'],'midi_nominal_from_author_sfz':rec['midi_nominal'],'split':'development' if rec['path'] in dev else 'reserved_holdout','source_duration_seconds':len(x)/r,'whole_file_peak_dbfs':db(peak),'first_15ms_rms_dbfs':db(pre),'activity_cue_seconds':{'low_boundary':lo_t,'high_boundary':hi_t,'threshold_low_dbfs':db(low),'threshold_high_dbfs':db(high),'physical_contact_verified':False},'conservative_positive_core_seconds':list(core),'core_scope':'Nominal note should be represented continuously in this independently chosen early tonal body; no physical offset asserted. Pitch audit is descriptive and retains invalid/edge partials.','physical_damping_reference':None,'offset_uncertainty':'Author SFZ names staccato/ordinary, not per-recording damping times. Edited/faded file end and level-threshold crossing are not physical NoteOff truth. Do not score lateOff from EOF.','wave_rms_5ms':{'hop_samples':hop,'dbfs':[round(db(v),4) for v in rms]},'descriptive_partial_pitch_audit':[{'center_seconds':t,'window_seconds':.16,'partials':estimates(x,r,rec['midi_nominal'],t)} for t in ((.11,) if is_stac else (.20,.50,1.0))]}
 ann['notes'].append(note)
(root/'independent-acoustic-annotations.json').write_text(json.dumps(ann,indent=2)+'\n')
# Development summary only; reserved descriptors stay in sealed annotation file until candidate freeze.
for n in ann['notes']:
 if n['split']=='development':
  vals=[p['inferred_midi'] for q in n['descriptive_partial_pitch_audit'] for p in q['partials']]
  print(Path(n['path']).name,'cue',n['activity_cue_seconds']['low_boundary'],n['activity_cue_seconds']['high_boundary'],'core',n['conservative_positive_core_seconds'],'partial_median',round(float(np.median(vals)),3) if vals else None,'target',n['midi_nominal_from_author_sfz'])
seal={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'source-manifest.json',root/'selection-before-capture.json',root/'split-before-any-detector.json',root/'independent-acoustic-annotations.json',root/'annotate_independent.py']}
(root/'PRE_CAPTURE_SEAL.json').write_text(json.dumps(seal,indent=2)+'\n')
