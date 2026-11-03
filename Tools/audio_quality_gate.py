#!/usr/bin/env python3
"""Independent frozen basic audio quality gate, for CTest and release packaging.

SPDX-License-Identifier: AGPL-3.0-or-later
This gate cannot establish physical or detector 5/10ms live-latency compliance.
A successful run certifies only the explicit basic accuracy criteria in
Tests/AudioFixtures/quality_gate.json. Failures must block automatic packaging.
"""
from pathlib import Path
import argparse,csv,datetime,hashlib,json,math,shutil,subprocess,sys,tempfile

ROOT=Path(__file__).resolve().parents[1]
EXPECTED_CASES={*(f'{n}_{mode}' for n in ['digital_silence','unpitched_click','noise_floor','broadband_noise','colored_noise'] for mode in ['mono','poly']),
    *(f'{n}_mono' for n in ['modal_40','modal_52','modal_64','modal_84','karplus_40','missing_fundamental_52','vibrato_64','repeated_52','continuous_repick_52']),
    *(f'recorded_{n}_mono' for n in ['E2','A2','D3','G3','B3','E3','E4','Fs3']),
    *(f'{n}_poly' for n in ['major_triad','open_e_minor','strummed_a_minor'])}

EXPECTED_POLICY={}
for base in ['digital_silence','unpitched_click','noise_floor','broadband_noise','colored_noise']:
    for mode in ['mono','poly']:EXPECTED_POLICY[base+'_'+mode]=('negative',None)
for base in ['modal_40','modal_52','modal_64','modal_84','karplus_40','missing_fundamental_52','vibrato_64','repeated_52','continuous_repick_52']:
    EXPECTED_POLICY[base+'_mono']=('synthetic-mono',60)
for base in ['E2','A2','D3','G3','B3','E3','E4','Fs3']:EXPECTED_POLICY['recorded_'+base+'_mono']=('recorded-mono',100)
for base in ['major_triad','open_e_minor','strummed_a_minor']:EXPECTED_POLICY[base+'_poly']=('polyphonic-chord',120)
INTEGER_FIELDS=['rate','block','expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs','hung','overflow','deadline_misses']
LATENCY_FIELDS=['latency_median_ms','latency_p95_ms','latency_p99_ms','latency_max_ms']
RELEASE_FIELDS=['release_median_ms','release_p95_ms']
CPU_FIELDS=['cpu_median_us','cpu_p95_us','cpu_p99_us','cpu_max_us']
CSV_FIELDS={'case','family','mode',*INTEGER_FIELDS,*LATENCY_FIELDS,*RELEASE_FIELDS,*CPU_FIELDS}

def validate_row(row,reference):
    name=row.get('case','<missing>')
    require(set(row)==CSV_FIELDS,f'{name}: invalid CSV schema')
    require(all(isinstance(value,str) for value in row.values()),f'{name}: truncated or invalid CSV row')
    require(name in reference,f'{name}: unknown reference case')
    ref=reference[name]
    values={key:int(row[key]) for key in INTEGER_FIELDS}
    for key,value in values.items():require(value>=0,f'{name}: negative {key}')
    require(values['rate']==48000 and values['block']==64,f'{name}: wrong sample rate/block size')
    require(values['expected']==len(ref['truth']) and row['mode']==ref['mode'] and row['family']==ref['family'],f'{name}: annotation mismatch')
    require(values['matched']+values['missed']==values['expected'],f'{name}: inconsistent matched/missed counts')
    require(values['early_offs']<=values['matched'] and values['wrong_before_correct']<=values['expected'] and values['octave_errors']<=values['false_note_ons'],f'{name}: inconsistent error counts')
    for group,mandatory in [(LATENCY_FIELDS,values['matched']>0),(RELEASE_FIELDS,values['matched']>values['early_offs']),(CPU_FIELDS,True)]:
        require(all(bool(row[key])==mandatory for key in group),f'{name}: missing or inconsistent measurement fields')
        if not mandatory:continue
        nums=[float(row[key]) for key in group]
        require(all(math.isfinite(n) for n in nums),f'{name}: non-finite measurement')
        require(all(a<=b for a,b in zip(nums,nums[1:])),f'{name}: invalid percentile ordering')
        if group==CPU_FIELDS:require(all(n>=0 for n in nums),f'{name}: negative callback time')
        if group==LATENCY_FIELDS:require(all(n>=-2000/48000-1e-6 for n in nums),f'{name}: latency contradicts matching tolerance')
        if group==RELEASE_FIELDS:require(all(n>=-10.000001 for n in nums),f'{name}: release contradicts early-off classification')

def require(condition,message='Validation failed'):
    if not condition:raise ValueError(message)

def sha(path: Path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evaluator',type=Path,required=True)
    parser.add_argument('--fixtures',type=Path,default=ROOT/'Tests/AudioFixtures')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();fixtures=args.fixtures.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    for name in ['summary.json','report-manifest.json','metrics.csv','events.csv','measurements.txt']:
        (output/name).unlink(missing_ok=True)
    subprocess.run([sys.executable,str(ROOT/'Tools/audio_validation_verify.py'),str(fixtures)],check=True)
    policy_path=fixtures/'quality_gate.json';policy=json.loads(policy_path.read_text())
    require(len(policy['cases']) == 30 and {c['name'] for c in policy['cases']} == EXPECTED_CASES, 'Fixed30case inventory changed')
    require(policy['version'] == 1 and policy['sample_rate'] == 48000 and (policy['block_size'] == 64), "Validation failed: policy['version'] == 1 and policy['sample_rate'] == 48000 and (policy['block_size'] == 64)")
    require(policy['required_zero'] == ['missed', 'false_note_ons', 'octave_errors', 'wrong_before_correct', 'early_offs', 'hung', 'overflow'], "Validation failed: policy['required_zero'] == ['missed', 'false_note_ons', 'octave_errors', 'wrong_before_correct', 'early_offs', 'hung', 'overflow']")
    require(policy['release_p95_max_ms'] == 100, "Validation failed: policy['release_p95_max_ms'] == 100")
    require({case['name']: (case['group'], case['maximum_detector_response_ms']) for case in policy['cases']} == EXPECTED_POLICY, 'Fixed per-case policy changed')
    for case in policy['cases']:
        require(case['maximum_detector_response_ms'] == {'negative': None, 'synthetic-mono': 60, 'recorded-mono': 100, 'polyphonic-chord': 120}[case['group']], "Validation failed: case['maximum_detector_response_ms'] == {'negative': None, 'synthetic-mono': 60, 'recorded-mono': 100, 'polyphonic-chord': 120}[case['group']]")
    reference={c['name']:c for c in json.loads((fixtures/'fixtures.json').read_text())['cases']}
    source_paths=[ROOT/'Tools/audio_validation.cpp',ROOT/'Tools/audio_quality_gate.py',*sorted((ROOT/'Source/Pitch').glob('*.h')),*sorted((ROOT/'Source/Pitch').glob('*.cpp'))]
    manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'evaluator_sha256':sha(args.evaluator),'fixture_manifest_sha256':sha(fixtures/'fixtures.json'),'fixture_inventory_sha256':sha(fixtures/'fixtures.tsv'),'policy_sha256':sha(policy_path),'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in source_paths},'scope':policy['scope'],'sample_rate':48000,'block_size':64}
    # Evaluator output is always fresh. A previous successful CSV cannot mask a
    # current nonzero exit or a run which failed before writing its result files.
    with tempfile.TemporaryDirectory(prefix='audio-quality-',dir=output) as temporary:
        current=Path(temporary)
        result=subprocess.run([str(args.evaluator.resolve()),str(fixtures),str(current)])
        manifest['evaluator_exit_code']=result.returncode
        if result.returncode:
            (output/'report-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            raise SystemExit(result.returncode)
        csv_path=current/'audio-cases.csv'
        with csv_path.open(newline='') as f:
            reader=csv.DictReader(f)
            require(reader.fieldnames is not None and len(reader.fieldnames)==len(CSV_FIELDS) and set(reader.fieldnames)==CSV_FIELDS,'Incomplete or duplicate CSV header')
            rows=list(reader)
        require(len(rows) == 81 and len({r['case'] for r in rows}) == 81, 'Expected81fresh independentcase measurements')
        measured={r['case']:r for r in rows}
        require(set(measured) == set(reference), 'Measurement/reference case inventory differs')
        for row in rows:validate_row(row,reference)
        failures=[];checks=[]
        for case in policy['cases']:
            row=measured[case['name']];issues=[]
            require(int(row['rate']) == 48000 and int(row['block']) == 64, "Validation failed: int(row['rate']) == 48000 and int(row['block']) == 64")
            if int(row['matched'])!=int(row['expected']):issues.append(f"matched {row['matched']} of {row['expected']} reference notes")
            for column in policy['required_zero']:
                if int(row[column])!=0:issues.append(f'{column}={row[column]}')
            maximum=case['maximum_detector_response_ms']
            if maximum is not None:
                if not row['latency_max_ms']:issues.append('No usable correct detector response')
                elif not math.isfinite(float(row['latency_max_ms'])):issues.append('Non-finite detector response')
                elif float(row['latency_max_ms'])>maximum:issues.append(f"detector response {row['latency_max_ms']}ms exceeds basic {maximum}ms ceiling")
                if not row['release_p95_ms']:issues.append('No valid release measurement')
                elif not math.isfinite(float(row['release_p95_ms'])):issues.append('Non-finite release measurement')
                elif float(row['release_p95_ms'])>policy['release_p95_max_ms']:issues.append(f"release P95 {row['release_p95_ms']}ms exceeds {policy['release_p95_max_ms']}ms")
            if case['group']=='negative' and (int(row['expected']) or int(row['false_note_ons'])):issues.append('Negative control produced pitched Note On')
            check={'case':case['name'],'group':case['group'],'passed':not issues,'issues':issues,'observed':row};checks.append(check)
            if issues:failures.append(check)
        shutil.copy2(csv_path,output/'metrics.csv')
        shutil.copy2(current/'audio-events.csv',output/'events.csv')
        shutil.copy2(current/'audio-summary.txt',output/'measurements.txt')
    summary={'gate':'FlysnapperAudioQuality','policy_version':1,'status':'passed' if not failures else 'failed','cases_checked':30,'cases_passed':30-len(failures),'cases_failed':len(failures),'physical_live_latency_verified':False,'user_5_10ms_goals_verified':False,'checks':checks}
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    manifest['status']=summary['status'];manifest['summary_sha256']=sha(output/'summary.json');manifest['metrics_sha256']=sha(output/'metrics.csv')
    (output/'report-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for check in failures:print('FAIL '+check['case']+': '+'; '.join(check['issues']))
    print(f"FlysnapperAudioQuality: {summary['cases_passed']}/30 basic accuracy cases passed; live5/10ms compliance is not certified")
    raise SystemExit(1 if failures else 0)

if __name__=='__main__':
    try:main()
    except (AssertionError,ValueError,KeyError,OSError,subprocess.SubprocessError) as error:
        print('FlysnapperAudioQuality validation error: '+str(error),file=sys.stderr)
        raise SystemExit(1)
