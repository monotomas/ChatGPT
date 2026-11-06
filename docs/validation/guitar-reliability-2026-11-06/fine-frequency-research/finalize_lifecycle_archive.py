from pathlib import Path
import json,hashlib,shutil,datetime
D=Path(__file__).resolve().parent;H=Path('/tmp/flysnapper-v4-poly-f0-lifecycle')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert json.loads((H/'parity.json').read_text())=={'cold_warm330_events_equal':True,'cold_warm132_events_equal':True}
out=D/'lifecycle';out.mkdir(exist_ok=True);entries=[]
for p in sorted(H.rglob('*')):
 if not p.is_file():continue
 rel=p.relative_to(H);data=p.read_bytes();binary=data.startswith(b'\x7fELF') or p.suffix in ('.o','.wav','.f32','.pcm','.bin')
 item={'original_path':str(p),'relative_path':str(rel),'bytes':len(data),'sha256':sha(p),'included':not binary}
 if binary:item['exclusion']='Executable/object excluded; exact compiled binary hash retained.'
 else:
  q=out/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q);assert sha(p)==sha(q)
 entries.append(item)
shutil.copyfile('/tmp/implement_f0_lifecycle.py',out/'implement_lifecycle.py')
(out/'preservation-inventory.json').write_text(json.dumps({'source_origin':str(H),'all_original_files':entries,'generation_script':'implement_lifecycle.py','original_uncertainty_source':'../preserved/uncertainty-f0/Source','no_binary_or_audio_payloads':True},indent=2)+'\n')
summary=json.loads((D/'results-summary.json').read_text())
for suite in [330,132]:
 for cold in [True,False]:
  label=('cold' if cold else 'warm')+str(suite);score=json.loads((out/(label+'-scores.json')).read_text());summary['counts']['lifecycle-'+('COLD' if cold else 'WARM')+'-'+str(suite)]={'raw_events':'lifecycle/'+label+'/events.csv','raw_events_sha256':sha(out/label/'events.csv'),'totals':score['totals']}
(D/'results-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
p=D/'reproduce.py';s=p.read_text().replace("choices=['base','observed-f0','uncertainty-f0']","choices=['base','observed-f0','uncertainty-f0','lifecycle']").replace("snap=D/'preserved'/a.variant;shutil.copytree", "snap=(D/'lifecycle' if a.variant=='lifecycle' else D/'preserved'/a.variant);shutil.copytree");p.write_text(s)
p=D/'README.md';s=p.read_text();s+='''
## Separate rejected cache-lifecycle ablation

`lifecycle/` preserves one further private semantic experiment and all four
330/132 cold/warm captures. It is **rejected**, not the archived uncertainty
implementation intended for the independent exact-kernel combination.

Immutable nominal model/Gram backups are prepared outside realtime. Separate
fine-to-nominal and fine/fine cross terms resolve the actually selected model
frequencies. Per-frame use flags clear on every observation; full detector reset
clears both small use/cache-valid flags plus prediction, leaving MB tables
untouched. This experiment also explicitly removes previous-F0 hysteresis and
held smoothing, requests exact current-frame partial-median F0, uses nominal
frequencies for shared-owner masking, and falls back nominal for any currently
unselected note. These are semantic changes, not a silent optimization. The
simple correctness implementation adds large nominal/mixed Gram backups and is
not a compact-memory proposal.

The matrix audit passes2,182,464 scalar checks across96 nominal/fine masks:
cross3.65767e-6, inverse9.99201e-16, basis5.96012e-8, exact symmetry. Reset leaves
all large tables byte-identical. Null/empty evidence falls back nominal. Two
histories containing25 different prior fine caches produce identical current
strengths, confidence, model F0 and prediction without an intervening reset.
All330 and132 **complete warm event streams equal their cold streams byte for
byte** after the lifecycle repair.

| Count | Cold330/810 references | Cold132/600 references |
| --- | ---: | ---: |
| Nominal / useful misses |144 /146 |174 /178 |
| Extra Ons |68 |73 |
| Repeated wanted Ons |12 |14 |
| False lower-octave Ons |40 |53 |
| Early / late first Offs |28 /250 |38 /168 |

Compared with archived uncertainty cold330, nominal misses rise138→144,
repeats4→12, early Offs16→28; fewer extras/late releases do not repair those
regressions. The nominal central default open-string case finds all six wanted
notes but repeats once; first E2 is90.646ms late and other first Ons are
63.646–84.646ms late. Every capture has balanced events/no active EOF voices;
that basic lifecycle condition does not make the output musically usable.
Shared-cloud wall timings are retained; no new thread-CPU profile or physical
interface latency measurement is claimed. Source/test/scorer/fixture production
files were unchanged, and measured-root guard changes were not compounded here.

`source.diff` lists exactly three private changed files, and context/source
manifests bind their actual compiled bytes. The independent read-only review
and algebra/history audits are retained. `reproduce.py --variant lifecycle`
restores this exact private variant; its full original warm/cold run is in
`lifecycle/run.py`. Only the original330/132 synthetic drivers were used for
musical scores. No further algorithm grid was run.
''';p.write_text(s)
p=D/'verify.py';s=p.read_text();s+='''
L=D/'lifecycle'
if L.exists():
 m=json.loads((L/'preservation-inventory.json').read_text())
 for item in m['all_original_files']:
  if item['included']:
   p=L/item['relative_path'];assert p.stat().st_size==item['bytes'] and sha(p)==item['sha256'],p
 actual={p.relative_to(L).as_posix():sha(p) for p in (L/'Source').rglob('*') if p.is_file()}
 assert actual==json.loads((L/'source-inventory.json').read_text())['files_sha256']
 for suite in [330,132]:assert (L/('cold'+str(suite))/'events.csv').read_bytes()==(L/('warm'+str(suite))/'events.csv').read_bytes()
 print('PASS: lifecycle source/raw bindings; complete330/132 warm/cold event parity')
seal=D/'archive-seal.json'
if seal.exists():
 m=json.loads(seal.read_text());actual={p.relative_to(D).as_posix():sha(p) for p in D.rglob('*') if p.is_file() and p!=seal}
 assert actual==m['all_archived_files_sha256'],'archive seal mismatch'
 print('PASS: complete archive seal '+str(len(actual))+' files')
''';p.write_text(s)
for p in D.rglob('*.py'):compile(p.read_text(),str(p),'exec')
seal={'sealed_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'all_archived_files_sha256':{p.relative_to(D).as_posix():sha(p) for p in sorted(D.rglob('*')) if p.is_file() and p.name!='archive-seal.json'},'binary_payloads_excluded':True,'complete_original_and_lifecycle_source_snapshots':10,'source_is_private_not_current_production_HEAD':True}
(D/'archive-seal.json').write_text(json.dumps(seal,indent=2)+'\n')
print(json.dumps({'original_retained':429,'lifecycle_retained':sum(e['included'] for e in entries),'lifecycle_binary_exclusions':sum(not e['included'] for e in entries),'sealed_files':len(seal['all_archived_files_sha256']),'seal_sha256':sha(D/'archive-seal.json')},indent=2))
