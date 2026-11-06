from pathlib import Path
import shutil,subprocess,hashlib,json,time
b=Path(__file__).parent
cpp=['PitchDetector.cpp','TimeDomainPursuit.cpp','YinEstimator.cpp','ArticulationTracker.cpp','PolyphonicTracker.cpp','NoiseConditioner.cpp','InputDynamics.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(label,source,driver,extra=()):
 out=b/label;out.mkdir(exist_ok=False);binary=out/'capture'
 command=['g++','-std=c++17','-O2','-DNDEBUG',*extra,'-I',str(source),str(driver),*[str(source/'Pitch'/s) for s in cpp],'-o',str(binary)]
 before={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.suffix in ('.cpp','.h')}
 with (out/'compile.txt').open('w') as log:r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
 assert r.returncode==0,(label,(out/'compile.txt').read_text())
 assert before=={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.suffix in ('.cpp','.h')}
 meta={'command':command,'source_sha256':before,'driver_sha256':sha(driver),'executable_sha256':sha(binary),'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0]}
 (out/'build.json').write_text(json.dumps(meta,indent=2)+'\n');return out,binary,meta
def run(label,source,driver,args=(),extra=()):
 out,binary,meta=build(label,source,driver,extra);start=time.perf_counter()
 with (out/'events.csv').open('w') as events,(out/'metrics.csv').open('w') as metrics:r=subprocess.run([str(binary),*args],stdout=events,stderr=metrics)
 meta.update(exit_code=r.returncode,elapsed_seconds=time.perf_counter()-start,events_sha256=sha(out/'events.csv'),metrics_sha256=sha(out/'metrics.csv'))
 (out/'run.json').write_text(json.dumps(meta,indent=2)+'\n');assert r.returncode==0;return meta
if __name__=='__main__':
 results={}
 for label in ('baseline','fast'):
  results[label]=run('cold330-'+label,b/(label+'-Source'),b/'probe-cold.cpp')
 print(json.dumps(results,indent=2))
