from pathlib import Path
import shutil,hashlib,json,subprocess,time,os
b=Path(__file__).parent;original=Path('/tmp/flysnapper-v4-uncertainty-profile-combo/fast-Source')
v=b/'uncertainty-fast-measured-root-guards';inp=b/'uncertainty-fast-input'
if (v/'compile-command.json').exists():raise RuntimeError('Refuse to mutate compiled frozen factor')
assert hashlib.sha256((original/'Pitch/TimeDomainPursuit.cpp').read_bytes()).hexdigest()=='88b78c18070ece578532c07dd2be2393136402658d67922ef11a3d75698e92c2'
for folder in (v,inp):folder.mkdir(exist_ok=True);shutil.copytree(original,folder/'Source',dirs_exist_ok=True)
patch=b/'measured-root-guards/restored-measured-evidence.patch';(v/'applied-measured-evidence.patch').write_bytes(patch.read_bytes())
# The profile-fast loop changes one old patch context. Transfer exactly
# the unchanged inserted physical-evidence blocks; no existing input bytes
# are removed or changed. Retain a new actual-input diff for audit.
g=(b/'measured-root-guards/Source/Pitch/TimeDomainPursuit.cpp').read_text()
p=v/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text();input_text=s
start=g.index('    const int anchor=evidence&&');stop=g.index('    // Private explicit standard-six-string prior:',start)
needle='    // Private explicit standard-six-string prior:';assert s.count(needle)==1;s=s.replace(needle,g[start:stop]+needle)
needle='    for(int iteration=0;iteration<maxVoices;++iteration){\n';assert s.count(needle)==1;s=s.replace(needle,needle+'        activeSupport.fill(-1);\n')
start=g.index('            // This is measured residual-wave evidence,');stop=g.index('            for(auto &a:observed)',start)
needle='            if(observedPower<1e-15)continue;\n';assert s.count(needle)==1;s=s.replace(needle,needle+g[start:stop])
start=g.index('    // The final evidence fit is independent of the imposed envelope.');stop=g.index('    for(int j=0;j<used;++j){const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];double magnitude=0;',start)
needle='    for(int j=0;j<used;++j){const int note=selected[size_t(j)];const auto& model=data->models[size_t(note)];double magnitude=0;\n';assert s.count(needle)==1;s=s.replace(needle,g[start:stop]+needle+'        if(!measuredSupported[size_t(j)])continue;\n')
p.write_text(s)
r=subprocess.run(['diff','-u',str(inp/'Source/Pitch/TimeDomainPursuit.cpp'),str(p)],stdout=subprocess.PIPE,text=True)
(v/'actual-factor.diff').write_text(r.stdout)
# Removing the five exact inserted blocks must reproduce the input byte-for-byte.
check=s.replace(g[start:stop],'',1).replace('        if(!measuredSupported[size_t(j)])continue;\n','',1)
hstart=g.index('    const int anchor=evidence&&');hstop=g.index('    // Private explicit standard-six-string prior:',hstart)
check=check.replace(g[hstart:hstop],'',1).replace('        activeSupport.fill(-1);\n','',1)
hstart=g.index('            // This is measured residual-wave evidence,');hstop=g.index('            for(auto &a:observed)',hstart)
check=check.replace(g[hstart:hstop],'',1)
assert check==input_text,'Factor transfer modified unrelated bytes'
# Prove bounded factor: only the pursuit implementation changed relative to
# the independently parity-checked fast+uncertainty input snapshot.
changed=[]
for a in sorted((inp/'Source').rglob('*')):
 if a.is_file():
  rel=a.relative_to(inp);c=v/rel
  if a.read_bytes()!=c.read_bytes():changed.append(rel.as_posix())
assert changed==['Source/Pitch/TimeDomainPursuit.cpp'],changed
for folder in (v,inp):
 files={x.relative_to(folder).as_posix():hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted((folder/'Source').rglob('*')) if x.is_file()}
 (folder/'source-inventory.json').write_text(json.dumps({'scope':'Frozen private project Source prior to compile; standalone detector CPP only, not full dependency closure.','sha256':files},indent=2)+'\n')
(v/'input-binding.json').write_text(json.dumps({'input_snapshot':str(inp),'origin':str(original),'input_TimeDomainPursuit_sha256':'88b78c18070ece578532c07dd2be2393136402658d67922ef11a3d75698e92c2','factor_patch_sha256':hashlib.sha256(patch.read_bytes()).hexdigest(),'changed_source_paths':changed,'phase_observer_compounded':False,'driver_lifecycle':'Fresh PitchDetector object for every technical configuration; original mutable-cache uncertainty model retained.'},indent=2)+'\n')
# Exact full-payload driver used in the preceding uncertainty/fast parity run.
driver=b/'uncertainty_fast_guard_probe.cpp';shutil.copyfile(Path('/tmp/flysnapper-v4-uncertainty-profile-combo/payload-cold.cpp'),driver)
sources=[str(p) for p in sorted((v/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp']
cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(v/'Source'),str(driver),*sources,'-o',str(v/'probe')]
(v/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n');start=time.monotonic()
r=subprocess.run(cmd,stdout=(v/'compile.stdout').open('w'),stderr=(v/'compile.stderr').open('w'))
(v/'compile-result.json').write_text(json.dumps({'exit_code':r.returncode,'elapsed_s':time.monotonic()-start},indent=2)+'\n');print('compile',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
allowed=sorted(os.sched_getaffinity(0));cpu=3 if 3 in allowed else allowed[0];cmd=['taskset','-c',str(cpu),str(v/'probe')]
(v/'run-command.json').write_text(json.dumps(cmd,indent=2)+'\n');start=time.monotonic()
r=subprocess.run(cmd,stdout=(v/'events.csv').open('w'),stderr=(v/'metrics.csv').open('w'))
(v/'run-result.json').write_text(json.dumps({'exit_code':r.returncode,'elapsed_s':time.monotonic()-start,'cpu':cpu,'performance_claim':False,'capture_configurations':330},indent=2)+'\n');print('capture',r.returncode,flush=True)
raise SystemExit(r.returncode)
