#!/usr/bin/env python3
"""Measure all three audio corpora with the same freshly built Windows evaluator.
SPDX-License-Identifier: AGPL-3.0-or-later
Completion proves execution/provenance, not musical acceptance or live latency.
"""
from __future__ import annotations
import argparse,collections,csv,datetime,hashlib,io,json,math,os,platform,subprocess,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SPECS={'public':('Tests/AudioFixtures',81,'development',()),
       'holdout':('Tests/AudioFixtures/holdout',69,'seen-regression',('--holdout',)),
       'confirmation':('Tests/AudioFixtures/confirmation',46,'seen-regression',('--confirmation',))}
RATES=(44100,48000,96000);BLOCKS=(32,64,128)
MANIFEST='diagnostics-manifest.json'
FILES=('audio-cases.csv','audio-events.csv','audio-summary.txt','measurement-manifest.json','runner-output.txt')
INTS=('expected','matched','missed','false_note_ons','octave_errors','wrong_before_correct','early_offs','hung','overflow','deadline_misses')
LATENCY=('latency_median_ms','latency_p95_ms','latency_p99_ms','latency_max_ms')
RELEASE=('release_median_ms','release_p95_ms')
CPU=('cpu_median_us','cpu_p95_us','cpu_p99_us','cpu_max_us')

def need(condition,message):
    if not condition:raise ValueError(message)
def sha(data):return hashlib.sha256(data).hexdigest()
def safe(name):
    need(isinstance(name,str) and name and not name.startswith('/') and not any(c in name for c in '\\\r\n\0:') and all(p not in ('','.','..') for p in name.split('/')),'Unsafe diagnostic path')
    return name
def canonical(values):
    need(isinstance(values,dict),'Missing source provenance')
    result={};seen=set()
    for name,digest in values.items():
        name=safe(name.replace('\\','/'))
        need(name.casefold() not in seen,'Duplicate provenance path');seen.add(name.casefold())
        need(isinstance(digest,str) and len(digest)==64 and all(c in '0123456789abcdef' for c in digest),'Invalid provenance digest')
        result[name]=digest
    return result
def inventory(base):
    need(base.is_dir() and not base.is_symlink(),'Diagnostic directory missing or linked')
    result={}
    for path in sorted(base.rglob('*')):
        need(not path.is_symlink(),'Linked diagnostic entry')
        if path.is_file():result[safe(path.relative_to(base).as_posix())]=path.read_bytes()
    return result
def orchestration_sources(root):
    return {name:sha((root/name).read_bytes()) for name in ('Tools/windows_audio_diagnostics.py','Tools/audio_validation_run.py','Tools/audio_validation_verify.py','Tools/audio_validation_holdout.py','Tools/audio_validation_confirmation.py')}

def verify_rows(metrics,events,reference):
    reader=csv.DictReader(io.StringIO(metrics.decode('utf-8'),newline=''))
    fields={'case','family','mode','rate','block',*INTS,*LATENCY,*RELEASE,*CPU}
    need(reader.fieldnames is not None and len(reader.fieldnames)==len(fields) and set(reader.fieldnames)==fields,'Diagnostic CSV schema')
    expected={(name,rate,block) for name in reference for rate in RATES for block in BLOCKS};measured={}
    for row in reader:
        need(set(row)==fields and all(isinstance(v,str) for v in row.values()),'Truncated diagnostic CSV')
        name=row['case'];rate=int(row['rate']);block=int(row['block']);key=(name,rate,block)
        need(key in expected and key not in measured,'Unknown/duplicate diagnostic case configuration')
        ref=reference[name];counts={field:int(row[field]) for field in INTS}
        need(all(n>=0 for n in counts.values()),'Negative diagnostic count')
        need(row['family']==ref['family'] and row['mode']==ref['mode'] and counts['expected']==len(ref['truth']),'Diagnostic annotation mismatch')
        need(counts['matched']+counts['missed']==counts['expected'] and counts['early_offs']<=counts['matched'] and counts['wrong_before_correct']<=counts['expected'] and counts['octave_errors']<=counts['false_note_ons'],'Inconsistent diagnostic counts')
        for group,required,minimum in ((LATENCY,counts['matched']>0,-2000/rate-1e-6),(RELEASE,counts['matched']>counts['early_offs'],-10.000001),(CPU,True,0)):
            need(all(bool(row[field])==required for field in group),'Missing/inconsistent diagnostic numbers')
            if required:
                values=[float(row[field]) for field in group]
                need(all(math.isfinite(n) and n>=minimum for n in values) and all(a<=b for a,b in zip(values,values[1:])),'Invalid diagnostic numbers/percentiles')
        measured[key]=counts
    need(set(measured)==expected,'Incomplete full audio matrix')
    reader=csv.DictReader(io.StringIO(events.decode('utf-8'),newline=''));event_fields={'case','rate','block','on','note','velocity','sample','time_ms'}
    need(reader.fieldnames is not None and len(reader.fieldnames)==len(event_fields) and set(reader.fieldnames)==event_fields,'Diagnostic event schema')
    ons=collections.Counter()
    for event in reader:
        need(set(event)==event_fields and all(isinstance(v,str) for v in event.values()),'Truncated diagnostic event')
        key=(event['case'],int(event['rate']),int(event['block']));on=int(event['on']);sample=int(event['sample']);time=float(event['time_ms'])
        need(key in expected and on in (0,1) and 0<=int(event['note'])<=127 and 0<=int(event['velocity'])<=127 and sample>=0 and math.isfinite(time) and abs(time-1000*sample/key[1])<.000002,'Invalid diagnostic event')
        if on:ons[key]+=1
    need(all(ons[key]==row['matched']+row['false_note_ons'] for key,row in measured.items()),'Diagnostic events/counts disagree')
    return len(measured)

def verify_saved(root,dist,build=None):
    """Portable data/provenance check used by packaging, without executing audio."""
    build=build or json.loads((dist/'build-manifest.json').read_text(encoding='utf-8'))
    basic_path=dist/'audio-validation/report-manifest.json';basic=json.loads(basic_path.read_text(encoding='utf-8'))
    source=canonical(basic['source_sha256']);all_files=inventory(dist/'windows-audio-diagnostics')
    manifest=json.loads(all_files[MANIFEST])
    need(manifest.get('schema')==1 and manifest.get('kind')=='WindowsAudioDiagnostics' and manifest.get('status')=='completed' and manifest.get('measurement_only') is True,'Windows diagnostic completion proof')
    evaluator_digest=manifest.get('evaluator_sha256')
    need(isinstance(evaluator_digest,str) and len(evaluator_digest)==64 and all(c in '0123456789abcdef' for c in evaluator_digest),'Missing native evaluator SHA256')
    need(basic.get('status')=='passed' and basic.get('evaluator_exit_code')==0 and build.get('tests')=='passed' and build.get('windows_verified') is True,'Diagnostics require the passed native build/basic evaluator')
    need(manifest.get('execution_platform')=='Windows' and manifest.get('source_commit')==build.get('source_commit') and build.get('platform')=='Windows-x64','Windows diagnostic source/platform mismatch')
    need(manifest.get('evaluator_sha256')==basic.get('evaluator_sha256') and manifest.get('basic_report_sha256')==sha(basic_path.read_bytes()),'Windows diagnostic evaluator/basic proof mismatch')
    need(canonical(manifest.get('source_sha256'))==source,'Windows diagnostic source mismatch')
    for name,digest in source.items():need(sha((root/name).read_bytes())==digest,'Stale diagnostic source: '+name)
    need(manifest.get('orchestration_source_sha256')==orchestration_sources(root),'Stale diagnostic orchestration')
    expected_files={f'{name}/{filename}' for name in SPECS for filename in FILES}
    need(set(all_files)-{MANIFEST}==set(manifest.get('files_sha256',{}))==expected_files,'Diagnostic inventory differs')
    for name,digest in manifest['files_sha256'].items():need(sha(all_files[name])==digest,'Diagnostic checksum: '+name)
    runner_sources={name:digest for name,digest in source.items() if name=='Tools/audio_validation.cpp' or name.startswith('Source/Pitch/')}
    need(set(manifest.get('datasets',{}))==set(SPECS),'Missing diagnostic corpus')
    for name,(fixture_dir,count,role,_) in SPECS.items():
        fixtures=root/fixture_dir;ref_json=json.loads((fixtures/'fixtures.json').read_text(encoding='utf-8'));reference={c['name']:c for c in ref_json['cases']}
        need(len(reference)==count==len(ref_json['cases']),'Diagnostic corpus inventory changed')
        m=json.loads(all_files[f'{name}/measurement-manifest.json'])
        need(m.get('runner_exit_code')==0 and m.get('source_changed_during_measurement') is False and m.get('measurement_not_acceptance') is True,'Diagnostic runner failed or source changed')
        need(m.get('binary_sha256')==manifest['evaluator_sha256'] and canonical(m.get('source_sha256'))==runner_sources,'Diagnostic runner binary/source mismatch')
        need(m.get('evaluation_role')==role and m.get('fixtures_sha256')==sha((fixtures/'fixtures.json').read_bytes()) and m.get('configuration')=='full:44100/48000/96000Hz x32/64/128samples','Diagnostic role/fixture/matrix mismatch')
        need(m.get('independent_holdout') is (name!='public') and m.get('second_confirmation') is (name=='confirmation'),'Diagnostic corpus identity mismatch')
        runs=verify_rows(all_files[f'{name}/audio-cases.csv'],all_files[f'{name}/audio-events.csv'],reference)
        need(manifest['datasets'][name]=={'case_configurations':runs,'reference_cases':count,'evaluation_role':role,'fixture_manifest_sha256':m['fixtures_sha256'],'runner_exit_code':0},'Diagnostic dataset completion mismatch')
    return all_files

def run(root,dist,source_commit):
    need(os.name=='nt' and sys.maxsize>2**32,'Native64-bit Windows is required for diagnostic execution')
    from package_windows import verified_payload,verified_audio_evidence,verify_pe
    _,build=verified_payload(dist,json.loads((root/'dependencies.lock.json').read_text(encoding='utf-8'))['project_version'])
    verified_audio_evidence(root,dist)
    need(build.get('source_commit')==source_commit and len(source_commit)==40,'Diagnostic CI source differs')
    candidates=list((root/'build').glob('w-*/FlysnapperAudioValidation.exe'));need(len(candidates)==1,'Expected exactly one current native audio evaluator')
    evaluator=candidates[0].resolve(strict=True);verify_pe(evaluator.read_bytes())
    basic_path=dist/'audio-validation/report-manifest.json';basic=json.loads(basic_path.read_text(encoding='utf-8'));source=canonical(basic['source_sha256']);evaluator_sha=sha(evaluator.read_bytes())
    need(evaluator_sha==basic.get('evaluator_sha256'),'Evaluator differs from passed basic CTest')
    before=orchestration_sources(root);output=dist/'windows-audio-diagnostics';need(not output.exists(),'Refusing stale Windows diagnostics');output.mkdir(parents=True)
    manifest={'schema':1,'kind':'WindowsAudioDiagnostics','status':'running','measurement_only':True,'execution_platform':platform.system(),'source_commit':source_commit,'evaluator_sha256':evaluator_sha,'basic_report_sha256':sha(basic_path.read_bytes()),'source_sha256':source,'orchestration_source_sha256':before,'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'datasets':{}}
    try:
        env=os.environ.copy();env.update(PYTHONUTF8='1',PYTHONIOENCODING='utf-8')
        for name,(fixture_dir,count,role,flags) in SPECS.items():
            destination=output/name;destination.mkdir()
            command=[sys.executable,str(root/'Tools/audio_validation_run.py'),'--executable',str(evaluator),'--full','--evaluation-role',role,'--output',str(destination),*flags]
            with (destination/'runner-output.txt').open('wb') as log:result=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=600)
            need(result.returncode==0,'Windows diagnostic execution failed: '+name)
            manifest['datasets'][name]={'case_configurations':count*9,'reference_cases':count,'evaluation_role':role,'fixture_manifest_sha256':sha((root/fixture_dir/'fixtures.json').read_bytes()),'runner_exit_code':0}
        need(sha(evaluator.read_bytes())==evaluator_sha and orchestration_sources(root)==before,'Evaluator/orchestration changed during diagnostics')
        manifest['files_sha256']={name:sha(data) for name,data in inventory(output).items()}
        manifest['status']='completed';(output/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        verify_saved(root,dist,build)
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        manifest['status']='failed';manifest['error']=str(error);(output/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');raise
    print('Windows audio diagnostic measurements completed:729+621+414 configurations. Musical errors remain reported; no live latency certification.')
    return manifest

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-root',type=Path,default=ROOT);p.add_argument('--dist',type=Path);p.add_argument('--source-commit',required=True);a=p.parse_args(argv)
    try:run(a.source_root.resolve(),a.dist or a.source_root/'dist',a.source_commit)
    except (ValueError,OSError,subprocess.SubprocessError,KeyError,TypeError) as error:p.exit(1,'Windows audio diagnostics failed: '+str(error)+'\n')
    return 0
if __name__=='__main__':raise SystemExit(main())
