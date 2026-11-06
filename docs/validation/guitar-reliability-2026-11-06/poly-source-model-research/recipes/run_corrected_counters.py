from pathlib import Path
import subprocess,hashlib
b=Path(__file__).parent
for source_name,driver,out_name in [('time-atoms-per-voice-phase-v2','chord_repick_probe_v1_fresh.cpp','lifecycle-fresh-v1-counter'),('baseline','chord_repick_probe_v2.cpp','chord-repicks-corrected-v2-baseline'),('time-atoms-per-voice-phase-v2','chord_repick_probe_v2.cpp','chord-repicks-corrected-v2')]:
 source=b/('Source' if source_name=='baseline' else source_name+'/Source');out=b/out_name if source_name=='baseline' else b/source_name/out_name;out.mkdir(exist_ok=True)
 src=[str(p) for p in (source/'Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp'];r=subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source),str(b/driver),*src,'-o',str(out/'probe')],stdout=(out/'compile.log').open('w'),stderr=subprocess.STDOUT);print(out_name,'compile',r.returncode,flush=True)
 if not r.returncode:
  with (out/'events.csv').open('w') as f:subprocess.run([str(out/'probe')],stdout=f,check=True)
 print(out_name,'capture complete',flush=True)
 if driver=='chord_repick_probe_v1_fresh.cpp':print('cold_matches_reused', (out/'events.csv').read_bytes()==(b/source_name/'chord-repicks/events.csv').read_bytes(),flush=True)
