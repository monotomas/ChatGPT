from pathlib import Path
import csv, hashlib, json, subprocess
import build_and_measure as measure

root = Path(__file__).resolve().parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
results = {}
for label in ('baseline','fast'):
    name = 'payload-cold330-' + label
    results[name] = measure.run(name, root / (label + '-Source'), root / 'payload-cold.cpp')
    results[name]['run_command'] = [str(root / name / 'capture')]
    (root / name / 'run.json').write_text(json.dumps(results[name], indent=2) + '\n')
    print(name + ' complete', flush=True)
old = root / 'payload-cold330-baseline/events.csv'
new = root / 'payload-cold330-fast/events.csv'
assert old.read_bytes() == new.read_bytes(), 'FULL PAYLOAD PARITY FAILED; STOP'
print('full payload330 parity PASS', flush=True)
for label in ('baseline','fast'):
    name = 'profile-thread-open30-' + label
    results[name] = measure.run(name, root / (label + '-timed-Source'), root / 'profile-thread-probe.cpp', args=('10',))
    results[name]['run_command'] = [str(root / name / 'capture'), '10']
    (root / name / 'run.json').write_text(json.dumps(results[name], indent=2) + '\n')
    print(name + ' complete', flush=True)
assert (root / 'profile-thread-open30-baseline/events.csv').read_bytes() == (root / 'profile-thread-open30-fast/events.csv').read_bytes()
payload = list(csv.DictReader(old.open()))
summary = {'private_status':'PRIVATE_RESEARCH_NOT_ADOPTED','full_payload_parity':{
    'byte_identical':True,'events_sha256':sha(old),'rows':len(payload),'columns':list(payload[0]),
    'configurations':len({tuple(r[k] for k in ('case','cents','timbre','dynamics')) for r in payload}),
    'types':{k:sum(r['type']==k for r in payload) for k in ('N','F','P')},
}, 'timing':{}}
for label in ('baseline','fast'):
    rows = list(csv.DictReader((root / ('profile-thread-open30-' + label) / 'metrics.csv').open()))
    summary['timing'][label] = {
      'configurations':len(rows),'callbacks':len(rows)*750,
      'totals':{k:sum(float(r[k]) for r in rows) for k in (
        'wall_deadlines','thread_deadlines','process_thread_us','analyse_calls',
        'analyse_thread_us','profile_candidates','profile_thread_us')},
      'maxima':{k:max(float(r[k]) for r in rows) for k in (
        'wall_p99_us','wall_max_us','thread_p99_us','thread_max_us')},
    }
old,new = [summary['timing'][label]['totals'] for label in ('baseline','fast')]
summary['timing']['ratios'] = {k:old[k]/new[k] for k in ('process_thread_us','analyse_thread_us','profile_thread_us')}
summary['timing_note_only_payload_parity'] = True
(root / 'factor-runs.json').write_text(json.dumps(results, indent=2) + '\n')
(root / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2), flush=True)
