from pathlib import Path
import shutil,hashlib,json,subprocess,time,os
b=Path(__file__).parent;v=b/'time-atoms-phase-null-trusted-bridge';v.mkdir(exist_ok=True)
if (v/'Source').exists():raise RuntimeError('Refuse to mutate an existing frozen bridge Source')
shutil.copytree(b/'time-atoms-phase-null-guard/Source',v/'Source')
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
old='std::array<bool,8> valid{};int warmup=0;int64_t sample=0,lastEvent=-1000000;'
new='std::array<bool,8> valid{};std::array<int64_t,8> trustedSample{};int warmup=0;int64_t sample=0,lastEvent=-1000000;'
assert old in s;s=s.replace(old,new)
s=s.replace('        const double dt=state.sample>0?double(clock-state.sample):0;\n','')
old='            if(reliable&&state.valid[size_t(h)]&&dt>0&&dt<=predictionSamples*4){'
new='            // Keep only trusted per-partial observations. A short confidence\n            // gap in an unexplained waveform must not erase the PRE-event\n            // phase or replace it with an unreliable cancellation angle.\n            // The existing observation-gap bound is unchanged.\n            const double dt=state.trustedSample[size_t(h)]>0?double(clock-state.trustedSample[size_t(h)]):0;\n            if(reliable&&state.valid[size_t(h)]&&dt>0&&dt<=predictionSamples*4){'
assert old in s;s=s.replace(old,new)
old='            }else{state.trend[size_t(h)]=0;state.departure[size_t(h)]=0;}\n            state.phase[size_t(h)]=angle[size_t(h)];state.omega[size_t(h)]=model.phase[size_t(h)];state.power[size_t(h)]=power[size_t(h)];state.valid[size_t(h)]=reliable;'
new='            }else if(reliable||nullExplained||!state.valid[size_t(h)]||dt>predictionSamples*4){\n                state.trend[size_t(h)]=0;state.departure[size_t(h)]=0;\n            }\n            if(reliable){\n                state.phase[size_t(h)]=angle[size_t(h)];state.omega[size_t(h)]=model.phase[size_t(h)];state.power[size_t(h)]=power[size_t(h)];\n                state.valid[size_t(h)]=true;state.trustedSample[size_t(h)]=clock;\n            }else if(nullExplained||dt>predictionSamples*4){\n                state.valid[size_t(h)]=false;state.trustedSample[size_t(h)]=0;\n            }'
assert old in s;s=s.replace(old,new);p.write_text(s)
patch=subprocess.run(['diff','-u',str(b/'time-atoms-phase-null-guard/Source/Pitch/TimeDomainPursuit.cpp'),str(p)],stdout=subprocess.PIPE,text=True)
(v/'trusted-phase-reference.patch').write_text(patch.stdout)
files={x.relative_to(v).as_posix():hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted((v/'Source').rglob('*')) if x.is_file()}
(v/'source-inventory.json').write_text(json.dumps({'scope':'Frozen private project Source prior to compile; standalone detector CPP only, not a full dependency closure.','sha256':files},indent=2)+'\n')
out=v/'chord-repicks-corrected-v2';out.mkdir()
sources=[str(p) for p in sorted((v/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp']
cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(b/'chord_repick_probe_v2.cpp'),*sources,'-o',str(out/'probe')]
(out/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
start=time.monotonic()
r=subprocess.run(cmd,stdout=(out/'compile.stdout').open('w'),stderr=(out/'compile.stderr').open('w'))
(out/'compile-result.json').write_text(json.dumps({'exit_code':r.returncode,'elapsed_s':time.monotonic()-start},indent=2)+'\n')
print('compile',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
allowed=sorted(os.sched_getaffinity(0));cpu=3 if 3 in allowed else allowed[0]
cmd=['taskset','-c',str(cpu),str(out/'probe')]
(out/'run-command.json').write_text(json.dumps(cmd,indent=2)+'\n');start=time.monotonic()
r=subprocess.run(cmd,stdout=(out/'events.csv').open('w'),stderr=(out/'run.stderr').open('w'))
(out/'run-result.json').write_text(json.dumps({'exit_code':r.returncode,'elapsed_s':time.monotonic()-start,'cpu':cpu,'performance_claim':False},indent=2)+'\n')
print('capture',r.returncode,flush=True)
raise SystemExit(r.returncode)
